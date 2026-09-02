#include "DSP/GuitarFx.h"
#include <cstring>
#include <cmath>

const char* GuitarFx::divisionName (int i)
{
    static const char* n[] = { "1/4", "1/8", "1/8.", "1/16" };
    if (i < 0 || i >= (int) Division::NumDivisions) return "1/8.";
    return n[i];
}

float GuitarFx::beatsFor (Division d) noexcept
{
    switch (d)
    {
        case Division::Quarter:       return 1.00f;
        case Division::Eighth:        return 0.50f;
        case Division::DottedEighth:  return 0.75f;
        case Division::Sixteenth:     return 0.25f;
        default:                      return 0.75f;
    }
}

void GuitarFx::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    // 1.5 s covers a quarter note at 40 BPM.
    maxDelay = juce::jmax (256, (int) std::lround (sampleRate * 1.5) + 8);
    delayL.assign ((size_t) maxDelay, 0.0f);
    delayR.assign ((size_t) maxDelay, 0.0f);
    const int n = juce::jmax (samplesPerBlock, 4096);
    sendL.assign ((size_t) n, 0.0f);
    sendR.assign ((size_t) n, 0.0f);
    writePos = 0;
    activeDelay = (float) (sampleRate * 0.4);
    fromDelay = activeDelay;
    xfade = 1.0f;
    xfadeSamples = juce::jmax (1, (int) std::lround (sampleRate * 0.050));
    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters p;
    p.roomSize   = 0.38f;
    p.damping    = 0.68f;
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 0.30f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);
}

void GuitarFx::reset() noexcept
{
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    writePos = 0;
    xfade = 1.0f;
    reverb.reset();
}

float GuitarFx::readDelay (const std::vector<float>& buf, float delaySamp) const noexcept
{
    delaySamp = juce::jlimit (4.0f, (float) (maxDelay - 4), delaySamp);
    const int i0 = (int) delaySamp;
    const float frac = delaySamp - (float) i0;
    int r0 = writePos - i0;
    if (r0 < 0) r0 += maxDelay;
    int r1 = r0 - 1;
    if (r1 < 0) r1 += maxDelay;
    return buf[(size_t) r0] * (1.0f - frac) + buf[(size_t) r1] * frac;
}

void GuitarFx::process (float* left, float* right, int numSamples, float bpm) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0 || maxDelay <= 4)
        return;

    const float dMix = delayMix.load (std::memory_order_relaxed);
    const float sMix = juce::jlimit (0.0f, 1.0f, spaceMix.load (std::memory_order_relaxed));
    const auto div = getDivision();
    const float bpmClamped = juce::jlimit (40.0f, 240.0f, bpm);
    const float want = juce::jlimit (4.0f, (float) (maxDelay - 4),
                                     (60.0f / bpmClamped) * beatsFor (div) * (float) sampleRate);

    if (std::abs (want - activeDelay) > 8.0f && xfade >= 1.0f)
    {
        fromDelay = activeDelay;
        activeDelay = want;
        xfade = 0.0f;
    }
    else if (xfade >= 1.0f)
    {
        activeDelay = want;
    }

    const float fb = 0.32f;
    const float xInc = 1.0f / (float) xfadeSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        xfade = juce::jmin (1.0f, xfade + xInc);
        const float a = std::cos (xfade * juce::MathConstants<float>::halfPi);
        const float b = std::sin (xfade * juce::MathConstants<float>::halfPi);

        const float wetL = readDelay (delayL, fromDelay) * a + readDelay (delayL, activeDelay) * b;
        const float wetR = readDelay (delayR, fromDelay) * a + readDelay (delayR, activeDelay) * b;

        const float inL = left[i];
        const float inR = right[i];
        delayL[(size_t) writePos] = inL + wetL * fb;
        delayR[(size_t) writePos] = inR + wetR * fb;
        writePos = (writePos + 1) % maxDelay;

        left[i]  = inL + wetL * dMix;
        right[i] = inR + wetR * dMix;
    }

    juce::Reverb::Parameters p;
    p.roomSize   = 0.30f + 0.15f * sMix;
    p.damping    = 0.60f + 0.15f * (1.0f - sMix * 0.5f);
    p.wetLevel   = 1.0f;
    p.dryLevel   = 0.0f;
    p.width      = 0.30f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    const float wetCap = juce::jmin (0.35f, sMix * 0.35f);
    if (wetCap < 0.001f || (int) sendL.size() < numSamples)
        return;

    std::memcpy (sendL.data(), left,  (size_t) numSamples * sizeof (float));
    std::memcpy (sendR.data(), right, (size_t) numSamples * sizeof (float));
    reverb.processStereo (sendL.data(), sendR.data(), numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        left[i]  = left[i]  + sendL[(size_t) i] * wetCap; // dry 1.0
        right[i] = right[i] + sendR[(size_t) i] * wetCap;
    }
}
