#include "Plugins/PluginRack.h"

const char* PluginRack::slotName (int id)
{
    static const char* names[] = { "PreAmp", "Amp", "Post", "Slot 4" };
    if (id < 0 || id >= NumSlots) return "Slot";
    return names[id];
}

PluginRack::PluginRack (PluginHost& h)
    : host (h)
{
}

PluginRack::~PluginRack()
{
    release();
    if (persistSlots)
        saveSlotState();
}

juce::File PluginRack::slotStateFile() const
{
    return host.settingsFile().getSiblingFile ("plugin-slots.xml");
}

void PluginRack::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    maxBlock = juce::jmax (samplesPerBlock, 512);

    for (auto& slot : slots)
    {
        const juce::ScopedLock sl (slot.lock);
        slot.buffer.setSize (2, maxBlock, false, true, true);
        if (slot.instance != nullptr)
        {
            slot.ready.store (0, std::memory_order_release);
            slot.seq.fetch_add (1, std::memory_order_acq_rel);
            slot.live.store (nullptr, std::memory_order_release);
            for (int i = 0; i < 50 && slot.inAudio.load() != 0; ++i)
                juce::Thread::sleep (1);
            slot.instance->releaseResources();
            slot.instance->setRateAndBufferSizeDetails (sampleRate, maxBlock);
            slot.instance->prepareToPlay (sampleRate, maxBlock);
            slot.latency.store (slot.instance->getLatencySamples(), std::memory_order_relaxed);
            slot.live.store (slot.instance.get(), std::memory_order_release);
            slot.ready.store (1, std::memory_order_release);
            slot.seq.fetch_add (1, std::memory_order_release);
        }
    }
    refreshLatency();
}

void PluginRack::reset()
{
    for (auto& slot : slots)
    {
        const juce::ScopedLock sl (slot.lock);
        if (slot.instance != nullptr)
            slot.instance->reset();
        slot.midi.clear();
        slot.buffer.clear();
    }
}

void PluginRack::closeEditorLocked (Slot& slot)
{
    slot.editor.reset();
}

void PluginRack::release()
{
    for (int i = 0; i < NumSlots; ++i)
        unloadPlugin (i);
}

void PluginRack::refreshLatency() noexcept
{
    int sum = 0;
    for (auto& s : slots)
    {
        if (s.ready.load (std::memory_order_acquire) == 0)
            continue;
        if (s.bypass.load (std::memory_order_relaxed) != 0)
            continue;
        sum += s.latency.load (std::memory_order_relaxed);
    }
    latencySum.store (sum, std::memory_order_relaxed);
}

void PluginRack::retireInstance (Slot& slot)
{
    slot.ready.store (0, std::memory_order_release);
    slot.seq.fetch_add (1, std::memory_order_acq_rel); // odd: writer
    slot.live.store (nullptr, std::memory_order_release);
    for (int i = 0; i < 200 && slot.inAudio.load (std::memory_order_acquire) != 0; ++i)
        juce::Thread::sleep (1);
    closeEditorLocked (slot);
    if (slot.instance != nullptr)
    {
        slot.instance->suspendProcessing (true);
        slot.instance->releaseResources();
    }
    slot.instance.reset();
    slot.latency.store (0, std::memory_order_relaxed);
    slot.seq.fetch_add (1, std::memory_order_release); // even
}

void PluginRack::publishInstance (Slot& slot, std::unique_ptr<juce::AudioPluginInstance> inst,
                                  const juce::PluginDescription& desc, bool bypassed)
{
    if (inst == nullptr)
        return;
    const int ch = juce::jmax (2, inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels());
    slot.buffer.setSize (ch, maxBlock, false, true, true);
    slot.seq.fetch_add (1, std::memory_order_acq_rel);
    slot.live.store (nullptr, std::memory_order_release);
    for (int i = 0; i < 200 && slot.inAudio.load (std::memory_order_acquire) != 0; ++i)
        juce::Thread::sleep (1);
    closeEditorLocked (slot);
    if (slot.instance != nullptr)
    {
        slot.instance->suspendProcessing (true);
        slot.instance->releaseResources();
    }
    slot.instance = std::move (inst);
    slot.desc = desc;
    slot.bypass.store (bypassed ? 1 : 0, std::memory_order_relaxed);
    slot.latency.store (slot.instance->getLatencySamples(), std::memory_order_relaxed);
    slot.live.store (slot.instance.get(), std::memory_order_release);
    slot.ready.store (1, std::memory_order_release);
    slot.seq.fetch_add (1, std::memory_order_release);
}

