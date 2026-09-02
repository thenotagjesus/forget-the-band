#include "SessionProcessor.h"
#include <cstring>
#include <cmath>

const char* SessionProcessor::busName (int bus)
{
    static const char* n[] = { "Guitar", "Drums", "Bass", "Keys", "Master" };
    if (bus < 0 || bus >= NumBuses) return "?";
    return n[bus];
}

SessionProcessor::SessionProcessor()
{
    for (auto& l : level) l.store (0.85f, std::memory_order_relaxed);
    level[(size_t) Drums].store  (0.70f);
    level[(size_t) Bass].store   (0.78f);
    level[(size_t) Keys].store   (0.52f);
    level[(size_t) Master].store (0.90f);
    for (auto& m : mute) m.store (0);
    for (auto& s : solo) s.store (0);
    for (auto& p : busPeak) p.store (0.0f);
}

void SessionProcessor::prepare (double sr, int samplesPerBlock, int)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    maxBlock = juce::jmax (samplesPerBlock, 4096);

    amp.prepare (sampleRate, maxBlock);
    fx.prepare (sampleRate, maxBlock);
    band.prepare (sampleRate);
    arrangement.prepare (sampleRate);
    analyzer.prepare (sampleRate);
    writer.prepare (sampleRate);
    guitarRack.prepare (sampleRate, maxBlock);
    daw.prepare (sampleRate, maxBlock);
    guitarRack.restoreSlotState();

    inputGainSmooth.reset (sampleRate, 0.008);
    inputGainSmooth.setCurrentAndTargetValue (1.0f);
    gateGainSmooth.reset (sampleRate, 0.005);
    gateGainSmooth.setCurrentAndTargetValue (1.0f);
    gateAtk = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.002)); // 2 ms
    gateRel = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.100)); // 100 ms
    gateOpen  = juce::Decibels::decibelsToGain (-40.0f);
    gateClose = juce::Decibels::decibelsToGain (-46.0f);

    auto alloc = [this] (std::vector<float>& v)
    {
        v.assign ((size_t) maxBlock, 0.0f);
    };
    alloc (inMono); alloc (gL); alloc (gR);
    alloc (dL); alloc (dR); alloc (bL); alloc (bR);
    alloc (kL); alloc (kR); alloc (mL); alloc (mR);

    gateEnv = 0.0f;
    gateOpenState = false;
    countInActive = false;
    clickEnv = 0.0f;
}

void SessionProcessor::reset()
{
    amp.reset();
    fx.reset();
    band.reset();
    gateEnv = 0.0f;
    gateOpenState = false;
    gateGainSmooth.setCurrentAndTargetValue (1.0f);
    clickEnv = 0.0f;
}

void SessionProcessor::release()
{
    writer.stop();
    daw.release();
    guitarRack.release();
    analyzer.release();
    sessionRunning.store (0);
    countingIn.store (0);
}

void SessionProcessor::setBusLevel (int bus, float v) noexcept
{
    if (bus >= 0 && bus < NumBuses)
        level[(size_t) bus].store (juce::jlimit (0.0f, 1.5f, v), std::memory_order_relaxed);
}

void SessionProcessor::setBusMute (int bus, bool m) noexcept
{
    if (bus >= 0 && bus < NumBuses)
        mute[(size_t) bus].store (m ? 1 : 0, std::memory_order_relaxed);
}

void SessionProcessor::setBusSolo (int bus, bool s) noexcept
{
    if (bus >= 0 && bus < NumBuses)
        solo[(size_t) bus].store (s ? 1 : 0, std::memory_order_relaxed);
}

float SessionProcessor::getBusLevel (int bus) const noexcept
{
    if (bus >= 0 && bus < NumBuses)
        return level[(size_t) bus].load (std::memory_order_relaxed);
    return 0.0f;
}

bool SessionProcessor::getBusMute (int bus) const noexcept
{
    if (bus >= 0 && bus < NumBuses)
        return mute[(size_t) bus].load (std::memory_order_relaxed) != 0;
    return false;
}

