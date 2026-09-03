#include "UI/SessionUI.h"
#include "SessionSettings.h"
#include <cmath>

SessionUI::MixerStrip::MixerStrip (const juce::String& name, juce::Colour acc)
    : accent (acc)
{
    title.setText (name, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, acc);
    addAndMakeVisible (title);

    level.setSliderStyle (juce::Slider::LinearVertical);
    level.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 18);
    level.setRange (0.0, 1.5, 0.01);
    level.setValue (0.85);
    addAndMakeVisible (level);

    mute.setClickingTogglesState (true);
    solo.setClickingTogglesState (true);
    arm.setClickingTogglesState (true);
    mon.setClickingTogglesState (true);
    arm.setColour (juce::TextButton::buttonOnColourId, juce::Colour (SessionLookAndFeel::kRecord));
    mon.setColour (juce::TextButton::buttonOnColourId, juce::Colour (SessionLookAndFeel::kGuitar));
    addAndMakeVisible (mute);
    addAndMakeVisible (solo);
    addAndMakeVisible (arm);
    addAndMakeVisible (mon);
    pan.setSliderStyle (juce::Slider::LinearHorizontal);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    pan.setRange (0.0, 1.0, 0.01);
    pan.setValue (0.5);
    addAndMakeVisible (pan);
}

void SessionUI::MixerStrip::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (accent.withAlpha (0.45f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.2f);

    auto r = meterBounds.toFloat();
    g.setColour (juce::Colour (SessionLookAndFeel::kBg));
    g.fillRoundedRectangle (r, 3.0f);
    const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, peak), -48.0f);
    const float n = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    auto fill = r.reduced (1.5f);
    fill.removeFromTop (fill.getHeight() * (1.0f - n));
    g.setColour (n > 0.92f ? juce::Colour (SessionLookAndFeel::kRecord) : accent);
    g.fillRoundedRectangle (fill, 2.0f);
}

void SessionUI::MixerStrip::resized()
{
    auto r = getLocalBounds().reduced (compact ? 4 : 8);
    title.setBounds (r.removeFromTop (compact ? 16 : 22));
    auto buttons = r.removeFromBottom (compact ? 44 : 28);
    if (compact)
    {
        auto row2 = buttons.removeFromBottom (20);
        mute.setBounds (row2.removeFromLeft (row2.getWidth() / 4).reduced (1));
        solo.setBounds (row2.removeFromLeft (row2.getWidth() / 3).reduced (1));
        arm.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (1));
        mon.setBounds (row2.reduced (1));
        pan.setBounds (buttons.reduced (1, 1));
    }
    else
    {
        mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2));
        solo.setBounds (buttons.reduced (2));
        arm.setVisible (false);
        mon.setVisible (false);
        pan.setVisible (false);
    }
    meterBounds = r.removeFromRight (compact ? 7 : 10).reduced (1, compact ? 4 : 8);
    r.removeFromRight (3);
    level.setBounds (r);
}

void SessionUI::setupRotary (juce::Slider& s, double min, double max, double def, double step)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    s.setRange (min, max, step);
    s.setValue (def);
}

SessionUI::SessionUI (SessionProcessor& processor, juce::AudioDeviceManager& devs)
    : proc (processor), devices (devs)
{
    setLookAndFeel (&look);
    setOpaque (true);

    product.setText ("Forget The Band", juce::dontSendNotification);
    product.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kAccent));
    brand.setText ("Centrophy", juce::dontSendNotification);
    brand.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    addAndMakeVisible (product);
    addAndMakeVisible (brand);

    keyReadout.setText ("Key  E", juce::dontSendNotification);
    bpmReadout.setText ("BPM  112", juce::dontSendNotification);
    barReadout.setText ("1.1", juce::dontSendNotification);
    barReadout.setJustificationType (juce::Justification::centred);
    tunerNote.setText ("--", juce::dontSendNotification);
    tunerNote.setJustificationType (juce::Justification::centred);
    recPath.setText ("Stems: Documents/Centrophy/ForgetTheBand/stems/", juce::dontSendNotification);
    recPath.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    recTime.setText ("", juce::dontSendNotification);
    recTime.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (keyReadout);
    addAndMakeVisible (bpmReadout);
    addAndMakeVisible (barReadout);
    addAndMakeVisible (tunerNote);
    addAndMakeVisible (recPath);
    addAndMakeVisible (recTime);

    styleLbl.setText ("Style", juce::dontSendNotification);
    followLbl.setText ("Follow", juce::dontSendNotification);
    styleBox.addItem ("Rock", 1);
    styleBox.addItem ("Blues", 2);
    styleBox.addItem ("Metal", 3);
    styleBox.addItem ("Funk", 4);
    styleBox.addItem ("Jazz", 5);
    styleBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (styleLbl);
    addAndMakeVisible (followLbl);
    addAndMakeVisible (styleBox);

    kitLbl.setText ("Kit", juce::dontSendNotification);
    bassVoiceLbl.setText ("Bass", juce::dontSendNotification);
    keysVoiceLbl.setText ("Keys", juce::dontSendNotification);
    for (int i = 0; i < (int) FollowerBand::DrumKit::NumKits; ++i)
        kitBox.addItem (FollowerBand::drumKitName (i), i + 1);
    for (int i = 0; i < (int) FollowerBand::BassVoice::NumVoices; ++i)
        bassVoiceBox.addItem (FollowerBand::bassVoiceName (i), i + 1);
    for (int i = 0; i < (int) FollowerBand::KeysVoice::NumVoices; ++i)
        keysVoiceBox.addItem (FollowerBand::keysVoiceName (i), i + 1);
    kitBox.setSelectedId (1, juce::dontSendNotification);
    bassVoiceBox.setSelectedId (1, juce::dontSendNotification);
    keysVoiceBox.setSelectedId (1, juce::dontSendNotification);
    fxVoiceLbl.setText ("FX", juce::dontSendNotification);
    for (int i = 0; i < (int) FxChair::Voice::NumVoices; ++i)
        fxVoiceBox.addItem (FxChair::voiceName (i), i + 1);
    fxVoiceBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (kitLbl);
    addAndMakeVisible (bassVoiceLbl);
    addAndMakeVisible (keysVoiceLbl);
    addAndMakeVisible (fxVoiceLbl);
    addAndMakeVisible (kitBox);
    addAndMakeVisible (bassVoiceBox);
    addAndMakeVisible (keysVoiceBox);
    addAndMakeVisible (fxVoiceBox);

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keyBox.setSelectedId (5, juce::dontSendNotification); // E
    autoKey.setToggleState (true, juce::dontSendNotification);
    autoBpm.setToggleState (true, juce::dontSendNotification);
    countInToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (autoKey);
    addAndMakeVisible (autoBpm);
    addAndMakeVisible (countInToggle);

    bpmLbl.setText ("BPM", juce::dontSendNotification);
    bpmSlider.setRange (60.0, 180.0, 1.0);
    bpmSlider.setValue (112.0);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    addAndMakeVisible (bpmLbl);
    addAndMakeVisible (bpmSlider);

    ampLbl.setText ("Amp / FX", juce::dontSendNotification);
    gainLbl.setText ("Gain", juce::dontSendNotification);
    gateLbl.setText ("Gate", juce::dontSendNotification);
    driveLbl.setText ("Drive", juce::dontSendNotification);
    toneLbl.setText ("Tone", juce::dontSendNotification);
    levelLbl.setText ("Level", juce::dontSendNotification);
    delayLbl.setText ("Delay", juce::dontSendNotification);
    spaceLbl.setText ("Space", juce::dontSendNotification);
    mixLbl.setText ("Mixer", juce::dontSendNotification);
    addAndMakeVisible (ampLbl);
    addAndMakeVisible (gainLbl);
    addAndMakeVisible (gateLbl);
    addAndMakeVisible (driveLbl);
    addAndMakeVisible (toneLbl);
    addAndMakeVisible (levelLbl);
    addAndMakeVisible (delayLbl);
    addAndMakeVisible (spaceLbl);
    addAndMakeVisible (mixLbl);

    setupRotary (gainSlider,  0.0, 24.0, 0.0,  0.1);
    setupRotary (gateSlider,  0.0,  1.0, 0.0, 0.01);
    setupRotary (driveSlider, 0.0,  1.0, 0.42, 0.01);
    setupRotary (toneSlider,  0.0,  1.0, 0.55, 0.01);
    setupRotary (levelSlider, 0.0,  1.5, 0.80, 0.01);
    setupRotary (delaySlider, 0.0,  1.0, 0.18, 0.01);
    setupRotary (spaceSlider, 0.0,  1.0, 0.16, 0.01);
    gainSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (gateSlider);
    addAndMakeVisible (driveSlider);
    addAndMakeVisible (toneSlider);
    addAndMakeVisible (levelSlider);
    addAndMakeVisible (delaySlider);
    addAndMakeVisible (spaceSlider);

    startBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (SessionLookAndFeel::kAccent).darker (0.15f));
    startBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (SessionLookAndFeel::kBg));
    stopBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (SessionLookAndFeel::kPanel2));
    recordBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (SessionLookAndFeel::kRecord).darker (0.25f));
    addAndMakeVisible (startBtn);
    addAndMakeVisible (stopBtn);
    addAndMakeVisible (lobbyBtn);
    addAndMakeVisible (bpmDown);
    addAndMakeVisible (bpmUp);
    addAndMakeVisible (recordBtn);
    addAndMakeVisible (audioBtn);

    addAndMakeVisible (guitarStrip);
    addAndMakeVisible (drumsStrip);
    addAndMakeVisible (bassStrip);
    addAndMakeVisible (keysStrip);
    addAndMakeVisible (fxStrip);
    addAndMakeVisible (masterStrip);

    playBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (SessionLookAndFeel::kAccent));
    cycleBtn.setClickingTogglesState (true);
    addAndMakeVisible (playBtn);
    addAndMakeVisible (rtzBtn);
    addAndMakeVisible (cycleBtn);
    addAndMakeVisible (newBtn);
    addAndMakeVisible (openBtn);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (bounceBtn);
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (viewArrange);
    addAndMakeVisible (viewMixer);
    viewArrange.setClickingTogglesState (true);
    viewMixer.setClickingTogglesState (true);
    viewArrange.setRadioGroupId (71);
    viewMixer.setRadioGroupId (71);
    viewArrange.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (ampBypass);
    addAndMakeVisible (scanBtn);
    guitarVstLbl.setText ("Guitar inserts", juce::dontSendNotification);
    trackVstLbl.setText ("Track inserts", juce::dontSendNotification);
    projectLbl.setText ("Untitled", juce::dontSendNotification);
    addAndMakeVisible (guitarVstLbl);
    addAndMakeVisible (trackVstLbl);
    addAndMakeVisible (projectLbl);
    addAndMakeVisible (arrange);

    const juce::uint32 cols[12] = {
        SessionLookAndFeel::kGuitar, 0xff6aa3c7, 0xff7aa37a, 0xffc7a36a,
        0xffa37ac7, 0xffc76a7a, 0xff6ac7b0, 0xffc7c76a,
        SessionLookAndFeel::kDrums, SessionLookAndFeel::kBass,
        SessionLookAndFeel::kKeys, SessionLookAndFeel::kAccent
    };
    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        dawStrips[(size_t) i] = std::make_unique<MixerStrip> (Daw::defaultTrackName (i), juce::Colour (cols[i]));
        dawStrips[(size_t) i]->compact = true;
        addAndMakeVisible (*dawStrips[(size_t) i]);
    }

    static const char* gNames[] = { "1  Pre", "2  Amp", "3  Post", "4  4" };
    static const char* tNames[] = { "1  Pre", "2  Amp", "3  Post", "4  4" };
    for (int i = 0; i < 4; ++i)
    {
        gSlotLbl[(size_t) i].setText (gNames[i], juce::dontSendNotification);
        tSlotLbl[(size_t) i].setText (tNames[i], juce::dontSendNotification);
        gByp[(size_t) i].setButtonText ("Byp");
        tByp[(size_t) i].setButtonText ("Byp");
        gEd[(size_t) i].setButtonText ("Ed");
        tEd[(size_t) i].setButtonText ("Ed");
        gUp[(size_t) i].setButtonText (juce::String::fromUTF8 ("\xe2\x86\x91"));
        gDn[(size_t) i].setButtonText (juce::String::fromUTF8 ("\xe2\x86\x93"));
        tUp[(size_t) i].setButtonText (juce::String::fromUTF8 ("\xe2\x86\x91"));
        tDn[(size_t) i].setButtonText (juce::String::fromUTF8 ("\xe2\x86\x93"));
        gByp[(size_t) i].setClickingTogglesState (true);
        tByp[(size_t) i].setClickingTogglesState (true);
        gUp[(size_t) i].setEnabled (i > 0);
        gDn[(size_t) i].setEnabled (i < 3);
        tUp[(size_t) i].setEnabled (i > 0);
        tDn[(size_t) i].setEnabled (i < 3);
        addAndMakeVisible (gSlotLbl[(size_t) i]);
        addAndMakeVisible (tSlotLbl[(size_t) i]);
        addAndMakeVisible (gSlot[(size_t) i]);
        addAndMakeVisible (tSlot[(size_t) i]);
        addAndMakeVisible (gByp[(size_t) i]);
        addAndMakeVisible (tByp[(size_t) i]);
        addAndMakeVisible (gEd[(size_t) i]);
        addAndMakeVisible (tEd[(size_t) i]);
        addAndMakeVisible (gUp[(size_t) i]);
        addAndMakeVisible (gDn[(size_t) i]);
        addAndMakeVisible (tUp[(size_t) i]);
        addAndMakeVisible (tDn[(size_t) i]);
    }


    formBox.addItemList ({ "Jam", "Radio", "12-Bar", "Changes" }, 1);
    formBox.setSelectedId (2, juce::dontSendNotification);
    scaleBox.addItemList ({ "Major", "Minor", "Pentatonic", "Blues" }, 1);
    scaleBox.setSelectedId (3, juce::dontSendNotification);
    feelBox.addItemList ({ "Grid", "Ahead", "Behind", "Swing" }, 1);
    feelBox.setSelectedId (1, juce::dontSendNotification);
    phraseBox.addItemList ({ "4 bars", "8 bars", "16 bars" }, 1);
    phraseBox.setSelectedId (2, juce::dontSendNotification);
    delayDivBox.addItemList ({ "1/4", "1/8", "1/8.", "1/16" }, 1);
    delayDivBox.setSelectedId (3, juce::dontSendNotification);
    formLbl.setText ("Form", juce::dontSendNotification);
    scaleLbl.setText ("Scale", juce::dontSendNotification);
    feelLbl.setText ("Feel", juce::dontSendNotification);
    phraseLbl.setText ("Phrase", juce::dontSendNotification);
    energyLbl.setText ("You / Band", juce::dontSendNotification);
    lockTempo.setToggleState (false, juce::dontSendNotification);
    grooveFloor.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (formBox);
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (feelBox);
    addAndMakeVisible (phraseBox);
    addAndMakeVisible (delayDivBox);
    addAndMakeVisible (formLbl);
    addAndMakeVisible (scaleLbl);
    addAndMakeVisible (feelLbl);
    addAndMakeVisible (phraseLbl);
    addAndMakeVisible (energyLbl);
    addAndMakeVisible (lockTempo);
    addAndMakeVisible (lockIntensity);
    addAndMakeVisible (grooveFloor);
    addAndMakeVisible (fadeSilence);
    addAndMakeVisible (energyDrift);
    addAndMakeVisible (calSoft);
    addAndMakeVisible (calMid);
    addAndMakeVisible (calHard);
    chordName.setText ("—", juce::dontSendNotification);
    nextChord.setText ("", juce::dontSendNotification);
    playerNote.setText ("you  --", juce::dontSendNotification);
    noteLaneLbl.setText ("Notes", juce::dontSendNotification);
    addAndMakeVisible (chordName);
    addAndMakeVisible (nextChord);
    addAndMakeVisible (playerNote);
    addAndMakeVisible (noteLaneLbl);
    midiBtn.setTooltip ("Save the notes you played as a .mid file");
    addAndMakeVisible (midiBtn);
    addAndMakeVisible (loadFxBtn);

    guitarStrip.level.setValue (proc.getBusLevel (SessionProcessor::Guitar));
    drumsStrip.level.setValue  (proc.getBusLevel (SessionProcessor::Drums));
    bassStrip.level.setValue   (proc.getBusLevel (SessionProcessor::Bass));
    keysStrip.level.setValue   (proc.getBusLevel (SessionProcessor::Keys));
    fxStrip.level.setValue     (proc.getBusLevel (SessionProcessor::Fx));
    masterStrip.level.setValue (proc.getBusLevel (SessionProcessor::Master));

    loadSettings();
    wireControls();
    guitarStrip.mute.setToggleState (false, juce::dontSendNotification);
    guitarStrip.mon.setToggleState (true, juce::dontSendNotification);
    proc.setBusMute (SessionProcessor::Guitar, false);
    proc.setGate ((float) gateSlider.getValue());
    {
        auto& gtr = proc.getDaw().getProject().tracks[(size_t) Daw::kGuitar];
        gtr.mute.store (0, std::memory_order_relaxed);
        gtr.monitor.store (1, std::memory_order_relaxed);
    }
    applyToProcessor();
    applyFonts();
    startTimerHz (20);
}

