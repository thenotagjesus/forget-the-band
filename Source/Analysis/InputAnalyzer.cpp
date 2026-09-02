#include "Analysis/InputAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    constexpr float kYinThreshold = 0.15f;
    constexpr float kMinHz = 70.0f;
    constexpr float kMaxHz = 1200.0f;
}

const char* InputAnalyzer::pitchClassName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    pc = ((pc % 12) + 12) % 12;
    return names[pc];
}

juce::String InputAnalyzer::noteNameFromMidi (int note)
{
    if (note < 0)
        return "--";
    static const char* spell[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (spell[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
}

float InputAnalyzer::getCents() const noexcept
{
    const float hz = freqHz.load (std::memory_order_relaxed);
    if (hz < 20.0f)
        return 0.0f;
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    return (midi - std::round (midi)) * 100.0f;
}

InputAnalyzer::InputAnalyzer()
    : juce::Thread ("SessionAnalyze")
{
}

InputAnalyzer::~InputAnalyzer()
{
    release();
}

void InputAnalyzer::prepare (double sr)
{
    release();
    sampleRate = sr > 1.0 ? sr : 44100.0;
    windowSize = 2048;
    hop = juce::jmax (256, (int) std::lround (sampleRate * 0.012)); // ~12 ms

    const int ringSize = juce::jmax (windowSize * 4, (int) std::lround (sampleRate * 2.0));
    fifo.setTotalSize (ringSize);
    ring.assign ((size_t) ringSize, 0.0f);
    window.assign ((size_t) windowSize, 0.0f);
    yinDiff.assign ((size_t) windowSize, 0.0f);
    yinCmnd.assign ((size_t) windowSize, 0.0f);
    gather.assign ((size_t) windowSize, 0.0f);
    chroma.fill (0.0f);
    ioiSec.fill (0.0f);
    ioiCount = ioiWrite = 0;
    lastEnergy = fluxAvg = 0.0f;
    samplesSinceOnset = 1.0e9;
    rmsSmooth = activitySmooth = 0.0f;
    intensitySmooth = 0.35f;
    hopsElapsed = 0.0;
    hopCounter = 0;
    pendingKey = keyPc.load();
    keyStableHops = bpmStableHops = 0;
    hopsPerBar = juce::jmax (8, (int) std::lround ((60.0 / (double) bpmSmoothed) * 4.0 * sampleRate / (double) hop));

    midiRing.assign (512, MidiPulse{});
    midiFifo.setTotalSize (512);
    midiFifo.reset();
    heldNote = -1;
    hangHops = 0;
    stableNoteHops = 0;
    pendingMidi = -1;
    liveHeld = -1;
    liveWrite.store (0);
    liveCount.store (0);
    for (auto& n : liveNotes) n = {};
    for (auto& c : chromaAtom) c.store (0.0f);
    onsetFlag.store (0);
    playerChordDeg.store (0);
    playerChordRoot.store (keyPc.load());
    {
        const juce::ScopedLock sl (historyLock);
        history.clear();
    }

    startThread (juce::Thread::Priority::low);
}

void InputAnalyzer::release()
{
    signalThreadShouldExit();
    stopThread (2000);
}

void InputAnalyzer::setManualKey (int pc) noexcept
{
    pc = ((pc % 12) + 12) % 12;
    autoKey.store (0, std::memory_order_relaxed);
    keyLocked.store (1, std::memory_order_relaxed);
    keyPc.store (pc, std::memory_order_relaxed);
    pendingKey = pc;
    keyStableHops = 0;
}

void InputAnalyzer::setManualBpm (float v) noexcept
{
    v = juce::jlimit (60.0f, 180.0f, v);
    autoBpm.store (0, std::memory_order_relaxed);
    bpmLocked.store (1, std::memory_order_relaxed);
    bpm.store (v, std::memory_order_relaxed);
    bpmSmoothed = v;
    bpmStableHops = 0;
}

void InputAnalyzer::unlockKey() noexcept
{
    autoKey.store (1, std::memory_order_relaxed);
    keyLocked.store (0, std::memory_order_relaxed);
    keyStableHops = 0;
}

void InputAnalyzer::unlockBpm() noexcept
{
    autoBpm.store (1, std::memory_order_relaxed);
    bpmLocked.store (0, std::memory_order_relaxed);
    lockTempo.store (0, std::memory_order_relaxed);
    bpmStableHops = 0;
}

void InputAnalyzer::setKeySeed (int pc) noexcept
{
    pc = ((pc % 12) + 12) % 12;
    keyPc.store (pc, std::memory_order_relaxed);
    pendingKey = pc;
    keyStableHops = 0;
}

void InputAnalyzer::setBpmSeed (float v) noexcept
{
    v = juce::jlimit (60.0f, 180.0f, v);
    bpm.store (v, std::memory_order_relaxed);
    bpmSmoothed = v;
}

void InputAnalyzer::nudgeBpm (float delta) noexcept
{
    setBpmSeed (bpm.load (std::memory_order_relaxed) + delta);
}

void InputAnalyzer::captureCal (int stage) noexcept
{
    const float r = juce::jmax (0.004f, rmsSmooth);
    if (stage <= 0) calSoft.store (r, std::memory_order_relaxed);
    else if (stage == 1) calMid.store (r, std::memory_order_relaxed);
    else calHard.store (r, std::memory_order_relaxed);
    if (calSoft.load() > 0.0f && calMid.load() > calSoft.load() && calHard.load() > calMid.load())
        calibrated.store (1, std::memory_order_relaxed);
}

void InputAnalyzer::pushSamples (const float* data, int numSamples) noexcept
{
    if (data == nullptr || numSamples <= 0)
        return;
    if (fifo.getFreeSpace() < numSamples)
        return; // drop rather than block the audio thread

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
    if (n1 > 0)
        std::memcpy (ring.data() + s1, data, (size_t) n1 * sizeof (float));
    if (n2 > 0)
        std::memcpy (ring.data() + s2, data + n1, (size_t) n2 * sizeof (float));
    fifo.finishedWrite (n1 + n2);
}

void InputAnalyzer::run()
{
    int gatherPos = 0;

    while (! threadShouldExit())
    {
        const int ready = fifo.getNumReady();
        if (ready <= 0)
        {
            wait (4);
            continue;
        }

        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToRead (ready, s1, n1, s2, n2);

        auto take = [this, &gatherPos] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                gather[(size_t) gatherPos] = ring[(size_t) (start + i)];
                gatherPos = (gatherPos + 1) % windowSize;

                if (++hopCounter >= hop)
                {
                    hopCounter = 0;
                    // Latest windowSize samples ending at gatherPos.
                    for (int k = 0; k < windowSize; ++k)
                    {
                        const int idx = (gatherPos + k) % windowSize;
                        window[(size_t) k] = gather[(size_t) idx];
                    }
                    analyseWindow (window.data(), windowSize);
                }
            }
        };

        take (s1, n1);
        take (s2, n2);
        fifo.finishedRead (n1 + n2);
    }
}

float InputAnalyzer::detectYin (const float* x, int n, float& conf) noexcept
{
    const int tauMin = juce::jmax (2, (int) std::floor (sampleRate / kMaxHz));
    const int tauMax = juce::jmin (n / 2 - 2, (int) std::ceil (sampleRate / kMinHz));
    if (tauMax <= tauMin + 4)
    {
        conf = 0.0f;
        return 0.0f;
    }

    yinDiff[0] = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        const int last = n - tau;
        for (int j = 0; j < last; ++j)
        {
            const float d = x[j] - x[j + tau];
            sum += d * d;
        }
        yinDiff[(size_t) tau] = sum;
    }

    yinCmnd[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += yinDiff[(size_t) tau];
        yinCmnd[(size_t) tau] = running > 1.0e-12f ? yinDiff[(size_t) tau] * (float) tau / running : 1.0f;
    }

    int tauEst = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (yinCmnd[(size_t) tau] < kYinThreshold)
        {
            while (tau + 1 < tauMax && yinCmnd[(size_t) (tau + 1)] < yinCmnd[(size_t) tau])
                ++tau;
            tauEst = tau;
            break;
        }
    }

    if (tauEst < 0)
    {
        float best = 1.0f;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (yinCmnd[(size_t) tau] < best)
            {
                best = yinCmnd[(size_t) tau];
                tauEst = tau;
            }
        }
        if (best > 0.45f)
        {
            conf = 0.0f;
            return 0.0f;
        }
    }

    const int t = tauEst;
    const float s0 = yinCmnd[(size_t) (t - 1)];
    const float s1 = yinCmnd[(size_t) t];
    const float s2 = yinCmnd[(size_t) (t + 1)];
    const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
    float better = (float) t;
    if (std::abs (denom) > 1.0e-8f)
        better += (s0 - s2) / denom;

    conf = juce::jlimit (0.0f, 1.0f, 1.0f - s1);
    return (float) sampleRate / juce::jmax (2.0f, better);
}

