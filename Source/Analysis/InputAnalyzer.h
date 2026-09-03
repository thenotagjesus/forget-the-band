#pragma once

#include <JuceHeader.h>
#include "Daw/DawModel.h"
#include "Analysis/AubioEngine.h"
#include "Analysis/BasicPitchWorker.h"
#include <array>
#include <cstdint>
#include <atomic>
#include <vector>

/**
 * Pitch / key / tempo / activity / transcription.
 * Audio thread: pushSamples() + drainMidi() only (lock-free FIFOs).
 *   - AubioEngine::process (onset / YIN / RMS) publishes atomics immediately.
 *   - BasicPitchWorker::pushSamples into a lock-free ring.
 * Worker thread: transcription, IOI tempo lock, Basic Pitch chroma/chord/key.
 * Homemade YIN remains as fallback if aubio prepare() failed.
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

    /** Audio thread. Drain pitch-to-MIDI events into midiScratch. */
    void drainMidi (juce::MidiBuffer& dest, int numSamples) noexcept;

    void run() override;

    float getFrequencyHz() const noexcept { return freqHz.load (std::memory_order_relaxed); }
    float getConfidence()  const noexcept { return confidence.load (std::memory_order_relaxed); }
    int   getMidiNote()    const noexcept { return midiNote.load (std::memory_order_relaxed); }
    float getCents() const noexcept;
    int   getKeyPc()       const noexcept { return keyPc.load (std::memory_order_relaxed); }
    float getBpm()         const noexcept { return bpm.load (std::memory_order_relaxed); }
    float getActivity()    const noexcept { return activity.load (std::memory_order_relaxed); }
    float getIntensity()   const noexcept { return intensity.load (std::memory_order_relaxed); }
    float getPlayerEnergy() const noexcept { return playerEnergy.load (std::memory_order_relaxed); }
    float getRms()         const noexcept { return rmsAtom.load (std::memory_order_relaxed); }
    bool  isKeyLocked()    const noexcept { return keyLocked.load (std::memory_order_relaxed) != 0; }
    bool  isBpmLocked()    const noexcept { return bpmLocked.load (std::memory_order_relaxed) != 0; }
    bool  isBpmConfident() const noexcept { return bpmConfident.load (std::memory_order_relaxed) != 0; }
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
    void captureCal (int stage) noexcept;
    void resetEngage() noexcept;

    void setTransportQuarter (double q) noexcept { transportQ.store (q, std::memory_order_relaxed); }

    void setManualKey (int pc) noexcept;
    void setManualBpm (float v) noexcept;
    void unlockKey() noexcept;
    void unlockBpm() noexcept;
    void setKeySeed (int pc) noexcept;
    void setBpmSeed (float v) noexcept;
    void nudgeBpm (float delta) noexcept;

    int   getPlayerChordDegree() const noexcept { return playerChordDeg.load (std::memory_order_relaxed); }
    int   getPlayerChordRoot()   const noexcept { return playerChordRoot.load (std::memory_order_relaxed); }
    bool  consumeOnset() noexcept { return onsetFlag.exchange (0, std::memory_order_relaxed) != 0; }
    bool  peekOnset() const noexcept { return onsetFlag.load (std::memory_order_relaxed) != 0; }
    void  copyChroma (float out[12]) const noexcept;
    juce::String getPlayerChordName() const;

    static constexpr int kLiveNotes = 48;
    void copyLiveNotes (Daw::LiveNote* dest, int maxN, int& written) const noexcept;
    std::vector<Daw::NoteEvent> copyHistory() const;
    void replaceHistory (std::vector<Daw::NoteEvent> notes);
    void clearTranscription();
    juce::String exportMidi (const juce::File& dest) const;

    static const char* pitchClassName (int pc);
    static juce::String noteNameFromMidi (int midiNoteNumber);

