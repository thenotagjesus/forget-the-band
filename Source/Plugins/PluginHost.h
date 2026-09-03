#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

/** Shared VST3 format manager + KnownPluginList (scan once, many racks). */
class PluginHost
{
public:
    PluginHost();
    ~PluginHost();

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownList;

    void scanDefaultVST3Paths (std::function<void()> onFinished);
    /** Import user-picked .vst3 files/folders off the message thread. onDone(addedCount). */
    void importVst3Files (const juce::Array<juce::File>& files, std::function<void(int added)> onDone);
    bool isScanning() const noexcept { return scanning.load (std::memory_order_relaxed) != 0; }
    juce::String getLastScanStatus() const { return lastScanStatus; }
    void setLastScanStatus (const juce::String& s) { lastScanStatus = s; }

    void loadPersistedList();
    void savePersistedList();

    juce::File settingsFile() const;
    juce::File deadMansPedalFile() const;
    juce::File starterFlagFile() const;
    juce::FileSearchPath defaultVST3Paths() const;

    /** True when the known list is empty or starter VST3s have never been seeded. */
    bool shouldAutoScan() const;
    bool isStarterSeeded() const;
    void markStarterSeeded();
    bool findTypeMatching (const juce::StringArray& needles, juce::PluginDescription& out) const;

private:
    std::atomic<int> scanning { 0 };
    juce::String lastScanStatus;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};
