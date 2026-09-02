#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

/**
 * Disk-backed stem recorder. Audio thread writes a lock-free ring only.
 * Message thread starts/stops; a worker thread writes 32-bit float WAVs.
 *
 * Stems: guitar, drums, bass, keys, fx (stereo each) + stereo master.
 */
class StemWriter
{
public:
    enum Stem : int
    {
        Guitar = 0,
        Drums,
        Bass,
        Keys,
        Fx,
        Master,
        NumStems
    };

    static constexpr int kPlanes = 12; // 6 stems × 2 ch
    static constexpr int kMaxSeconds = 45 * 60;

    StemWriter();
    ~StemWriter();

    void prepare (double sampleRate);
    void reset();

    /** Message thread. Creates a timestamped folder and starts the writer. */
    juce::String beginRecording();
    /** Message thread. Stops and joins the writer, flushing remaining samples. */
    void stop();

    bool isRecording() const noexcept { return recording.load (std::memory_order_relaxed) != 0; }
    juce::File getSessionDirectory() const { return sessionDir; }
    float getRecordedSeconds() const noexcept;
    int getDroppedBuffers() const noexcept { return dropped.load (std::memory_order_relaxed); }

    /** Audio thread. Copies 5 stereo buses into the ring if armed. */
    void push (const float* guitarL, const float* guitarR,
               const float* drumsL,  const float* drumsR,
               const float* bassL,   const float* bassR,
               const float* keysL,   const float* keysR,
               const float* fxL,     const float* fxR,
               const float* masterL, const float* masterR,
               int numSamples) noexcept;

    void setMeta (juce::String style, juce::String key, float bpm)
    {
        metaStyle = std::move (style);
        metaKey = std::move (key);
        metaBpm = bpm;
    }

    static const char* stemFileName (int s);

private:
    class Worker : public juce::Thread
    {
    public:
        Worker (const juce::String& name, std::function<void()> workFn)
            : juce::Thread (name), work (std::move (workFn)) {}
        void run() override { if (work) work(); }
        std::function<void()> work;
    };

    void writerLoop();
    bool openWriters();
    void closeWriters();
    void writeSidecar();

    double sampleRate = 44100.0;
    int recRingSize = 0;
    int maxSamples = 0;

    juce::AbstractFifo recFifo { 4096 };
    std::array<std::vector<float>, kPlanes> recRing;
    std::array<std::vector<float>, kPlanes> writeScratch;

    std::array<std::unique_ptr<juce::AudioFormatWriter>, NumStems> wavWriters;
    std::unique_ptr<Worker> writer;
    juce::File sessionDir;

    std::atomic<int> recording { 0 };
    std::atomic<int> recordedLength { 0 };
    std::atomic<int> recSamplesPushed { 0 };
    std::atomic<int> dropped { 0 };

    juce::String metaStyle { "Rock" };
    juce::String metaKey { "E" };
    float metaBpm = 112.0f;
};
