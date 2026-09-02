#pragma once

#include <JuceHeader.h>
#include "Plugins/PluginHost.h"
#include <array>
#include <atomic>
#include <cstdint>

/** Four insert slots for hosted VST3s (adapted from Centrophy Engine).
    Instantiation/prepare off the audio thread; the audio thread only processBlock()s
    ready instances into preallocated buffers.
    Slots: PreAmp, AmpReplace, Post, Slot4. */
class PluginRack
{
public:
    enum SlotId : int
    {
        PreAmp = 0,
        AmpReplace,
        Post,
        Slot4,
        NumSlots
    };

    static const char* slotName (int id);

    int orderAt (int pos) const noexcept;
    void swapOrder (int posA, int posB);

    explicit PluginRack (PluginHost& host);
    ~PluginRack();

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void release();

    /** Audio thread: process a slot. `right` may be nullptr (mono in-place). RT-safe. */
    void process (SlotId id,
                  float* left,
                  float* right,
                  int numSamples,
                  const juce::MidiBuffer& guitarMidi) noexcept;

    void setBypass (int slot, bool bypass) noexcept;
    bool isBypassed (int slot) const noexcept;
    bool isReady (int slot) const noexcept;
    bool hasReadySlot() const noexcept;
    /** True when Slot 2 is loaded, ready, and not bypassed — skip AmpCab. */
    bool isVstAmpActive() const noexcept { return vstAmpActive.load (std::memory_order_relaxed) != 0; }
    int getLatencySamples() const noexcept { return latencySum.load (std::memory_order_relaxed); }

    PluginHost& getHost() noexcept { return host; }

    /** Audio thread: run all four slots in order. */
    void processChain (float* left, float* right, int numSamples,
                       const juce::MidiBuffer& midi) noexcept;

    /** Worker thread instantiate. Never call createPluginInstance on the audio thread. */
    juce::String loadPlugin (int slot, const juce::PluginDescription& desc);
    juce::String loadPlugin (int slot, const juce::PluginDescription& desc,
                             const juce::MemoryBlock& state);
    void unloadPlugin (int slot);

    void saveToXml (juce::XmlElement& xml) const;
    void loadFromXml (const juce::XmlElement& xml);

    juce::String getSlotPluginName (int slot) const;
    juce::PluginDescription getSlotDescription (int slot) const;

    /** Message thread only. */
    void showEditor (int slot);
    void closeEditor (int slot);

    /** Message thread: persist / restore last-loaded plugin identifiers per slot. */
    void saveSlotState();
    void restoreSlotState();
    void setPersistSlots (bool v) { persistSlots = v; }

private:
    struct EditorWindow : public juce::DocumentWindow
    {
        EditorWindow (const juce::String& name, juce::AudioProcessor& p, juce::AudioProcessorEditor* editor)
            : DocumentWindow (name, juce::Colour (0xff1a1f27), DocumentWindow::closeButton), proc (&p)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (editor, true);
            setResizable (true, false);
            setVisible (true);
        }

        ~EditorWindow() override
        {
            if (auto* e = dynamic_cast<juce::AudioProcessorEditor*> (getContentComponent()))
                if (proc != nullptr)
                    proc->editorBeingDeleted (e);
            clearContentComponent();
        }

        void closeButtonPressed() override { setVisible (false); }
        juce::AudioProcessor* proc = nullptr;
    };

    struct Slot
    {
        mutable juce::CriticalSection lock;
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::PluginDescription desc;
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer midi;
        std::unique_ptr<EditorWindow> editor;
        std::atomic<int> bypass { 1 };
        std::atomic<int> ready { 0 };
        std::atomic<juce::AudioPluginInstance*> live { nullptr };
        std::atomic<uint32_t> seq { 0 };
        std::atomic<int> inAudio { 0 };
        std::atomic<int> latency { 0 };
    };

    void closeEditorLocked (Slot& slot);
    void refreshVstAmpFlag() noexcept;
    void publishInstance (Slot& slot, std::unique_ptr<juce::AudioPluginInstance> inst,
                          const juce::PluginDescription& desc, bool bypassed);
    void retireInstance (Slot& slot);
    juce::File slotStateFile() const;
    std::atomic<int> vstAmpActive { 0 };
    std::atomic<int> latencySum { 0 };
    void refreshLatency() noexcept;

    juce::String orderString() const;
    void applyOrderString (const juce::String& s);

    PluginHost& host;
    std::array<Slot, NumSlots> slots;
    std::array<std::atomic<int>, NumSlots> order;
    bool persistSlots = true;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginRack)
};