void PluginRack::refreshVstAmpFlag() noexcept
{
    auto& s = slots[(size_t) AmpReplace];
    const bool on = s.ready.load (std::memory_order_acquire) != 0
                 && s.bypass.load (std::memory_order_relaxed) == 0
                 && s.instance != nullptr;
    vstAmpActive.store (on ? 1 : 0, std::memory_order_relaxed);
    refreshLatency();
}

void PluginRack::setBypass (int slot, bool bypass) noexcept
{
    if (slot >= 0 && slot < NumSlots)
        slots[(size_t) slot].bypass.store (bypass ? 1 : 0, std::memory_order_relaxed);
    refreshVstAmpFlag();
}

bool PluginRack::isBypassed (int slot) const noexcept
{
    if (slot >= 0 && slot < NumSlots)
        return slots[(size_t) slot].bypass.load (std::memory_order_relaxed) != 0;
    return true;
}

bool PluginRack::isReady (int slot) const noexcept
{
    if (slot >= 0 && slot < NumSlots)
        return slots[(size_t) slot].ready.load (std::memory_order_relaxed) != 0;
    return false;
}

void PluginRack::process (SlotId id,
                          float* left,
                          float* right,
                          int numSamples,
                          const juce::MidiBuffer& guitarMidi) noexcept
{
    if (id < 0 || id >= NumSlots || left == nullptr || numSamples <= 0)
        return;

    auto& slot = slots[(size_t) id];
    if (slot.bypass.load (std::memory_order_relaxed) != 0)
        return;
    if (slot.ready.load (std::memory_order_acquire) == 0)
        return;

    slot.inAudio.fetch_add (1, std::memory_order_acq_rel);
    const auto seq0 = slot.seq.load (std::memory_order_acquire);
    if ((seq0 & 1u) != 0)
    {
        slot.inAudio.fetch_sub (1, std::memory_order_release);
        return; // swap in progress: pass dry
    }
    auto* inst = slot.live.load (std::memory_order_acquire);
    if (inst == nullptr || slot.seq.load (std::memory_order_acquire) != seq0)
    {
        slot.inAudio.fetch_sub (1, std::memory_order_release);
        return;
    }

    const int n = juce::jmin (numSamples, slot.buffer.getNumSamples());
    if (n <= 0)
        return;

    const int nCh = juce::jmax (1, slot.buffer.getNumChannels());
    if (right != nullptr && nCh > 1)
    {
        slot.buffer.copyFrom (0, 0, left, n);
        slot.buffer.copyFrom (1, 0, right, n);
        for (int ch = 2; ch < nCh; ++ch)
            slot.buffer.clear (ch, 0, n);
    }
    else
    {
        for (int ch = 0; ch < nCh; ++ch)
            slot.buffer.copyFrom (ch, 0, left, n);
    }

    slot.midi.clear();
    if (inst->acceptsMidi())
        slot.midi.addEvents (guitarMidi, 0, n, 0);

    inst->processBlock (slot.buffer, slot.midi);

    if (right != nullptr && nCh > 1)
    {
        juce::FloatVectorOperations::copy (left, slot.buffer.getReadPointer (0), n);
        juce::FloatVectorOperations::copy (right, slot.buffer.getReadPointer (1), n);
    }
    else if (nCh > 1)
    {
        const float* l = slot.buffer.getReadPointer (0);
        const float* r = slot.buffer.getReadPointer (1);
        for (int i = 0; i < n; ++i)
            left[i] = 0.5f * (l[i] + r[i]);
    }
    else
    {
        juce::FloatVectorOperations::copy (left, slot.buffer.getReadPointer (0), n);
        if (right != nullptr)
            juce::FloatVectorOperations::copy (right, slot.buffer.getReadPointer (0), n);
    }

    slot.inAudio.fetch_sub (1, std::memory_order_release);
}

