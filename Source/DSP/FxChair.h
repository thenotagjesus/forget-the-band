#pragma once

#include "DSP/SampleBank.h"
#include "DSP/Biquad.h"
#include <array>
#include <atomic>
#include <cstdint>

/** Fourth chair: sampler + original RT-safe SFX generator. Mute when empty. */
class FxChair
{
public:
    enum class Voice : int { Auto = 0, Hits, Risers, Foley, NumVoices };

    void prepare (double sampleRate, SampleBank* bank);
    void reset() noexcept;

    void setEnabled (bool e) noexcept { enabled.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed) != 0; }
    void setVoice (Voice v) noexcept { voice.store ((int) v, std::memory_order_relaxed); }
    Voice getVoice() const noexcept { return (Voice) voice.load (std::memory_order_relaxed); }
    void setIndustrial (bool e) noexcept { industrial.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isIndustrial() const noexcept { return industrial.load (std::memory_order_relaxed) != 0; }

    void reloadUser() { if (bank) bank->scanUserFx(); }

    /** Audio thread. Uses band step/fill/phrase + intensity. */
    void process (int step16,
                  int absBar,
                  int phraseBars,
                  bool fillBar,
                  bool crashDownbeat,
                  float intensity,
                  float* left, float* right,
                  int numSamples) noexcept;

    static const char* voiceName (int i);

private:
    enum GenKind : int { GenNone = 0, GenRiser, GenReverse, GenBoom, GenTape, GenZap };

    struct Gen
    {
        GenKind kind = GenNone;
        float env = 0, env2 = 0, phase = 0, hz = 0, noise = 0;
        int age = 0;
        int life = 0;
    };

    float noise() noexcept;
    void trigger (GenKind k, float vel) noexcept;
    void triggerSampler (Voice vox, float inten, bool fill, bool crash, bool phrase, int step) noexcept;
    void renderGen (float& l, float& r) noexcept;

    SampleBank* bank = nullptr;
    std::atomic<int> enabled { 0 };
    std::atomic<int> voice { (int) Voice::Auto };
    std::atomic<int> industrial { 0 };
    double sampleRate = 44100.0;

    std::array<Gen, 4> gens {};
    int lastStep = -1;
    int lastAbsBar = -1;
    uint32_t rng = 0xA5F10Bu;
    float fade = 0.0f;
    Biquad hp, lp;
};
