#include "MainComponent.h"
#include "SessionSettings.h"

namespace
{
    bool isDigitalName (const juce::String& n)
    {
        return n.containsIgnoreCase ("Digital")
            || n.containsIgnoreCase ("S/PDIF")
            || n.containsIgnoreCase ("SPDIF")
            || n.containsIgnoreCase ("S/P-DIF")
            || n.containsIgnoreCase ("Digital Audio Interface");
    }

    bool isLine6Name (const juce::String& n)
    {
        return n.containsIgnoreCase ("UX2")
            || n.containsIgnoreCase ("Line 6")
            || n.containsIgnoreCase ("POD Studio")
            || n.containsIgnoreCase ("TonePort");
    }

    bool isAsio4AllName (const juce::String& n)
    {
        return n.containsIgnoreCase ("ASIO4ALL")
            || n.containsIgnoreCase ("ASIO 4 ALL");
    }

    bool isExclusiveTypeName (const juce::String& n)
    {
        return n.containsIgnoreCase ("Exclusive");
    }

    bool isAsioTypeName (const juce::String& n)
    {
        return n.containsIgnoreCase ("ASIO");
    }

    bool isRealtekName (const juce::String& n)
    {
        return n.containsIgnoreCase ("Realtek")
            || n.containsIgnoreCase ("High Definition Audio");
    }

    // Analog Line 6 / ASIO UX2 only. Digital Audio Interface is never guitar.
    int scoreIn (const juce::String& n, bool asioType)
    {
        if (n.isEmpty() || isDigitalName (n) || isAsio4AllName (n) || isRealtekName (n))
            return -1;
        if (! isLine6Name (n))
            return -1;
        int s = asioType ? 40 : 20;
        if (n.containsIgnoreCase ("Guitar") || n.containsIgnoreCase ("Instrument"))
            s += 80;
        if (n.containsIgnoreCase ("Microphone") || n.containsIgnoreCase ("Mic "))
            s += 50;
        if (n.containsIgnoreCase ("Analog"))
            s += 20;
        else if (n.containsIgnoreCase ("Line") && ! n.containsIgnoreCase ("Line 6"))
            s += 20;
        return s;
    }

    int scoreOut (const juce::String& n, bool asioType)
    {
        if (n.isEmpty() || isDigitalName (n) || isAsio4AllName (n) || isRealtekName (n))
            return -1;
        if (! isLine6Name (n))
            return -1;
        int s = asioType ? 40 : 20;
        if (n.containsIgnoreCase ("Speaker") || n.containsIgnoreCase ("Headphone"))
            s += 80;
        if (n.containsIgnoreCase ("Analog"))
            s += 20;
        return s;
    }

    bool isUsableLine6Input (const juce::String& n, const juce::String& typeName)
    {
        return scoreIn (n, isAsioTypeName (typeName)) > 0;
    }

    bool isUsableLine6Output (const juce::String& n, const juce::String& typeName)
    {
        return scoreOut (n, isAsioTypeName (typeName)) > 0;
    }

    bool savedSetupIsUnusable (const juce::XmlElement& xml)
    {
        const auto type = xml.getStringAttribute ("deviceType");
        juce::String inName = xml.getStringAttribute ("audioInputDeviceName");
        juce::String outName = xml.getStringAttribute ("audioOutputDeviceName");
        if (xml.getStringAttribute ("audioDeviceName").isNotEmpty())
        {
            if (inName.isEmpty())  inName  = xml.getStringAttribute ("audioDeviceName");
            if (outName.isEmpty()) outName = xml.getStringAttribute ("audioDeviceName");
        }
        const auto inChans = xml.getStringAttribute ("audioDeviceInChans");

        if (isExclusiveTypeName (type))
            return true;
        if (isDigitalName (inName) || isDigitalName (outName))
            return true;
        if (inName.isEmpty())
            return true;
        if (inChans.isNotEmpty() && ! inChans.containsChar ('1'))
            return true;
        if (! isAsioTypeName (type) && isDigitalName (inName))
            return true;
        return false;
    }
}

MainComponent::MainComponent()
{
    setOpaque (true);
    setSize (1280, 800);
    addAndMakeVisible (landing);
    addChildComponent (ui);
    ui.setVisible (false);
    landing.onEnterSession = [this] { enterSession(); };
    landing.onShowAudioSettings = [this] { showAudioDeviceSelector(); };
    ui.onShowAudioSettings = [this] { showAudioDeviceSelector(); };
    ui.onBackToLobby = [this] { returnToLobby(); };

    auto xml = juce::XmlDocument::parse (SessionSettings::deviceXml());
    const juce::XmlElement* useXml = nullptr;
    if (xml != nullptr && ! savedSetupIsUnusable (*xml))
        useXml = xml.get();

    // Plenty of channels so a later setup can enable guitar + stereo.
    deviceManager.initialise (8, 2, useXml, true);
    deviceManager.addAudioCallback (this);

    if (useXml == nullptr)
        applyPreferredDeviceSetup();
    else
        repairInputChannelsIfDeaf();
    preferLine6IfPresent();
    ensureGuitarMonitor();
    refreshDeviceStatus();

    deviceManager.addChangeListener (this);
}

