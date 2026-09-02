#include "UI/ArrangeView.h"
#include "UI/SessionLookAndFeel.h"

ArrangeView::ArrangeView (DawEngine& engine)
    : daw (engine)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

double ArrangeView::samplesPerBeat() const noexcept
{
    const double sr = daw.getSampleRate() > 1.0 ? daw.getSampleRate() : 44100.0;
    return (60.0 / (double) juce::jmax (40.0f, bpm)) * sr;
}

int64_t ArrangeView::xToSample (int x) const noexcept
{
    const double beats = (double) juce::jmax (0, x - nameW) / (double) pxPerBeat;
    return (int64_t) std::llround (beats * samplesPerBeat());
}

int ArrangeView::sampleToX (int64_t s) const noexcept
{
    const double beats = (double) s / samplesPerBeat();
    return nameW + (int) std::lround (beats * (double) pxPerBeat);
}

juce::Rectangle<int> ArrangeView::laneRect (int track) const noexcept
{
    const int lanes = Daw::kMasterIndex; // no master lane
    const int h = juce::jmax (18, (getHeight() - headerH) / juce::jmax (1, lanes));
    return { 0, headerH + track * h, getWidth(), h };
}

int ArrangeView::hitTrack (juce::Point<int> p) const noexcept
{
    if (p.y < headerH)
        return -1;
    const int lanes = Daw::kMasterIndex;
    const int h = juce::jmax (18, (getHeight() - headerH) / juce::jmax (1, lanes));
    const int t = (p.y - headerH) / h;
    return (t >= 0 && t < lanes) ? t : -1;
}

Daw::Clip* ArrangeView::hitClip (juce::Point<int> p, int& trackOut) noexcept
{
    trackOut = hitTrack (p);
    if (trackOut < 0)
        return nullptr;
    const juce::ScopedLock sl (daw.getProject().lock);
    auto& clips = daw.getProject().tracks[(size_t) trackOut].clips;
    for (auto it = clips.rbegin(); it != clips.rend(); ++it)
    {
        if (*it == nullptr)
            continue;
        const int x0 = sampleToX ((*it)->startSamples);
        const int x1 = sampleToX ((*it)->startSamples + (*it)->lengthSamples);
        auto r = laneRect (trackOut).reduced (0, 3);
        r.setX (x0);
        r.setWidth (juce::jmax (4, x1 - x0));
        if (r.contains (p))
            return it->get();
    }
    return nullptr;
}

juce::Colour ArrangeView::trackColour (int t) const noexcept
{
    if (t == Daw::kGuitar) return juce::Colour (SessionLookAndFeel::kGuitar);
    if (t == Daw::kDrums)  return juce::Colour (SessionLookAndFeel::kDrums);
    if (t == Daw::kBass)   return juce::Colour (SessionLookAndFeel::kBass);
    if (t == Daw::kKeys)   return juce::Colour (SessionLookAndFeel::kKeys);
    return juce::Colour (SessionLookAndFeel::kAccent).withRotatedHue (0.08f * (float) t);
}

void ArrangeView::drawWave (juce::Graphics& g, juce::Rectangle<float> r, const Daw::Clip& c, juce::Colour col) const
{
    if (c.peaks.empty() || r.getWidth() < 2.0f)
        return;
    const float mid = r.getCentreY();
    const float amp = r.getHeight() * 0.42f;
    const int n = (int) c.peaks.size();
    juce::Path p;
    const int px = juce::jmax (1, (int) r.getWidth());
    for (int x = 0; x < px; ++x)
    {
        const int i = juce::jlimit (0, n - 1, (int) ((float) x / (float) px * (float) n));
        const float y = c.peaks[(size_t) i] * amp;
        const float xf = r.getX() + (float) x;
        if (x == 0)
            p.startNewSubPath (xf, mid - y);
        else
            p.lineTo (xf, mid - y);
    }
    for (int x = px - 1; x >= 0; --x)
    {
        const int i = juce::jlimit (0, n - 1, (int) ((float) x / (float) px * (float) n));
        p.lineTo (r.getX() + (float) x, mid + c.peaks[(size_t) i] * amp);
    }
    p.closeSubPath();
    g.setColour (col.withAlpha (0.85f));
    g.fillPath (p);
}