private:
    struct MidiPulse { juce::uint8 status = 0, note = 0, vel = 0; };

    void analyseWindow (const float* x, int n) noexcept;
    float detectYin (const float* x, int n, float& conf) noexcept;
    void updateKey (float hz, float conf, float rms) noexcept;
    void updateTempo (float rms, int n) noexcept;
    void updateTempoFromIoi() noexcept;
    void pushIoi (float sec) noexcept;
    void maybeLock() noexcept;
    void emitNote (float hz, float conf, float rms) noexcept;
    void pushMidi (juce::uint8 status, juce::uint8 note, juce::uint8 vel) noexcept;
    void updatePlayerChord() noexcept;
    void closeHeldNote (double endQ) noexcept;
    double currentQuarter() const noexcept;
    void applyAubioHop() noexcept;
    void applyBandState (bool onset, float rms, bool fromBasicPitch) noexcept;
    void pullBasicPitch() noexcept;
    int  degreeFromRoot (int rootPc, int key) noexcept;

    AubioEngine aubio;
    BasicPitchWorker pitchWorker;

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

    juce::AbstractFifo midiFifo { 512 };
    std::vector<MidiPulse> midiRing;

    std::atomic<float> freqHz { 0.0f };
    std::atomic<float> confidence { 0.0f };
    std::atomic<int>   midiNote { -1 };
    std::atomic<int>   keyPc { 4 };
    std::atomic<float> bpm { 96.0f };
    std::atomic<float> activity { 0.0f };
    std::atomic<float> intensity { 0.35f };
    std::atomic<float> playerEnergy { 0.0f };
    std::atomic<float> rmsAtom { 0.0f };
    std::atomic<float> fitAtom { 1.0f };
    std::atomic<int>   keyLocked { 0 };
    std::atomic<int>   bpmLocked { 0 };
    std::atomic<int>   autoKey { 1 };
    std::atomic<int>   autoBpm { 0 };
    std::atomic<int>   lockTempo { 1 };
    std::atomic<int>   lockIntensity { 0 };
    std::atomic<int>   energyDrift { 0 };
    std::atomic<int>   engaged { 0 };
    std::atomic<int>   bpmConfident { 0 };
    std::atomic<int>   calibrated { 0 };
    std::atomic<int>   scaleMask { 0x4A9 };
    std::atomic<float> calSoft { 0.02f };
    std::atomic<float> calMid { 0.08f };
    std::atomic<float> calHard { 0.20f };
    std::atomic<int>   playerChordDeg { 0 };
    std::atomic<int>   playerChordRoot { 4 };
    std::atomic<int>   playerChordQuality { 0 };
    std::atomic<int>   onsetFlag { 0 };
    std::atomic<int>   liveCount { 0 };
    std::atomic<int>   aubioOk { 0 };
    std::atomic<uint32_t> hopSerial { 0 };

    std::array<float, 12> chroma {};
    std::array<std::atomic<float>, 12> chromaAtom {};
    int pendingKey = 4;
    int keyStableHops = 0;
    int hopsPerBar = 80;

    float lastEnergy = 0.0f;
    float fluxAvg = 0.0f;
    double samplesSinceOnset = 1.0e9;
    double lastNoteOnSec = -1.0;
    std::array<std::atomic<float>, 8> ioiSec {};
    std::atomic<int> ioiCount { 0 };
    std::atomic<int> ioiWrite { 0 };
    float bpmSmoothed = 96.0f;
    int bpmStableHops = 0;
    int ioiConsensusN = 0;
    int tempoDisagreeN = 0;
    std::atomic<int> ioiFresh { 0 };
    float onsetRateSmooth = 0.0f;

    float rmsSmooth = 0.0f;
    float activitySmooth = 0.0f;
    float intensitySmooth = 0.35f;
    double hopsElapsed = 0.0;

    int heldNote = -1;
    int hangHops = 0;
    int stableNoteHops = 0;
    int pendingMidi = -1;
    Daw::NoteEvent openEvent;
    int pendingChord = 0;
    int chordStableHops = 0;

    std::array<Daw::LiveNote, kLiveNotes> liveNotes {};
    std::atomic<int> liveWrite { 0 };
    int liveHeld = -1;

    std::atomic<double> transportQ { 0.0 };

    juce::CriticalSection historyLock;
    std::vector<Daw::NoteEvent> history;
};
