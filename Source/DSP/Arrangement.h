#pragma once

#include <JuceHeader.h>
#include "DSP/FollowerBand.h"
#include <array>
#include <atomic>

/**
 * Sits above FollowerBand::process.
 * Sections + chord chart, chroma chord follow with hysteresis,
 * 16th-phase lock to player onsets, difficulty thinning.
 * Audio-thread tick is allocation-free.
 */
class Arrangement
{
public:
    static constexpr int kMaxSections = 8;
    static constexpr int kMaxChords = 16;

    struct Section
    {
        char name[16] {};
        int bars = 8;
        int nChords = 0;
        int chords[kMaxChords] {}; // FollowerBand::Degree
        int difficulty = 2;        // 0 easy .. 3 full
    };

    Arrangement() { resetDefault(); }

    void resetDefault() noexcept;
    void prepare (double sampleRate) noexcept;

    /** Audio thread. chroma[12] from InputAnalyzer, onset from consumeOnset. */
    void tick (const float chroma[12],
               bool onset,
               int keyPc,
               int style,
               int form,
               int absBar,
               int stepInBar,
               float intensity,
               double stepAccum,
               double samplesPer16th) noexcept;

    int  getFollowDegree() const noexcept { return followDeg.load (std::memory_order_relaxed); }
    bool hasFollow() const noexcept { return followConf.load (std::memory_order_relaxed) > 0.42f; }
    float getFollowConfidence() const noexcept { return followConf.load (std::memory_order_relaxed); }

    /** Consume slew-limited 16th phase correction (samples). Audio thread. */
    double consumePhaseNudge() noexcept { return phaseNudge.exchange (0.0, std::memory_order_relaxed); }

    /** Bit0 drums, bit1 bass, bit2 keys. Audio thread. */
    int thinMask() const noexcept { return thin.load (std::memory_order_relaxed); }

    int sectionChord (int absBar) const noexcept;
    int currentSection (int absBar) const noexcept;
    int getDifficulty (int absBar) const noexcept;

    int numSections() const noexcept { return nSections; }
    const Section& section (int i) const noexcept { return sections[(size_t) juce::jlimit (0, kMaxSections - 1, i)]; }

private:
    int scoreTemplates (const float chroma[12], int keyPc, int style,
                        int& bestDeg, bool& bestMinor) const noexcept;

    std::array<Section, kMaxSections> sections {};
    int nSections = 0;
    double sampleRate = 44100.0;

    std::atomic<int> followDeg { 0 };
    std::atomic<float> followConf { 0.0f };
    std::atomic<double> phaseNudge { 0.0 };
    std::atomic<int> thin { 0x7 };

    int pendingDeg = 0;
    int pendingHops = 0;
    int committedDeg = 0;
    float committedScore = 0.0f;
};