SessionUI::~SessionUI()
{
    stopTimer();
    saveSettings();
    setLookAndFeel (nullptr);
}

void SessionUI::markDirty()
{
    settingsDirty = true;
}

void SessionUI::wireControls()
{
    styleBox.onChange = [this]
    {
        const int id = styleBox.getSelectedId();
        proc.getBand().setStyle ((FollowerBand::Style) juce::jlimit (0, (int) FollowerBand::Style::NumStyles - 1, id - 1));
        markDirty();
    };
    kitBox.onChange = [this]
    {
        proc.getBand().setDrumKit ((FollowerBand::DrumKit) juce::jlimit (0, (int) FollowerBand::DrumKit::NumKits - 1,
                                                                        kitBox.getSelectedId() - 1));
        markDirty();
    };
    bassVoiceBox.onChange = [this]
    {
        proc.getBand().setBassVoice ((FollowerBand::BassVoice) juce::jlimit (0, (int) FollowerBand::BassVoice::NumVoices - 1,
                                                                            bassVoiceBox.getSelectedId() - 1));
        markDirty();
    };
    keysVoiceBox.onChange = [this]
    {
        proc.getBand().setKeysVoice ((FollowerBand::KeysVoice) juce::jlimit (0, (int) FollowerBand::KeysVoice::NumVoices - 1,
                                                                            keysVoiceBox.getSelectedId() - 1));
        markDirty();
    };
    fxVoiceBox.onChange = [this]
    {
        proc.getFxChair().setVoice ((FxChair::Voice) juce::jlimit (0, (int) FxChair::Voice::NumVoices - 1,
                                                                  fxVoiceBox.getSelectedId() - 1));
        markDirty();
    };

    autoKey.onClick = [this]
    {
        if (autoKey.getToggleState())
            proc.getAnalyzer().unlockKey();
        else
            proc.getAnalyzer().setManualKey (keyBox.getSelectedId() - 1);
        markDirty();
    };
    keyBox.onChange = [this]
    {
        if (! autoKey.getToggleState())
            proc.getAnalyzer().setManualKey (keyBox.getSelectedId() - 1);
        markDirty();
    };

    autoBpm.onClick = [this]
    {
        if (autoBpm.getToggleState())
        {
            lockTempo.setToggleState (false, juce::dontSendNotification);
            proc.getAnalyzer().setAutoBpm (true);
            proc.getAnalyzer().setLockTempo (false);
            proc.getAnalyzer().unlockBpm();
        }
        else
            proc.getAnalyzer().setManualBpm ((float) bpmSlider.getValue());
        markDirty();
    };
    bpmSlider.onValueChange = [this]
    {
        if (! autoBpm.getToggleState())
            proc.getAnalyzer().setManualBpm ((float) bpmSlider.getValue());
        markDirty();
    };

    countInToggle.onClick = [this]
    {
        proc.setCountInEnabled (countInToggle.getToggleState());
        markDirty();
    };

    gainSlider.onValueChange  = [this] { proc.setInputGainDb ((float) gainSlider.getValue()); markDirty(); };
    gateSlider.onValueChange  = [this] { proc.setGate ((float) gateSlider.getValue()); markDirty(); };
    driveSlider.onValueChange = [this] { proc.getAmp().setDrive ((float) driveSlider.getValue()); markDirty(); };
    toneSlider.onValueChange  = [this] { proc.getAmp().setTone  ((float) toneSlider.getValue()); markDirty(); };
    levelSlider.onValueChange = [this] { proc.getAmp().setLevel ((float) levelSlider.getValue()); markDirty(); };
    delaySlider.onValueChange = [this] { proc.setDelayMix ((float) delaySlider.getValue()); markDirty(); };
    spaceSlider.onValueChange = [this] { proc.setSpaceMix ((float) spaceSlider.getValue()); markDirty(); };

    auto bindStrip = [this] (MixerStrip& s, int bus)
    {
        s.level.onValueChange = [this, &s, bus] { proc.setBusLevel (bus, (float) s.level.getValue()); markDirty(); };
        s.mute.onClick = [this, &s, bus] { proc.setBusMute (bus, s.mute.getToggleState()); };
        s.solo.onClick = [this, &s, bus] { proc.setBusSolo (bus, s.solo.getToggleState()); };
    };
    bindStrip (guitarStrip, SessionProcessor::Guitar);
    bindStrip (drumsStrip,  SessionProcessor::Drums);
    bindStrip (bassStrip,   SessionProcessor::Bass);
    bindStrip (keysStrip,   SessionProcessor::Keys);
    bindStrip (fxStrip,     SessionProcessor::Fx);
    bindStrip (masterStrip, SessionProcessor::Master);

    startBtn.onClick = [this]
    {
        proc.startSession();
        refreshTransport();
    };
    stopBtn.onClick = [this]
    {
        proc.stopSession();
        refreshTransport();
    };
    lobbyBtn.onClick = [this]
    {
        if (onBackToLobby)
            onBackToLobby();
    };
    bpmDown.onClick = [this]
    {
        proc.getAnalyzer().nudgeBpm (-1.0f);
        bpmSlider.setValue ((double) proc.getAnalyzer().getBpm(), juce::dontSendNotification);
        markDirty();
    };
    bpmUp.onClick = [this]
    {
        proc.getAnalyzer().nudgeBpm (1.0f);
        bpmSlider.setValue ((double) proc.getAnalyzer().getBpm(), juce::dontSendNotification);
        markDirty();
    };
    recordBtn.onClick = [this]
    {
        if (proc.isRecording())
        {
            proc.stopRecording();
        }
        else
        {
            const auto err = proc.startRecording();
            if (err.isNotEmpty())
            {
                juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                             "Record", err);
            }
        }
        refreshTransport();
    };
    audioBtn.onClick = [this]
    {
        if (onShowAudioSettings)
            onShowAudioSettings();
    };

    playBtn.onClick = [this]
    {
        auto& d = proc.getDaw();
        if (d.isPlaying()) d.stop();
        else d.play();
        refreshTransport();
    };
    rtzBtn.onClick = [this] { proc.getDaw().returnToZero(); };
    cycleBtn.onClick = [this]
    {
        proc.getDaw().setCycle (cycleBtn.getToggleState());
    };
    newBtn.onClick    = [this] { doNewProject(); };
    openBtn.onClick   = [this] { doOpenProject(); };
    saveBtn.onClick   = [this] { doSaveProject(); };
    bounceBtn.onClick = [this] { doBounce(); };
    undoBtn.onClick   = [this] { proc.getDaw().getProject().undoLast(); };
    midiBtn.onClick   = [this] { exportPlayerMidi(); };
    loadFxBtn.onClick = [this]
    {
        auto dir = SampleBank::userFxDir();
        auto chooser = std::make_shared<juce::FileChooser> (
            "Load FX samples", dir, "*.wav;*.aif;*.aiff;*.flac", true);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, chooser, dir] (const juce::FileChooser& c)
        {
            auto files = c.getResults();
            if (files.isEmpty())
            {
                dir.revealToUser();
                proc.getSamples().scanUserFx();
                return;
            }
            int n = 0;
            for (auto& f : files)
            {
                if (! f.existsAsFile())
                    continue;
                auto dest = dir.getChildFile (f.getFileName());
                if (f.getFullPathName() == dest.getFullPathName())
                {
                    ++n;
                    continue;
                }
                if (dest.existsAsFile())
                    dest.deleteFile();
                if (f.copyFileTo (dest))
                    ++n;
            }
            proc.getSamples().scanUserFx();
            recPath.setText ("FX  " + juce::String (n) + " loaded  " + dir.getFullPathName(),
                             juce::dontSendNotification);
            if (n == 0)
            {
                juce::NativeMessageBox::showAsync (
                    juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::InfoIcon)
                        .withTitle ("Load FX")
                        .withMessage ("Put wav, aiff or flac files in:\n" + dir.getFullPathName())
                        .withButton ("Reveal")
                        .withButton ("OK"),
                    [dir] (int r)
                    {
                        if (r == 1)
                            dir.revealToUser();
                    });
            }
        });
    };
    viewArrange.onClick = [this] { mixerFront = false; resized(); };
    viewMixer.onClick   = [this] { mixerFront = true;  resized(); };
    ampBypass.onClick = [this]
    {
        proc.setAmpBypass (ampBypass.getToggleState());
        markDirty();
    };
    scanBtn.onClick = [this]
    {
#if JUCE_WINDOWS
        const juce::File startDir ("C:/Program Files/Common Files/VST3");
#else
        const auto vstPaths = proc.getPluginHost().defaultVST3Paths();
        const juce::File startDir = vstPaths.getNumPaths() > 0 ? vstPaths[0] : juce::File();
#endif
        auto chooser = std::make_shared<juce::FileChooser> (
            "Load VST3 plugins", startDir, "*.vst3", true);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, chooser] (const juce::FileChooser& c)
        {
            auto files = c.getResults();
            if (files.isEmpty())
                return;

            juce::StringArray beforeIds;
            for (const auto& d : proc.getPluginHost().knownList.getTypes())
                beforeIds.add (d.createIdentifierString());

            scanBtn.setEnabled (false);
            scanBtn.setButtonText ("Loading...");
            proc.getPluginHost().importVst3Files (files, [this, beforeIds] (int added)
            {
                scanBtn.setEnabled (true);
                scanBtn.setButtonText ("Plugins");
                refreshPluginCombos();
                if (added < 1)
                {
                    juce::NativeMessageBox::showMessageBoxAsync (
                        juce::MessageBoxIconType::InfoIcon,
                        "Plugins",
                        "No VST3 found in that pick.");
                    return;
                }

                juce::PluginDescription firstNew;
                bool have = false;
                for (const auto& d : proc.getPluginHost().knownList.getTypes())
                {
                    if (! beforeIds.contains (d.createIdentifierString()))
                    {
                        firstNew = d;
                        have = true;
                        break;
                    }
                }
                if (! have)
                    return;

                auto& rack = proc.getGuitarRack();
                int slot = 0;
                for (int i = 0; i < PluginRack::NumSlots; ++i)
                {
                    if (rack.getSlotPluginName (i).isEmpty())
                    {
                        slot = i;
                        break;
                    }
                }
                const auto err = rack.loadPlugin (slot, firstNew);
                if (err.isNotEmpty())
                    juce::NativeMessageBox::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon, "VST3", err);
                refreshPluginCombos();
            });
        });
    };
    arrange.onSelectTrack = [this] (int t) { selectDawTrack (t); };

    auto mapSession = [this] (MixerStrip& s, int dawTrack)
    {
        s.level.onValueChange = [this, &s, dawTrack]
        {
            proc.getDaw().getProject().tracks[(size_t) dawTrack].level.store ((float) s.level.getValue());
            if (dawTrack == Daw::kGuitar) proc.setBusLevel (SessionProcessor::Guitar, (float) s.level.getValue());
            if (dawTrack == Daw::kDrums)  proc.setBusLevel (SessionProcessor::Drums,  (float) s.level.getValue());
            if (dawTrack == Daw::kBass)   proc.setBusLevel (SessionProcessor::Bass,   (float) s.level.getValue());
            if (dawTrack == Daw::kKeys)   proc.setBusLevel (SessionProcessor::Keys,   (float) s.level.getValue());
            if (dawTrack == Daw::kMasterIndex) proc.setBusLevel (SessionProcessor::Master, (float) s.level.getValue());
            markDirty();
        };
        s.mute.onClick = [this, &s, dawTrack]
        {
            proc.getDaw().getProject().tracks[(size_t) dawTrack].mute.store (s.mute.getToggleState() ? 1 : 0);
        };
        s.solo.onClick = [this, &s, dawTrack]
        {
            proc.getDaw().getProject().tracks[(size_t) dawTrack].solo.store (s.solo.getToggleState() ? 1 : 0);
        };
    };
    mapSession (guitarStrip, Daw::kGuitar);
    mapSession (drumsStrip,  Daw::kDrums);
    mapSession (bassStrip,   Daw::kBass);
    mapSession (keysStrip,   Daw::kKeys);
    mapSession (masterStrip, Daw::kMasterIndex);

    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        auto* s = dawStrips[(size_t) i].get();
        s->level.onValueChange = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].level.store ((float) s->level.getValue());
        };
        s->pan.onValueChange = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].pan.store ((float) s->pan.getValue());
        };
        s->mute.onClick = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].mute.store (s->mute.getToggleState() ? 1 : 0);
        };
        s->solo.onClick = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].solo.store (s->solo.getToggleState() ? 1 : 0);
        };
        s->arm.onClick = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].arm.store (s->arm.getToggleState() ? 1 : 0);
        };
        s->mon.onClick = [this, s, i]
        {
            proc.getDaw().getProject().tracks[(size_t) i].monitor.store (s->mon.getToggleState() ? 1 : 0);
        };
        s->addMouseListener (this, false);
    }

    for (int i = 0; i < 4; ++i)
    {
        bindPluginSlot (gSlot[(size_t) i], gByp[(size_t) i], gEd[(size_t) i],
                        [this] () -> PluginRack& { return proc.getGuitarRack(); }, i);
        bindPluginSlot (tSlot[(size_t) i], tByp[(size_t) i], tEd[(size_t) i],
                        [this] () -> PluginRack& { return selectedTrackRack(); }, i);
        gUp[(size_t) i].onClick = [this, i]
        {
            proc.getGuitarRack().swapOrder (i, i - 1);
            refreshPluginCombos();
        };
        gDn[(size_t) i].onClick = [this, i]
        {
            proc.getGuitarRack().swapOrder (i, i + 1);
            refreshPluginCombos();
        };
        tUp[(size_t) i].onClick = [this, i]
        {
            selectedTrackRack().swapOrder (i, i - 1);
            refreshPluginCombos();
        };
        tDn[(size_t) i].onClick = [this, i]
        {
            selectedTrackRack().swapOrder (i, i + 1);
            refreshPluginCombos();
        };
    }
    refreshPluginCombos();
    refreshDawMixer();

    formBox.onChange = [this]
    {
        proc.getBand().setForm ((FollowerBand::Form) (formBox.getSelectedId() - 1));
        markDirty();
    };
    scaleBox.onChange = [this]
    {
        proc.getBand().setScale ((FollowerBand::Scale) (scaleBox.getSelectedId() - 1));
        applyScaleMask();
        markDirty();
    };
    feelBox.onChange = [this]
    {
        proc.getBand().setFeel ((FollowerBand::Feel) (feelBox.getSelectedId() - 1));
        markDirty();
    };
    phraseBox.onChange = [this]
    {
        const int bars[] = { 4, 8, 16 };
        const int id = juce::jlimit (1, 3, phraseBox.getSelectedId());
        proc.getBand().setPhraseBars (bars[id - 1]);
        markDirty();
    };
    delayDivBox.onChange = [this]
    {
        proc.getFx().setDivision ((GuitarFx::Division) (delayDivBox.getSelectedId() - 1));
        markDirty();
    };
    lockTempo.onClick = [this]
    {
        proc.getAnalyzer().setLockTempo (lockTempo.getToggleState());
        if (! lockTempo.getToggleState() && autoBpm.getToggleState())
            proc.getAnalyzer().unlockBpm();
        markDirty();
    };
    lockIntensity.onClick = [this]
    {
        proc.getAnalyzer().setLockIntensity (lockIntensity.getToggleState());
        markDirty();
    };
    grooveFloor.onClick = [this]
    {
        proc.setGrooveFloor (grooveFloor.getToggleState());
        markDirty();
    };
    fadeSilence.onClick = [this]
    {
        proc.setFadeOnSilence (fadeSilence.getToggleState());
        markDirty();
    };
    energyDrift.onClick = [this]
    {
        proc.getAnalyzer().setEnergyDrift (energyDrift.getToggleState());
        markDirty();
    };
    calSoft.onClick = [this] { proc.getAnalyzer().captureCal (0); };
    calMid.onClick  = [this] { proc.getAnalyzer().captureCal (1); };
    calHard.onClick = [this] { proc.getAnalyzer().captureCal (2); };
    applyScaleMask();
}

