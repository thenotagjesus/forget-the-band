#include "DSP/DrumEngine.h"
#include <JuceHeader.h>

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;
}

void DrumEngine::prepare (double sampleRate, SampleBank* bank)
{
    sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    invSr = 1.0f / (float) sr;
    samples = bank;
    const float fsr = (float) sr;
    kHp.setHighPass (fsr, 30.0f, 0.70f);
    kClickHp.setHighPass (fsr, 2200.0f, 0.75f);
    sBp.setPeaking (fsr, 2100.0f, 0.80f, 6.5f);
    sSnapHp.setHighPass (fsr, 4500.0f, 0.72f);
    hHp.setHighPass (fsr, 6200.0f, 0.70f);
    tHp.setHighPass (fsr, 55.0f, 0.70f);
    setKit (kit);
    reset();
}

void DrumEngine::reset() noexcept
{
    kHp.reset(); kClickHp.reset(); sBp.reset(); sSnapHp.reset(); hHp.reset(); tHp.reset();
    kEnv = kClick = 0;
    kPhase = kSubPhase = 0;
    sEnv = sToneEnv = sSnap = 0;
    sPhase = 0;
    hClosed = hOpen = 0;
    hPhaseA = hPhaseB = 0;
    tEnv.fill (0); tHz.fill (140.0f); tPhase.fill (0); tDec.fill (0.99915f);
    tFloorHz = { 220.0f, 140.0f, 90.0f };
    rEnv = 0;
    rPh.fill (0);
    cEnv = 0;
    cPh.fill (0);
    envFollow = 0;
}

