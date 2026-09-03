#pragma once

#include "DSP/Biquad.h"
#include "DSP/SampleBank.h"
#include <array>
#include <cmath>
#include <cstdint>

/**
 * RT-safe drum kit: independent kick / snare / hats / toms / ride / crash
 * plus a glue bus. SampleBank one-shots layer on top of the synth body
 * (sample ~0.7, body ~0.45). Missing files still sound like a kit.
 * Audio thread: trig* + render only. No alloc, no locks.
 */
class DrumEngine
{
public:
    void prepare (double sampleRate, SampleBank* bank);
    void reset() noexcept;
    void setKit (int kit) noexcept; // FollowerBand::DrumKit as int

    void trigKick (float vel) noexcept;
    void trigSnare (float vel, bool ghost = false) noexcept;
    void trigHat (float vel, bool open) noexcept;
    void trigTom (int which, float vel) noexcept; // 0 high, 1 mid, 2 floor
    void trigRide (float vel) noexcept;
    void trigCrash (float vel) noexcept;

    /** One sample. Mixes SampleBank bus 0 + synth, then tanh + compressor. */
    void render (float& left, float& right) noexcept;

private:
    float noise() noexcept;
    float oscSin (float& phase, float hz) noexcept;
    int kickSlot() const noexcept;
    int snareSlot() const noexcept;
    int hatSlot() const noexcept;

    SampleBank* samples = nullptr;
    double sr = 44100.0;
    float invSr = 1.0f / 44100.0f;
    int kit = 0;
    uint32_t rng = 0xC0FFEE01u;

    // Kick: sine body + pitch drop + sub + HP beater click
    float kEnv = 0, kDec = 0.99935f;
    float kHz = 185.0f, kFloor = 48.0f, kRate = 0.99885f;
    float kPhase = 0, kSubPhase = 0;
    float kClick = 0, kClickDec = 0.935f, kClickAmt = 0.32f;
    float kSubAmt = 0.55f;
    Biquad kHp, kClickHp;

    // Snare: 180–220 Hz tone + BP noise + short HP snap
    float sEnv = 0, sToneEnv = 0, sSnap = 0;
    float sDec = 0.99905f, sToneDec = 0.9986f, sSnapDec = 0.91f;
    float sPhase = 0, sToneHz = 200.0f;
    float sGrit = 0.0f, sSnapAmt = 0.50f, sToneAmt = 0.42f;
    Biquad sBp, sSnapHp;

    // Hats: closed vs open as two envelopes; metallic HP noise + two sines
    float hClosed = 0, hOpen = 0;
    float hClosedDec = 0.9938f, hOpenDec = 0.99918f;
    float hPhaseA = 0, hPhaseB = 0;
    float hHzA = 7800.0f, hHzB = 10400.0f;
    float hatSampleAge = 1.0e9f;
    Biquad hHp;

    // Toms: high / mid / floor with pitch drop
    std::array<float, 3> tEnv {}, tHz {}, tPhase {}, tDec {}, tFloorHz {};
    Biquad tHp;

    // Ride: inharmonic partials + noise
    float rEnv = 0, rDec = 0.99888f;
    std::array<float, 4> rPh {};

    // Crash: long noise + partials; chokes hats
    float cEnv = 0, cDec = 0.99974f;
    std::array<float, 3> cPh {};

    // Drum bus
    float envFollow = 0.0f;
    float makeup = 1.85f;
};
