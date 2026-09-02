#pragma once

#include <JuceHeader.h>
#include "Daw/DawEngine.h"

/** Horizontal arrange: bars/beats, clip waveforms, playhead, drag-move, loop, zoom. */
class ArrangeView : public juce::Component
{
public:
    explicit ArrangeView (DawEngine& engine);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& key) override;

    std::function<void (int track)> onSelectTrack;

    void setBpm (float b) noexcept { bpm = juce::jlimit (40.0f, 240.0f, b); }
    int getSelectedTrack() const noexcept { return selectedTrack; }
    int getSelectedClip() const noexcept { return selectedClip; }

private:
    juce::Rectangle<int> laneRect (int track) const noexcept;
    int64_t xToSample (int x) const noexcept;
    int sampleToX (int64_t s) const noexcept;
    double samplesPerBeat() const noexcept;
    int hitTrack (juce::Point<int> p) const noexcept;
    Daw::Clip* hitClip (juce::Point<int> p, int& trackOut) noexcept;
    void drawWave (juce::Graphics& g, juce::Rectangle<float> r, const Daw::Clip& c, juce::Colour col) const;
    juce::Colour trackColour (int t) const noexcept;

    DawEngine& daw;
    float bpm = 112.0f;
    float pxPerBeat = 36.0f;
    int nameW = 92;
    int headerH = 24;
    int selectedTrack = 0;
    int selectedClip = -1;
    int dragTrack = -1;
    int dragClip = -1;
    int64_t dragOriginStart = 0;
    int dragMouseX0 = 0;
    bool draggingLoop = false;
    bool dragLoopEnd = false;
    int64_t loopGrab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangeView)
};
