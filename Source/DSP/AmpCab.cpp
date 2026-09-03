#include "DSP/AmpCab.h"

void AmpCab::prepare (double sr, int maxBlock)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    delaySamples = juce::jlimit (4, 31, (int) std::lround (0.00035 * sampleRate));
    sawviEngine.prepare (sampleRate, maxBlock > 0 ? maxBlock : 512);
    reset();
    refreshCoeffs();
}

void AmpCab::reset() noexcept
{
    preHp.reset(); preLp.reset(); midScoop.reset();
    cabThump.reset(); cabCone.reset(); cabAir.reset(); postLp.reset();
    toneTilt.reset();
    for (auto& s : delayLine) s = 0.0f;
    delayIndex = 0;
    sawviEngine.reset();
}

float AmpCab::waveshape (float x, float amount) noexcept
{
    const float g = 0.7f + amount * 4.5f;
    const float a = x * g;
    const float odd = a / (1.0f + std::abs (a));
    const float even = 0.12f * amount * a * a / (1.0f + a * a);
    return juce::jlimit (-1.2f, 1.2f, odd + even);
}

void AmpCab::refreshCoeffs() noexcept
{
    const float d = drive.load (std::memory_order_relaxed);
    const float t = tone.load (std::memory_order_relaxed);
    const float sr = (float) sampleRate;
    preHp.setHighPass (sr, 85.0f, 0.70f);
    preLp.setLowPass  (sr, juce::jmap (d, 0.0f, 1.0f, 7200.0f, 4800.0f), 0.65f);
    midScoop.setPeaking (sr, 780.0f, 0.85f, juce::jmap (d, 0.0f, 1.0f, -1.5f, -4.0f));
    cabThump.setPeaking (sr, 105.0f, 1.10f, 4.5f);
    cabCone.setPeaking  (sr, 2100.0f, 0.70f, juce::jmap (t, 0.0f, 1.0f, -3.0f, 2.5f));
    cabAir.setHighShelf (sr, 4200.0f, juce::jmap (t, 0.0f, 1.0f, -8.0f, 3.0f));
    postLp.setLowPass   (sr, 6200.0f, 0.60f);
    toneTilt.setLP (sr, juce::jmap (t, 0.0f, 1.0f, 900.0f, 2800.0f));
}

void AmpCab::processInsane (const float* in, float* outL, float* outR, int numSamples) noexcept
{
    sawvi::Params p;
    const float d = drive.load (std::memory_order_relaxed);
    const float t = tone.load (std::memory_order_relaxed);
    const float lvl = level.load (std::memory_order_relaxed);
    // Bake SAWVI defaults; Drive scales Gain around the 0.72 voice.
    p.gain   = juce::jlimit (0.0f, 1.0f, d * (0.72f / 0.42f));
    p.mids   = juce::jlimit (0.0f, 1.0f, t);
    p.output = juce::jlimit (0.0f, 1.0f, lvl * (0.50f / 0.80f));
    sawviEngine.setParams (p);

    for (int i = 0; i < numSamples; ++i)
        outL[i] = in[i];
    sawviEngine.process (outL, nullptr, numSamples);
    // fastTanh limiter parks SAWVI near 0 dBFS. Sit with the kit (~-9 dB)
    // so Insane guitar is not 12 dB over drums/bass/keys.
    const float sit = 0.35f;
    for (int i = 0; i < numSamples; ++i)
        outL[i] *= sit;
    if (outR != outL)
        for (int i = 0; i < numSamples; ++i)
            outR[i] = outL[i];
}

void AmpCab::processModest (const float* in, float* outL, float* outR, int numSamples) noexcept
{
    refreshCoeffs();
    const float d = drive.load (std::memory_order_relaxed);
    const float lvl = level.load (std::memory_order_relaxed);
    const int tap = delaySamples;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = preHp.process (in[i]);
        x = preLp.process (x);
        x = midScoop.process (x);
        x = waveshape (x, d);
        x = cabThump.process (x);
        x = cabCone.process (x);
        x = cabAir.process (x);
        x = postLp.process (x);

        const float dark = toneTilt.process (x);
        const float tn = tone.load (std::memory_order_relaxed);
        x = x * (0.45f + 0.55f * tn) + dark * (0.55f - 0.35f * tn);
        x *= lvl * 0.85f;

        delayLine[(size_t) delayIndex] = x;
        const int rIdx = (delayIndex - tap + 32) & 31;
        const float r = delayLine[(size_t) rIdx] * 0.92f + x * 0.08f;
        delayIndex = (delayIndex + 1) & 31;

        outL[i] = x;
        outR[i] = r;
    }
}

void AmpCab::process (const float* in, float* outL, float* outR, int numSamples) noexcept
{
    if (in == nullptr || outL == nullptr || outR == nullptr || numSamples <= 0)
        return;

    if (insane.load (std::memory_order_relaxed) != 0)
        processInsane (in, outL, outR, numSamples);
    else
        processModest (in, outL, outR, numSamples);
}
