#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

#if defined(FTB_HAS_AUBIO) && FTB_HAS_AUBIO
#include <aubio.h>
#endif

/** aubio onset + YIN(+fft) + RMS. Alloc only in prepare/release. process() is audio-thread safe. */
class AubioEngine
{
public:
    AubioEngine() = default;
    ~AubioEngine() { release(); }

    AubioEngine (const AubioEngine&) = delete;
    AubioEngine& operator= (const AubioEngine&) = delete;

    bool prepare (double sampleRate, int hop = 512, int win = 2048);
    void release();

    /** Audio thread. Accumulate into a preallocated hop; run aubio_*_do when full. No alloc/lock. */
    void process (const float* x, int n) noexcept;

    bool isReady() const noexcept { return ready.load (std::memory_order_relaxed) != 0; }

    /** True if at least one hop completed since the last consumeHop(). */
    bool consumeHop() noexcept { return hopFlag.exchange (0, std::memory_order_relaxed) != 0; }

    float lastHz() const noexcept { return hz.load (std::memory_order_relaxed); }
    float confidence() const noexcept { return conf.load (std::memory_order_relaxed); }
    float rms() const noexcept { return rmsLin.load (std::memory_order_relaxed); }
    bool  onsetThisHop() const noexcept { return onsetFlag.load (std::memory_order_relaxed) != 0; }
    int   hopSize() const noexcept { return hop; }

private:
#if defined(FTB_HAS_AUBIO) && FTB_HAS_AUBIO
    struct DelOnset { void operator() (aubio_onset_t* p) const noexcept { if (p) del_aubio_onset (p); } };
    struct DelPitch { void operator() (aubio_pitch_t* p) const noexcept { if (p) del_aubio_pitch (p); } };
    struct DelFvec  { void operator() (fvec_t* p) const noexcept { if (p) del_fvec (p); } };

    std::unique_ptr<aubio_onset_t, DelOnset> onset;
    std::unique_ptr<aubio_pitch_t, DelPitch> pitch;
    std::unique_ptr<fvec_t, DelFvec> hopVec;
    std::unique_ptr<fvec_t, DelFvec> onsetOut;
    std::unique_ptr<fvec_t, DelFvec> pitchOut;
#endif

    int hop = 512;
    int win = 2048;
    int fill = 0;
    std::atomic<int> ready { 0 };
    std::atomic<int> hopFlag { 0 };
    std::atomic<int> onsetFlag { 0 };
    std::atomic<float> hz { 0.0f };
    std::atomic<float> conf { 0.0f };
    std::atomic<float> rmsLin { 0.0f };
};
