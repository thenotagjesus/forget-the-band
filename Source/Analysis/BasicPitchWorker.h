#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <atomic>
#include <memory>
#include <vector>

/** Spotify Basic Pitch (nmp.onnx) on a worker thread. Audio thread only calls pushSamples(). */
class BasicPitchWorker : public juce::Thread
{
public:
    static constexpr int kAudioSampleRate = 22050;
    static constexpr int kFftHop = 256;
    static constexpr int kAudioNSamples = 22050 * 2 - 256; // 43844
    static constexpr int kNFreqBinsNotes = 88;
    static constexpr int kAnnotNFrames = (22050 / 256) * 2; // 172
    static constexpr int kMidiA0 = 21;
    static constexpr int kAdvanceSamples = 11025; // 0.5 s at 22050

    enum ChordQuality : int { Maj = 0, Min = 1, Dom7 = 2, Fifth = 3 };

    BasicPitchWorker();
    ~BasicPitchWorker() override;

    void prepare (double deviceSampleRate);
    void release();

    /** Audio thread. Lock-free ring; drops if full. */
    void pushSamples (const float* x, int n) noexcept;

    bool isAvailable() const noexcept { return available.load (std::memory_order_relaxed) != 0; }
    bool consumeReady() noexcept { return readyFlag.exchange (0, std::memory_order_relaxed) != 0; }

    void copyChroma (float out[12]) const noexcept;
    int  chordRoot() const noexcept { return chordRoot_.load (std::memory_order_relaxed); }
    int  chordQuality() const noexcept { return chordQuality_.load (std::memory_order_relaxed); }
    int  keyPc() const noexcept { return keyPc_.load (std::memory_order_relaxed); }
    uint64_t polyLo() const noexcept { return polyLo_.load (std::memory_order_relaxed); }
    uint64_t polyHi() const noexcept { return polyHi_.load (std::memory_order_relaxed); }
    float lastInferMs() const noexcept { return lastInferMs_.load (std::memory_order_relaxed); }

    void run() override;

    static juce::File findModelFile();

private:
    void drainAndMaybeInfer();
    void resampleIntoWindow();
    void decodeAndPublish (const float* note, int frames, int bins);

    struct OrtState;
    std::unique_ptr<OrtState> ort;

    double deviceSr = 44100.0;

    juce::AbstractFifo fifo { 4096 };
    std::vector<float> ring;

    std::vector<float> srcStaging;
    int srcFill = 0;
    double fracPos = 0.0;

    std::vector<float> window;      // 43844 @ 22050
    int windowFill = 0;
    std::vector<float> inputFlat;   // layout for ORT [1, 43844, 1]

    std::array<std::atomic<float>, 12> chromaAtom {};
    std::array<float, 12> chromaSmooth {};
    std::atomic<int> chordRoot_ { 0 };
    std::atomic<int> chordQuality_ { 0 };
    std::atomic<int> keyPc_ { 0 };
    std::atomic<uint64_t> polyLo_ { 0 };
    std::atomic<uint64_t> polyHi_ { 0 };
    std::atomic<int> available { 0 };
    std::atomic<int> readyFlag { 0 };
    std::atomic<float> lastInferMs_ { 0.0f };
};