void InputAnalyzer::updateKey (float hz, float conf, float rms) noexcept
{
    for (auto& c : chroma)
        c *= 0.992f;

    if (conf > 0.45f && hz >= kMinHz && rms > 0.01f)
    {
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
        chroma[(size_t) pc] += conf * (0.4f + 2.0f * rms);
        const int asRootOfFifth = (pc + 5) % 12;
        chroma[(size_t) asRootOfFifth] += 0.25f * conf * rms;
    }
    for (int i = 0; i < 12; ++i)
        chromaAtom[(size_t) i].store (chroma[(size_t) i], std::memory_order_relaxed);

    int best = 0;
    float bestV = chroma[0];
    for (int i = 1; i < 12; ++i)
    {
        if (chroma[(size_t) i] > bestV)
        {
            bestV = chroma[(size_t) i];
            best = i;
        }
    }

    if (autoKey.load (std::memory_order_relaxed) == 0)
        return;

    if (best == pendingKey)
        ++keyStableHops;
    else
    {
        pendingKey = best;
        keyStableHops = 0;
    }

    if (keyLocked.load (std::memory_order_relaxed) == 0)
        keyPc.store (pendingKey, std::memory_order_relaxed);
}

void InputAnalyzer::updateTempo (float rms, int n) noexcept
{
    juce::ignoreUnused (rms);

    float energy = 0.0f;
    // Use the tail of the window as a short flux frame.
    const int fluxN = juce::jmin (n, hop);
    const float* x = window.data() + (n - fluxN);
    for (int i = 0; i < fluxN; ++i)
        energy += x[i] * x[i];
    energy = std::sqrt (energy / (float) juce::jmax (1, fluxN));

    const float flux = juce::jmax (0.0f, energy - lastEnergy);
    lastEnergy = energy;
    fluxAvg = fluxAvg * 0.92f + flux * 0.08f;

    samplesSinceOnset += (double) hop;

    const float thresh = juce::jmax (0.008f, fluxAvg * 2.4f);
    const double minGap = sampleRate * 0.18; // ~330 BPM cap as 8ths; we'll fold later
    const bool onset = flux > thresh && energy > 0.02f && samplesSinceOnset > minGap;

    if (onset)
    {
        onsetFlag.store (1, std::memory_order_relaxed);
        const float ioi = (float) (samplesSinceOnset / sampleRate);
        samplesSinceOnset = 0.0;
        if (ioi >= 0.22f && ioi <= 1.05f) // ~57–273 quarter-note BPM before folding
        {
            ioiSec[(size_t) ioiWrite] = ioi;
            ioiWrite = (ioiWrite + 1) % (int) ioiSec.size();
            ioiCount = juce::jmin ((int) ioiSec.size(), ioiCount + 1);
        }
    }

    if (autoBpm.load (std::memory_order_relaxed) == 0
        || lockTempo.load (std::memory_order_relaxed) != 0)
        return;

    if (ioiCount >= 4)
    {
        std::array<float, 12> tmp = ioiSec;
        const int used = ioiCount;
        std::sort (tmp.begin(), tmp.begin() + used);
        const float median = tmp[(size_t) (used / 2)];
        float est = 60.0f / juce::jmax (0.2f, median);
        while (est > 180.0f) est *= 0.5f;
        while (est < 70.0f)  est *= 2.0f;
        est = juce::jlimit (70.0f, 180.0f, est);
        bpmSmoothed = bpmSmoothed * 0.85f + est * 0.15f;

        if (std::abs (est - bpmSmoothed) < 4.0f)
            ++bpmStableHops;
        else
            bpmStableHops = 0;

        if (bpmLocked.load (std::memory_order_relaxed) == 0)
            bpm.store (bpmSmoothed, std::memory_order_relaxed);
    }
}

