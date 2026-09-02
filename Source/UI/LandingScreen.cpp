#include "UI/LandingScreen.h"
#include "SessionProcessor.h"

LandingScreen::ChairCard::ChairCard (Kind k, juce::Colour acc)
    : kind (k), accent (acc)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LandingScreen::ChairCard::setSeated (bool v)
{
    seated = v;
    repaint();
}

void LandingScreen::ChairCard::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    seated = ! seated;
    repaint();
    if (onChanged)
        onChanged();
}

void LandingScreen::ChairCard::drawGlyph (juce::Graphics& g, juce::Rectangle<float> r, bool on) const
{
    g.setColour (on ? accent : juce::Colour (SessionLookAndFeel::kMuted).withAlpha (0.45f));
    if (kind == Drums)
    {
        auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.38f;
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 3.0f);
        g.drawEllipse (c.x - rad * 0.55f, c.y - rad * 0.55f, rad * 1.1f, rad * 1.1f, 2.0f);
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    }
    else if (kind == Bass)
    {
        auto body = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.28f);
        g.fillRoundedRectangle (body, 10.0f);
        g.fillRect (body.getRight() - 8.0f, body.getCentreY() - 4.0f, r.getWidth() * 0.16f, 8.0f);
    }
    else
    {
        auto keys = r.reduced (r.getWidth() * 0.12f, r.getHeight() * 0.32f);
        g.fillRoundedRectangle (keys, 3.0f);
        g.setColour (on ? juce::Colour (SessionLookAndFeel::kBg) : juce::Colour (SessionLookAndFeel::kPanel));
        const float kw = keys.getWidth() / 7.0f;
        for (int i = 1; i < 7; ++i)
            g.fillRect (keys.getX() + kw * (float) i - 0.5f, keys.getY() + 3.0f, 1.2f, keys.getHeight() - 6.0f);
        g.setColour (on ? accent.darker (0.35f) : juce::Colour (SessionLookAndFeel::kMuted).withAlpha (0.5f));
        for (int i : { 0, 1, 3, 4, 5 })
        {
            const float x = keys.getX() + kw * ((float) i + 0.65f);
            g.fillRect (x, keys.getY() + 3.0f, kw * 0.55f, keys.getHeight() * 0.58f);
        }
    }
}

void LandingScreen::ChairCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    const auto fill = seated ? juce::Colour (SessionLookAndFeel::kPanel2)
                             : juce::Colour (SessionLookAndFeel::kBg);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (seated ? accent : juce::Colour (SessionLookAndFeel::kLine));
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, seated ? 2.4f : 1.2f);

    if (! seated)
    {
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (r, 14.0f);
    }

    auto inner = r.reduced (16.0f);
    const char* name = kind == Drums ? "DRUMS" : (kind == Bass ? "BASS" : "KEYS");
    g.setColour (seated ? juce::Colour (SessionLookAndFeel::kText)
                        : juce::Colour (SessionLookAndFeel::kMuted));
    g.setFont (juce::Font (juce::FontOptions (22.0f)).boldened());
    g.drawText (name, inner.removeFromTop (28.0f), juce::Justification::centred, false);

    auto glyph = inner.removeFromTop (inner.getHeight() - 36.0f);
    drawGlyph (g, glyph, seated);

    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    g.setColour (seated ? accent : juce::Colour (SessionLookAndFeel::kMuted));
    g.drawText (seated ? "IN THE ROOM" : "SITTING OUT",
                inner, juce::Justification::centred, false);
}