bool PluginRack::hasReadySlot() const noexcept
{
    for (int i = 0; i < NumSlots; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (s.ready.load (std::memory_order_acquire) != 0
            && s.bypass.load (std::memory_order_relaxed) == 0)
            return true;
    }
    return false;
}

void PluginRack::processChain (float* left, float* right, int numSamples,
                               const juce::MidiBuffer& midi) noexcept
{
    if (left == nullptr || numSamples <= 0 || ! hasReadySlot())
        return;
    process (PreAmp,     left, right, numSamples, midi);
    process (AmpReplace, left, right, numSamples, midi);
    process (Post,       left, right, numSamples, midi);
    process (Slot4,      left, right, numSamples, midi);
}

juce::String PluginRack::loadPlugin (int slotIndex, const juce::PluginDescription& desc)
{
    return loadPlugin (slotIndex, desc, juce::MemoryBlock{});
}

juce::String PluginRack::loadPlugin (int slotIndex, const juce::PluginDescription& desc,
                                     const juce::MemoryBlock& state)
{
    if (slotIndex < 0 || slotIndex >= NumSlots)
        return "Invalid slot";

    const double sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    const int block = juce::jmax (maxBlock, 512);
    const bool persist = persistSlots;
    juce::Thread::launch ([this, slotIndex, desc, state, sr, block, persist]
    {
        juce::String error;
        auto inst = host.formatManager.createPluginInstance (desc, sr, block, error);
        if (inst == nullptr)
        {
            juce::MessageManager::callAsync ([error]
            {
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "VST3",
                    error.isNotEmpty() ? error : juce::String ("Failed to instantiate plugin"));
            });
            return;
        }

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        if (! inst->setBusesLayout (layout))
            inst->enableAllBuses();
        inst->setRateAndBufferSizeDetails (sr, block);
        inst->prepareToPlay (sr, block);
        if (state.getSize() > 0)
            inst->setStateInformation (state.getData(), (int) state.getSize());
        inst->suspendProcessing (false);

        auto& slot = slots[(size_t) slotIndex];
        {
            const juce::ScopedLock sl (slot.lock);
            publishInstance (slot, std::move (inst), desc, false);
        }
        refreshVstAmpFlag();
        if (persist)
            saveSlotState();
    });
    return {};
}

void PluginRack::unloadPlugin (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= NumSlots)
        return;
    auto& slot = slots[(size_t) slotIndex];
    {
        const juce::ScopedLock sl (slot.lock);
        retireInstance (slot);
        slot.desc = {};
    }
    refreshVstAmpFlag();
    if (persistSlots)
        saveSlotState();
}

juce::String PluginRack::getSlotPluginName (int slot) const
{
    if (slot < 0 || slot >= NumSlots)
        return {};
    auto& s = slots[(size_t) slot];
    const juce::ScopedLock sl (s.lock);
    if (s.instance == nullptr)
        return {};
    return s.desc.name;
}

juce::PluginDescription PluginRack::getSlotDescription (int slot) const
{
    if (slot < 0 || slot >= NumSlots)
        return {};
    auto& s = slots[(size_t) slot];
    const juce::ScopedLock sl (s.lock);
    return s.desc;
}

void PluginRack::showEditor (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= NumSlots)
        return;

    auto& slot = slots[(size_t) slotIndex];
    const juce::ScopedLock sl (slot.lock);
    if (slot.instance == nullptr)
        return;

    if (slot.editor != nullptr)
    {
        slot.editor->setVisible (true);
        slot.editor->toFront (true);
        return;
    }

    if (! slot.instance->hasEditor())
        return;

    if (auto* ed = slot.instance->createEditor())
    {
        const auto title = juce::String (slotName (slotIndex)) + " — " + slot.desc.name;
        slot.editor = std::make_unique<EditorWindow> (title, *slot.instance, ed);
    }
}

