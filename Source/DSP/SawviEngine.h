#pragma once

#include <algorithm>
#include <cmath>

namespace sawvi
{
inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToLin (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float linToDb (float lin) noexcept
{
    return 20.0f * std::log10 (std::max (lin, 1.0e-12f));
}

inline float clamp (float x, float lo, float hi) noexcept
{
    return std::min (hi, std::max (lo, x));
}

inline float flush (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}

inline float fastTanh (float x) noexcept
{
    // Padé / rational tanh, odd, f(0)=0, good enough for audio
    x = clamp (x, -4.5f, 4.5f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Direct-form I transposed (RBJ cookbook). Real-time safe, no alloc.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void identity() noexcept
    {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
    }

    void set (float B0, float B1, float B2, float A0, float A1, float A2) noexcept
    {
        const float inv = 1.0f / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv;
        a1 = A1 * inv; a2 = A2 * inv;
    }

    void lowpass (float fs, float f, float q) noexcept
    {
        f = clamp (f, 20.0f, fs * 0.45f);
        q = std::max (q, 0.05f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        set (0.5f * (1.0f - cw), 1.0f - cw, 0.5f * (1.0f - cw),
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    void highpass (float fs, float f, float q) noexcept
    {
        f = clamp (f, 8.0f, fs * 0.45f);
        q = std::max (q, 0.05f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        set (0.5f * (1.0f + cw), -(1.0f + cw), 0.5f * (1.0f + cw),
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    void peaking (float fs, float f, float q, float db) noexcept
    {
        f = clamp (f, 20.0f, fs * 0.45f);
        q = std::max (q, 0.05f);
        const float A = std::pow (10.0f, db / 40.0f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        set (1.0f + alpha * A, -2.0f * cw, 1.0f - alpha * A,
             1.0f + alpha / A, -2.0f * cw, 1.0f - alpha / A);
    }

    void notch (float fs, float f, float q) noexcept
    {
        f = clamp (f, 20.0f, fs * 0.45f);
        q = std::max (q, 0.05f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        set (1.0f, -2.0f * cw, 1.0f,
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    void bandpass (float fs, float f, float q) noexcept
    {
        // Constant 0 dB peak gain
        f = clamp (f, 20.0f, fs * 0.45f);
        q = std::max (q, 0.05f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / (2.0f * q);
        set (alpha, 0.0f, -alpha,
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    void lowshelf (float fs, float f, float db) noexcept
    {
        f = clamp (f, 20.0f, fs * 0.45f);
        const float A = std::pow (10.0f, db / 40.0f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw * 0.5f * std::sqrt (2.0f);
        const float sA = std::sqrt (A);
        set (A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sA * alpha),
             2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw),
             A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sA * alpha),
             (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sA * alpha,
             -2.0f * ((A - 1.0f) + (A + 1.0f) * cw),
             (A + 1.0f) + (A - 1.0f) * cw - 2.0f * sA * alpha);
    }

    void highshelf (float fs, float f, float db) noexcept
    {
        f = clamp (f, 20.0f, fs * 0.45f);
        const float A = std::pow (10.0f, db / 40.0f);
        const float w = kTwoPi * f / fs;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw * 0.5f * std::sqrt (2.0f);
        const float sA = std::sqrt (A);
        set (A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sA * alpha),
             -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw),
             A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sA * alpha),
             (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sA * alpha,
             2.0f * ((A - 1.0f) - (A + 1.0f) * cw),
             (A + 1.0f) - (A - 1.0f) * cw - 2.0f * sA * alpha);
    }
};

struct OnePole
{
    float z = 0.0f;
    float a = 0.0f;

    void reset() noexcept { z = 0.0f; }

    void setHz (float fs, float hz) noexcept
    {
        hz = clamp (hz, 1.0f, fs * 0.45f);
        a = 1.0f - std::exp (-kTwoPi * hz / fs);
    }

    float process (float x) noexcept
    {
        z += a * (x - z);
        return z;
    }
};

struct DCBlock
{
    float xm1 = 0.0f, ym1 = 0.0f, R = 0.995f;

    void reset() noexcept { xm1 = ym1 = 0.0f; }

    void prepare (float fs) noexcept
    {
        R = 1.0f - 18.0f / std::max (fs, 1000.0f);
    }

    float process (float x) noexcept
    {
        const float y = x - xm1 + R * ym1;
        xm1 = x;
        ym1 = y;
        return y;
    }
};

struct Smooth
{
    float y = 0.0f;
    float t = 0.0f;
    float c = 0.001f;

    void setTime (float seconds, float fs) noexcept
    {
        seconds = std::max (seconds, 0.0005f);
        c = 1.0f - std::exp (-1.0f / (seconds * fs));
    }

    void snap (float v) noexcept { y = t = v; }
    void setTarget (float v) noexcept { t = v; }

    float tick() noexcept
    {
        y += c * (t - y);
        return y;
    }

    float current() const noexcept { return y; }
};

// Parameter IDs (stable, match JUCE APVTS):
// Gate, Tight, Gain, Chainsaw, Industrial, Mids, Presence, Output
//
// Defaults already sit in Static-X x Gojira x chainsaw territory.
// Knobs are trim around that voice.

struct Params
{
    float gate        = 0.65f;
    float tight       = 0.70f;
    float gain        = 0.72f;
    float chainsaw    = 0.45f;
    float industrial  = 0.15f;
    float mids        = 0.62f;
    float presence    = 0.55f;
    float output      = 0.50f;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params& p); // audio-thread, sets smoother targets
    void process (float* left, float* right, int numSamples);

    float gateOpen() const noexcept { return gateGain_; }
    float lastPeak() const noexcept { return peak_; }

private:
    float processSample (float x);
    float waveshape (float x) const noexcept;
    float chebyshevBuzz (float x) const noexcept;
    void  updateFilters();
    void  designOversampler();
    float oversampledShape (float x);

    double fs_ = 48000.0;
    int    maxBlock_ = 512;
    bool   useOS_ = true;
    int    coeffCounter_ = 0;

    Params target_{};
    Smooth sGate_, sTight_, sGain_, sSaw_, sInd_, sMids_, sPres_, sOut_;

    // Tightness
    Biquad hpf1_, hpf2_, lowShelf_;

    // Amp pre / post
    Biquad preMid_, preShelf_, postLp1_, postLp2_;

    // Mids / presence (user)
    Biquad midEq_, presenceEq_;

    // Chainsaw band 1.2–4.5 kHz
    Biquad sawHp_, sawLp_;

    // Industrial resonance
    Biquad indPeak_;

    // Cab (original design — biquads, not a ripped IR)
    Biquad cabLp1_, cabLp2_, cabNotch1_, cabNotch2_, cabBody_, cabAir_;

    DCBlock dc_;
    OnePole envFast_;   // pick envelope for industrial (side path only)
    OnePole envGateAtk_;
    OnePole envGateRel_;

    float gateEnv_  = 0.0f;
    float gateGain_ = 0.0f;
    int   gateHold_ = 0;
    int   gateHoldLen_ = 1;
    float gateAtk_ = 0.5f;
    float gateRel_ = 0.05f;
    float gateClose_ = 0.1f;

    float indPhase_ = 0.0f;
    float indInc_   = 0.0f;

    float peak_ = 0.0f;
    float peakDecay_ = 0.99f;

    // 2x oversampler (windowed-sinc FIR, zero-insert / decimate)
    static constexpr int kOsN = 32;
    float osC_[kOsN] {};
    float osUpZ_[kOsN] {};
    float osDnZ_[kOsN] {};
    int   osUpI_ = 0;
    int   osDnI_ = 0;
    float osPrev_ = 0.0f;
};
} // namespace sawvi
