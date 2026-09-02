#pragma once

#include <JuceHeader.h>
#include "DSP/Biquad.h"

/** Modest guitar amp + IIR cab. Allocation-free after prepare. Not a multi-amp suite. */
class AmpCab
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset() noexcept;

    void setDrive (float v) noexcept { drive.store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
    void setTone  (float v) noexcept { tone.store  (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
    void setLevel (float v) noexcept { level.store (juce::jlimit (0.0f, 1.5f, v), std::memory_order_relaxed); }

    float getDrive() const noexcept { return drive.load (std::memory_order_relaxed); }
    float getTone()  const noexcept { return tone.load  (std::memory_order_relaxed); }
    float getLevel() const noexcept { return level.load (std::memory_order_relaxed); }

    /** Mono in, stereo out (tiny width delay on R). RT-safe. */
    void process (const float* in, float* outL, float* outR, int numSamples) noexcept;

private:
    void refreshCoeffs() noexcept;
    static float waveshape (float x, float amount) noexcept;

    std::atomic<float> drive { 0.42f };
    std::atomic<float> tone  { 0.55f };
    std::atomic<float> level { 0.80f };

    double sampleRate = 44100.0;
    Biquad preHp, preLp, midScoop, cabThump, cabCone, cabAir, postLp;
    OnePole toneTilt;
    float delayLine[32] {};
    int delayIndex = 0;
    int delaySamples = 12;
};
