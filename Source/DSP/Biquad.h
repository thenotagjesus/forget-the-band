#pragma once

#include <cmath>
#include <algorithm>

/** Allocation-free biquad. Coefficients are plain floats — safe to recompute on the audio thread. */
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

    void setLowPass (float sr, float hz, float q) noexcept
    {
        const float w = 2.0f * 3.14159265358979323846f * std::max (10.0f, hz) / sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float a = s / (2.0f * std::max (0.1f, q));
        const float b0n = (1.0f - c) * 0.5f;
        const float b1n = 1.0f - c;
        const float b2n = (1.0f - c) * 0.5f;
        const float a0n = 1.0f + a;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a;
        b0 = b0n / a0n; b1 = b1n / a0n; b2 = b2n / a0n;
        a1 = a1n / a0n; a2 = a2n / a0n;
    }

    void setHighPass (float sr, float hz, float q) noexcept
    {
        const float w = 2.0f * 3.14159265358979323846f * std::max (10.0f, hz) / sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float a = s / (2.0f * std::max (0.1f, q));
        const float b0n = (1.0f + c) * 0.5f;
        const float b1n = -(1.0f + c);
        const float b2n = (1.0f + c) * 0.5f;
        const float a0n = 1.0f + a;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a;
        b0 = b0n / a0n; b1 = b1n / a0n; b2 = b2n / a0n;
        a1 = a1n / a0n; a2 = a2n / a0n;
    }

    void setPeaking (float sr, float hz, float q, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * 3.14159265358979323846f * std::max (10.0f, hz) / sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float a = s / (2.0f * std::max (0.1f, q));
        const float b0n = 1.0f + a * A;
        const float b1n = -2.0f * c;
        const float b2n = 1.0f - a * A;
        const float a0n = 1.0f + a / A;
        const float a1n = -2.0f * c;
        const float a2n = 1.0f - a / A;
        b0 = b0n / a0n; b1 = b1n / a0n; b2 = b2n / a0n;
        a1 = a1n / a0n; a2 = a2n / a0n;
    }

    void setLowShelf (float sr, float hz, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * 3.14159265358979323846f * std::max (10.0f, hz) / sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float beta = std::sqrt (A) * s;
        const float b0n =    A * ((A + 1.0f) - (A - 1.0f) * c + beta);
        const float b1n =  2.0f * A * ((A - 1.0f) - (A + 1.0f) * c);
        const float b2n =    A * ((A + 1.0f) - (A - 1.0f) * c - beta);
        const float a0n =        ((A + 1.0f) + (A - 1.0f) * c + beta);
        const float a1n = -2.0f *     ((A - 1.0f) + (A + 1.0f) * c);
        const float a2n =        ((A + 1.0f) + (A - 1.0f) * c - beta);
        b0 = b0n / a0n; b1 = b1n / a0n; b2 = b2n / a0n;
        a1 = a1n / a0n; a2 = a2n / a0n;
    }

    void setHighShelf (float sr, float hz, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * 3.14159265358979323846f * std::max (10.0f, hz) / sr;
        const float c = std::cos (w);
        const float s = std::sin (w);
        const float beta = std::sqrt (A) * s;
        const float b0n =    A * ((A + 1.0f) + (A - 1.0f) * c + beta);
        const float b1n = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c);
        const float b2n =    A * ((A + 1.0f) + (A - 1.0f) * c - beta);
        const float a0n =        ((A + 1.0f) - (A - 1.0f) * c + beta);
        const float a1n =  2.0f *     ((A - 1.0f) - (A + 1.0f) * c);
        const float a2n =        ((A + 1.0f) - (A - 1.0f) * c - beta);
        b0 = b0n / a0n; b1 = b1n / a0n; b2 = b2n / a0n;
        a1 = a1n / a0n; a2 = a2n / a0n;
    }
};

struct OnePole
{
    float a = 0.05f;
    float z = 0.0f;

    void setLP (float sr, float hz) noexcept
    {
        const float x = std::exp (-2.0f * 3.14159265358979323846f * std::max (1.0f, hz) / sr);
        a = 1.0f - x;
    }

    void reset() noexcept { z = 0.0f; }

    float process (float x) noexcept
    {
        z += a * (x - z);
        return z;
    }
};
