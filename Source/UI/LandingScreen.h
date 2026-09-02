#pragma once

#include <JuceHeader.h>
#include <functional>
#include "SessionSettings.h"
#include "UI/SessionLookAndFeel.h"

class SessionProcessor;

/** Pre-session lobby: roster chairs + jam setup. Shown before the DAW. */
class LandingScreen : public juce::Component,
                      private juce::Timer
{
public:
    explicit LandingScreen (SessionProcessor& processor);
    ~LandingScreen() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    SessionSettings::Setup getSetup() const;
    void setSetup (const SessionSettings::Setup& s);
    void loadSettings();
    void persistSetup();
    void setDeviceStatus (const juce::String& text);

    std::function<void()> onEnterSession;
    std::function<void()> onShowAudioSettings;

private:
    class ChairCard : public juce::Component
    {
    public:
        enum Kind { Drums, Bass, Keys, Fx };

        ChairCard (Kind k, juce::Colour accent);
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void setSeated (bool v);
        bool isSeated() const noexcept { return seated; }
        std::function<void()> onChanged;

    private:
        void drawGlyph (juce::Graphics& g, juce::Rectangle<float> r, bool on) const;

        Kind kind;
        juce::Colour accent;
        bool seated = true;
    };

    void timerCallback() override;
    float computeScale() const noexcept;
    int sx (float v) const noexcept { return (int) std::round (v * uiScale); }
    juce::Font uiFont (float px, bool bold = false) const;
    void applyFonts();
    void refreshRosterLabel();
    void fillVoiceBoxes();
    void suggestVoicesFromStyle (int style, bool force);
    bool keyPressed (const juce::KeyPress& key) override;

    SessionProcessor& proc;
    SessionLookAndFeel look;
    float uiScale = 1.0f;

    juce::Label title, subtitle, rosterLbl, setupLbl;
    ChairCard drumsCard { ChairCard::Drums, juce::Colour (SessionLookAndFeel::kDrums) };
    ChairCard bassCard  { ChairCard::Bass,  juce::Colour (SessionLookAndFeel::kBass)  };
    ChairCard keysCard  { ChairCard::Keys,  juce::Colour (SessionLookAndFeel::kKeys)  };
    ChairCard fxCard    { ChairCard::Fx,    juce::Colour (SessionLookAndFeel::kFx)    };
    juce::ComboBox drumsKitBox, bassVoiceBox, keysVoiceBox, fxVoiceBox;
    int lastStyleForSuggest = 0;

    juce::Label styleLbl, formLbl, scaleLbl, feelLbl, keyLbl, bpmLbl;
    juce::ComboBox styleBox, formBox, scaleBox, feelBox, keyBox;
    juce::ToggleButton lockKey { "Lock key" };
    juce::ToggleButton slewToggle { "Follow tempo" };
    juce::Slider bpmSlider;
    juce::TextButton enterBtn { "ENTER SESSION" };
    juce::TextButton audioBtn { "Audio" };
    juce::Label deviceLbl;

    juce::Rectangle<int> inMeterBounds;
    juce::Rectangle<int> chairsBounds, setupBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LandingScreen)
};
