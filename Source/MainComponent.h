#pragma once

#include <JuceHeader.h>
#include "SessionProcessor.h"
#include "UI/SessionUI.h"
#include "UI/LandingScreen.h"

class MainComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void showAudioDeviceSelector();

private:
    void applyPreferredDeviceSetup();
    void preferLine6IfPresent();
    void repairInputChannelsIfDeaf();
    void enterSession();
    void returnToLobby();
    bool commitDeviceSetup (juce::AudioDeviceManager::AudioDeviceSetup setup);
    void saveDeviceState();
    void refreshDeviceStatus();
    void ensureGuitarMonitor();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    SessionProcessor processor;
    juce::AudioDeviceManager deviceManager;
    LandingScreen landing { processor };
    SessionUI ui { processor, deviceManager };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