void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (SessionLookAndFeel::kBg));
    auto& proj = daw.getProject();
    bpm = proj.bpm;

    const int lanes = Daw::kMasterIndex;
    const double spb = samplesPerBeat();
    const int64_t viewEnd = juce::jmax (proj.endSamples(),
                                        (int64_t) (spb * 32.0),
                                        daw.getPosition() + (int64_t) (spb * 8.0));
    const int lastBeat = (int) std::ceil ((double) viewEnd / spb) + 4;

    g.setColour (juce::Colour (SessionLookAndFeel::kPanel));
    g.fillRect (0, 0, getWidth(), headerH);

    g.setFont (juce::FontOptions (11.0f));
    for (int b = 0; b <= lastBeat; ++b)
    {
        const int x = sampleToX ((int64_t) std::llround ((double) b * spb));
        if (x < nameW || x > getWidth())
            continue;
        const bool bar = (b % 4) == 0;
        g.setColour (juce::Colour (bar ? SessionLookAndFeel::kLine : 0xff1a2030));
        g.drawVerticalLine (x, (float) headerH, (float) getHeight());
        if (bar)
        {
            g.setColour (juce::Colour (SessionLookAndFeel::kMuted));
            g.drawText (juce::String (b / 4 + 1), x + 3, 2, 40, headerH - 4,
                        juce::Justification::centredLeft, false);
        }
    }

    // Loop region
    if (proj.loopEnd > proj.loopStart)
    {
        const int x0 = sampleToX (proj.loopStart);
        const int x1 = sampleToX (proj.loopEnd);
        g.setColour (juce::Colour (SessionLookAndFeel::kAccent).withAlpha (proj.cycle ? 0.16f : 0.07f));
        g.fillRect (x0, 0, juce::jmax (2, x1 - x0), getHeight());
        g.setColour (juce::Colour (SessionLookAndFeel::kAccent).withAlpha (0.8f));
        g.fillRect (x0, 0, 2, headerH);
        g.fillRect (x1 - 2, 0, 2, headerH);
    }

    const juce::CriticalSection::ScopedTryLockType sl (proj.lock);
    for (int t = 0; t < lanes; ++t)
    {
        auto lane = laneRect (t);
        const bool sel = (t == selectedTrack);
        g.setColour (juce::Colour (sel ? SessionLookAndFeel::kPanel2 : SessionLookAndFeel::kPanel)
                         .brighter (t % 2 == 0 ? 0.02f : 0.0f));
        g.fillRect (lane);
        g.setColour (juce::Colour (SessionLookAndFeel::kLine));
        g.drawHorizontalLine (lane.getBottom() - 1, 0.0f, (float) getWidth());

        auto name = lane.removeFromLeft (nameW);
        g.setColour (juce::Colour (SessionLookAndFeel::kBg).withAlpha (0.55f));
        g.fillRect (name);
        g.setColour (trackColour (t));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (proj.tracks[(size_t) t].name, name.reduced (6, 0),
                    juce::Justification::centredLeft, true);

        if (! sl.isLocked())
            continue;
        for (auto& cp : proj.tracks[(size_t) t].clips)
        {
            if (cp == nullptr)
                continue;
            const int x0 = sampleToX (cp->startSamples);
            const int x1 = sampleToX (cp->startSamples + cp->lengthSamples);
            auto cr = juce::Rectangle<int> (x0, laneRect (t).getY() + 3,
                                           juce::jmax (4, x1 - x0), laneRect (t).getHeight() - 6);
            const bool clipSel = (t == selectedTrack && cp->id == selectedClip);
            auto col = trackColour (t);
            g.setColour (col.withAlpha (clipSel ? 0.55f : 0.32f));
            g.fillRoundedRectangle (cr.toFloat(), 3.0f);
            g.setColour (col.withAlpha (clipSel ? 1.0f : 0.7f));
            g.drawRoundedRectangle (cr.toFloat(), 3.0f, clipSel ? 1.6f : 1.0f);
            if (cp->ready.load (std::memory_order_relaxed) != 0)
                drawWave (g, cr.toFloat().reduced (2.0f, 2.0f), *cp, col.brighter (0.15f));
            g.setColour (juce::Colour (SessionLookAndFeel::kText).withAlpha (0.85f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (cp->name, cr.reduced (4, 1), juce::Justification::centredLeft, true);
        }
    }

    const int px = sampleToX (daw.getPosition());
    g.setColour (juce::Colour (0xffffc857));
    g.drawVerticalLine (px, 0.0f, (float) getHeight());
}

void ArrangeView::resized() {}

void ArrangeView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (e.y < headerH && e.x >= nameW)
    {
        draggingLoop = e.mods.isAltDown() || e.mods.isShiftDown();
        if (draggingLoop)
        {
            auto& p = daw.getProject();
            const int64_t s = xToSample (e.x);
            const int64_t mid = (p.loopStart + p.loopEnd) / 2;
            dragLoopEnd = s >= mid;
            loopGrab = s;
        }
        else
        {
            daw.setPosition (xToSample (e.x));
        }
        return;
    }

    int tr = -1;
    if (auto* c = hitClip (e.getPosition(), tr))
    {
        selectedTrack = tr;
        selectedClip = c->id;
        dragTrack = tr;
        dragClip = c->id;
        dragOriginStart = c->startSamples;
        dragMouseX0 = e.x;
        daw.selectedTrack.store (tr);
        if (onSelectTrack)
            onSelectTrack (tr);
        return;
    }

    selectedClip = -1;
    const int t = hitTrack (e.getPosition());
    if (t >= 0)
    {
        selectedTrack = t;
        daw.selectedTrack.store (t);
        if (e.x >= nameW)
            daw.setPosition (xToSample (e.x));
        if (onSelectTrack)
            onSelectTrack (t);
    }
}