void InputAnalyzer::maybeLock() noexcept
{
    hopsPerBar = juce::jmax (8, (int) std::lround ((60.0 / (double) juce::jmax (40.0f, bpmSmoothed))
                                                   * 4.0 * sampleRate / (double) hop));
    const int barsNeeded = 4 * hopsPerBar;

    if (autoKey.load() != 0 && keyLocked.load() == 0 && keyStableHops >= barsNeeded)
        keyLocked.store (1, std::memory_order_relaxed);

    if (autoBpm.load() != 0 && bpmLocked.load() == 0 && bpmStableHops >= barsNeeded / 2)
        bpmLocked.store (1, std::memory_order_relaxed);
}

void InputAnalyzer::analyseWindow (const float* x, int n) noexcept
{
    float sum2 = 0.0f;
    for (int i = 0; i < n; ++i)
        sum2 += x[i] * x[i];
    const float rms = std::sqrt (sum2 / (float) juce::jmax (1, n));
    rmsSmooth = rmsSmooth * 0.85f + rms * 0.15f;
    rmsAtom.store (rmsSmooth, std::memory_order_relaxed);

    float conf = 0.0f;
    float hz = 0.0f;
    if (rms > 0.008f)
        hz = detectYin (x, n, conf);

    if (conf > 0.4f && hz > 0.0f)
    {
        freqHz.store (hz, std::memory_order_relaxed);
        confidence.store (conf, std::memory_order_relaxed);
        const int midi = (int) std::lround (69.0f + 12.0f * std::log2 (hz / 440.0f));
        midiNote.store (juce::jlimit (0, 127, midi), std::memory_order_relaxed);
    }
    else
    {
        confidence.store (confidence.load() * 0.9f, std::memory_order_relaxed);
        if (rmsSmooth < 0.006f)
        {
            freqHz.store (0.0f, std::memory_order_relaxed);
            midiNote.store (-1, std::memory_order_relaxed);
        }
    }

    emitNote (hz, conf, rmsSmooth);
    updateKey (hz, conf, rmsSmooth);
    updateTempo (rmsSmooth, n);
    updatePlayerChord();

    const float onsetBusy = juce::jlimit (0.0f, 1.0f, (float) ioiCount / 8.0f);
    const float loud = juce::jlimit (0.0f, 1.0f, rmsSmooth * 8.0f);
    const float act = 0.55f * loud + 0.45f * onsetBusy;
    activitySmooth = activitySmooth * 0.80f + act * 0.20f;
    activity.store (activitySmooth, std::memory_order_relaxed);

    float player = activitySmooth;
    if (calibrated.load (std::memory_order_relaxed) != 0)
    {
        const float a = calSoft.load(), b = calMid.load(), c = calHard.load();
        if (rmsSmooth <= a) player = 0.12f * rmsSmooth / juce::jmax (0.004f, a);
        else if (rmsSmooth <= b) player = 0.12f + 0.38f * (rmsSmooth - a) / juce::jmax (0.004f, b - a);
        else if (rmsSmooth <= c) player = 0.50f + 0.50f * (rmsSmooth - b) / juce::jmax (0.004f, c - b);
        else player = 1.0f;
        player = juce::jlimit (0.0f, 1.0f, 0.7f * player + 0.3f * onsetBusy);
    }
    playerEnergy.store (player, std::memory_order_relaxed);
    // Require a pitched note, not broadband hiss (S/PDIF open / USB hash).
    if (rmsSmooth > 0.018f && conf > 0.45f && hz >= kMinHz && hz <= kMaxHz)
        engaged.store (1, std::memory_order_relaxed);

    if (lockIntensity.load (std::memory_order_relaxed) == 0)
    {
        if (player > intensitySmooth)
            intensitySmooth += (player - intensitySmooth) * 0.06f;
        else
            intensitySmooth += (player - intensitySmooth) * 0.018f;
        intensity.store (intensitySmooth, std::memory_order_relaxed);
    }

    const int note = midiNote.load (std::memory_order_relaxed);
    if (note >= 0)
    {
        const int iv = ((note % 12) - keyPc.load() + 24) % 12;
        const int mask = scaleMask.load (std::memory_order_relaxed);
        const float hit = (mask & (1 << iv)) ? 1.0f : 0.0f;
        fitAtom.store (fitAtom.load() * 0.85f + hit * 0.15f, std::memory_order_relaxed);
    }

    if (energyDrift.load() != 0 && lockTempo.load() == 0)
    {
        const float base = bpmSmoothed;
        bpm.store (juce::jlimit (60.0f, 180.0f, base * (1.0f + 0.012f * (intensitySmooth - 0.5f))),
                   std::memory_order_relaxed);
    }

    hopsElapsed += 1.0;
    maybeLock();
}