void SessionUI::applyToProcessor()
{
    proc.getBand().setStyle ((FollowerBand::Style) juce::jlimit (0, (int) FollowerBand::Style::NumStyles - 1,
                                                                styleBox.getSelectedId() - 1));
    proc.getBand().setDrumKit ((FollowerBand::DrumKit) juce::jlimit (0, (int) FollowerBand::DrumKit::NumKits - 1,
                                                                    kitBox.getSelectedId() - 1));
    proc.getBand().setBassVoice ((FollowerBand::BassVoice) juce::jlimit (0, (int) FollowerBand::BassVoice::NumVoices - 1,
                                                                        bassVoiceBox.getSelectedId() - 1));
    proc.getBand().setKeysVoice ((FollowerBand::KeysVoice) juce::jlimit (0, (int) FollowerBand::KeysVoice::NumVoices - 1,
                                                                        keysVoiceBox.getSelectedId() - 1));
    proc.getFxChair().setVoice ((FxChair::Voice) juce::jlimit (0, (int) FxChair::Voice::NumVoices - 1,
                                                              fxVoiceBox.getSelectedId() - 1));
    if (autoKey.getToggleState())
        proc.getAnalyzer().unlockKey();
    else
        proc.getAnalyzer().setManualKey (keyBox.getSelectedId() - 1);

    if (autoBpm.getToggleState())
    {
        proc.getAnalyzer().unlockBpm();
        proc.getAnalyzer().setBpmSeed ((float) bpmSlider.getValue());
        proc.getAnalyzer().setLockTempo (lockTempo.getToggleState());
    }
    else
        proc.getAnalyzer().setManualBpm ((float) bpmSlider.getValue());

    proc.setCountInEnabled (countInToggle.getToggleState());
    proc.setInputGainDb ((float) gainSlider.getValue());
    proc.setGate ((float) gateSlider.getValue());
    proc.getAmp().setDrive ((float) driveSlider.getValue());
    proc.getAmp().setTone  ((float) toneSlider.getValue());
    proc.getAmp().setLevel ((float) levelSlider.getValue());
    proc.setDelayMix ((float) delaySlider.getValue());
    proc.setSpaceMix ((float) spaceSlider.getValue());
    proc.setBusLevel (SessionProcessor::Guitar, (float) guitarStrip.level.getValue());
    proc.setBusLevel (SessionProcessor::Drums,  (float) drumsStrip.level.getValue());
    proc.setBusLevel (SessionProcessor::Bass,   (float) bassStrip.level.getValue());
    proc.setBusLevel (SessionProcessor::Keys,   (float) keysStrip.level.getValue());
    proc.setBusLevel (SessionProcessor::Fx,     (float) fxStrip.level.getValue());
    proc.setBusLevel (SessionProcessor::Master, (float) masterStrip.level.getValue());
    proc.setAmpBypass (ampBypass.getToggleState());
    proc.getBand().setForm ((FollowerBand::Form) juce::jmax (0, formBox.getSelectedId() - 1));
    proc.getBand().setScale ((FollowerBand::Scale) juce::jmax (0, scaleBox.getSelectedId() - 1));
    proc.getBand().setFeel ((FollowerBand::Feel) juce::jmax (0, feelBox.getSelectedId() - 1));
    proc.getBand().setPhraseBars (phraseBox.getSelectedId() == 1 ? 4 : (phraseBox.getSelectedId() == 3 ? 16 : 8));
    proc.getFx().setDivision ((GuitarFx::Division) juce::jmax (0, delayDivBox.getSelectedId() - 1));
    proc.getAnalyzer().setLockTempo (lockTempo.getToggleState());
    proc.getAnalyzer().setLockIntensity (lockIntensity.getToggleState());
    proc.setGrooveFloor (grooveFloor.getToggleState());
    proc.setFadeOnSilence (fadeSilence.getToggleState());
    proc.getAnalyzer().setEnergyDrift (energyDrift.getToggleState());
    applyScaleMask();
    auto& tracks = proc.getDaw().getProject().tracks;
    tracks[(size_t) Daw::kGuitar].level.store ((float) guitarStrip.level.getValue());
    tracks[(size_t) Daw::kDrums].level.store  ((float) drumsStrip.level.getValue());
    tracks[(size_t) Daw::kBass].level.store   ((float) bassStrip.level.getValue());
    tracks[(size_t) Daw::kKeys].level.store   ((float) keysStrip.level.getValue());
    tracks[(size_t) Daw::kMasterIndex].level.store ((float) masterStrip.level.getValue());
}

