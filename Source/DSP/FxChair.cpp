#include "DSP/FxChair.h"
#include <cmath>

const char* FxChair::voiceName (int i)
{
    static const char* n[] = { "Auto", "Hits", "Risers", "Foley" };
    if (i < 0 || i >= (int) Voice::NumVoices) return "Auto";
    return n[i];
}

void FxChair::prepare (double sr, SampleBank* b)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    bank = b;
    const float fsr = (float) sampleRate;
    hp.setHighPass (fsr, 420.0f, 0.70f);
    lp.setLowPass (fsr, 6500.0f, 0.70f);
    reset();
}

void FxChair::reset() noexcept
{
    hp.reset();
    lp.reset();
    for (auto& g : gens) g = {};
    lastStep = -1;
    lastAbsBar = -1;
    fade = 0.0f;
}

float FxChair::noise() noexcept
{
    rng = rng * 1664525u + 1013904223u;
    return (float) ((int) (rng >> 8) & 0xffff) / 32768.0f - 1.0f;
}

void FxChair::trigger (GenKind k, float vel) noexcept
{
    if (k == GenNone)
        return;
    Gen* slot = &gens[0];
    for (auto& g : gens)
    {
        if (g.kind == GenNone) { slot = &g; break; }
        if (g.age > slot->age) slot = &g;
    }
    slot->kind = k;
    slot->env = juce::jlimit (0.15f, 1.0f, vel);
    slot->env2 = 0.0f;
    slot->phase = 0.0f;
    slot->age = 0;
    slot->noise = 0.0f;
    const float sr = (float) sampleRate;
    switch (k)
    {
        case GenRiser:   slot->hz = 220.0f; slot->life = (int) (sr * 0.85f); slot->env = 0.02f; slot->env2 = vel; break;
        case GenReverse: slot->hz = 4800.0f; slot->life = (int) (sr * 0.70f); slot->env = 0.01f; slot->env2 = vel; break;
        case GenBoom:    slot->hz = 88.0f;  slot->life = (int) (sr * 0.55f); break;
        case GenTape:    slot->hz = 330.0f; slot->life = (int) (sr * 0.45f); break;
        case GenZap:     slot->hz = 2100.0f; slot->life = (int) (sr * 0.18f); break;
        default: break;
    }
}

void FxChair::triggerSampler (Voice vox, float inten, bool fill, bool crash, bool phrase, int step) noexcept
{
    if (bank == nullptr)
        return;
    const float g = 0.35f + 0.55f * inten;
    auto want = [&] (Voice v) { return vox == Voice::Auto || vox == v; };

    const bool ind = industrial.load (std::memory_order_relaxed) != 0;
    if (crash && want (Voice::Hits))
    {
        bank->play (SampleBank::Crash, g * 0.55f, 1.0f, 1);
        bank->play (SampleBank::FxBoom, g * (ind ? 0.88f : 0.70f), 1.0f, 1);
        bank->play (ind ? SampleBank::FxImpactMetal : SampleBank::FxHit,
                    g * (ind ? 0.70f : 0.45f), 1.0f, 1);
    }
    if (fill && step >= 12 && want (Voice::Risers))
        bank->play (SampleBank::FxImpactBell, g * 0.40f, 0.85f, 1);
    if (phrase && want (Voice::Hits))
        bank->play (ind ? SampleBank::FxImpactMetal : SampleBank::FxImpactPlate,
                    g * (ind ? 0.62f : 0.50f), 1.0f, 1);
    if (crash && want (Voice::Foley) && ! ind)
        bank->play (SampleBank::FxFoleyGlass, g * 0.40f, 1.0f, 1);
    if (ind && crash && want (Voice::Hits))
        bank->play (SampleBank::FxBoom, g * 0.40f, 0.85f, 1);

    const int uc = bank->userCount();
    if (uc > 0 && (crash || (fill && step == 14) || (inten > 0.72f && step == 0)))
        bank->playUser ((int) (rng % (uint32_t) uc), g * 0.55f, 1.0f, 1);
}