LandingScreen::LandingScreen (SessionProcessor& processor)
    : proc (processor)
{
    setLookAndFeel (&look);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    title.setText ("F#$*ktheband", juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kAccent));
    title.setJustificationType (juce::Justification::centred);
    subtitle.setText ("Who's in the room?", juce::dontSendNotification);
    subtitle.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kText));
    subtitle.setJustificationType (juce::Justification::centred);
    rosterLbl.setText ("Click a chair to seat or empty it. Empty chairs make no sound.",
                       juce::dontSendNotification);
    rosterLbl.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    rosterLbl.setJustificationType (juce::Justification::centred);
    setupLbl.setText ("Session setup", juce::dontSendNotification);
    setupLbl.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (rosterLbl);
    addAndMakeVisible (setupLbl);

    auto bindChair = [this] (ChairCard& c)
    {
        c.onChanged = [this]
        {
            refreshRosterLabel();
            persistSetup();
        };
        addAndMakeVisible (c);
    };
    bindChair (drumsCard);
    bindChair (bassCard);
    bindChair (keysCard);

    styleLbl.setText ("Style", juce::dontSendNotification);
    formLbl.setText ("Form", juce::dontSendNotification);
    scaleLbl.setText ("Scale", juce::dontSendNotification);
    feelLbl.setText ("Feel", juce::dontSendNotification);
    keyLbl.setText ("Key", juce::dontSendNotification);
    bpmLbl.setText ("BPM", juce::dontSendNotification);
    addAndMakeVisible (styleLbl);
    addAndMakeVisible (formLbl);
    addAndMakeVisible (scaleLbl);
    addAndMakeVisible (feelLbl);
    addAndMakeVisible (keyLbl);
    addAndMakeVisible (bpmLbl);

    styleBox.addItemList ({ "Rock", "Blues", "Metal", "Funk", "Jazz" }, 1);
    styleBox.setSelectedId (1, juce::dontSendNotification);
    formBox.addItemList ({ "Jam", "Radio", "12-Bar", "Changes" }, 1);
    formBox.setSelectedId (2, juce::dontSendNotification);
    scaleBox.addItemList ({ "Major", "Minor", "Pentatonic", "Blues" }, 1);
    scaleBox.setSelectedId (3, juce::dontSendNotification);
    feelBox.addItemList ({ "Grid", "Ahead", "Behind", "Swing" }, 1);
    feelBox.setSelectedId (1, juce::dontSendNotification);
    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keyBox.setSelectedId (5, juce::dontSendNotification);
    addAndMakeVisible (styleBox);
    addAndMakeVisible (formBox);
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (feelBox);
    addAndMakeVisible (keyBox);

    lockKey.setToggleState (false, juce::dontSendNotification);
    slewToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (lockKey);
    addAndMakeVisible (slewToggle);

    bpmSlider.setRange (60.0, 180.0, 1.0);
    bpmSlider.setValue (112.0);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
    addAndMakeVisible (bpmSlider);

    enterBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (SessionLookAndFeel::kAccent));
    enterBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (SessionLookAndFeel::kBg));
    enterBtn.onClick = [this]
    {
        persistSetup();
        if (onEnterSession)
            onEnterSession();
    };
    audioBtn.onClick = [this]
    {
        if (onShowAudioSettings)
            onShowAudioSettings();
    };
    deviceLbl.setText ("Audio: —", juce::dontSendNotification);
    deviceLbl.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    deviceLbl.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (enterBtn);
    addAndMakeVisible (audioBtn);
    addAndMakeVisible (deviceLbl);

    auto persist = [this] { persistSetup(); };
    styleBox.onChange = persist;
    formBox.onChange  = persist;
    scaleBox.onChange = persist;
    feelBox.onChange  = persist;
    keyBox.onChange   = persist;
    lockKey.onClick   = persist;
    slewToggle.onClick = persist;
    bpmSlider.onValueChange = persist;

    loadSettings();
    applyFonts();
    refreshRosterLabel();
    startTimerHz (12);
}

LandingScreen::~LandingScreen()
{
    stopTimer();
    persistSetup();
    setLookAndFeel (nullptr);
}

void LandingScreen::loadSettings()
{
    setSetup (SessionSettings::loadSetup());
}

void LandingScreen::setSetup (const SessionSettings::Setup& s)
{
    drumsCard.setSeated (s.drumsIn);
    bassCard.setSeated (s.bassIn);
    keysCard.setSeated (s.keysIn);
    styleBox.setSelectedId (s.style + 1, juce::dontSendNotification);
    formBox.setSelectedId  (s.form + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (s.scale + 1, juce::dontSendNotification);
    feelBox.setSelectedId  (s.feel + 1, juce::dontSendNotification);
    keyBox.setSelectedId   (s.keyPc + 1, juce::dontSendNotification);
    lockKey.setToggleState (! s.followKey, juce::dontSendNotification);
    bpmSlider.setValue ((double) s.bpm, juce::dontSendNotification);
    slewToggle.setToggleState (s.slew, juce::dontSendNotification);
    refreshRosterLabel();
}

SessionSettings::Setup LandingScreen::getSetup() const
{
    SessionSettings::Setup s;
    s.drumsIn = drumsCard.isSeated();
    s.bassIn  = bassCard.isSeated();
    s.keysIn  = keysCard.isSeated();
    s.style   = juce::jlimit (0, 4, styleBox.getSelectedId() - 1);
    s.form    = juce::jlimit (0, 3, formBox.getSelectedId() - 1);
    s.scale   = juce::jlimit (0, 3, scaleBox.getSelectedId() - 1);
    s.feel    = juce::jlimit (0, 3, feelBox.getSelectedId() - 1);
    s.keyPc   = juce::jlimit (0, 11, keyBox.getSelectedId() - 1);
    s.followKey = ! lockKey.getToggleState();
    s.bpm     = (float) bpmSlider.getValue();
    s.slew    = slewToggle.getToggleState();
    return s;
}

void LandingScreen::persistSetup()
{
    SessionSettings::mergeSetup (getSetup());
}

void LandingScreen::setDeviceStatus (const juce::String& text)
{
    deviceLbl.setText (text, juce::dontSendNotification);
}

void LandingScreen::refreshRosterLabel()
{
    int n = (drumsCard.isSeated() ? 1 : 0)
          + (bassCard.isSeated()  ? 1 : 0)
          + (keysCard.isSeated()  ? 1 : 0);
    juce::String who;
    auto add = [&who] (bool on, const char* name)
    {
        if (! on) return;
        if (who.isNotEmpty()) who << " + ";
        who << name;
    };
    add (drumsCard.isSeated(), "Drums");
    add (bassCard.isSeated(),  "Bass");
    add (keysCard.isSeated(),  "Keys");
    if (n == 0)
        subtitle.setText ("Empty room  —  you solo", juce::dontSendNotification);
    else
        subtitle.setText ("Who's in the room?   " + who, juce::dontSendNotification);
}

bool LandingScreen::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        persistSetup();
        if (onEnterSession)
            onEnterSession();
        return true;
    }
    return false;
}