void InputAnalyzer::resetEngage() noexcept
{
    engaged.store (0, std::memory_order_relaxed);
}

void InputAnalyzer::copyChroma (float out[12]) const noexcept
{
    for (int i = 0; i < 12; ++i)
        out[i] = chromaAtom[(size_t) i].load (std::memory_order_relaxed);
}

juce::String InputAnalyzer::getPlayerChordName() const
{
    static const char* pcN[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* degN[] = { "", "m", "", "m", "m", "", "", "m" };
    const int root = playerChordRoot.load (std::memory_order_relaxed);
    const int deg = playerChordDeg.load (std::memory_order_relaxed);
    juce::String s (pcN[((root % 12) + 12) % 12]);
    // minor-ish degrees: vi, ii, bIII
    if (deg == 3 || deg == 7 || deg == 4)
        s << "m";
    return s;
}

double InputAnalyzer::currentQuarter() const noexcept
{
    const double tq = transportQ.load (std::memory_order_relaxed);
    if (tq > 0.0)
        return tq;
    const double sec = hopsElapsed * (double) hop / juce::jmax (1.0, sampleRate);
    const float b = juce::jmax (40.0f, bpmSmoothed);
    return sec * ((double) b / 60.0);
}

void InputAnalyzer::pushMidi (juce::uint8 status, juce::uint8 note, juce::uint8 vel) noexcept
{
    if (midiFifo.getFreeSpace() < 1)
        return;
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    midiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0)
    {
        midiRing[(size_t) s1].status = status;
        midiRing[(size_t) s1].note = note;
        midiRing[(size_t) s1].vel = vel;
        midiFifo.finishedWrite (1);
    }
}