bool SessionProcessor::getBusSolo (int bus) const noexcept
{
    if (bus >= 0 && bus < NumBuses)
        return solo[(size_t) bus].load (std::memory_order_relaxed) != 0;
    return false;
}

float SessionProcessor::getBusPeak (int bus) const noexcept
{
    if (bus >= 0 && bus < NumBuses)
        return busPeak[(size_t) bus].load (std::memory_order_relaxed);
    return 0.0f;
}

void SessionProcessor::setInputGainDb (float db) noexcept
{
    inputGainDb.store (juce::jlimit (0.0f, 24.0f, db), std::memory_order_relaxed);
}

void SessionProcessor::setGate (float v) noexcept
{
    gateAmt.store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed);
}

float SessionProcessor::busGain (int bus) const noexcept
{
    if (bus < 0 || bus >= NumBuses)
        return 0.0f;
    if (mute[(size_t) bus].load (std::memory_order_relaxed) != 0)
        return 0.0f;

    if (bus != Master)
    {
        bool anySolo = false;
        for (int i = 0; i < Master; ++i)
            if (solo[(size_t) i].load (std::memory_order_relaxed) != 0)
                anySolo = true;
        if (anySolo && solo[(size_t) bus].load (std::memory_order_relaxed) == 0)
            return 0.0f;
    }

    return level[(size_t) bus].load (std::memory_order_relaxed);
}

void SessionProcessor::setBandRoster (bool drums, bool bass, bool keys) noexcept
{
    band.setMemberEnabled (FollowerBand::MemberDrums, drums);
    band.setMemberEnabled (FollowerBand::MemberBass,  bass);
    band.setMemberEnabled (FollowerBand::MemberKeys,  keys);
    setBusMute (Drums, ! drums);
    setBusMute (Bass,  ! bass);
    setBusMute (Keys,  ! keys);
    auto& tracks = daw.getProject().tracks;
    tracks[(size_t) Daw::kDrums].mute.store (drums ? 0 : 1, std::memory_order_relaxed);
    tracks[(size_t) Daw::kBass].mute.store  (bass  ? 0 : 1, std::memory_order_relaxed);
    tracks[(size_t) Daw::kKeys].mute.store  (keys  ? 0 : 1, std::memory_order_relaxed);
}

void SessionProcessor::applyJamSetup (const SessionSettings::Setup& s) noexcept
{
    band.setStyle ((FollowerBand::Style) juce::jlimit (0, (int) FollowerBand::Style::NumStyles - 1, s.style));
    band.setDrumKit ((FollowerBand::DrumKit) juce::jlimit (0, (int) FollowerBand::DrumKit::NumKits - 1, s.drumsKit));
    band.setBassVoice ((FollowerBand::BassVoice) juce::jlimit (0, (int) FollowerBand::BassVoice::NumVoices - 1, s.bassVoice));
    band.setKeysVoice ((FollowerBand::KeysVoice) juce::jlimit (0, (int) FollowerBand::KeysVoice::NumVoices - 1, s.keysVoice));
    band.setForm  ((FollowerBand::Form)  juce::jlimit (0, (int) FollowerBand::Form::NumForms - 1, s.form));
    band.setScale ((FollowerBand::Scale) juce::jlimit (0, (int) FollowerBand::Scale::NumScales - 1, s.scale));
    band.setFeel  ((FollowerBand::Feel)  juce::jlimit (0, (int) FollowerBand::Feel::NumFeels - 1, s.feel));
    band.setPhraseBars (s.phraseBars);
    setBandRoster (s.drumsIn, s.bassIn, s.keysIn);

    if (s.followKey)
    {
        analyzer.unlockKey();
        analyzer.setKeySeed (s.keyPc);
    }
    else
    {
        analyzer.setManualKey (s.keyPc);
    }

    if (s.slew)
    {
        analyzer.unlockBpm();
        analyzer.setBpmSeed (s.bpm);
        analyzer.setLockTempo (false);
    }
    else
    {
        analyzer.setManualBpm (s.bpm);
        analyzer.setLockTempo (true);
    }

    int mask = 0;
    const auto sc = (FollowerBand::Scale) juce::jlimit (0, (int) FollowerBand::Scale::NumScales - 1, s.scale);
    for (int i = 0; i < 12; ++i)
        if (FollowerBand::scaleHas (sc, i))
            mask |= (1 << i);
    analyzer.setScaleIntervals (mask);
}