void FxChair::renderGen (float& l, float& r) noexcept
{
    float acc = 0.0f;
    for (auto& g : gens)
    {
        if (g.kind == GenNone)
            continue;
        ++g.age;
        const float t = (float) g.age / (float) juce::jmax (1, g.life);
        float s = 0.0f;
        switch (g.kind)
        {
            case GenRiser:
            {
                g.env += (g.env2 - g.env) * 0.00035f;
                g.hz *= 1.00028f;
                g.phase += g.hz / (float) sampleRate;
                g.phase -= std::floor (g.phase);
                const float n = hp.process (noise());
                s = (0.55f * n + 0.45f * std::sin (juce::MathConstants<float>::twoPi * g.phase)) * g.env * t;
                break;
            }
            case GenReverse:
            {
                g.env += (g.env2 - g.env) * 0.0004f;
                const float n = hp.process (noise());
                const float rise = t * t;
                s = n * g.env * rise * (t < 0.92f ? 1.0f : (1.0f - (t - 0.92f) / 0.08f));
                break;
            }
            case GenBoom:
            {
                g.hz *= 0.99955f;
                if (g.hz < 32.0f) g.hz = 32.0f;
                g.phase += g.hz / (float) sampleRate;
                g.phase -= std::floor (g.phase);
                g.env *= 0.99955f;
                s = std::sin (juce::MathConstants<float>::twoPi * g.phase) * g.env
                  + 0.22f * noise() * g.env * g.env;
                break;
            }
            case GenTape:
            {
                g.hz *= 0.99972f;
                g.phase += g.hz / (float) sampleRate;
                g.phase -= std::floor (g.phase);
                g.env *= 0.9994f;
                s = (0.7f * std::sin (juce::MathConstants<float>::twoPi * g.phase)
                   + 0.3f * noise()) * g.env;
                break;
            }
            case GenZap:
            {
                g.hz *= 0.9978f;
                g.phase += g.hz / (float) sampleRate;
                g.phase -= std::floor (g.phase);
                g.env *= 0.9975f;
                const float fm = std::sin (juce::MathConstants<float>::twoPi * g.phase * 1.7f);
                s = std::sin (juce::MathConstants<float>::twoPi * g.phase + fm * 1.4f) * g.env;
                break;
            }
            default: break;
        }
        acc += s;
        if (g.age >= g.life || g.env < 0.0008f)
            g = {};
    }
    acc = lp.process (acc);
    l += acc;
    r += acc;
}

void FxChair::process (int step16,
                       int absBar,
                       int phraseBars,
                       bool fillBar,
                       bool crashDownbeat,
                       float intensity,
                       float* left, float* right,
                       int numSamples) noexcept
{
    const bool run = enabled.load (std::memory_order_relaxed) != 0;
    const auto vox = (Voice) juce::jlimit (0, (int) Voice::NumVoices - 1,
                                           voice.load (std::memory_order_relaxed));
    const float inten = juce::jlimit (0.0f, 1.0f, intensity);
    const int ph = juce::jmax (4, phraseBars);
    const bool phrase = (absBar > 0 && (absBar % ph) == 0 && step16 == 0);

    if (run && step16 != lastStep)
    {
        lastStep = step16;
        const bool newBar = (absBar != lastAbsBar);
        lastAbsBar = absBar;

        auto want = [&] (Voice v) { return vox == Voice::Auto || vox == v; };
        const float spice = 0.04f + 0.22f * inten;

        if (crashDownbeat && step16 == 0 && want (Voice::Hits))
            trigger (GenBoom, 0.55f + 0.35f * inten);
        if (fillBar && step16 == 8 && want (Voice::Risers))
            trigger (GenRiser, 0.45f + 0.4f * inten);
        if (fillBar && step16 == 12 && want (Voice::Risers))
            trigger (GenReverse, 0.40f + 0.4f * inten);
        if (phrase && want (Voice::Hits))
            trigger (inten > 0.7f ? GenTape : GenZap, 0.40f + 0.35f * inten);
        if (step16 == 0 && inten > 0.78f && want (Voice::Hits) && (rng & 3) == 0)
            trigger (GenBoom, 0.28f + 0.25f * inten);
        if (step16 == 0 && inten > 0.62f && want (Voice::Foley) && (rng & 7) < 2)
        {
            if (bank) bank->play (SampleBank::FxFoleyWood, 0.22f + 0.25f * inten, 1.0f, 1);
        }
        if (inten > 0.55f && (step16 == 4 || step16 == 12) && want (Voice::Hits))
        {
            const float r = 0.5f + 0.5f * noise();
            if (r < spice)
                trigger (GenZap, 0.22f + 0.3f * inten);
        }

        triggerSampler (vox, inten, fillBar, crashDownbeat && step16 == 0, phrase, step16);
        juce::ignoreUnused (newBar);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        fade += ((run ? 1.0f : 0.0f) - fade) * 0.0009f;
        float l = 0.0f, r = 0.0f;
        renderGen (l, r);
        if (bank)
            bank->mix (1, l, r);
        const float g = fade * 0.85f;
        left[i]  = l * g;
        right[i] = r * g;
    }
}