void DrumEngine::setKit (int k) noexcept
{
    kit = juce::jlimit (0, 4, k);
    const float fsr = (float) sr;
    switch (kit)
    {
        case 1: // Metal — tighter, more click, 55 Hz floor
            kHp.setHighPass (fsr, 48.0f, 0.75f);
            kClickHp.setHighPass (fsr, 2800.0f, 0.78f);
            sBp.setPeaking (fsr, 2650.0f, 0.90f, 8.5f);
            sSnapHp.setHighPass (fsr, 5200.0f, 0.75f);
            hHp.setHighPass (fsr, 8200.0f, 0.76f);
            kFloor = 55.0f;
            kRate = 0.99770f;
            kDec = 0.99725f;
            kClickAmt = 0.58f;
            kSubAmt = 0.42f;
            kClickDec = 0.90f;
            sToneHz = 220.0f;
            sToneAmt = 0.28f;
            sSnapAmt = 0.72f;
            sGrit = 0.38f;
            sDec = 0.99815f;
            hHzA = 8600.0f; hHzB = 11200.0f;
            hClosedDec = 0.9905f; hOpenDec = 0.9986f;
            rDec = 0.9984f;
            makeup = 1.95f;
            break;
        case 2: // Jazz — rounder, less click
            kHp.setHighPass (fsr, 26.0f, 0.70f);
            kClickHp.setHighPass (fsr, 1600.0f, 0.70f);
            sBp.setPeaking (fsr, 1250.0f, 0.70f, 4.0f);
            sSnapHp.setHighPass (fsr, 3200.0f, 0.68f);
            hHp.setHighPass (fsr, 4800.0f, 0.65f);
            kFloor = 46.0f;
            kRate = 0.99918f;
            kDec = 0.99952f;
            kClickAmt = 0.12f;
            kSubAmt = 0.62f;
            kClickDec = 0.955f;
            sToneHz = 180.0f;
            sToneAmt = 0.22f;
            sSnapAmt = 0.28f;
            sGrit = 0.0f;
            sDec = 0.99912f;
            hHzA = 6400.0f; hHzB = 9100.0f;
            hClosedDec = 0.9955f; hOpenDec = 0.99935f;
            rDec = 0.99912f;
            makeup = 1.70f;
            break;
        case 3: // Funk — drier, more snap
            kHp.setHighPass (fsr, 32.0f, 0.70f);
            kClickHp.setHighPass (fsr, 2400.0f, 0.74f);
            sBp.setPeaking (fsr, 2100.0f, 0.85f, 5.5f);
            sSnapHp.setHighPass (fsr, 5600.0f, 0.78f);
            hHp.setHighPass (fsr, 7000.0f, 0.72f);
            kFloor = 50.0f;
            kRate = 0.99855f;
            kDec = 0.99835f;
            kClickAmt = 0.40f;
            kSubAmt = 0.48f;
            kClickDec = 0.91f;
            sToneHz = 200.0f;
            sToneAmt = 0.18f;
            sSnapAmt = 0.82f;
            sGrit = 0.08f;
            sDec = 0.99755f;
            hHzA = 7600.0f; hHzB = 10800.0f;
            hClosedDec = 0.9922f; hOpenDec = 0.9989f;
            rDec = 0.9985f;
            makeup = 1.80f;
            break;
        case 4: // Electro
            kHp.setHighPass (fsr, 36.0f, 0.72f);
            kClickHp.setHighPass (fsr, 2600.0f, 0.76f);
            sBp.setPeaking (fsr, 1800.0f, 0.90f, 7.0f);
            sSnapHp.setHighPass (fsr, 4800.0f, 0.74f);
            hHp.setHighPass (fsr, 9000.0f, 0.80f);
            kFloor = 50.0f;
            kRate = 0.99805f;
            kDec = 0.99785f;
            kClickAmt = 0.45f;
            kSubAmt = 0.70f;
            kClickDec = 0.88f;
            sToneHz = 188.0f;
            sToneAmt = 0.08f;
            sSnapAmt = 0.60f;
            sGrit = 0.22f;
            sDec = 0.9974f;
            hHzA = 9200.0f; hHzB = 12100.0f;
            hClosedDec = 0.9895f; hOpenDec = 0.9982f;
            rDec = 0.9980f;
            makeup = 1.88f;
            break;
        default: // Acoustic
            kHp.setHighPass (fsr, 30.0f, 0.70f);
            kClickHp.setHighPass (fsr, 2200.0f, 0.75f);
            sBp.setPeaking (fsr, 1900.0f, 0.75f, 6.0f);
            sSnapHp.setHighPass (fsr, 4500.0f, 0.72f);
            hHp.setHighPass (fsr, 6200.0f, 0.70f);
            kFloor = 48.0f;
            kRate = 0.99885f;
            kDec = 0.99935f;
            kClickAmt = 0.32f;
            kSubAmt = 0.55f;
            kClickDec = 0.935f;
            sToneHz = 200.0f;
            sToneAmt = 0.42f;
            sSnapAmt = 0.50f;
            sGrit = 0.0f;
            sDec = 0.99905f;
            hHzA = 7800.0f; hHzB = 10400.0f;
            hClosedDec = 0.9938f; hOpenDec = 0.99918f;
            rDec = 0.99888f;
            makeup = 1.85f;
            break;
    }
}

float DrumEngine::noise() noexcept
{
    rng = rng * 1664525u + 1013904223u;
    return (float) ((int) (rng >> 8) & 0xffff) / 32768.0f - 1.0f;
}

float DrumEngine::oscSin (float& phase, float hz) noexcept
{
    phase += hz * invSr;
    phase -= std::floor (phase);
    return std::sin (kTwoPi * phase);
}

int DrumEngine::kickSlot() const noexcept
{
    if (kit == 3) return SampleBank::KickFunk;
    if (kit == 1) return SampleBank::KickMetal;
    return SampleBank::KickAcoustic;
}

int DrumEngine::snareSlot() const noexcept
{
    if (kit == 3) return SampleBank::SnareFunk;
    if (kit == 1) return SampleBank::SnareMetal;
    return SampleBank::SnareAcoustic;
}

int DrumEngine::hatSlot() const noexcept
{
    if (kit == 3) return SampleBank::HatFunk;
    if (kit == 1) return SampleBank::HatMetal;
    return SampleBank::HatAcoustic;
}