void InputAnalyzer::drainMidi (juce::MidiBuffer& dest, int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);
    const int ready = midiFifo.getNumReady();
    if (ready <= 0)
        return;
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    midiFifo.prepareToRead (ready, s1, n1, s2, n2);
    auto take = [&] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& e = midiRing[(size_t) (start + i)];
            if (e.status == 0x90)
                dest.addEvent (juce::MidiMessage::noteOn (1, (int) e.note, e.vel), 0);
            else if (e.status == 0x80)
                dest.addEvent (juce::MidiMessage::noteOff (1, (int) e.note), 0);
        }
    };
    take (s1, n1);
    take (s2, n2);
    midiFifo.finishedRead (n1 + n2);
}

void InputAnalyzer::closeHeldNote (double endQ) noexcept
{
    if (heldNote < 0)
        return;
    openEvent.durationQuarter = juce::jmax (0.06, endQ - openEvent.startQuarter);
    {
        const juce::ScopedLock sl (historyLock);
        history.push_back (openEvent);
        if (history.size() > 4096)
            history.erase (history.begin(), history.begin() + (int) (history.size() - 4096));
    }
    if (liveHeld >= 0 && liveHeld < kLiveNotes)
    {
        liveNotes[(size_t) liveHeld].durBeat = (float) (openEvent.durationQuarter);
        liveNotes[(size_t) liveHeld].active = 0;
    }
    liveHeld = -1;
    heldNote = -1;
}