void SessionUI::loadSettings()
{
    auto xml = juce::XmlDocument::parse (SessionSettings::uiXml());
    if (xml == nullptr)
        return;

    syncFromSetup (SessionSettings::Setup::fromXml (*xml));
    countInToggle.setToggleState (xml->getIntAttribute ("countIn", 1) != 0, juce::dontSendNotification);
    gainSlider.setValue (xml->getDoubleAttribute ("gain", 0.0), juce::dontSendNotification);
    gateSlider.setValue (xml->getDoubleAttribute ("gate", 0.0), juce::dontSendNotification);
    driveSlider.setValue (xml->getDoubleAttribute ("drive", 0.42), juce::dontSendNotification);
    toneSlider.setValue (xml->getDoubleAttribute ("tone", 0.55), juce::dontSendNotification);
    levelSlider.setValue (xml->getDoubleAttribute ("level", 0.80), juce::dontSendNotification);
    delaySlider.setValue (xml->getDoubleAttribute ("delay", 0.18), juce::dontSendNotification);
    spaceSlider.setValue (xml->getDoubleAttribute ("space", 0.16), juce::dontSendNotification);
    guitarStrip.level.setValue (xml->getDoubleAttribute ("gtr", 0.85), juce::dontSendNotification);
    drumsStrip.level.setValue  (xml->getDoubleAttribute ("drm", 0.70), juce::dontSendNotification);
    bassStrip.level.setValue   (xml->getDoubleAttribute ("bas", 0.78), juce::dontSendNotification);
    keysStrip.level.setValue   (xml->getDoubleAttribute ("keyLvl", 0.52), juce::dontSendNotification);
    fxStrip.level.setValue     (xml->getDoubleAttribute ("fxLvl", 0.62), juce::dontSendNotification);
    masterStrip.level.setValue (xml->getDoubleAttribute ("mst", 0.90), juce::dontSendNotification);
}

void SessionUI::saveSettings()
{
    juce::XmlElement xml ("SessionUI");
    readSetup().writeTo (xml);
    xml.setAttribute ("countIn", countInToggle.getToggleState() ? 1 : 0);
    xml.setAttribute ("gain",    gainSlider.getValue());
    xml.setAttribute ("gate",    gateSlider.getValue());
    xml.setAttribute ("drive",   driveSlider.getValue());
    xml.setAttribute ("tone",    toneSlider.getValue());
    xml.setAttribute ("level",   levelSlider.getValue());
    xml.setAttribute ("delay",   delaySlider.getValue());
    xml.setAttribute ("space",   spaceSlider.getValue());
    xml.setAttribute ("gtr",     guitarStrip.level.getValue());
    xml.setAttribute ("drm",     drumsStrip.level.getValue());
    xml.setAttribute ("bas",     bassStrip.level.getValue());
    xml.setAttribute ("keyLvl",  keysStrip.level.getValue());
    xml.setAttribute ("fxLvl",   fxStrip.level.getValue());
    xml.setAttribute ("mst",     masterStrip.level.getValue());
    xml.writeTo (SessionSettings::uiXml());
    settingsDirty = false;
}