void SessionProcessor::startSession()
{
    band.reset();
    analyzer.resetEngage();
    band.setEnabled (false);
    waitingNotes.store (0, std::memory_order_relaxed);
    countingIn.store (0, std::memory_order_relaxed);
    sessionRunning.store (1, std::memory_order_relaxed);
    // Do not wait for a YIN lock — silent/unpitched input used to stall forever.
    if (countInEnabled.load (std::memory_order_relaxed) != 0)
        countInRequest.store (1, std::memory_order_relaxed);
    else
        goBand.store (1, std::memory_order_relaxed);
}

void SessionProcessor::stopSession()
{
    band.setEnabled (false);
    sessionRunning.store (0, std::memory_order_relaxed);
    countInRequest.store (0, std::memory_order_relaxed);
    countingIn.store (0, std::memory_order_relaxed);
    waitingNotes.store (0, std::memory_order_relaxed);
}

juce::String SessionProcessor::startRecording()
{
    auto err = daw.startRecord();
    if (err.isNotEmpty())
        return err;
    return writer.beginRecording();
}

void SessionProcessor::stopRecording()
{
    daw.stopRecord();
    writer.setMeta (FollowerBand::styleName ((int) band.getStyle()),
                    InputAnalyzer::pitchClassName (analyzer.getKeyPc()),
                    analyzer.getBpm());
    writer.stop();
}

void SessionProcessor::beginCountIn() noexcept
{
    countInActive = true;
    countInBeatsDone = 0;
    countInSample = 0.0;
    clickEnv = 0.0f;
    clickPhase = 0.0f;
    countingIn.store (1, std::memory_order_relaxed);
    countInBeatAtom.store (1, std::memory_order_relaxed);
}

void SessionProcessor::finishCountIn() noexcept
{
    countInActive = false;
    countingIn.store (0, std::memory_order_relaxed);
    countInBeatAtom.store (0, std::memory_order_relaxed);
    band.reset();
    band.setEnabled (true);
    daw.play();
}

void SessionProcessor::renderClick (float* left, float* right, int n) noexcept
{
    if (! countInActive || n <= 0)
        return;

    const float bpm = juce::jlimit (60.0f, 180.0f, analyzer.getBpm());
    const double samplesPerBeat = (60.0 / (double) bpm) * sampleRate;

    for (int i = 0; i < n; ++i)
    {
        if (countInSample <= 0.0)
        {
            const bool accent = (countInBeatsDone == 0 || countInBeatsDone == 3);
            clickHz = accent ? 1260.0f : 880.0f;
            clickEnv = accent ? 0.72f : 0.55f;
            clickPhase = 0.0f;
            countInBeatAtom.store (countInBeatsDone + 1, std::memory_order_relaxed);
        }

        clickPhase += clickHz / (float) sampleRate;
        clickPhase -= std::floor (clickPhase);
        const float s = std::sin (juce::MathConstants<float>::twoPi * clickPhase) * clickEnv;
        clickEnv *= 0.9991f;
        left[i]  += s;
        right[i] += s;

        countInSample += 1.0;
        if (countInSample >= samplesPerBeat)
        {
            countInSample -= samplesPerBeat;
            ++countInBeatsDone;
            if (countInBeatsDone >= 4)
            {
                finishCountIn();
                break;
            }
        }
    }
}

void SessionProcessor::processBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const int nCh = buffer.getNumChannels();
    const float* ins[32] = {};
    float* outs[32] = {};
    const int nUse = juce::jmin (nCh, 32);
    for (int c = 0; c < nUse; ++c)
    {
        ins[c]  = buffer.getReadPointer  (c, startSample);
        outs[c] = buffer.getWritePointer (c, startSample);
    }
    processDuplex (ins, nUse, outs, nUse, numSamples);
}

