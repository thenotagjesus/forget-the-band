#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

/**
 * Pitch / key / tempo / activity analysis.
 * Audio thread: pushSamples() only (lock-free FIFO).
 * Worker thread: YIN, chroma, onset IOIs. Publishes atomics the audio thread may read.
 */
class InputAnalyzer : public juce::Thread
{
public:
    InputAnalyzer();
    ~InputAnalyzer() override;

    void prepare (double sampleRate);
    void release();

    /** Audio thread. Copies into a lock-free ring; drops if the ring is full. */
    void pushSamples (const float* data, int numSamples) noexcept;

    void run() override;

    float getFrequencyHz() const noexcept { return freqHz.load (std::memory_order_relaxed); }
    float getConfidence()  const noexcept { return confidence.load (std::memory_order_relaxed); }
    int   getMidiNote()    const noexcept { return midiNote.load (std::memory_order_relaxed); }
    /** Cents vs nearest MIDI note. 0 if no reliable pitch. */
    float getCents() const noexcept;
    int   getKeyPc()       const noexcept { return keyPc.load (std::memory_order_relaxed); }
    float getBpm()         const noexcept { return bpm.load (std::memory_order_relaxed); }
    float getActivity()    const noexcept { return activity.load (std::memory_order_relaxed); }
    float getIntensity()   const noexcept { return intensity.load (std::memory_order_relaxed); }
    float getPlayerEnergy() const noexcept { return playerEnergy.load (std::memory_order_relaxed); }
    float getRms()         const noexcept { return rmsAtom.load (std::memory_order_relaxed); }
    bool  isKeyLocked()    const noexcept { return keyLocked.load (std::memory_order_relaxed) != 0; }
    bool  isBpmLocked()    const noexcept { return bpmLocked.load (std::memory_order_relaxed) != 0; }
    bool  hasEngaged()     const noexcept { return engaged.load (std::memory_order_relaxed) != 0; }
    float getFit()         const noexcept { return fitAtom.load (std::memory_order_relaxed); }
    bool  isCalibrated()   const noexcept { return calibrated.load (std::memory_order_relaxed) != 0; }

    void setAutoKey (bool v) noexcept { autoKey.store (v ? 1 : 0, std::memory_order_relaxed); }
    void setAutoBpm (bool v) noexcept { autoBpm.store (v ? 1 : 0, std::memory_order_relaxed); }
    bool isAutoKey() const noexcept { return autoKey.load (std::memory_order_relaxed) != 0; }
    bool isAutoBpm() const noexcept { return autoBpm.load (std::memory_order_relaxed) != 0; }

    void setLockTempo (bool v) noexcept { lockTempo.store (v ? 1 : 0, std::memory_order_relaxed); }
    void setLockIntensity (bool v) noexcept { lockIntensity.store (v ? 1 : 0, std::memory_order_relaxed); }
    void setEnergyDrift (bool v) noexcept { energyDrift.store (v ? 1 : 0, std::memory_order_relaxed); }
    bool isLockTempo() const noexcept { return lockTempo.load (std::memory_order_relaxed) != 0; }
    bool isLockIntensity() const noexcept { return lockIntensity.load (std::memory_order_relaxed) != 0; }
    bool isEnergyDrift() const noexcept { return energyDrift.load (std::memory_order_relaxed) != 0; }

    void setScaleIntervals (int mask) noexcept { scaleMask.store (mask, std::memory_order_relaxed); }
    void captureCal (int stage) noexcept; // 0 soft, 1 mid, 2 hard
    void resetEngage() noexcept { engaged.store (0, std::memory_order_relaxed); }

    /** Manual override. pc 0–11. Clears auto-lock for key. */
    void setManualKey (int pc) noexcept;
    /** Manual override. bpm 60–180. Clears auto-lock for tempo. */
    void setManualBpm (float v) noexcept;
    void unlockKey() noexcept;
    void unlockBpm() noexcept;
    /** Seed published key without changing follow/lock flags. */
    void setKeySeed (int pc) noexcept;
    /** Seed published BPM without changing follow/lock flags. */
    void setBpmSeed (float v) noexcept;
    /** Nudge published BPM; keeps slew/auto flags. */
    void nudgeBpm (float delta) noexcept;

    static const char* pitchClassName (int pc);
    static juce::String noteNameFromMidi (int note);

private:
    void analyseWindow (const float* x, int n) noexcept;
    float detectYin (const float* x, int n, float& conf) noexcept;
    void updateKey (float hz, float conf, float rms) noexcept;
    void updateTempo (float rms, int n) noexcept;
    void maybeLock() noexcept;

    double sampleRate = 44100.0;
    int hop = 512;
    int hopCounter = 0;
    int windowSize = 2048;

    juce::AbstractFifo fifo { 4096 };
    std::vector<float> ring;
    std::vector<float> window;
    std::vector<float> yinDiff;
    std::vector<float> yinCmnd;
    std::vector<float> gather;

    std::atomic<float> freqHz { 0.0f };
    std::atomic<float> confidence { 0.0f };
    std::atomic<int>   midiNote { -1 };
    std::atomic<int>   keyPc { 4 };          // E, guitar-friendly default
    std::atomic<float> bpm { 112.0f };
    std::atomic<float> activity { 0.0f };
    std::atomic<float> intensity { 0.35f };
    std::atomic<float> playerEnergy { 0.0f };
    std::atomic<float> rmsAtom { 0.0f };
    std::atomic<float> fitAtom { 1.0f };
    std::atomic<int>   keyLocked { 0 };
    std::atomic<int>   bpmLocked { 1 };
    std::atomic<int>   autoKey { 1 };
    std::atomic<int>   autoBpm { 0 };
    std::atomic<int>   lockTempo { 1 };
    std::atomic<int>   lockIntensity { 0 };
    std::atomic<int>   energyDrift { 0 };
    std::atomic<int>   engaged { 0 };
    std::atomic<int>   calibrated { 0 };
    std::atomic<int>   scaleMask { 0x4A9 }; // pentatonic 0,3,5,7,10
    std::atomic<float> calSoft { 0.02f };
    std::atomic<float> calMid { 0.08f };
    std::atomic<float> calHard { 0.20f };

    std::array<float, 12> chroma {};
    int pendingKey = 4;
    int keyStableHops = 0;
    int hopsPerBar = 80;

    float lastEnergy = 0.0f;
    float fluxAvg = 0.0f;
    double samplesSinceOnset = 1.0e9;
    std::array<float, 12> ioiSec {};
    int ioiCount = 0;
    int ioiWrite = 0;
    float bpmSmoothed = 112.0f;
    int bpmStableHops = 0;

    float rmsSmooth = 0.0f;
    float activitySmooth = 0.0f;
    float intensitySmooth = 0.35f;
    double hopsElapsed = 0.0;
};