void InputAnalyzer::emitNote (float hz, float conf, float rms) noexcept
{
    // Port of PitchDetector::emitMidi onto the analysis worker (not the audio thread).
    const float gate = 0.42f;
    const bool voiced = (conf > gate && rms > 0.01f && hz >= 70.0f && hz <= 1200.0f);
    const int note = voiced ? juce::jlimit (0, 127, (int) std::lround (69.0f + 12.0f * std::log2 (hz / 440.0f)))
                            : -1;
    const double q = currentQuarter();

    if (note >= 0)
    {
        if (note == pendingMidi)
            ++stableNoteHops;
        else
        {
            pendingMidi = note;
            stableNoteHops = 0;
        }

        const bool stable = (note == heldNote) || (stableNoteHops >= 2);
        hangHops = juce::jmax (4, (int) std::lround (0.12 * sampleRate / (double) juce::jmax (1, hop)));

        if (stable && note != heldNote)
        {
            if (heldNote >= 0)
            {
                pushMidi (0x80, (juce::uint8) heldNote, 0);
                closeHeldNote (q);
            }
            const int vel = juce::jlimit (1, 127, (int) (rms * conf * 180.0f));
            pushMidi (0x90, (juce::uint8) note, (juce::uint8) vel);
            heldNote = note;
            openEvent = {};
            openEvent.startQuarter = q;
            openEvent.midi = note;
            openEvent.cents = getCents();
            openEvent.rms = rms;
            openEvent.velocity = (float) vel / 127.0f;
            openEvent.confidence = conf;
            openEvent.setNameFromMidi (note);

            const int idx = liveWrite.load (std::memory_order_relaxed) % kLiveNotes;
            liveNotes[(size_t) idx].startBeat = (float) q;
            liveNotes[(size_t) idx].durBeat = 0.25f;
            liveNotes[(size_t) idx].midi = note;
            liveNotes[(size_t) idx].cents = openEvent.cents;
            liveNotes[(size_t) idx].vel = openEvent.velocity;
            liveNotes[(size_t) idx].active = 1;
            liveHeld = idx;
            liveWrite.store (idx + 1, std::memory_order_relaxed);
            liveCount.store (juce::jmin (kLiveNotes, liveCount.load() + 1), std::memory_order_relaxed);
        }
        else if (note == heldNote && liveHeld >= 0)
        {
            liveNotes[(size_t) liveHeld].durBeat = (float) juce::jmax (0.06, q - openEvent.startQuarter);
            liveNotes[(size_t) liveHeld].cents = getCents();
        }
    }
    else if (heldNote >= 0)
    {
        --hangHops;
        if (hangHops <= 0 || rmsSmooth < 0.005f)
        {
            pushMidi (0x80, (juce::uint8) heldNote, 0);
            closeHeldNote (q);
            pendingMidi = -1;
            stableNoteHops = 0;
        }
    }
}