void SessionUI::syncFromSetup (const SessionSettings::Setup& s)
{
    styleBox.setSelectedId (s.style + 1, juce::dontSendNotification);
    kitBox.setSelectedId        (s.drumsKit  + 1, juce::dontSendNotification);
    bassVoiceBox.setSelectedId  (s.bassVoice + 1, juce::dontSendNotification);
    keysVoiceBox.setSelectedId  (s.keysVoice + 1, juce::dontSendNotification);
    fxVoiceBox.setSelectedId    (s.fxVoice   + 1, juce::dontSendNotification);
    formBox.setSelectedId  (s.form + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (s.scale + 1, juce::dontSendNotification);
    feelBox.setSelectedId  (s.feel + 1, juce::dontSendNotification);
    keyBox.setSelectedId   (s.keyPc + 1, juce::dontSendNotification);
    autoKey.setToggleState (s.followKey, juce::dontSendNotification);
    bpmSlider.setValue ((double) s.bpm, juce::dontSendNotification);
    autoBpm.setToggleState (s.slew, juce::dontSendNotification);
    lockTempo.setToggleState (! s.slew, juce::dontSendNotification);
    phraseBox.setSelectedId (s.phraseBars <= 4 ? 1 : (s.phraseBars >= 16 ? 3 : 2), juce::dontSendNotification);
    drumsStrip.mute.setToggleState (! s.drumsIn, juce::dontSendNotification);
    bassStrip.mute.setToggleState  (! s.bassIn, juce::dontSendNotification);
    keysStrip.mute.setToggleState  (! s.keysIn, juce::dontSendNotification);
    fxStrip.mute.setToggleState    (! s.fxIn, juce::dontSendNotification);
}

SessionSettings::Setup SessionUI::readSetup() const
{
    SessionSettings::Setup s;
    s.drumsIn = proc.getBand().isMemberEnabled (FollowerBand::MemberDrums);
    s.bassIn  = proc.getBand().isMemberEnabled (FollowerBand::MemberBass);
    s.keysIn  = proc.getBand().isMemberEnabled (FollowerBand::MemberKeys);
    s.fxIn    = proc.getFxChair().isEnabled();
    s.style   = juce::jlimit (0, 4, styleBox.getSelectedId() - 1);
    s.drumsKit  = juce::jlimit (0, 4, kitBox.getSelectedId() - 1);
    s.bassVoice = juce::jlimit (0, 3, bassVoiceBox.getSelectedId() - 1);
    s.keysVoice = juce::jlimit (0, 4, keysVoiceBox.getSelectedId() - 1);
    s.fxVoice   = juce::jlimit (0, 3, fxVoiceBox.getSelectedId() - 1);
    s.form    = juce::jlimit (0, 3, formBox.getSelectedId() - 1);
    s.scale   = juce::jlimit (0, 3, scaleBox.getSelectedId() - 1);
    s.feel    = juce::jlimit (0, 3, feelBox.getSelectedId() - 1);
    s.keyPc   = juce::jlimit (0, 11, keyBox.getSelectedId() - 1);
    s.followKey = autoKey.getToggleState();
    s.bpm     = (float) bpmSlider.getValue();
    s.slew    = autoBpm.getToggleState() && ! lockTempo.getToggleState();
    s.phraseBars = phraseBox.getSelectedId() == 1 ? 4 : (phraseBox.getSelectedId() == 3 ? 16 : 8);
    return s;
}

void SessionUI::refreshTransport()
{
    const bool run = proc.isSessionRunning();
    startBtn.setEnabled (! run);
    stopBtn.setEnabled (run);
    if (! run)
        startBtn.setButtonText ("Start");
    else if (proc.isWaitingForNotes())
        startBtn.setButtonText ("Waiting…");
    else if (proc.isCountingIn())
        startBtn.setButtonText ("Count-in");
    else
        startBtn.setButtonText ("Jamming");
    startBtn.setColour (juce::TextButton::buttonColourId,
                        run ? juce::Colour (SessionLookAndFeel::kAccent).darker (0.45f)
                            : juce::Colour (SessionLookAndFeel::kAccent).darker (0.15f));

    const bool playing = proc.getDaw().isPlaying();
    playBtn.setButtonText (playing ? "Stop" : "Play");
    playBtn.setToggleState (playing, juce::dontSendNotification);
    cycleBtn.setToggleState (proc.getDaw().isCycling(), juce::dontSendNotification);
    projectLbl.setText (proc.getDaw().getProject().name, juce::dontSendNotification);

    if (proc.isRecording())
    {
        int secs = (int) std::floor (proc.getWriter().getRecordedSeconds());
        if (secs <= 0)
        {
            const double sr = proc.getDaw().getSampleRate();
            secs = (int) (proc.getDaw().getPosition() / juce::jmax (1.0, sr));
        }
        recordBtn.setButtonText ("Stop Rec");
        recTime.setText (juce::String::formatted ("REC  %d:%02d", secs / 60, secs % 60),
                         juce::dontSendNotification);
        recTime.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kRecord));
    }
    else
    {
        recordBtn.setButtonText ("Record");
        recTime.setText ("", juce::dontSendNotification);
    }
}

juce::Font SessionUI::uiFont (float px, bool bold) const
{
    auto opts = juce::FontOptions ((float) std::round (px * scale));
    juce::Font f (opts);
    if (bold)
        f = f.boldened();
    return f;
}

void SessionUI::applyFonts()
{
    look.setUiScale (scale);
    product.setFont (uiFont (28.0f, true));
    brand.setFont (uiFont (14.0f));
    keyReadout.setFont (uiFont (20.0f, true));
    bpmReadout.setFont (uiFont (20.0f, true));
    barReadout.setFont (uiFont (22.0f, true));
    tunerNote.setFont (uiFont (22.0f, true));
    recPath.setFont (uiFont (13.0f));
    recTime.setFont (uiFont (15.0f, true));
    styleLbl.setFont (uiFont (13.0f));
    followLbl.setFont (uiFont (13.0f));
    ampLbl.setFont (uiFont (13.0f));
    mixLbl.setFont (uiFont (13.0f));
    bpmLbl.setFont (uiFont (13.0f));
    guitarVstLbl.setFont (uiFont (12.0f));
    trackVstLbl.setFont (uiFont (12.0f));
    for (int i = 0; i < 4; ++i)
    {
        gSlotLbl[(size_t) i].setFont (uiFont (11.0f));
        tSlotLbl[(size_t) i].setFont (uiFont (11.0f));
    }
    projectLbl.setFont (uiFont (13.0f, true));
    chordName.setFont (uiFont (26.0f, true));
    playerNote.setFont (uiFont (16.0f, true));
    noteLaneLbl.setFont (uiFont (11.0f));
    nextChord.setFont (uiFont (14.0f));
}

float SessionUI::computeScale() const noexcept
{
    // 1280x720 = 1.0. Do not multiply OS DPI — JUCE already applies it.
    const float sxv = (float) getWidth()  / 1280.0f;
    const float syv = (float) getHeight() / 720.0f;
    return juce::jlimit (0.80f, 1.45f, juce::jmin (sxv, syv));
}

void SessionUI::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, float peak, juce::Colour c) const
{
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, peak), -48.0f);
    const float n = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    auto fill = r.toFloat().reduced (2.0f);
    fill.setWidth (fill.getWidth() * n);
    g.setColour (n > 0.92f ? juce::Colour (SessionLookAndFeel::kRecord) : c);
    g.fillRoundedRectangle (fill, 3.0f);
}

void SessionUI::drawTuner (juce::Graphics& g, juce::Rectangle<int> r) const
{
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (juce::Colour (SessionLookAndFeel::kLine));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto inner = r.reduced (sx (10), sx (8));
    const float hz = proc.getAnalyzer().getFrequencyHz();
    const float cents = proc.getAnalyzer().getCents();
    const bool live = hz > 20.0f && proc.getAnalyzer().getConfidence() > 0.25f;

    g.setFont (uiFont (11.0f));
    g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
    g.drawText ("TUNER", inner.removeFromTop (sx (14)), juce::Justification::centredLeft, false);

    auto needleArea = inner.removeFromBottom (sx (22));
    g.setColour (juce::Colour (SessionLookAndFeel::kLine));
    g.fillRoundedRectangle (needleArea.toFloat(), 3.0f);
    const float midX = (float) needleArea.getCentreX();
    g.setColour (juce::Colour (SessionLookAndFeel::kMuted).withAlpha (0.7f));
    g.drawVerticalLine ((int) midX, (float) needleArea.getY(), (float) needleArea.getBottom());

    if (live)
    {
        const float n = juce::jlimit (-50.0f, 50.0f, cents) / 50.0f; // -1..1
        const float x = midX + n * (float) (needleArea.getWidth() / 2 - 4);
        const bool inTune = std::abs (cents) < 8.0f;
        g.setColour (inTune ? juce::Colour (SessionLookAndFeel::kLocked)
                            : juce::Colour (SessionLookAndFeel::kHunt));
        g.fillEllipse (x - 4.0f * scale, (float) needleArea.getCentreY() - 4.0f * scale,
                       8.0f * scale, 8.0f * scale);
    }
}

void SessionUI::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (SessionLookAndFeel::kBg));

    auto fillPanel = [&g] (juce::Rectangle<int> r, juce::uint32 c)
    {
        g.setColour (juce::Colour (c));
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
    };
    fillPanel (headerBounds, SessionLookAndFeel::kPanel);
    fillPanel (transportBounds, SessionLookAndFeel::kHero);
    fillPanel (followBounds, SessionLookAndFeel::kPanel);
    fillPanel (hudBounds, SessionLookAndFeel::kHero);
    drawEnergy (g, playerMeterBounds, proc.getPlayerEnergy(), juce::Colour (SessionLookAndFeel::kGuitar), "You");
    drawEnergy (g, bandMeterBounds, proc.getBandEnergy(), juce::Colour (SessionLookAndFeel::kAccent), "Band");
    drawNeck (g, neckBounds);
    drawNoteLane (g, noteLaneBounds);

    drawMeter (g, inMeterBounds, proc.getInputPeak(), juce::Colour (SessionLookAndFeel::kGuitar));
    g.setFont (uiFont (11.0f));
    g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
    g.drawText ("IN", inMeterBounds.translated (0, -sx (14)), juce::Justification::centredLeft, false);

    drawTuner (g, tunerBounds);

    if (proc.isRecording())
    {
        const float pulse = 0.45f + 0.55f * (0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.012f));
        g.setColour (juce::Colour (SessionLookAndFeel::kRecord).withAlpha (pulse));
        auto rec = transportBounds.withTrimmedLeft (transportBounds.getWidth() - sx (210))
                                  .removeFromLeft (sx (14))
                                  .withSizeKeepingCentre (sx (10), sx (10));
        // Draw next to recTime instead.
        auto pulseR = recTime.getBounds().translated (-sx (16), 0).withWidth (sx (12)).withSizeKeepingCentre (sx (10), sx (10));
        juce::ignoreUnused (rec);
        if (recTime.getText().isNotEmpty())
            g.fillEllipse (pulseR.toFloat());
    }
}

