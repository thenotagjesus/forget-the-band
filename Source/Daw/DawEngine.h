#pragma once

#include "Daw/Project.h"
#include <array>
#include <atomic>
#include <functional>

/** Linear DAW transport + clip playback + take recorder. RT-safe process(). */
class DawEngine
{
public:
    explicit DawEngine (PluginHost& host);

    Project& getProject() noexcept { return project; }
    const Project& getProject() const noexcept { return project; }

    void prepare (double sampleRate, int block);
    void release();

    void play();
    void stop();
    void returnToZero();
    void setCycle (bool c) noexcept;
    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed) != 0; }
    bool isRecording() const noexcept { return recording.load (std::memory_order_relaxed) != 0; }
    bool isCycling() const noexcept { return project.cycle; }

    juce::String startRecord();
    void stopRecord();
    void setDeviceLatency (int inSamples, int outSamples) noexcept
    {
        inputLatency.store (juce::jmax (0, inSamples), std::memory_order_relaxed);
        outputLatency.store (juce::jmax (0, outSamples), std::memory_order_relaxed);
    }
    void setGuitarRackLatency (int s) noexcept
    {
        guitarRackLatency.store (juce::jmax (0, s), std::memory_order_relaxed);
    }

    int64_t getPosition() const noexcept { return position.load (std::memory_order_relaxed); }
    void setPosition (int64_t s) noexcept { position.store (juce::jmax ((int64_t) 0, s), std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return sampleRate; }

    /** Mix clips (+ optional live buses) into track work buffers, then to master L/R. */
    void process (float* guitarL, float* guitarR,
                  float* drumsL, float* drumsR,
                  float* bassL, float* bassR,
                  float* keysL, float* keysR,
                  float* masterL, float* masterR,
                  int n,
                  bool sessionLive,
                  const juce::MidiBuffer& midi) noexcept;

    juce::String bounceMixdown (const juce::File& dest);
    float getTrackPeak (int t) const noexcept;

    std::atomic<int> selectedTrack { 0 };

private:
    struct RecChan
    {
        juce::AbstractFifo fifo { 4096 };
        std::vector<float> ringL, ringR;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::File file;
        std::atomic<int> armed { 0 };
        int samplesWritten = 0;
        int64_t firstWritePos = -1;
    };

    class Worker : public juce::Thread
    {
    public:
        Worker (const juce::String& n, std::function<void()> fn)
            : juce::Thread (n), work (std::move (fn)) {}
        void run() override { if (work) work(); }
        std::function<void()> work;
    };

    void mixClips (int track, float* l, float* r, int64_t pos, int n) noexcept;
    void writerLoop();
    void finishTakes();
    void punchTrim (int track, int64_t a, int64_t b);
    int pdcSamples (int track) const noexcept;
    bool inPunchWindow (int64_t abs) const noexcept;

    std::atomic<int> inputLatency { 0 };
    std::atomic<int> outputLatency { 0 };
    std::atomic<int> guitarRackLatency { 0 };

    PluginHost& host;
    Project project;
    double sampleRate = 44100.0;
    int maxBlock = 512;
    std::atomic<int> playing { 0 };
    std::atomic<int> recording { 0 };
    std::atomic<int64_t> position { 0 };
    int64_t recordOrigin = 0;

    std::array<RecChan, Daw::kMasterIndex> rec {};
    std::unique_ptr<Worker> writer;
    std::atomic<int> recActive { 0 };
};