void DrumEngine::trigKick (float vel) noexcept
{
    vel = juce::jlimit (0.0f, 1.0f, vel);
    kEnv = juce::jmax (kEnv, vel);
    switch (kit)
    {
        case 1:  kHz = 220.0f; break; // Metal
        case 2:  kHz = 160.0f; break; // Jazz
        case 3:  kHz = 190.0f; break; // Funk
        case 4:  kHz = 205.0f; break; // Electro
        default: kHz = 185.0f; break; // Acoustic 160–220 start
    }
    kClick = juce::jmax (kClick, vel * kClickAmt);
    kPhase = 0;
    kSubPhase = 0;
    if (samples != nullptr && samples->isReady (kickSlot()))
        samples->play (kickSlot(), vel * 0.70f, 1.0f, 0);
}

void DrumEngine::trigSnare (float vel, bool ghost) noexcept
{
    vel = juce::jlimit (0.0f, 1.0f, vel);
    const float v = ghost ? vel * 0.22f : vel;
    sEnv = juce::jmax (sEnv, v);
    sToneEnv = juce::jmax (sToneEnv, v);
    sSnap = juce::jmax (sSnap, v * sSnapAmt);
    sToneDec = ghost ? 0.9945f : 0.9986f;
    if (ghost)
        sDec = 0.9964f;
    else if (kit == 1)
        sDec = 0.99815f;
    else if (kit == 3)
        sDec = 0.99755f;
    else
        sDec = 0.99905f;
    sPhase = 0;
    if (samples != nullptr && samples->isReady (snareSlot()))
        samples->play (snareSlot(), v * 0.70f, 1.0f, 0);
}

void DrumEngine::trigHat (float vel, bool open) noexcept
{
    vel = juce::jlimit (0.0f, 1.0f, vel);
    if (open)
    {
        hOpen = juce::jmax (hOpen, vel);
        hClosed *= 0.22f;
    }
    else
    {
        hClosed = juce::jmax (hClosed, vel);
        hOpen *= 0.50f; // closed chokes open
    }
    if (samples != nullptr && samples->isReady (hatSlot()))
        samples->play (hatSlot(), vel * 0.70f, 1.0f, 0);
}

void DrumEngine::trigTom (int which, float vel) noexcept
{
    which = juce::jlimit (0, 2, which);
    vel = juce::jlimit (0.0f, 1.0f, vel);
    // start Hz (pitch drop) → body floor ~220 / 140 / 90
    static constexpr float startHz[3] = { 280.0f, 190.0f, 130.0f };
    static constexpr float floorHz[3] = { 220.0f, 140.0f,  90.0f };
    tEnv[(size_t) which] = juce::jmax (tEnv[(size_t) which], vel);
    tHz[(size_t) which] = startHz[which];
    tFloorHz[(size_t) which] = floorHz[which];
    tDec[(size_t) which] = 0.99912f;
    tPhase[(size_t) which] = 0;
}

void DrumEngine::trigRide (float vel) noexcept
{
    vel = juce::jlimit (0.0f, 1.0f, vel);
    rEnv = juce::jmax (rEnv, vel);
}

void DrumEngine::trigCrash (float vel) noexcept
{
    vel = juce::jlimit (0.0f, 1.0f, vel);
    hClosed = 0.0f;
    hOpen *= 0.08f; // choke hats
    cEnv = juce::jmax (cEnv, vel);
    cPh.fill (0);
    if (samples != nullptr && samples->isReady (SampleBank::Crash))
        samples->play (SampleBank::Crash, vel * 0.70f, 1.0f, 0);
}