void InputAnalyzer::updatePlayerChord() noexcept
{
    // Score chroma against diatonic / modal triads relative to the sounding key.
    const int key = keyPc.load (std::memory_order_relaxed);
    struct Deg { int deg; int st; bool minor; };
    const Deg table[] = {
        { 0, 0,  false }, // I
        { 1, 5,  false }, // IV
        { 2, 7,  false }, // V
        { 3, 9,  true  }, // vi
        { 4, 3,  true  }, // bIII
        { 5, 8,  false }, // bVI
        { 6, 10, false }, // bVII
        { 7, 2,  true  }  // ii
    };
    int best = 0;
    float bestV = -1.0f;
    for (const auto& d : table)
    {
        const int root = (key + d.st) % 12;
        const int third = (root + (d.minor ? 3 : 4)) % 12;
        const int fifth = (root + 7) % 12;
        const int seventh = (root + 10) % 12;
        const float v = chroma[(size_t) root] * 1.45f
                      + chroma[(size_t) third] * 1.05f
                      + chroma[(size_t) fifth] * 0.85f
                      + chroma[(size_t) seventh] * 0.35f;
        if (v > bestV)
        {
            bestV = v;
            best = d.deg;
        }
    }

    if (best == pendingChord)
        ++chordStableHops;
    else
    {
        pendingChord = best;
        chordStableHops = 0;
    }

    // Hysteresis: don't panic-modulate. Hold until a few hops agree, and require energy.
    const bool enough = bestV > 0.08f && rmsSmooth > 0.01f;
    if (enough && chordStableHops >= 6)
    {
        playerChordDeg.store (best, std::memory_order_relaxed);
        const int st = table[best].st;
        playerChordRoot.store ((key + st) % 12, std::memory_order_relaxed);
    }
}

void InputAnalyzer::copyLiveNotes (Daw::LiveNote* dest, int maxN, int& written) const noexcept
{
    written = 0;
    if (dest == nullptr || maxN <= 0)
        return;
    const int n = juce::jmin (maxN, kLiveNotes);
    const int w = liveWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const int idx = ((w - 1 - i) % kLiveNotes + kLiveNotes) % kLiveNotes;
        dest[i] = liveNotes[(size_t) idx];
        ++written;
    }
}

std::vector<Daw::NoteEvent> InputAnalyzer::copyHistory() const
{
    const juce::ScopedLock sl (historyLock);
    return history;
}

void InputAnalyzer::replaceHistory (std::vector<Daw::NoteEvent> notes)
{
    const juce::ScopedLock sl (historyLock);
    history = std::move (notes);
}

void InputAnalyzer::clearTranscription()
{
    const juce::ScopedLock sl (historyLock);
    history.clear();
    heldNote = -1;
    liveHeld = -1;
    liveWrite.store (0);
    liveCount.store (0);
    for (auto& n : liveNotes) n = {};
}

juce::String InputAnalyzer::exportMidi (const juce::File& dest) const
{
    auto notes = copyHistory();
    juce::MidiMessageSequence seq;
    const float b = juce::jmax (40.0f, bpm.load (std::memory_order_relaxed));
    const double ticks = 960.0;
    for (const auto& n : notes)
    {
        if (n.midi < 0)
            continue;
        const double t0 = n.startQuarter * ticks;
        const double t1 = (n.startQuarter + juce::jmax (0.06, n.durationQuarter)) * ticks;
        const int vel = juce::jlimit (1, 127, (int) std::lround (n.velocity * 127.0f));
        seq.addEvent (juce::MidiMessage::noteOn (1, n.midi, (juce::uint8) vel), t0);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.midi), t1);
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    mf.addTrack (seq);
    dest.getParentDirectory().createDirectory();
    auto out = dest.createOutputStream();
    if (out == nullptr)
        return "Could not create MIDI file";
    if (! mf.writeTo (*out, 1))
        return "Failed to write MIDI";
    juce::ignoreUnused (b);
    return {};
}