void LandingScreen::timerCallback()
{
    repaint (inMeterBounds.expanded (2));
}

juce::Font LandingScreen::uiFont (float px, bool bold) const
{
    auto f = juce::Font (juce::FontOptions ((float) std::round (px * uiScale)));
    return bold ? f.boldened() : f;
}

void LandingScreen::applyFonts()
{
    look.setUiScale (uiScale);
    title.setFont (uiFont (42.0f, true));
    subtitle.setFont (uiFont (20.0f, true));
    rosterLbl.setFont (uiFont (13.0f));
    setupLbl.setFont (uiFont (13.0f));
    deviceLbl.setFont (uiFont (12.0f));
}

float LandingScreen::computeScale() const noexcept
{
    const float sxv = (float) getWidth()  / 1280.0f;
    const float syv = (float) getHeight() / 800.0f;
    return juce::jlimit (0.75f, 1.45f, juce::jmin (sxv, syv));
}

void LandingScreen::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (SessionLookAndFeel::kBg));

    auto panel = [&g] (juce::Rectangle<int> r, juce::uint32 c)
    {
        g.setColour (juce::Colour (c));
        g.fillRoundedRectangle (r.toFloat(), 12.0f);
    };
    panel (setupBounds, SessionLookAndFeel::kPanel);

    auto meter = inMeterBounds.toFloat();
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (meter, 3.0f);
    const float peak = proc.getInputPeak();
    const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, peak), -48.0f);
    const float n = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    auto fill = meter.reduced (1.5f);
    fill.setWidth (fill.getWidth() * n);
    g.setColour (n > 0.92f ? juce::Colour (SessionLookAndFeel::kRecord)
                           : juce::Colour (SessionLookAndFeel::kGuitar));
    g.fillRoundedRectangle (fill, 2.0f);
    g.setFont (uiFont (11.0f));
    g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
    g.drawText ("IN", inMeterBounds.translated (0, -sx (14)), juce::Justification::centredLeft, false);
}

void LandingScreen::resized()
{
    uiScale = computeScale();
    applyFonts();

    auto r = getLocalBounds().reduced (sx (28), sx (20));
    title.setBounds (r.removeFromTop (sx (48)));
    subtitle.setBounds (r.removeFromTop (sx (28)));
    rosterLbl.setBounds (r.removeFromTop (sx (22)));
    r.removeFromTop (sx (8));

    auto bottom = r.removeFromBottom (sx (78));
    {
        auto status = bottom.removeFromTop (sx (22));
        auto meterCol = status.removeFromRight (sx (170));
        inMeterBounds = meterCol.removeFromBottom (sx (10)).reduced (sx (4), 0);
        deviceLbl.setBounds (status.reduced (sx (4), 0));
        audioBtn.setBounds (bottom.removeFromLeft (sx (90)).reduced (sx (4), sx (8)));
        enterBtn.setBounds (bottom.reduced (sx (40), sx (4)));
    }

    r.removeFromBottom (sx (10));
    setupBounds = r.removeFromBottom (sx (168));
    {
        auto s = setupBounds.reduced (sx (16), sx (12));
        setupLbl.setBounds (s.removeFromTop (sx (18)));
        s.removeFromTop (sx (6));
        auto row1 = s.removeFromTop (sx (56));
        auto place = [this] (juce::Rectangle<int>& row, juce::Label& lab, juce::Component& c, int w)
        {
            auto col = row.removeFromLeft (sx ((float) w));
            lab.setBounds (col.removeFromTop (sx (16)));
            c.setBounds (col.reduced (sx (2), sx (4)));
            row.removeFromLeft (sx (10));
        };
        place (row1, styleLbl, styleBox, 150);
        place (row1, formLbl,  formBox,  150);
        place (row1, scaleLbl, scaleBox, 160);
        place (row1, feelLbl,  feelBox,  140);

        auto row2 = s;
        place (row2, keyLbl, keyBox, 110);
        lockKey.setBounds (row2.removeFromLeft (sx (110)).reduced (sx (2), sx (12)));
        row2.removeFromLeft (sx (10));
        place (row2, bpmLbl, bpmSlider, 280);
        slewToggle.setBounds (row2.removeFromLeft (sx (90)).reduced (sx (2), sx (12)));
    }

    r.removeFromBottom (sx (12));
    chairsBounds = r;
    {
        auto c = chairsBounds;
        const int gap = sx (14);
        const int w = (c.getWidth() - gap * 2) / 3;
        drumsCard.setBounds (c.removeFromLeft (w).reduced (sx (4)));
        c.removeFromLeft (gap);
        bassCard.setBounds (c.removeFromLeft (w).reduced (sx (4)));
        c.removeFromLeft (gap);
        keysCard.setBounds (c.reduced (sx (4)));
    }
}
