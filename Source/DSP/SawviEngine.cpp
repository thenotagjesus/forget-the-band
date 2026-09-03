#include "DSP/SawviEngine.h"

#include <cstring>

namespace sawvi
{
void Engine::prepare (double sampleRate, int maxBlockSize)
{
    fs_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
    maxBlock_ = std::max (maxBlockSize, 16);
    useOS_ = fs_ < 88000.0;

    const float fs = (float) fs_;

    sGate_.setTime  (0.018f, fs);
    sTight_.setTime (0.030f, fs);
    sGain_.setTime  (0.020f, fs);
    sSaw_.setTime   (0.022f, fs);
    sInd_.setTime   (0.025f, fs);
    sMids_.setTime  (0.025f, fs);
    sPres_.setTime  (0.025f, fs);
    sOut_.setTime   (0.018f, fs);

    sGate_.snap  (target_.gate);
    sTight_.snap (target_.tight);
    sGain_.snap  (target_.gain);
    sSaw_.snap   (target_.chainsaw);
    sInd_.snap   (target_.industrial);
    sMids_.snap  (target_.mids);
    sPres_.snap  (target_.presence);
    sOut_.snap   (target_.output);

    gateAtk_     = 1.0f - std::exp (-1.0f / (0.00028f * fs)); // ~0.28 ms, keep pick
    gateRel_     = 1.0f - std::exp (-1.0f / (0.048f   * fs)); // 48 ms
    gateClose_   = 1.0f - std::exp (-1.0f / (0.011f   * fs)); // 11 ms close
    gateHoldLen_ = std::max (1, (int) (0.026f * fs));         // 26 ms hold

    envFast_.setHz (fs, 900.0f); // fast follower for industrial resonance only
    dc_.prepare (fs);
    indInc_ = kTwoPi * 37.0f / fs; // mechanical AM ~37 Hz
    peakDecay_ = std::exp (-1.0f / (0.18f * fs));

    designOversampler();
    updateFilters();
    reset();
}

void Engine::reset()
{
    hpf1_.reset(); hpf2_.reset(); lowShelf_.reset();
    preMid_.reset(); preShelf_.reset(); postLp1_.reset(); postLp2_.reset();
    midEq_.reset(); presenceEq_.reset();
    sawHp_.reset(); sawLp_.reset();
    indPeak_.reset();
    cabLp1_.reset(); cabLp2_.reset(); cabNotch1_.reset(); cabNotch2_.reset();
    cabBody_.reset(); cabAir_.reset();
    dc_.reset();
    envFast_.reset();
    gateEnv_ = 0.0f;
    gateGain_ = 0.0f;
    gateHold_ = 0;
    indPhase_ = 0.0f;
    peak_ = 0.0f;
    osUpI_ = osDnI_ = 0;
    osPrev_ = 0.0f;
    std::memset (osUpZ_, 0, sizeof (osUpZ_));
    std::memset (osDnZ_, 0, sizeof (osDnZ_));
    coeffCounter_ = 0;
}

void Engine::setParams (const Params& p)
{
    target_.gate       = clamp (p.gate,       0.0f, 1.0f);
    target_.tight      = clamp (p.tight,      0.0f, 1.0f);
    target_.gain       = clamp (p.gain,       0.0f, 1.0f);
    target_.chainsaw   = clamp (p.chainsaw,   0.0f, 1.0f);
    target_.industrial = clamp (p.industrial, 0.0f, 1.0f);
    target_.mids       = clamp (p.mids,       0.0f, 1.0f);
    target_.presence   = clamp (p.presence,   0.0f, 1.0f);
    target_.output     = clamp (p.output,     0.0f, 1.0f);

    sGate_.setTarget  (target_.gate);
    sTight_.setTarget (target_.tight);
    sGain_.setTarget  (target_.gain);
    sSaw_.setTarget   (target_.chainsaw);
    sInd_.setTarget   (target_.industrial);
    sMids_.setTarget  (target_.mids);
    sPres_.setTarget  (target_.presence);
    sOut_.setTarget   (target_.output);
}

void Engine::designOversampler()
{
    // Hamming-windowed sinc lowpass, cutoff 0.25 of the 2x rate (= original Nyquist).
    float sum = 0.0f;
    for (int i = 0; i < kOsN; ++i)
    {
        const float k  = (float) i - (float) (kOsN - 1) * 0.5f;
        const float fc = 0.25f;
        const float sinc = (std::fabs (k) < 1.0e-6f)
                             ? (2.0f * fc)
                             : std::sin (kTwoPi * fc * k) / (kPi * k);
        const float w = 0.54f - 0.46f * std::cos (kTwoPi * (float) i / (float) (kOsN - 1));
        osC_[i] = sinc * w;
        sum += osC_[i];
    }
    for (int i = 0; i < kOsN; ++i)
        osC_[i] /= sum;
}

void Engine::updateFilters()
{
    const float fs    = (float) fs_;
    const float tight = sTight_.current();
    const float mids  = sMids_.current();
    const float pres  = sPres_.current();
    const float ind   = sInd_.current();

    // Tightness: 4th-order HPF + low shelf.
    // Bass VI low E is ~41 Hz. Default tight=0.70 -> HPF ~80 Hz, shelf ~-6.5 dB @ 185 Hz.
    // Distortion harmonics restore perceived weight without the flub.
    const float hpfHz   = 40.0f + tight * 85.0f;          // 40–125 Hz
    const float shelfDb = -0.8f - tight * 8.2f;           // -0.8 to -9 dB
    hpf1_.highpass (fs, hpfHz, 0.707f);
    hpf2_.highpass (fs, hpfHz, 0.707f);
    lowShelf_.lowshelf (fs, 185.0f, shelfDb);

    // Amp pre-emphasis: Gojira mid-forward, pick stays on the audio path (no slow env).
    preMid_.peaking (fs, 820.0f, 0.85f, 3.8f);
    preShelf_.highshelf (fs, 1350.0f, 7.2f);

    postLp1_.lowpass (fs, 7200.0f, 0.70f);
    postLp2_.lowpass (fs, 8800.0f, 0.55f);

    // User mids: 0 = -8 dB, 0.5 = 0 dB, 1 = +8 dB around 900 Hz
    midEq_.peaking (fs, 900.0f, 0.78f, (mids - 0.5f) * 16.0f);

    // Chainsaw band
    sawHp_.highpass (fs, 1200.0f, 0.75f);
    sawLp_.lowpass  (fs, 4500.0f, 0.75f);

    // Industrial resonant mid (amount follows a FAST envelope in processSample)
    const float q = 2.4f + ind * 3.6f;
    indPeak_.peaking (fs, 1120.0f, q, 1.5f + ind * 7.0f);

    // Cab / speaker: original biquad stack, not a commercial IR.
    cabLp1_.lowpass (fs, 5300.0f, 0.72f);
    cabLp2_.lowpass (fs, 7600.0f, 0.50f);
    cabNotch1_.peaking (fs, 390.0f, 2.6f, -7.5f);  // body cancellation
    cabNotch2_.peaking (fs, 740.0f, 3.2f, -4.8f);
    cabBody_.peaking   (fs, 240.0f, 1.05f, 2.2f);  // small cab thump
    cabAir_.highshelf  (fs, 9200.0f, -6.5f);

    // Presence: speaker presence peak ~3.15 kHz
    presenceEq_.peaking (fs, 3150.0f, 1.12f, -2.0f + pres * 9.5f);
}

float Engine::waveshape (float x) const noexcept
{
    // Asymmetric high-gain shaper through the origin: f(0)=0, f(-x) != -f(x)
    // => odd (square/synth) + even (thickness). No DC offset added.
    const float g = (x >= 0.0f) ? 1.22f : 0.84f;
    const float t = fastTanh (g * x);
    // Extra flattening toward a mechanical square without a slow envelope.
    const float sq = t * (1.38f - 0.38f * t * t);
    return 0.58f * t + 0.42f * sq;
}

float Engine::chebyshevBuzz (float x) const noexcept
{
    // Odd Chebyshev + x|x| even term. All terms vanish at 0.
    x = fastTanh (x * 2.4f);
    const float x2 = x * x;
    const float T3 = (4.0f * x2 - 3.0f) * x;
    const float T5 = ((16.0f * x2 - 20.0f) * x2 + 5.0f) * x;
    const float even = x * std::fabs (x);
    return 0.28f * x + 0.40f * T3 + 0.18f * T5 + 0.20f * even;
}

float Engine::oversampledShape (float x)
{
    if (! useOS_)
        return waveshape (x);

    auto firUp = [this]() -> float
    {
        float s = 0.0f;
        int j = osUpI_;
        for (int k = 0; k < kOsN; ++k)
        {
            j = (j + kOsN - 1) % kOsN;
            s += osC_[k] * osUpZ_[j];
        }
        return s;
    };
    auto firDn = [this]() -> float
    {
        float s = 0.0f;
        int j = osDnI_;
        for (int k = 0; k < kOsN; ++k)
        {
            j = (j + kOsN - 1) % kOsN;
            s += osC_[k] * osDnZ_[j];
        }
        return s;
    };

    // Zero-insert upsample (gain * 2), shape, lowpass, decimate.
    osUpZ_[osUpI_] = x;
    osUpI_ = (osUpI_ + 1) % kOsN;
    const float u0 = firUp() * 2.0f;

    osUpZ_[osUpI_] = 0.0f;
    osUpI_ = (osUpI_ + 1) % kOsN;
    const float u1 = firUp() * 2.0f;

    osDnZ_[osDnI_] = waveshape (u0);
    osDnI_ = (osDnI_ + 1) % kOsN;
    (void) firDn();

    osDnZ_[osDnI_] = waveshape (u1);
    osDnI_ = (osDnI_ + 1) % kOsN;
    return firDn();
}

float Engine::processSample (float x)
{
    const float gateAmt = sGate_.tick();
    const float tight   = sTight_.tick();
    const float gain    = sGain_.tick();
    const float sawAmt  = sSaw_.tick();
    const float indAmt  = sInd_.tick();
    const float mids    = sMids_.tick();
    const float pres    = sPres_.tick();
    const float outAmt  = sOut_.tick();
    (void) tight; (void) mids; (void) pres;

    // --- Tightness ---
    x = hpf1_.process (x);
    x = hpf2_.process (x);
    x = lowShelf_.process (x);

    // --- Noise gate (Static-X chop). Fast open so the pick is not eaten. ---
    const float absx = std::fabs (x);
    if (absx > gateEnv_)
    {
        gateEnv_ += gateAtk_ * (absx - gateEnv_);
        gateHold_ = gateHoldLen_;
    }
    else if (gateHold_ > 0)
    {
        --gateHold_;
    }
    else
    {
        gateEnv_ += gateRel_ * (absx - gateEnv_);
    }

    float gTarget = 1.0f;
    if (gateAmt > 0.02f)
    {
        // 0 -> ~-70 dB, 1 -> ~-22 dB. Default 0.65 ~ -39 dB, gated chug.
        const float threshOpen  = dbToLin (-70.0f + gateAmt * 48.0f);
        const float threshClose = threshOpen * 0.52f;
        const bool  isOpen      = gateGain_ > 0.45f;
        const float th          = isOpen ? threshClose : threshOpen;
        gTarget = (gateEnv_ > th) ? 1.0f : 0.0f;
    }

    const float gCoeff = (gTarget > gateGain_) ? gateAtk_ : gateClose_;
    gateGain_ += gCoeff * (gTarget - gateGain_);
    x *= gateGain_;

    // Split after gate: amp core + parallel chainsaw
    const float gated = x;

    // --- Amp ---
    x = preMid_.process (x);
    x = preShelf_.process (x);

    // Drive: default 0.72 is high-gain industrial, 0 is already a bit of grind.
    const float drive = 3.2f + gain * gain * 52.0f;
    x = oversampledShape (x * drive);
    x *= 0.38f; // makeup so the cab/limiter sit in a sane range

    x = postLp1_.process (x);
    x = postLp2_.process (x);

    // --- Chainsaw: BP 1.2–4.5 kHz, extra Chebyshev saturation ---
    if (sawAmt > 0.001f)
    {
        float s = gated * (4.0f + gain * 10.0f);
        s = sawHp_.process (s);
        s = sawLp_.process (s);
        s = chebyshevBuzz (s);
        x += sawAmt * 0.55f * s;
    }

    // --- Industrial: AM + fast-envelope resonant mid. 0 = hard off. ---
    if (indAmt > 0.001f)
    {
        indPhase_ += indInc_;
        if (indPhase_ > kTwoPi)
            indPhase_ -= kTwoPi;
        const float am = 1.0f + indAmt * 0.13f * std::sin (indPhase_);
        x *= am;

        const float env = envFast_.process (std::fabs (gated));
        const float res = indPeak_.process (x);
        x += res * (indAmt * 0.22f * clamp (env * 8.0f, 0.0f, 1.0f));
    }

    // --- Tone stack ---
    x = midEq_.process (x);

    // --- Cab / speaker ---
    x = cabBody_.process (x);
    x = cabNotch1_.process (x);
    x = cabNotch2_.process (x);
    x = cabLp1_.process (x);
    x = cabLp2_.process (x);
    x = presenceEq_.process (x);
    x = cabAir_.process (x);

    x = dc_.process (x);

    // Output trim ±12 dB around unity
    x *= dbToLin ((outAmt - 0.5f) * 24.0f);

    // Safety limiter / soft clip (through origin)
    x = fastTanh (x * 1.08f);

    x = flush (x);

    const float ax = std::fabs (x);
    peak_ = (ax > peak_) ? ax : peak_ * peakDecay_;

    return x;
}

void Engine::process (float* left, float* right, int numSamples)
{
    if (left == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        float in = left[i];
        if (right != nullptr && right != left)
            in = 0.5f * (in + right[i]);

        const float y = processSample (in);
        left[i] = y;
        if (right != nullptr && right != left)
            right[i] = y;

        if (++coeffCounter_ >= 64)
        {
            coeffCounter_ = 0;
            updateFilters();
        }
    }
}
} // namespace sawvi