void SessionUI::resized()
{
    scale = computeScale();
    applyFonts();

    auto r = getLocalBounds().reduced (sx (12));
    headerBounds = r.removeFromTop (sx (64));
    {
        auto h = headerBounds.reduced (sx (12), sx (8));
        auto left = h.removeFromLeft (sx (170));
        product.setBounds (left.removeFromTop (sx (28)));
        brand.setBounds (left.removeFromTop (sx (16)));
        h.removeFromLeft (sx (6));
        tunerBounds = h.removeFromLeft (sx (170)).reduced (0, sx (2));
        tunerNote.setBounds (tunerBounds.reduced (sx (8), sx (6)).withTrimmedBottom (sx (20)).withTrimmedTop (sx (12)));
        h.removeFromLeft (sx (8));
        auto meters = h.removeFromLeft (sx (150));
        inMeterBounds = meters.removeFromBottom (sx (10));
        meters.removeFromBottom (sx (2));
        keyReadout.setBounds (h.removeFromLeft (sx (190)));
        bpmReadout.setBounds (h.removeFromLeft (sx (190)));
        projectLbl.setBounds (h);
    }

    r.removeFromTop (sx (8));
    transportBounds = r.removeFromTop (sx (56));
    {
        auto t = transportBounds.reduced (sx (8), sx (8));
        playBtn.setBounds (t.removeFromLeft (sx (70)).reduced (sx (2)));
        rtzBtn.setBounds (t.removeFromLeft (sx (54)).reduced (sx (2)));
        cycleBtn.setBounds (t.removeFromLeft (sx (68)).reduced (sx (2)));
        recordBtn.setBounds (t.removeFromLeft (sx (100)).reduced (sx (2)));
        recTime.setBounds (t.removeFromLeft (sx (90)).reduced (sx (2), sx (4)));
        t.removeFromLeft (sx (6));
        startBtn.setBounds (t.removeFromLeft (sx (140)).reduced (sx (2)));
        stopBtn.setBounds (t.removeFromLeft (sx (70)).reduced (sx (2)));
        lobbyBtn.setBounds (t.removeFromLeft (sx (72)).reduced (sx (2)));
        countInToggle.setBounds (t.removeFromLeft (sx (90)).reduced (0, sx (6)));
        barReadout.setBounds (t.removeFromLeft (sx (80)));
        audioBtn.setBounds (t.removeFromRight (sx (80)).reduced (sx (2)));
        bounceBtn.setBounds (t.removeFromRight (sx (74)).reduced (sx (2)));
        undoBtn.setBounds (t.removeFromRight (sx (62)).reduced (sx (2)));
        saveBtn.setBounds (t.removeFromRight (sx (58)).reduced (sx (2)));
        openBtn.setBounds (t.removeFromRight (sx (58)).reduced (sx (2)));
        newBtn.setBounds (t.removeFromRight (sx (54)).reduced (sx (2)));
        recPath.setBounds (t.reduced (sx (6), sx (8)));
    }

    r.removeFromTop (sx (8));
    followBounds = r.removeFromTop (sx (48));
    {
        auto f = followBounds.reduced (sx (10), sx (6));
        followLbl.setBounds (f.removeFromLeft (sx (50)).reduced (0, sx (6)));
        styleLbl.setBounds (f.removeFromLeft (sx (40)).reduced (0, sx (8)));
        styleBox.setBounds (f.removeFromLeft (sx (110)).reduced (sx (2), sx (4)));
        f.removeFromLeft (sx (8));
        autoKey.setBounds (f.removeFromLeft (sx (86)).reduced (0, sx (6)));
        keyBox.setBounds (f.removeFromLeft (sx (70)).reduced (sx (2), sx (4)));
        f.removeFromLeft (sx (8));
        autoBpm.setBounds (f.removeFromLeft (sx (90)).reduced (0, sx (6)));
        bpmLbl.setBounds (f.removeFromLeft (sx (36)).reduced (0, sx (8)));
        bpmSlider.setBounds (f.removeFromLeft (sx (180)).reduced (sx (2), sx (6)));
        bpmDown.setBounds (f.removeFromLeft (sx (28)).reduced (sx (1), sx (4)));
        bpmUp.setBounds (f.removeFromLeft (sx (28)).reduced (sx (1), sx (4)));
        viewArrange.setBounds (f.removeFromRight (sx (80)).reduced (sx (2)));
        viewMixer.setBounds (f.removeFromRight (sx (70)).reduced (sx (2)));
    }

    r.removeFromTop (sx (4));
    auto jam = r.removeFromTop (sx (40));
    {
        formLbl.setBounds (jam.removeFromLeft (sx (40)).reduced (0, sx (10)));
        formBox.setBounds (jam.removeFromLeft (sx (92)).reduced (sx (2), sx (6)));
        scaleLbl.setBounds (jam.removeFromLeft (sx (42)).reduced (0, sx (10)));
        scaleBox.setBounds (jam.removeFromLeft (sx (110)).reduced (sx (2), sx (6)));
        feelLbl.setBounds (jam.removeFromLeft (sx (36)).reduced (0, sx (10)));
        feelBox.setBounds (jam.removeFromLeft (sx (88)).reduced (sx (2), sx (6)));
        phraseLbl.setBounds (jam.removeFromLeft (sx (50)).reduced (0, sx (10)));
        phraseBox.setBounds (jam.removeFromLeft (sx (88)).reduced (sx (2), sx (6)));
        lockTempo.setBounds (jam.removeFromLeft (sx (100)).reduced (0, sx (6)));
        lockIntensity.setBounds (jam.removeFromLeft (sx (110)).reduced (0, sx (6)));
        grooveFloor.setBounds (jam.removeFromLeft (sx (104)).reduced (0, sx (6)));
        fadeSilence.setBounds (jam.removeFromLeft (sx (108)).reduced (0, sx (6)));
        energyDrift.setBounds (jam.removeFromLeft (sx (108)).reduced (0, sx (6)));
        calSoft.setBounds (jam.removeFromLeft (sx (52)).reduced (sx (2), sx (6)));
        calMid.setBounds (jam.removeFromLeft (sx (48)).reduced (sx (2), sx (6)));
        calHard.setBounds (jam.removeFromLeft (sx (52)).reduced (sx (2), sx (6)));
    }

    r.removeFromTop (sx (6));
    hudBounds = r.removeFromTop (sx (108));
    {
        auto h = hudBounds;
        auto top = h.removeFromTop (sx (56));
        chordName.setBounds (top.removeFromLeft (sx (130)).reduced (sx (4), sx (8)));
        nextChord.setBounds (top.removeFromLeft (sx (130)).reduced (sx (4), sx (16)));
        playerNote.setBounds (top.removeFromLeft (sx (90)).reduced (sx (4), sx (16)));
        energyLbl.setBounds (top.removeFromLeft (sx (70)).reduced (0, sx (16)));
        playerMeterBounds = top.removeFromLeft (sx (120)).reduced (sx (2), sx (12));
        bandMeterBounds = top.removeFromLeft (sx (120)).reduced (sx (2), sx (12));
        loadFxBtn.setBounds (top.removeFromRight (sx (78)).reduced (sx (4), sx (10)));
        midiBtn.setBounds (top.removeFromRight (sx (108)).reduced (sx (4), sx (10)));
        neckBounds = top.reduced (sx (4), sx (6));
        auto lane = h;
        noteLaneLbl.setBounds (lane.removeFromLeft (sx (52)).reduced (sx (2), sx (8)));
        noteLaneBounds = lane.reduced (sx (4), sx (4));
    }

    r.removeFromTop (sx (6));
    auto ampRow = r.removeFromTop (sx (100));
    {
        ampLbl.setBounds (ampRow.removeFromTop (sx (16)));
        ampBypass.setBounds (ampLbl.getBounds().withX (ampLbl.getRight() + sx (8)).withWidth (sx (110)));
        const int col = ampRow.getWidth() / 8;
        auto place = [this, col] (juce::Rectangle<int>& row, juce::Label& lab, juce::Slider& sl)
        {
            auto c = row.removeFromLeft (col);
            lab.setBounds (c.removeFromTop (sx (14)));
            sl.setBounds (c.reduced (sx (2), 0));
        };
        place (ampRow, gainLbl,  gainSlider);
        place (ampRow, gateLbl,  gateSlider);
        place (ampRow, driveLbl, driveSlider);
        place (ampRow, toneLbl,  toneSlider);
        place (ampRow, levelLbl, levelSlider);
        place (ampRow, delayLbl, delaySlider);
        place (ampRow, spaceLbl, spaceSlider);
        delayDivBox.setBounds (delaySlider.getBounds().removeFromBottom (sx (18)).translated (0, sx (2)));
    }

    r.removeFromTop (sx (6));
    auto vstRow = r.removeFromTop (sx (64));
    {
        guitarVstLbl.setBounds (vstRow.removeFromLeft (sx (78)).reduced (0, sx (18)));
        scanBtn.setBounds (vstRow.removeFromRight (sx (72)).reduced (sx (2), sx (12)));
        const int slotW = vstRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto c = vstRow.removeFromLeft (slotW).reduced (sx (2), sx (2));
            gSlotLbl[(size_t) i].setBounds (c.removeFromTop (sx (14)));
            gDn[(size_t) i].setBounds (c.removeFromRight (sx (22)));
            gUp[(size_t) i].setBounds (c.removeFromRight (sx (22)));
            gEd[(size_t) i].setBounds (c.removeFromRight (sx (28)));
            gByp[(size_t) i].setBounds (c.removeFromRight (sx (32)));
            gSlot[(size_t) i].setBounds (c);
        }
    }

    r.removeFromTop (sx (6));
    auto mixH = sx (186);
    auto mixArea = r.removeFromBottom (mixH);
    {
        auto mixHead = mixArea.removeFromTop (sx (28));
        mixLbl.setBounds (mixHead.removeFromLeft (sx (58)).reduced (0, sx (4)));
        kitLbl.setBounds (mixHead.removeFromLeft (sx (28)).reduced (0, sx (6)));
        kitBox.setBounds (mixHead.removeFromLeft (sx (96)).reduced (sx (2), sx (2)));
        mixHead.removeFromLeft (sx (8));
        bassVoiceLbl.setBounds (mixHead.removeFromLeft (sx (36)).reduced (0, sx (6)));
        bassVoiceBox.setBounds (mixHead.removeFromLeft (sx (88)).reduced (sx (2), sx (2)));
        mixHead.removeFromLeft (sx (8));
        keysVoiceLbl.setBounds (mixHead.removeFromLeft (sx (36)).reduced (0, sx (6)));
        keysVoiceBox.setBounds (mixHead.removeFromLeft (sx (88)).reduced (sx (2), sx (2)));
        mixHead.removeFromLeft (sx (8));
        fxVoiceLbl.setBounds (mixHead.removeFromLeft (sx (24)).reduced (0, sx (6)));
        fxVoiceBox.setBounds (mixHead.removeFromLeft (sx (80)).reduced (sx (2), sx (2)));
    }
    {
        auto sessionMix = mixArea.removeFromRight (sx (360));
        const int sw = sessionMix.getWidth() / 6;
        guitarStrip.setBounds (sessionMix.removeFromLeft (sw).reduced (sx (1)));
        drumsStrip.setBounds  (sessionMix.removeFromLeft (sw).reduced (sx (1)));
        bassStrip.setBounds   (sessionMix.removeFromLeft (sw).reduced (sx (1)));
        keysStrip.setBounds   (sessionMix.removeFromLeft (sw).reduced (sx (1)));
        fxStrip.setBounds     (sessionMix.removeFromLeft (sw).reduced (sx (1)));
        masterStrip.setBounds (sessionMix.reduced (sx (1)));

        const int dw = mixArea.getWidth() / Daw::kNumTracks;
        for (int i = 0; i < Daw::kNumTracks; ++i)
            dawStrips[(size_t) i]->setBounds (mixArea.removeFromLeft (dw).reduced (sx (1)));
    }

    r.removeFromTop (sx (4));
    auto trackVst = r.removeFromBottom (sx (64));
    {
        trackVstLbl.setBounds (trackVst.removeFromLeft (sx (78)).reduced (0, sx (18)));
        const int slotW = trackVst.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto c = trackVst.removeFromLeft (slotW).reduced (sx (2), sx (2));
            tSlotLbl[(size_t) i].setBounds (c.removeFromTop (sx (14)));
            tDn[(size_t) i].setBounds (c.removeFromRight (sx (22)));
            tUp[(size_t) i].setBounds (c.removeFromRight (sx (22)));
            tEd[(size_t) i].setBounds (c.removeFromRight (sx (28)));
            tByp[(size_t) i].setBounds (c.removeFromRight (sx (32)));
            tSlot[(size_t) i].setBounds (c);
        }
    }

    r.removeFromTop (sx (4));
    arrange.setBounds (r);
    arrange.setVisible (! mixerFront);
}

