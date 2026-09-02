#pragma once

#include <JuceHeader.h>

class SessionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 kBg      = 0xff101216;
    static constexpr juce::uint32 kPanel   = 0xff181c24;
    static constexpr juce::uint32 kPanel2  = 0xff1e2430;
    static constexpr juce::uint32 kHero    = 0xff1c2836;
    static constexpr juce::uint32 kLine    = 0xff2a3140;
    static constexpr juce::uint32 kText    = 0xffe6e8ec;
    static constexpr juce::uint32 kMuted   = 0xff8b929c;
    static constexpr juce::uint32 kAccent  = 0xff2ec4a7;
    static constexpr juce::uint32 kRecord  = 0xffe23d3d;
    static constexpr juce::uint32 kGuitar  = 0xffe8a23a;
    static constexpr juce::uint32 kDrums   = 0xffc45c8a;
    static constexpr juce::uint32 kBass    = 0xff5b8def;
    static constexpr juce::uint32 kKeys    = 0xff9b7ae8;
    static constexpr juce::uint32 kLocked  = 0xff2ec4a7;
    static constexpr juce::uint32 kHunt    = 0xffe8a23a;

    SessionLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (kBg));
        setColour (juce::Label::textColourId, juce::Colour (kText));
        setColour (juce::TextButton::buttonColourId, juce::Colour (kPanel2));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (kAccent));
        setColour (juce::TextButton::textColourOffId, juce::Colour (kText));
        setColour (juce::TextButton::textColourOnId, juce::Colour (kBg));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (kPanel2));
        setColour (juce::ComboBox::textColourId, juce::Colour (kText));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (kLine));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (kAccent));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (kPanel));
        setColour (juce::PopupMenu::textColourId, juce::Colour (kText));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (kAccent).withAlpha (0.35f));
        setColour (juce::Slider::thumbColourId, juce::Colour (kAccent));
        setColour (juce::Slider::trackColourId, juce::Colour (kAccent).withAlpha (0.7f));
        setColour (juce::Slider::backgroundColourId, juce::Colour (kLine));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (kText));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (kPanel2));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (kLine));
        setColour (juce::ToggleButton::textColourId, juce::Colour (kText));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (kAccent));
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (kMuted));
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (kPanel2));
        setColour (juce::TextEditor::textColourId, juce::Colour (kText));
        setColour (juce::TextEditor::outlineColourId, juce::Colour (kLine));
    }

    juce::Font getLabelFont (juce::Label&) override              { return withH (15.0f); }
    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        const auto t = b.getButtonText();
        if (t.containsIgnoreCase ("Start") || t.containsIgnoreCase ("Jamming")
            || t.containsIgnoreCase ("ENTER") || t.containsIgnoreCase ("Lobby"))
            return withH (18.0f).boldened();
        return withH (16.0f);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override        { return withH (15.0f); }
    juce::Font getPopupMenuFont() override                       { return withH (15.0f); }

    void setUiScale (float s) { uiScale = s; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float start, float end, juce::Slider&) override
    {
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.42f;
        juce::Path bg;
        bg.addCentredArc (c.x, c.y, rad, rad, 0, start, end, true);
        g.setColour (juce::Colour (kLine));
        g.strokePath (bg, juce::PathStrokeType (4.0f));
        juce::Path val;
        val.addCentredArc (c.x, c.y, rad, rad, 0, start, start + (end - start) * pos, true);
        g.setColour (juce::Colour (kAccent));
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ang = start + (end - start) * pos;
        const juce::Point<float> tip (c.x + std::cos (ang - juce::MathConstants<float>::halfPi) * (rad - 2.0f),
                                      c.y + std::sin (ang - juce::MathConstants<float>::halfPi) * (rad - 2.0f));
        g.setColour (juce::Colour (kText));
        g.drawLine (c.x, c.y, tip.x, tip.y, 2.0f);
        g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7.0f, 7.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour& bg, bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto c = bg;
        if (highlighted) c = c.brighter (0.12f);
        if (down) c = c.darker (0.08f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (kLine));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float minPos, float maxPos, juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
            return;
        }

        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f, 4.0f);
        const float cx = r.getCentreX();
        g.setColour (juce::Colour (kLine));
        g.fillRoundedRectangle (cx - 2.5f, r.getY(), 5.0f, r.getHeight(), 2.5f);
        const float yThumb = juce::jmap (pos, minPos, maxPos, r.getBottom(), r.getY());
        g.setColour (juce::Colour (kAccent).withAlpha (0.55f));
        g.fillRoundedRectangle (cx - 2.5f, yThumb, 5.0f, r.getBottom() - yThumb, 2.5f);
        g.setColour (juce::Colour (kText));
        g.fillRoundedRectangle (cx - 8.0f, yThumb - 5.0f, 16.0f, 10.0f, 3.0f);
    }

private:
    juce::Font withH (float px) const
    {
        return juce::FontOptions ((float) std::round (px * uiScale));
    }

    float uiScale = 1.0f;
};