void MainComponent::enterSession()
{
    const auto setup = landing.getSetup();
    processor.applyJamSetup (setup);
    ui.syncFromSetup (setup);
    landing.setVisible (false);
    ui.setVisible (true);
    processor.startSession();
    ui.saveSettings();
    resized();
}

void MainComponent::returnToLobby()
{
    if (processor.isRecording())
        processor.stopRecording();
    processor.stopSession();
    landing.setSetup (ui.readSetup());
    ui.setVisible (false);
    landing.setVisible (true);
    landing.grabKeyboardFocus();
    refreshDeviceStatus();
    resized();
}

MainComponent::~MainComponent()
{
    deviceManager.removeChangeListener (this);
    saveDeviceState();
    ui.saveSettings();
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

void MainComponent::saveDeviceState()
{
    if (auto xml = std::unique_ptr<juce::XmlElement> (deviceManager.createStateXml()))
        xml->writeTo (SessionSettings::deviceXml());
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    saveDeviceState();
    refreshDeviceStatus();
}

void MainComponent::refreshDeviceStatus()
{
    const auto type = deviceManager.getCurrentAudioDeviceType();
    const auto setup = deviceManager.getAudioDeviceSetup();
    auto* dev = deviceManager.getCurrentAudioDevice();
    juce::String s;
    s << (type.isNotEmpty() ? type : juce::String ("(no type)"));
    s << "  |  " << (setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : juce::String ("(no in)"));
    s << "  →  " << (setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : juce::String ("(no out)"));
    if (dev == nullptr)
        s << "  (device closed)";
    else
    {
        s << "  " << juce::String (dev->getCurrentSampleRate() / 1000.0, 1) << " kHz";
        s << " / " << juce::String (dev->getCurrentBufferSizeSamples());
        const int nIn = setup.inputChannels.countNumberOfSetBits();
        s << "  in" << nIn;
    }
    landing.setDeviceStatus (s);
}

void MainComponent::ensureGuitarMonitor()
{
    processor.setGate (0.0f);
    processor.setBusMute (SessionProcessor::Guitar, false);
    auto& tracks = processor.getDaw().getProject().tracks;
    tracks[(size_t) Daw::kGuitar].mute.store (0, std::memory_order_relaxed);
    tracks[(size_t) Daw::kGuitar].monitor.store (1, std::memory_order_relaxed);
}

bool MainComponent::commitDeviceSetup (juce::AudioDeviceManager::AudioDeviceSetup setup)
{
    if (isDigitalName (setup.inputDeviceName))
        setup.inputDeviceName.clear();
    if (isDigitalName (setup.outputDeviceName))
        setup.outputDeviceName.clear();

    setup.outputChannels.clear();
    setup.outputChannels.setRange (0, 2, true);
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    if (setup.bufferSize > 0 && setup.bufferSize < 256)
        setup.bufferSize = 256;

    auto tryOnce = [this, &setup] (int nInBits, double sr) -> bool
    {
        setup.inputChannels.clear();
        if (nInBits > 0)
            setup.inputChannels.setRange (0, nInBits, true);
        setup.sampleRate = sr;
        const auto err = deviceManager.setAudioDeviceSetup (setup, true);
        auto* dev = deviceManager.getCurrentAudioDevice();
        return err.isEmpty() && dev != nullptr && dev->getCurrentSampleRate() > 1.0;
    };

    const double firstSr = setup.sampleRate > 1.0 ? setup.sampleRate : 44100.0;
    const int bitTries[] = { 4, 2, 1, 0 };
    const double rates[] = { firstSr, 48000.0, 44100.0 };

    for (double sr : rates)
        for (int bits : bitTries)
            if (tryOnce (bits, sr))
                return true;

    // Last resort: do not leave the device closed.
    if (deviceManager.getCurrentAudioDevice() == nullptr)
        deviceManager.initialiseWithDefaultDevices (8, 2);
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

void MainComponent::repairInputChannelsIfDeaf()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup (setup);

    const auto type = deviceManager.getCurrentAudioDeviceType();
    if (isDigitalName (setup.inputDeviceName) || isExclusiveTypeName (type))
        return;

    const int have = setup.inputChannels.countNumberOfSetBits();
    bool need = have == 0;
    if (isUsableLine6Input (setup.inputDeviceName, type) || isAsioTypeName (type))
        for (int b = 0; b < 4; ++b)
            if (! setup.inputChannels[b])
                need = true;
    if (setup.bufferSize > 0 && setup.bufferSize < 256)
        need = true;

    if (need)
        commitDeviceSetup (setup);
}

void MainComponent::applyPreferredDeviceSetup()
{
    auto pickType = [this] (const juce::String& needle, bool allowExclusive) -> juce::String
    {
        for (auto* t : deviceManager.getAvailableDeviceTypes())
        {
            if (t == nullptr)
                continue;
            const auto name = t->getTypeName();
            if (! name.containsIgnoreCase (needle))
                continue;
            if (! allowExclusive && isExclusiveTypeName (name))
                continue;
            return name;
        }
        return {};
    };

    // ASIO UX2 first. Never Exclusive as the first choice (fails if anything else has the device).
    juce::String chosen = pickType ("ASIO", true);
    if (chosen.isEmpty()) chosen = pickType ("Windows Audio", false);
    if (chosen.isEmpty()) chosen = pickType ("WASAPI", false);
    if (chosen.isEmpty()) chosen = pickType ("Windows Audio", true);
    if (chosen.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType (chosen, true);

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type != nullptr)
            type->scanForDevices();
}

void MainComponent::preferLine6IfPresent()
{
    juce::AudioDeviceManager::AudioDeviceSetup current;
    deviceManager.getAudioDeviceSetup (current);
    const auto curType = deviceManager.getCurrentAudioDeviceType();
    const bool alreadyAnalog = isUsableLine6Input (current.inputDeviceName, curType)
                            && isUsableLine6Output (current.outputDeviceName, curType);
    if (alreadyAnalog)
    {
        repairInputChannelsIfDeaf();
        return;
    }

    auto tryTypes = [&] (bool asioOnly) -> bool
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (type == nullptr)
                continue;

            const auto typeName = type->getTypeName();
            const bool isAsio = isAsioTypeName (typeName);
            if (asioOnly != isAsio)
                continue;
            if (! asioOnly && isExclusiveTypeName (typeName))
                continue;

            type->scanForDevices();
            const auto inputs  = type->getDeviceNames (true);
            const auto outputs = type->getDeviceNames (false);

            juce::String inName, outName;
            int bestIn = 0, bestOut = 0;
            for (auto& n : inputs)
            {
                const int s = scoreIn (n, isAsio);
                if (s > bestIn) { bestIn = s; inName = n; }
            }
            for (auto& n : outputs)
            {
                const int s = scoreOut (n, isAsio);
                if (s > bestOut) { bestOut = s; outName = n; }
            }

            // ASIO UX2 is one driver name without "Speakers"/"Guitar".
            if (inName.isEmpty())
            {
                for (auto& n : inputs)
                    if (isLine6Name (n) && ! isDigitalName (n) && ! isAsio4AllName (n))
                    { inName = n; break; }
            }
            if (outName.isEmpty())
            {
                for (auto& n : outputs)
                    if (isLine6Name (n) && ! isDigitalName (n) && ! isAsio4AllName (n))
                    { outName = n; break; }
            }

            if (inName.isEmpty() && outName.isEmpty())
                continue;

            // If ASIO UX2 exists, never keep Realtek / Digital in.
            deviceManager.setCurrentAudioDeviceType (typeName, true);

            juce::AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup (setup);
            if (inName.isNotEmpty())
                setup.inputDeviceName = inName;
            else
                setup.inputDeviceName.clear();
            if (outName.isNotEmpty())
                setup.outputDeviceName = outName;
            else if (inName.isNotEmpty() && outputs.contains (inName))
                setup.outputDeviceName = inName;

            if (isDigitalName (setup.inputDeviceName) || isRealtekName (setup.inputDeviceName))
                setup.inputDeviceName = inName;

            setup.sampleRate = 44100.0;
            setup.bufferSize = 256;
            if (commitDeviceSetup (setup))
                return true;
        }
        return false;
    };

    if (! tryTypes (true))
        tryTypes (false);
}

void MainComponent::showAudioDeviceSelector()
{
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type != nullptr)
            type->scanForDevices();

    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                             0, 8, 2, 8,
                                                             false, false, true, false);
    selector->setSize (540, 500);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (selector);
    opts.dialogTitle = "Audio Devices";
    opts.dialogBackgroundColour = juce::Colour (0xff181c24);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.launchAsync();
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double sr = (device != nullptr) ? device->getCurrentSampleRate() : 44100.0;
    const int block = (device != nullptr) ? device->getCurrentBufferSizeSamples() : 256;
    processor.prepare (sr, juce::jmax (block, 256), 2);
}

void MainComponent::audioDeviceStopped()
{
    processor.release();
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    processor.processDuplex (inputChannelData, numInputChannels,
                             outputChannelData, numOutputChannels,
                             numSamples);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101216));
}

void MainComponent::resized()
{
    const auto b = getLocalBounds();
    landing.setBounds (b);
    ui.setBounds (b);
}