void SessionUI::timerCallback()
{
    auto& an = proc.getAnalyzer();
    const int pc = an.getKeyPc();
    juce::String keyText = "Key  ";
    keyText << InputAnalyzer::pitchClassName (pc);
    if (an.isKeyLocked())
        keyText << "  LOCKED";
    else if (an.isAutoKey())
        keyText << "  hunting";
    else
        keyText << "  MANUAL";
    keyReadout.setText (keyText, juce::dontSendNotification);
    keyReadout.setColour (juce::Label::textColourId,
                          an.isKeyLocked() ? juce::Colour (SessionLookAndFeel::kLocked)
                                           : (an.isAutoKey() ? juce::Colour (SessionLookAndFeel::kHunt)
                                                             : juce::Colour (SessionLookAndFeel::kText)));

    juce::String bpmText = "BPM  ";
    bpmText << juce::String (an.getBpm(), 0);
    if (an.isLockTempo())
        bpmText << "  LOCKED";
    else if (an.isAutoBpm())
        bpmText << (an.isBpmConfident() ? "  FOLLOWING" : "  hunting");
    else
        bpmText << "  MANUAL";
    bpmReadout.setText (bpmText, juce::dontSendNotification);
    bpmReadout.setColour (juce::Label::textColourId,
                          an.isLockTempo() ? juce::Colour (SessionLookAndFeel::kLocked)
                                           : (an.isAutoBpm() ? (an.isBpmConfident()
                                                                ? juce::Colour (SessionLookAndFeel::kLocked)
                                                                : juce::Colour (SessionLookAndFeel::kHunt))
                                                             : juce::Colour (SessionLookAndFeel::kText)));

    const float hz = an.getFrequencyHz();
    if (hz > 20.0f)
    {
        juce::String t = InputAnalyzer::noteNameFromMidi (an.getMidiNote());
        const float cents = an.getCents();
        t << (cents >= 0.0f ? "  +" : "  ") << juce::String (cents, 0);
        tunerNote.setText (t, juce::dontSendNotification);
    }
    else
    {
        tunerNote.setText ("--", juce::dontSendNotification);
    }

    if (proc.isCountingIn())
    {
        barReadout.setText ("COUNT " + juce::String (proc.getCountInBeat()), juce::dontSendNotification);
        barReadout.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kHunt));
    }
    else if (proc.isSessionRunning())
    {
        const int bar = proc.getBand().getAbsBarIndex() + 1;
        const int beat = proc.getBand().getBeatInBar() + 1;
        barReadout.setText (juce::String (bar) + "." + juce::String (beat), juce::dontSendNotification);
        barReadout.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kAccent));
    }
    else
    {
        barReadout.setText ("—", juce::dontSendNotification);
        barReadout.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kMuted));
    }

    if (autoKey.getToggleState() && ! keyBox.isMouseOverOrDragging())
        keyBox.setSelectedId (pc + 1, juce::dontSendNotification);
    if (autoBpm.getToggleState() && ! bpmSlider.isMouseButtonDown())
        bpmSlider.setValue ((double) an.getBpm(), juce::dontSendNotification);

    if (proc.isRecording())
    {
        recPath.setText (proc.getWriter().getSessionDirectory().getFullPathName(),
                         juce::dontSendNotification);
    }
    else
    {
        const auto dir = proc.getWriter().getSessionDirectory();
        if (dir.isDirectory())
            recPath.setText ("Last  " + dir.getFullPathName(), juce::dontSendNotification);
    }

    guitarStrip.setPeak (proc.getBusPeak (SessionProcessor::Guitar));
    drumsStrip.setPeak  (proc.getBusPeak (SessionProcessor::Drums));
    bassStrip.setPeak   (proc.getBusPeak (SessionProcessor::Bass));
    keysStrip.setPeak   (proc.getBusPeak (SessionProcessor::Keys));
    fxStrip.setPeak     (proc.getBusPeak (SessionProcessor::Fx));
    masterStrip.setPeak (proc.getBusPeak (SessionProcessor::Master));

    auto& daw = proc.getDaw();
    daw.getProject().bpm = an.getBpm();
    arrange.setBpm (an.getBpm());
    for (int i = 0; i < Daw::kNumTracks; ++i)
        dawStrips[(size_t) i]->setPeak (daw.getTrackPeak (i));
    arrange.repaint();

    auto& band = proc.getBand();
    juce::String ch = band.chordName() + "   " + band.romanName();
    if (proc.isWaitingForNotes())
        ch = "Ready  —  play to enter";
    chordName.setText (ch, juce::dontSendNotification);
    {
        const int n = an.getMidiNote();
        juce::String pn = "you  " + InputAnalyzer::noteNameFromMidi (n);
        if (n >= 0)
        {
            const int c = (int) std::lround (an.getCents());
            if (c != 0)
                pn << (c > 0 ? " +" : " ") << c;
        }
        playerNote.setText (pn, juce::dontSendNotification);
        playerNote.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kGuitar));
    }
    if (band.isChangeComing())
        nextChord.setText ("next  " + band.nextChordName() + "  " + FollowerBand::degreeName (band.getNextDegree()),
                           juce::dontSendNotification);
    else
        nextChord.setText (band.getScale() == FollowerBand::Scale::Pentatonic ? "" : band.nextChordName(),
                           juce::dontSendNotification);
    nextChord.setColour (juce::Label::textColourId,
                         band.isChangeComing() ? juce::Colour (SessionLookAndFeel::kHunt)
                                               : juce::Colour (SessionLookAndFeel::kMuted));

    if (! proc.isSessionRunning() && daw.isPlaying())
    {
        const double sr = juce::jmax (1.0, daw.getSampleRate());
        const float bpm = juce::jmax (40.0f, an.getBpm());
        const double beats = (double) daw.getPosition() / ((60.0 / (double) bpm) * sr);
        const int bar = (int) (beats / 4.0) + 1;
        const int beat = ((int) beats % 4) + 1;
        barReadout.setText (juce::String (bar) + "." + juce::String (beat), juce::dontSendNotification);
        barReadout.setColour (juce::Label::textColourId, juce::Colour (SessionLookAndFeel::kAccent));
    }

    refreshTransport();

    if (settingsDirty && ++persistTicks >= 40)
    {
        persistTicks = 0;
        saveSettings();
    }

    repaint();
}