void DrumEngine::render (float& left, float& right) noexcept
{
    // --- kick ---
    kHz *= kRate;
    if (kHz < kFloor) kHz = kFloor;
    const float body = oscSin (kPhase, kHz);
    const float sub  = oscSin (kSubPhase, kHz * 0.5f);
    const float click = kClickHp.process (noise()) * kClick;
    kClick *= kClickDec;
    kEnv *= kDec;
    float kick = (body * 0.88f + sub * kSubAmt) * kEnv + click;
    kick = kHp.process (kick);

    // --- snare ---
    const float snN = sBp.process (noise());
    float sn = snN * sEnv;
    sn += sToneAmt * oscSin (sPhase, sToneHz) * sToneEnv;
    sn += sSnapHp.process (noise()) * sSnap;
    if (sGrit > 0.001f)
        sn = sn + sGrit * std::tanh (sn * 2.4f);
    sEnv *= sDec;
    sToneEnv *= sToneDec;
    sSnap *= sSnapDec;

    // --- hats (two envelopes, stereo width slightly L) ---
    const float hN = hHp.process (noise());
    const float hM = oscSin (hPhaseA, hHzA) + 0.72f * oscSin (hPhaseB, hHzB);
    const float closed = (hN * 0.78f + hM * 0.32f) * hClosed;
    const float opened = (hN * 0.52f + hM * 0.58f) * hOpen;
    hClosed *= hClosedDec;
    hOpen   *= hOpenDec;
    const float hats = closed + opened;

    // --- toms ---
    float toms = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        tHz[(size_t) i] *= 0.99925f;
        if (tHz[(size_t) i] < tFloorHz[(size_t) i])
            tHz[(size_t) i] = tFloorHz[(size_t) i];
        const float t = (oscSin (tPhase[(size_t) i], tHz[(size_t) i]) * 0.88f
                         + noise() * 0.12f) * tEnv[(size_t) i];
        tEnv[(size_t) i] *= tDec[(size_t) i];
        toms += t;
    }
    toms = tHp.process (toms);

    // --- ride: inharmonic partials, not a single 1540 Hz sine ---
    rEnv *= rDec;
    const float r0 = oscSin (rPh[0],  920.0f);
    const float r1 = oscSin (rPh[1], 1260.0f); // 1.37x
    const float r2 = oscSin (rPh[2], 1620.0f); // 1.76x
    const float r3 = oscSin (rPh[3], 2217.0f); // 2.41x
    const float ride = (0.34f * r0 + 0.24f * r1 + 0.18f * r2 + 0.12f * r3
                        + 0.16f * hN) * rEnv;

    // --- crash ---
    cEnv *= cDec;
    const float c0 = oscSin (cPh[0], 420.0f);
    const float c1 = oscSin (cPh[1], 680.0f);
    const float c2 = oscSin (cPh[2], 1350.0f);
    const float crash = (hN * 0.72f + 0.18f * c0 + 0.14f * c1 + 0.10f * c2) * cEnv;

    float sampL = 0.0f, sampR = 0.0f;
    if (samples != nullptr)
        samples->mix (0, sampL, sampR);

    // Synth body ~0.45 under sample ~0.7 (already in play() gain).
    const float bodyAmt = 0.45f;
    float l = kick * 0.95f * bodyAmt
            + sn   * 0.82f * bodyAmt
            + hats * 1.10f * bodyAmt
            + toms * 0.90f * bodyAmt
            + ride * 0.88f * bodyAmt
            + crash * 0.80f * bodyAmt
            + sampL;
    float r = kick * 0.95f * bodyAmt
            + sn   * 1.12f * bodyAmt
            + hats * 0.86f * bodyAmt
            + toms * 0.90f * bodyAmt
            + ride * 0.92f * bodyAmt
            + crash * 0.84f * bodyAmt
            + sampR;

    // One-pole envelope follower compressor: thresh 0.35, ratio 2.5
    const float mono = 0.5f * (std::abs (l) + std::abs (r));
    const float coef = (mono > envFollow) ? 0.018f : 0.004f;
    envFollow += (mono - envFollow) * coef;
    float gr = 1.0f;
    if (envFollow > 0.35f)
    {
        const float compressed = 0.35f + (envFollow - 0.35f) / 2.5f;
        gr = compressed / envFollow;
    }
    l *= gr * makeup;
    r *= gr * makeup;
    l = std::tanh (l);
    r = std::tanh (r);

    left  = l;
    right = r;
}