void PluginRack::closeEditor (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= NumSlots)
        return;
    auto& slot = slots[(size_t) slotIndex];
    const juce::ScopedLock sl (slot.lock);
    closeEditorLocked (slot);
}

void PluginRack::saveToXml (juce::XmlElement& xml) const
{
    for (int i = 0; i < NumSlots; ++i)
    {
        auto& slot = slots[(size_t) i];
        const juce::ScopedLock sl (slot.lock);
        auto* s = xml.createNewChildElement ("Slot");
        s->setAttribute ("index", i);
        s->setAttribute ("bypass", slot.bypass.load() ? 1 : 0);
        const auto d = slot.desc;
        s->setAttribute ("id", d.createIdentifierString());
        s->setAttribute ("file", d.fileOrIdentifier);
        s->setAttribute ("name", d.name);
        if (slot.instance != nullptr)
        {
            for (int k = 0; k < 50 && slot.inAudio.load() != 0; ++k)
                juce::Thread::sleep (1);
            juce::MemoryBlock mb;
            slot.instance->getStateInformation (mb);
            if (mb.getSize() > 0)
                s->createNewChildElement ("State")->addTextElement (mb.toBase64Encoding());
        }
    }
}

void PluginRack::loadFromXml (const juce::XmlElement& xml)
{
    const auto types = host.knownList.getTypes();
    for (auto* child = xml.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (child->getTagName() != "Slot")
            continue;
        const int idx = child->getIntAttribute ("index", -1);
        if (idx < 0 || idx >= NumSlots)
            continue;
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;
        juce::PluginDescription match;
        bool found = false;
        for (const auto& t : types)
        {
            if (t.createIdentifierString() == id || t.fileOrIdentifier == child->getStringAttribute ("file"))
            {
                match = t;
                found = true;
                break;
            }
        }
        if (! found)
            continue;
        juce::MemoryBlock state;
        if (auto* st = child->getChildByName ("State"))
            state.fromBase64Encoding (st->getAllSubText());
        persistSlots = false;
        loadPlugin (idx, match, state);
        persistSlots = true;
        setBypass (idx, child->getIntAttribute ("bypass", 0) != 0);
    }
}

void PluginRack::saveSlotState()
{
    juce::XmlElement xml ("Slots");
    for (int i = 0; i < NumSlots; ++i)
    {
        auto* s = xml.createNewChildElement ("Slot");
        s->setAttribute ("index", i);
        s->setAttribute ("bypass", isBypassed (i) ? 1 : 0);
        const auto d = getSlotDescription (i);
        s->setAttribute ("id", d.createIdentifierString());
        s->setAttribute ("file", d.fileOrIdentifier);
        s->setAttribute ("name", d.name);
    }
    xml.writeTo (slotStateFile());
}

void PluginRack::restoreSlotState()
{
    auto xml = juce::XmlDocument::parse (slotStateFile());
    if (xml == nullptr)
        return;

    const auto types = host.knownList.getTypes();
    for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (child->getTagName() != "Slot")
            continue;
        const int idx = child->getIntAttribute ("index", -1);
        if (idx < 0 || idx >= NumSlots)
            continue;
        const auto id = child->getStringAttribute ("id");
        if (id.isEmpty())
            continue;
        juce::PluginDescription match;
        bool found = false;
        for (const auto& t : types)
        {
            if (t.createIdentifierString() == id || t.fileOrIdentifier == child->getStringAttribute ("file"))
            {
                match = t;
                found = true;
                break;
            }
        }
        if (! found)
            continue;
        persistSlots = false;
        loadPlugin (idx, match);
        persistSlots = true;
        setBypass (idx, child->getIntAttribute ("bypass", 0) != 0);
    }
}