void SessionUI::drawNoteLane (juce::Graphics& g, juce::Rectangle<int> r) const
{
    if (r.isEmpty())
        return;
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (juce::Colour (SessionLookAndFeel::kLine));
    g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);

    auto& an = proc.getAnalyzer();
    auto& band = proc.getBand();
    const float bpm = juce::jmax (40.0f, an.getBpm());
    const double sr = juce::jmax (1.0, proc.getDaw().getSampleRate());
    double nowQ = (double) proc.getDaw().getPosition() / ((60.0 / (double) bpm) * sr);
    if (nowQ <= 0.01)
        nowQ = (double) band.getAbsBarIndex() * 4.0 + (double) band.getBeatFraction() * 4.0;

    const double windowQ = 8.0; // two bars
    const double leftQ = nowQ - windowQ * 0.75;
    const float w = (float) r.getWidth();
    auto xAt = [&] (double q) -> float
    {
        return (float) r.getX() + (float) ((q - leftQ) / windowQ) * w;
    };

    // Bar / beat grid aligned to the playhead.
    for (int b = (int) std::floor (leftQ); b <= (int) std::ceil (leftQ + windowQ); ++b)
    {
        const float x = xAt ((double) b);
        if (x < (float) r.getX() || x > (float) r.getRight())
            continue;
        const bool bar = (b % 4) == 0;
        g.setColour (juce::Colour (bar ? SessionLookAndFeel::kAccent : SessionLookAndFeel::kLine)
                         .withAlpha (bar ? 0.45f : 0.25f));
        g.fillRect (x, (float) r.getY() + 2.0f, bar ? 2.0f : 1.0f, (float) r.getHeight() - 4.0f);
        if (bar)
        {
            g.setFont (uiFont (10.0f));
            g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
            g.drawText (juce::String (b / 4 + 1), juce::Rectangle<float> (x + 3.0f, (float) r.getY() + 1.0f, 28.0f, 12.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    const float playX = xAt (nowQ);
    g.setColour (juce::Colour (SessionLookAndFeel::kGuitar));
    g.fillRect (playX, (float) r.getY(), 2.0f, (float) r.getHeight());

    Daw::LiveNote live[InputAnalyzer::kLiveNotes];
    int written = 0;
    an.copyLiveNotes (live, InputAnalyzer::kLiveNotes, written);

    auto drawEv = [&] (double start, double dur, int midi, bool active)
    {
        if (midi < 0)
            return;
        const double end = start + juce::jmax (0.08, dur);
        if (end < leftQ || start > leftQ + windowQ)
            return;
        float x0 = juce::jlimit ((float) r.getX() + 2.0f, (float) r.getRight() - 8.0f, xAt (start));
        float x1 = juce::jlimit (x0 + 16.0f, (float) r.getRight() - 2.0f, xAt (end));
        auto box = juce::Rectangle<float> (x0, (float) r.getY() + 14.0f, juce::jmax (18.0f, x1 - x0), (float) r.getHeight() - 18.0f);
        g.setColour (juce::Colour (SessionLookAndFeel::kGuitar).withAlpha (active ? 0.90f : 0.55f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setFont (uiFont (11.0f, true));
        g.setColour (juce::Colour (SessionLookAndFeel::kBg));
        g.drawText (InputAnalyzer::noteNameFromMidi (midi), box, juce::Justification::centred, false);
    };

    for (int i = written - 1; i >= 0; --i)
        drawEv ((double) live[i].startBeat, (double) live[i].durBeat, live[i].midi, live[i].active != 0);

    const auto hist = an.copyHistory();
    const int nHist = juce::jmin ((int) hist.size(), 64);
    for (int i = (int) hist.size() - nHist; i < (int) hist.size(); ++i)
        if (i >= 0)
            drawEv (hist[(size_t) i].startQuarter, hist[(size_t) i].durationQuarter, hist[(size_t) i].midi, false);
}

void SessionUI::exportPlayerMidi()
{
    auto dest = SessionSettings::projectsDir().getChildFile (
        proc.getDaw().getProject().name + "-notes.mid");
    const auto err = proc.getAnalyzer().exportMidi (dest);
    if (err.isNotEmpty())
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export MIDI", err);
    else
        recPath.setText ("Export MIDI  " + dest.getFullPathName(), juce::dontSendNotification);
}

PluginRack& SessionUI::selectedTrackRack()
{
    auto& t = proc.getDaw().getProject().tracks[(size_t) juce::jlimit (0, Daw::kNumTracks - 1, selectedTrack)];
    if (t.inserts == nullptr)
        t.inserts = std::make_unique<PluginRack> (proc.getPluginHost());
    return *t.inserts;
}

void SessionUI::selectDawTrack (int t)
{
    selectedTrack = juce::jlimit (0, Daw::kNumTracks - 1, t);
    proc.getDaw().selectedTrack.store (selectedTrack);
    trackVstLbl.setText ("Track inserts  ·  " + proc.getDaw().getProject().tracks[(size_t) selectedTrack].name,
                         juce::dontSendNotification);
    refreshPluginCombos();
}

void SessionUI::fillPluginCombo (juce::ComboBox& box, PluginRack& rack, int slot)
{
    const auto current = rack.getSlotDescription (slot).createIdentifierString();
    box.clear (juce::dontSendNotification);
    box.addItem ("(empty)", 1);
    int sel = 1;
    int id = 2;
    for (const auto& d : proc.getPluginHost().knownList.getTypes())
    {
        box.addItem (d.name + "  [" + d.pluginFormatName + "]", id);
        if (d.createIdentifierString() == current)
            sel = id;
        ++id;
    }
    box.setSelectedId (sel, juce::dontSendNotification);
}

void SessionUI::bindPluginSlot (juce::ComboBox& box, juce::TextButton& byp, juce::TextButton& ed,
                                std::function<PluginRack&()> rackFn, int visualIndex)
{
    box.onChange = [this, &box, rackFn, visualIndex]
    {
        auto& rack = rackFn();
        const int slot = rack.orderAt (visualIndex);
        const int id = box.getSelectedId();
        if (id <= 1)
        {
            rack.unloadPlugin (slot);
            return;
        }
        const auto types = proc.getPluginHost().knownList.getTypes();
        const int idx = id - 2;
        if (idx < 0 || idx >= types.size())
            return;
        const auto err = rack.loadPlugin (slot, types.getReference (idx));
        if (err.isNotEmpty())
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "VST3", err);
    };
    byp.onClick = [rackFn, visualIndex, &byp]
    {
        auto& rack = rackFn();
        rack.setBypass (rack.orderAt (visualIndex), byp.getToggleState());
    };
    ed.onClick = [rackFn, visualIndex]
    {
        auto& rack = rackFn();
        rack.showEditor (rack.orderAt (visualIndex));
    };
}

void SessionUI::refreshPluginCombos()
{
    auto& gRack = proc.getGuitarRack();
    auto& tRack = selectedTrackRack();
    for (int i = 0; i < 4; ++i)
    {
        const int gid = gRack.orderAt (i);
        const int tid = tRack.orderAt (i);
        gSlotLbl[(size_t) i].setText (juce::String (i + 1) + "  " + PluginRack::slotName (gid),
                                      juce::dontSendNotification);
        tSlotLbl[(size_t) i].setText (juce::String (i + 1) + "  " + PluginRack::slotName (tid),
                                      juce::dontSendNotification);
        fillPluginCombo (gSlot[(size_t) i], gRack, gid);
        fillPluginCombo (tSlot[(size_t) i], tRack, tid);
        gByp[(size_t) i].setToggleState (gRack.isBypassed (gid), juce::dontSendNotification);
        tByp[(size_t) i].setToggleState (tRack.isBypassed (tid), juce::dontSendNotification);
    }
}

void SessionUI::refreshDawMixer()
{
    auto& tracks = proc.getDaw().getProject().tracks;
    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        auto* s = dawStrips[(size_t) i].get();
        s->title.setText (tracks[(size_t) i].name, juce::dontSendNotification);
        s->level.setValue ((double) tracks[(size_t) i].level.load(), juce::dontSendNotification);
        s->pan.setValue ((double) tracks[(size_t) i].pan.load(), juce::dontSendNotification);
        s->mute.setToggleState (tracks[(size_t) i].mute.load() != 0, juce::dontSendNotification);
        s->solo.setToggleState (tracks[(size_t) i].solo.load() != 0, juce::dontSendNotification);
        s->arm.setToggleState (tracks[(size_t) i].arm.load() != 0, juce::dontSendNotification);
        s->mon.setToggleState (tracks[(size_t) i].monitor.load() != 0, juce::dontSendNotification);
    }
}

void SessionUI::doNewProject()
{
    const auto name = juce::Time::getCurrentTime().formatted ("Jam-%Y%m%d-%H%M");
    auto& host = proc.getPluginHost();
    auto& p = proc.getDaw().getProject();
    p.resetNew (host, name);
    proc.getAnalyzer().clearTranscription();
    p.prepare (proc.getDaw().getSampleRate(), 4096);
    refreshDawMixer();
    projectLbl.setText (name, juce::dontSendNotification);
}

void SessionUI::doOpenProject()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Open Session project", SessionSettings::projectsDir(), "*", true);
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectDirectories,
                          [this, chooser] (const juce::FileChooser& c)
    {
        auto f = c.getResult();
        if (! f.isDirectory())
            return;
        const auto err = proc.getDaw().getProject().load (f, proc.getPluginHost(), &proc.getGuitarRack());
        if (err.isNotEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open", err);
            return;
        }
        proc.getDaw().getProject().prepare (proc.getDaw().getSampleRate(), 4096);
        proc.getAnalyzer().replaceHistory (proc.getDaw().getProject().notes);
        refreshDawMixer();
        projectLbl.setText (proc.getDaw().getProject().name, juce::dontSendNotification);
    });
}

void SessionUI::doSaveProject()
{
    proc.getDaw().getProject().notes = proc.getAnalyzer().copyHistory();
    const auto err = proc.getDaw().getProject().save (&proc.getGuitarRack());
    if (err.isNotEmpty())
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save", err);
    else
        recPath.setText ("Saved  " + proc.getDaw().getProject().folder.getFullPathName(),
                         juce::dontSendNotification);
}

void SessionUI::doBounce()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Bounce mixdown",
        proc.getDaw().getProject().folder.getChildFile (proc.getDaw().getProject().name + "-mix.wav"),
        "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, chooser] (const juce::FileChooser& c)
    {
        auto f = c.getResult();
        if (f == juce::File())
            return;
        if (! f.hasFileExtension ("wav"))
            f = f.withFileExtension ("wav");
        const auto err = proc.getDaw().bounceMixdown (f);
        if (err.isNotEmpty())
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Bounce", err);
        else
            recPath.setText ("Bounce  " + f.getFullPathName(), juce::dontSendNotification);
    });
}

void SessionUI::applyScaleMask()
{
    const auto sc = (FollowerBand::Scale) juce::jmax (0, scaleBox.getSelectedId() - 1);
    int mask = 0;
    for (int i = 0; i < 12; ++i)
        if (FollowerBand::scaleHas (sc, i))
            mask |= (1 << i);
    proc.getAnalyzer().setScaleIntervals (mask);
}

void SessionUI::drawEnergy (juce::Graphics& g, juce::Rectangle<int> r, float v, juce::Colour c, const juce::String& lab) const
{
    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    auto fill = r.toFloat().reduced (2.0f);
    fill.setWidth (fill.getWidth() * juce::jlimit (0.0f, 1.0f, v));
    g.setColour (c);
    g.fillRoundedRectangle (fill, 3.0f);
    g.setFont (uiFont (10.0f));
    g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
    g.drawText (lab, r.translated (0, -sx (14)), juce::Justification::centredLeft, false);
}

void SessionUI::drawNeck (juce::Graphics& g, juce::Rectangle<int> r) const
{
    if (r.getWidth() < 40 || r.getHeight() < 20)
        return;
    static const int openMidi[6] = { 64, 59, 55, 50, 45, 40 }; // high E to low E
    auto& band = proc.getBand();
    const int key = band.getSoundingKey();
    const int deg = band.getChordDegree();
    const int rootIv = (deg == FollowerBand::DegI ? 0 : deg == FollowerBand::DegIV ? 5
                      : deg == FollowerBand::DegV ? 7 : deg == FollowerBand::Degvi ? 9
                      : deg == FollowerBand::DegbIII ? 3 : deg == FollowerBand::DegbVI ? 8
                      : deg == FollowerBand::DegbVII ? 10 : 2);
    const auto sc = band.getScale();
    const bool minor = FollowerBand::scaleHas (sc, 3) && ! FollowerBand::scaleHas (sc, 4);
    const int third = minor ? 3 : 4;
    const int chordIv[4] = { rootIv, (rootIv + third) % 12, (rootIv + 7) % 12, (rootIv + 10) % 12 };

    g.setColour (juce::Colour (SessionLookAndFeel::kPanel2));
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    const int frets = 5;
    const float cellW = (float) r.getWidth() / (float) (frets + 1);
    const float cellH = (float) r.getHeight() / 6.0f;
    for (int s = 0; s < 6; ++s)
    {
        const float y = (float) r.getY() + (0.5f + (float) s) * cellH;
        g.setColour (juce::Colour (SessionLookAndFeel::kLine));
        g.drawLine ((float) r.getX() + 4.0f, y, (float) r.getRight() - 4.0f, y, 1.0f);
        for (int f = 0; f <= frets; ++f)
        {
            const int midi = openMidi[s] + f;
            const int iv = ((midi % 12) - key + 24) % 12;
            bool chord = false;
            for (int c : chordIv)
                if (c == iv) chord = true;
            const bool inScale = FollowerBand::scaleHas (sc, iv);
            const float x = (float) r.getX() + (0.5f + (float) f) * cellW;
            if (chord)
            {
                g.setColour (juce::Colour (SessionLookAndFeel::kAccent));
                g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            }
            else if (inScale)
            {
                g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
                g.drawEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f, 1.2f);
            }
        }
    }
}
