#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

/**
 * Bundled CC0 one-shots + user FX drops.
 * load() / prepare() / scanUserFx() are off the audio thread.
 * Audio thread only: isReady, play, playUser, mix.
 */
class SampleBank
{
public:
    enum Slot : int
    {
        KickAcoustic = 0,
        SnareAcoustic,
        HatAcoustic,
        KickFunk,
        SnareFunk,
        HatFunk,
        KickMetal,
        SnareMetal,
        HatMetal,
        Crash,
        BassPluck,
        KeysHammer,
        FxImpactMetal,
        FxImpactPlate,
        FxImpactBell,
        FxBoom,
        FxZap,
        FxHit,
        FxClap,
        FxFoleyWood,
        FxFoleyGlass,
        KeysStrings,
        NumBundled
    };

    static constexpr int kMaxUser = 24;
    static constexpr int kVoices = 16;

    void prepare (double sampleRate);
    void release();

    /** Off audio thread. Reload bundled files + user FX folder. */
    void loadAll();
    void scanUserFx();

    bool isReady (int slot) const noexcept;
    /** One-line kit status for the UI ("Kit samples ready" / missing kick). */
    juce::String statusLine() const;
    /** bus 0 = band (drums/bass/keys), bus 1 = FX chair. Audio thread. */
    void play (int slot, float gain, float rate = 1.0f, int bus = 0) noexcept;
    void playUser (int index, float gain, float rate = 1.0f, int bus = 1) noexcept;
    void mix (int bus, float& l, float& r) noexcept;

    int userCount() const noexcept { return userCountAtom.load (std::memory_order_relaxed); }

    static const char* slotFile (int slot);
    static juce::File bundledDir();
    static juce::File userFxDir();

private:
    struct Buf
    {
        std::vector<float> mono;
        double nativeSr = 44100.0;
        int n = 0;
        std::atomic<int> ready { 0 };
    };

    struct Voice
    {
        const Buf* src = nullptr;
        double pos = 0.0;
        double rate = 1.0;
        float gain = 0.0f;
        int active = 0;
        int bus = 0;
    };

    bool loadWavFile (Buf& dest, const juce::File& f);
    bool loadWavStream (Buf& dest, juce::InputStream* in);
    bool loadFromBinary (Buf& dest, const char* slotRel);
    void startVoice (const Buf* src, float gain, float rate, int bus) noexcept;
    void tryLoadSlot (int slot);

    double sampleRate = 44100.0;
    std::array<Buf, NumBundled> bundled {};
    std::array<Buf, kMaxUser> user {};
    std::array<Voice, kVoices> voices {};
    std::atomic<int> userCountAtom { 0 };
    int voiceWrite = 0;
};