void ArrangeView::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingLoop)
    {
        auto& p = daw.getProject();
        const int64_t s = juce::jmax ((int64_t) 0, xToSample (e.x));
        if (dragLoopEnd)
            p.loopEnd = juce::jmax (p.loopStart + (int64_t) samplesPerBeat(), s);
        else
            p.loopStart = juce::jlimit ((int64_t) 0, p.loopEnd - (int64_t) samplesPerBeat(), s);
        return;
    }
    if (dragClip < 0 || dragTrack < 0)
        return;
    const int64_t delta = xToSample (e.x) - xToSample (dragMouseX0);
    const juce::ScopedLock sl (daw.getProject().lock);
    if (auto* c = daw.getProject().findClip (dragTrack, dragClip))
        c->startSamples = juce::jmax ((int64_t) 0, dragOriginStart + delta);
}

void ArrangeView::mouseUp (const juce::MouseEvent&)
{
    if (dragClip >= 0 && dragTrack >= 0)
    {
        const juce::ScopedLock sl (daw.getProject().lock);
        if (auto* c = daw.getProject().findClip (dragTrack, dragClip))
        {
            if (c->startSamples != dragOriginStart)
                daw.getProject().pushMoveUndo (dragTrack, dragClip, dragOriginStart, c->startSamples);
        }
    }
    dragClip = -1;
    dragTrack = -1;
    draggingLoop = false;
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    pxPerBeat = juce::jlimit (8.0f, 120.0f, pxPerBeat * (w.deltaY > 0 ? 1.12f : 0.89f));
    repaint();
}

bool ArrangeView::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        && selectedClip >= 0 && selectedTrack >= 0)
    {
        auto& p = daw.getProject();
        const juce::ScopedLock sl (p.lock);
        auto& clips = p.tracks[(size_t) selectedTrack].clips;
        for (auto it = clips.begin(); it != clips.end(); ++it)
        {
            if (*it && (*it)->id == selectedClip)
            {
                p.pushDeleteUndo (selectedTrack, std::move (*it));
                clips.erase (it);
                selectedClip = -1;
                return true;
            }
        }
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        daw.getProject().undoLast();
        return true;
    }
    return false;
}
