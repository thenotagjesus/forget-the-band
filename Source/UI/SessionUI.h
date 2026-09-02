#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include "SessionProcessor.h"
#include "UI/SessionLookAndFeel.h"
#include "UI/ArrangeView.h"

class SessionUI : public juce::Component,
                  private juce::Timer
{
public:
    SessionUI (SessionProcessor& processor, juce::AudioDeviceManager& devices);
    ~SessionUI() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void loadSettings();
    void saveSettings();
    void syncFromSetup (const SessionSettings::Setup& s);
    SessionSettings::Setup readSetup() const;

    std::function<void()> onShowAudioSettings;
    std::function<void()> onBackToLobby;

private:
    class MixerStrip : public juce::Component
    {
    public:
        MixerStrip (const juce::String& name, juce::Colour accent);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void setPeak (float p) noexcept { peak = p; }

        juce::Slider level;
        juce::Slider pan;
        juce::TextButton mute { "M" }, solo { "S" }, arm { "R" }, mon { "I" };
        juce::Label title;
        juce::Colour accent;
        float peak = 0.0f;
        juce::Rectangle<int> meterBounds;
        bool compact = false;
    };

    void timerCallback() override;
    float computeScale() const noexcept;
    int sx (float v) const noexcept { return (int) std::round (v * scale); }
    juce::Font uiFont (float px, bool bold = false) const;
    void applyFonts();
    void wireControls();
    void refreshTransport();
    void applyToProcessor();
    void markDirty();
    void drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float peak, juce::Colour c) const;
    void drawTuner (juce::Graphics& g, juce::Rectangle<int> r) const;
    void drawNeck (juce::Graphics& g, juce::Rectangle<int> r) const;
    void drawEnergy (juce::Graphics& g, juce::Rectangle<int> r, float v, juce::Colour c, const juce::String& lab) const;
    void drawNoteLane (juce::Graphics& g, juce::Rectangle<int> r) const;
    void exportPlayerMidi();
    void setupRotary (juce::Slider& s, double min, double max, double def, double step);
    void applyScaleMask();
    void fillPluginCombo (juce::ComboBox& box, PluginRack& rack, int slot);
    void bindPluginSlot (juce::ComboBox& box, juce::TextButton& byp, juce::TextButton& ed,
                         std::function<PluginRack&()> rackFn, int slot);
    void refreshPluginCombos();
    void refreshDawMixer();
    void doNewProject();
    void doOpenProject();
    void doSaveProject();
    void doBounce();
    PluginRack& selectedTrackRack();
    void selectDawTrack (int t);

    SessionProcessor& proc;
    juce::AudioDeviceManager& devices;
    SessionLookAndFeel look;
    float scale = 1.0f;
    bool settingsDirty = false;
    int persistTicks = 0;
    bool mixerFront = false;
    int selectedTrack = 0;

    juce::Label brand, product, keyReadout, bpmReadout, barReadout, recPath;
    juce::Label tunerNote, recTime, projectLbl;
    juce::ComboBox styleBox, keyBox, formBox, scaleBox, feelBox, phraseBox, delayDivBox;
    juce::ComboBox kitBox, bassVoiceBox, keysVoiceBox;
    juce::Label kitLbl, bassVoiceLbl, keysVoiceLbl;
    juce::ToggleButton autoKey { "Auto Key" }, autoBpm { "Auto BPM" }, countInToggle { "Count-in" };
    juce::ToggleButton lockTempo { "Lock Tempo" }, lockIntensity { "Lock Intensity" };
    juce::ToggleButton grooveFloor { "Keep Groove" }, fadeSilence { "Fade on rest" };
    juce::ToggleButton energyDrift { "Energy drift" };
    juce::TextButton calSoft { "Soft" }, calMid { "Mid" }, calHard { "Hard" };
    juce::Label chordName, nextChord, playerNote, formLbl, scaleLbl, feelLbl, phraseLbl, energyLbl;
    juce::Label noteLaneLbl;
    juce::Slider bpmSlider;
    juce::Slider gainSlider, gateSlider, driveSlider, toneSlider, levelSlider, delaySlider, spaceSlider;
    juce::Label styleLbl, driveLbl, toneLbl, levelLbl, bpmLbl, ampLbl, mixLbl;
    juce::Label gainLbl, gateLbl, delayLbl, spaceLbl, followLbl;
    juce::TextButton startBtn { "Start Session" }, stopBtn { "Stop" };
    juce::TextButton lobbyBtn { "Lobby" };
    juce::TextButton bpmDown { "-" }, bpmUp { "+" };
    juce::TextButton recordBtn { "Record" }, audioBtn { "Audio" };
    juce::TextButton playBtn { "Play" }, rtzBtn { "RTZ" }, cycleBtn { "Cycle" };
    juce::TextButton newBtn { "New" }, openBtn { "Open" }, saveBtn { "Save" };
    juce::TextButton bounceBtn { "Bounce" }, undoBtn { "Undo" }, midiBtn { "MIDI" };
    juce::TextButton viewArrange { "Arrange" }, viewMixer { "Mixer" };
    juce::ToggleButton ampBypass { "Amp Bypass" };
    juce::TextButton scanBtn { "Scan VST3" };
    juce::Label guitarVstLbl, trackVstLbl;

    MixerStrip guitarStrip { "Guitar", juce::Colour (SessionLookAndFeel::kGuitar) };
    MixerStrip drumsStrip  { "Drums",  juce::Colour (SessionLookAndFeel::kDrums)  };
    MixerStrip bassStrip   { "Bass",   juce::Colour (SessionLookAndFeel::kBass)   };
    MixerStrip keysStrip   { "Keys",   juce::Colour (SessionLookAndFeel::kKeys)   };
    MixerStrip masterStrip { "Master", juce::Colour (SessionLookAndFeel::kAccent) };

    std::array<std::unique_ptr<MixerStrip>, Daw::kNumTracks> dawStrips;

    ArrangeView arrange { proc.getDaw() };

    std::array<juce::ComboBox, 4> gSlot, tSlot;
    std::array<juce::TextButton, 4> gByp, gEd, tByp, tEd;
    std::array<juce::Label, 4> gSlotLbl, tSlotLbl;

    juce::Rectangle<int> headerBounds, transportBounds, followBounds, tunerBounds, inMeterBounds;
    juce::Rectangle<int> hudBounds, neckBounds, playerMeterBounds, bandMeterBounds, noteLaneBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionUI)
};
