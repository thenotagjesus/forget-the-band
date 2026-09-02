#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

/** Tempo-sync delay + space. Buffers allocated in prepare only. */
class GuitarFx
{
public:
    enum class Division : int
    {
        Quarter = 0,
        Eighth,
        DottedEighth,
        Sixteenth,
        NumDivisions
    };

    static const char* divisionName (int i);
    static float beatsFor (Division d) noexcept;

    void prepare (double sampleRate, int samplesPerBlock);
    void reset() noexcept;

    void setDelayMix (float v) noexcept { delayMix.store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
    void setSpaceMix (float v) noexcept { spaceMix.store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
    void setDivision (Division d) noexcept { division.store ((int) d, std::memory_order_relaxed); }
    float getDelayMix() const noexcept { return delayMix.load (std::memory_order_relaxed); }
    float getSpaceMix() const noexcept { return spaceMix.load (std::memory_order_relaxed); }
    Division getDivision() const noexcept
    {
        return (Division) juce::jlimit (0, (int) Division::NumDivisions - 1,
                                        division.load (std::memory_order_relaxed));
    }

    /** In-place stereo. RT-safe. Dual-line 50 ms equal-power crossfade on BPM/division change. */
    void process (float* left, float* right, int numSamples, float bpm) noexcept;

private:
    float readDelay (const std::vector<float>& buf, float delaySamp) const noexcept;

    std::atomic<float> delayMix { 0.18f };
    std::atomic<float> spaceMix { 0.16f };
    std::atomic<int> division { (int) Division::DottedEighth };

    double sampleRate = 44100.0;
    int maxDelay = 1;
    int writePos = 0;
    float activeDelay = 0.0f;
    float fromDelay = 0.0f;
    float xfade = 1.0f;
    int xfadeSamples = 1;

    std::vector<float> delayL, delayR;
    std::vector<float> sendL, sendR;
    juce::Reverb reverb;
};