void SessionProcessor::processDuplex (const float* const* inChannels, int numIns,
                                      float* const* outChannels, int numOuts,
                                      int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    auto silenceOuts = [&]()
    {
        if (outChannels == nullptr)
            return;
        for (int c = 0; c < numOuts; ++c)
            if (outChannels[c] != nullptr)
                std::memset (outChannels[c], 0, (size_t) numSamples * sizeof (float));
    };

    if (maxBlock > 0 && numSamples > maxBlock)
    {
        int done = 0;
        while (done < numSamples)
        {
            const int chunk = juce::jmin (maxBlock, numSamples - done);
            const float* inOff[32] = {};
            float* outOff[32] = {};
            const int nIn  = juce::jmin (juce::jmax (0, numIns),  32);
            const int nOut = juce::jmin (juce::jmax (0, numOuts), 32);
            if (inChannels != nullptr)
                for (int c = 0; c < nIn; ++c)
                    inOff[c] = (inChannels[c] != nullptr) ? (inChannels[c] + done) : nullptr;
            if (outChannels != nullptr)
                for (int c = 0; c < nOut; ++c)
                    outOff[c] = (outChannels[c] != nullptr) ? (outChannels[c] + done) : nullptr;
            processDuplex (inOff, nIn, outOff, nOut, chunk);
            done += chunk;
        }
        return;
    }

    if ((int) inMono.size() < numSamples || (int) mL.size() < numSamples)
    {
        silenceOuts();
        return;
    }

    const int n = numSamples;

    if (sessionRunning.load (std::memory_order_relaxed) == 0)
    {
        countInActive = false;
        countingIn.store (0, std::memory_order_relaxed);
        waitingNotes.store (0, std::memory_order_relaxed);
        goBand.store (0, std::memory_order_relaxed);
        countInRequest.store (0, std::memory_order_relaxed);
    }
    else if (goBand.exchange (0, std::memory_order_relaxed) != 0)
    {
        waitingNotes.store (0, std::memory_order_relaxed);
        finishCountIn();
    }
    else if (countInRequest.exchange (0, std::memory_order_relaxed) != 0)
    {
        waitingNotes.store (0, std::memory_order_relaxed);
        beginCountIn();
    }
    else if (waitingNotes.load (std::memory_order_relaxed) != 0 && analyzer.hasEngaged())
    {
        waitingNotes.store (0, std::memory_order_relaxed);
        if (countInEnabled.load (std::memory_order_relaxed) != 0)
            beginCountIn();
        else
            finishCountIn();
    }

    // Guitar from INPUT channels only. Pick the loudest non-null input channel.
    // Keep ch0 only if e0 >= bestE * 0.5 (~3 dB). Scan all numIns (cap 8).
    // Never read outputs. Never write inputs.
    {
        const int nScan = juce::jmin (juce::jmax (0, numIns), 8);
        int bestCh = -1;
        float bestE = 0.0f;
        float e0 = 0.0f;
        bool have0 = false;
        if (inChannels != nullptr)
        {
            for (int c = 0; c < nScan; ++c)
            {
                const float* p = inChannels[c];
                if (p == nullptr)
                    continue;
                float e = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float s = p[i];
                    e += s * s;
                }
                if (c == 0)
                {
                    e0 = e;
                    have0 = true;
                }
                if (bestCh < 0 || e > bestE)
                {
                    bestE = e;
                    bestCh = c;
                }
            }
            // ~3 dB (energy ratio 0.5). Ch0 wins only if within 3 dB of loudest.
            if (have0 && e0 >= bestE * 0.5f)
                bestCh = 0;
        }
        if (bestCh >= 0 && inChannels != nullptr && inChannels[bestCh] != nullptr)
            std::memcpy (inMono.data(), inChannels[bestCh], (size_t) n * sizeof (float));
        else
            std::memset (inMono.data(), 0, (size_t) n * sizeof (float));
    }

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (inMono[(size_t) i]));
    inputPeak.store (inputPeak.load (std::memory_order_relaxed) * 0.6f + peakIn * 0.4f,
                     std::memory_order_relaxed);

    // Input gain (SmoothedValue), then tuner tap on the hot pre-gate signal.
    inputGainSmooth.setTargetValue (
        juce::Decibels::decibelsToGain (inputGainDb.load (std::memory_order_relaxed)));
    for (int i = 0; i < n; ++i)
        inMono[(size_t) i] *= inputGainSmooth.getNextValue();

    analyzer.pushSamples (inMono.data(), n);

    midiScratch.clear();
    analyzer.drainMidi (midiScratch, n);
    daw.setGuitarRackLatency (guitarRack.getLatencySamples());

    {
        const float bpmQ = juce::jmax (40.0f, analyzer.getBpm());
        const double sr = juce::jmax (1.0, sampleRate);
        const double spq = (60.0 / (double) bpmQ) * sr;
        double q = (double) daw.getPosition() / juce::jmax (1.0, spq);
        if (sessionRunning.load (std::memory_order_relaxed) != 0 && q <= 0.0)
        {
            const int bar = band.getAbsBarIndex();
            const float frac = band.getBeatFraction();
            q = (double) bar * 4.0 + (double) frac * 4.0;
        }
        analyzer.setTransportQuarter (q);
    }

    const float gAmt = gateAmt.load (std::memory_order_relaxed);
    const bool useGate = gAmt > 0.02f;
    if (useGate)
    {
        const float openDb = juce::jmap (gAmt, 0.0f, 1.0f, -72.0f, -28.0f);
        gateOpen  = juce::Decibels::decibelsToGain (openDb);
        gateClose = juce::Decibels::decibelsToGain (openDb - 6.0f);
    }
    if (! useGate)
        gateGainSmooth.setTargetValue (1.0f);
    for (int i = 0; i < n; ++i)
    {
        float x = inMono[(size_t) i];
        if (useGate)
        {
            const float ax = std::abs (x);
            const float coeff = ax > gateEnv ? gateAtk : gateRel;
            gateEnv += (ax - gateEnv) * coeff;
            if (gateEnv > gateOpen) gateOpenState = true;
            else if (gateEnv < gateClose) gateOpenState = false;
            gateGainSmooth.setTargetValue (gateOpenState ? 1.0f : 0.0f);
            x *= gateGainSmooth.getNextValue();
        }
        else
        {
            gateEnv *= 0.99f;
            x *= gateGainSmooth.getNextValue();
        }
        inMono[(size_t) i] = x;
    }

    for (int i = 0; i < n; ++i)
    {
        gL[(size_t) i] = inMono[(size_t) i];
        gR[(size_t) i] = inMono[(size_t) i];
    }
    guitarRack.process (PluginRack::PreAmp, gL.data(), gR.data(), n, midiScratch);
    if (guitarRack.isVstAmpActive())
    {
        guitarRack.process (PluginRack::AmpReplace, gL.data(), gR.data(), n, midiScratch);
    }
    else if (ampBypass.load (std::memory_order_relaxed) == 0)
    {
        for (int i = 0; i < n; ++i)
            inMono[(size_t) i] = 0.5f * (gL[(size_t) i] + gR[(size_t) i]);
        amp.process (inMono.data(), gL.data(), gR.data(), n);
    }
    guitarRack.process (PluginRack::Post,  gL.data(), gR.data(), n, midiScratch);
    guitarRack.process (PluginRack::Slot4, gL.data(), gR.data(), n, midiScratch);
    fx.process (gL.data(), gR.data(), n, analyzer.getBpm());

    {
        float player = analyzer.getPlayerEnergy();
        float be = bandEnergy.load (std::memory_order_relaxed);
        if (analyzer.isLockIntensity())
            player = be;
        be += (player - be) * 0.012f; // band lags the player
        if (grooveFloor.load (std::memory_order_relaxed) != 0
            && fadeSilence.load (std::memory_order_relaxed) == 0
            && waitingNotes.load() == 0
            && countingIn.load() == 0)
            be = juce::jmax (0.32f, be);
        bandEnergy.store (be, std::memory_order_relaxed);
    }
    {
        float chroma[12] = {};
        analyzer.copyChroma (chroma);
        const bool onset = analyzer.consumeOnset();
        arrangement.tick (chroma, onset,
                          analyzer.getKeyPc(),
                          (int) band.getStyle(),
                          (int) band.getForm(),
                          band.getAbsBarIndex(),
                          band.getStepInBar(),
                          bandEnergy.load (std::memory_order_relaxed),
                          band.getStepAccum(),
                          band.getSamplesPer16th());
        if (arrangement.hasFollow())
            band.setFollowedDegree (arrangement.getFollowDegree());
        else
            band.setFollowedDegree (-1);
        band.setThinMask (arrangement.thinMask());
        band.applyPhaseNudge (arrangement.consumePhaseNudge());
    }
    band.process (analyzer.getKeyPc(),
                  analyzer.getBpm(),
                  bandEnergy.load (std::memory_order_relaxed),
                  dL.data(), dR.data(),
                  bL.data(), bR.data(),
                  kL.data(), kR.data(),
                  n);

    daw.process (gL.data(), gR.data(),
                 dL.data(), dR.data(),
                 bL.data(), bR.data(),
                 kL.data(), kR.data(),
                 mL.data(), mR.data(),
                 n,
                 sessionRunning.load (std::memory_order_relaxed) != 0
                     && countingIn.load (std::memory_order_relaxed) == 0,
                 midiScratch);
    // Master inserts live on the DAW Master track, not the guitar rack.

    const float gG = busGain (Guitar);
    const float gD = busGain (Drums);
    const float gB = busGain (Bass);
    const float gK = busGain (Keys);
    const float gM = busGain (Master);

    juce::ignoreUnused (gG, gD, gB, gK, gM);
    float pG = 0, pD = 0, pB = 0, pK = 0, peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        pG = juce::jmax (pG, std::abs (gL[(size_t) i]), std::abs (gR[(size_t) i]));
        pD = juce::jmax (pD, std::abs (dL[(size_t) i]), std::abs (dR[(size_t) i]));
        pB = juce::jmax (pB, std::abs (bL[(size_t) i]), std::abs (bR[(size_t) i]));
        pK = juce::jmax (pK, std::abs (kL[(size_t) i]), std::abs (kR[(size_t) i]));
        peakOut = juce::jmax (peakOut, std::abs (mL[(size_t) i]), std::abs (mR[(size_t) i]));
    }

    renderClick (mL.data(), mR.data(), n);

    auto smoothPeak = [this] (int bus, float peak)
    {
        const float prev = busPeak[(size_t) bus].load (std::memory_order_relaxed);
        busPeak[(size_t) bus].store (prev * 0.6f + peak * 0.4f, std::memory_order_relaxed);
    };
    smoothPeak (Guitar, pG);
    smoothPeak (Drums,  pD);
    smoothPeak (Bass,   pB);
    smoothPeak (Keys,   pK);
    smoothPeak (Master, peakOut);
    outputPeak.store (outputPeak.load (std::memory_order_relaxed) * 0.6f + peakOut * 0.4f,
                      std::memory_order_relaxed);

    writer.push (gL.data(), gR.data(),
                 dL.data(), dR.data(),
                 bL.data(), bR.data(),
                 kL.data(), kR.data(),
                 mL.data(), mR.data(),
                 n);

    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (mL[(size_t) i])) mL[(size_t) i] = 0.0f;
        if (! std::isfinite (mR[(size_t) i])) mR[(size_t) i] = 0.0f;
        mL[(size_t) i] = juce::jlimit (-1.5f, 1.5f, mL[(size_t) i]);
        mR[(size_t) i] = juce::jlimit (-1.5f, 1.5f, mR[(size_t) i]);
    }

    if (outChannels != nullptr)
    {
        if (numOuts > 0 && outChannels[0] != nullptr)
            std::memcpy (outChannels[0], mL.data(), (size_t) n * sizeof (float));
        if (numOuts > 1 && outChannels[1] != nullptr)
            std::memcpy (outChannels[1], mR.data(), (size_t) n * sizeof (float));
        for (int c = 2; c < numOuts; ++c)
            if (outChannels[c] != nullptr)
                std::memset (outChannels[c], 0, (size_t) n * sizeof (float));
    }
}
