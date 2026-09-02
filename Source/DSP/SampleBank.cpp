#include "DSP/SampleBank.h"
#include "SessionSettings.h"

#if __has_include("SessionSamples.h")
 #include "SessionSamples.h"
 #define FTB_HAS_SAMPLES 1
#else
 #define FTB_HAS_SAMPLES 0
#endif

const char* SampleBank::slotFile (int slot)
{
    static const char* n[] = {
        "drums/acoustic_kick.wav",
        "drums/acoustic_snare.wav",
        "drums/acoustic_hat.wav",
        "drums/funk_kick.wav",
        "drums/funk_snare.wav",
        "drums/funk_hat.wav",
        "drums/metal_kick.wav",
        "drums/metal_snare.wav",
        "drums/metal_hat.wav",
        "drums/crash.wav",
        "bass/pluck.wav",
        "keys/hammer.wav",
        "fx/impact_metal.wav",
        "fx/impact_plate.wav",
        "fx/impact_bell.wav",
        "fx/boom.wav",
        "fx/zap.wav",
        "fx/hit.wav",
        "fx/clap.wav",
        "fx/foley_wood.wav",
        "fx/foley_glass.wav"
    };
    if (slot < 0 || slot >= NumBundled) return "";
    return n[slot];
}

juce::File SampleBank::bundledDir()
{
    auto tryDir = [] (juce::File d) -> juce::File
    {
        if (d.getChildFile ("drums").isDirectory() || d.getChildFile ("LICENSE.txt").existsAsFile())
            return d;
        return {};
    };

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    if (auto d = tryDir (exe.getChildFile ("Assets").getChildFile ("Samples")); d.exists())
        return d;
    if (auto d = tryDir (exe.getParentDirectory().getChildFile ("Assets").getChildFile ("Samples")); d.exists())
        return d;

    auto cwd = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 6; ++i)
    {
        if (auto d = tryDir (cwd.getChildFile ("Assets").getChildFile ("Samples")); d.exists())
            return d;
        cwd = cwd.getParentDirectory();
    }
    return {};
}

juce::File SampleBank::userFxDir()
{
    auto d = SessionSettings::productDir().getChildFile ("samples").getChildFile ("fx");
    d.createDirectory();
    return d;
}

void SampleBank::prepare (double sr)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    for (auto& v : voices)
        v = {};
    voiceWrite = 0;
    loadAll();
}

void SampleBank::release()
{
    for (auto& v : voices)
        v = {};
}

void SampleBank::loadAll()
{
    for (int s = 0; s < NumBundled; ++s)
        tryLoadSlot (s);
    scanUserFx();
}

void SampleBank::tryLoadSlot (int slot)
{
    auto& dest = bundled[(size_t) slot];
    dest.ready.store (0, std::memory_order_release);

#if FTB_HAS_SAMPLES
    {
        juce::String res = juce::File (slotFile (slot)).getFileName();
        res = res.replaceCharacter ('-', '_').replaceCharacter ('.', '_');
        int sz = 0;
        if (const char* data = SessionSamples::getNamedResource (res.toRawUTF8(), sz))
        {
            auto* mis = new juce::MemoryInputStream (data, (size_t) sz, false);
            if (loadWavStream (dest, mis))
                return;
        }
    }
#endif

    const auto dir = bundledDir();
    if (dir.exists())
    {
        const auto f = dir.getChildFile (slotFile (slot));
        if (loadWavFile (dest, f))
            return;
    }
}

bool SampleBank::loadWavFile (Buf& dest, const juce::File& f)
{
    if (! f.existsAsFile())
        return false;
    return loadWavStream (dest, f.createInputStream().release());
}

bool SampleBank::loadWavStream (Buf& dest, juce::InputStream* in)
{
    if (in == nullptr)
        return false;
    juce::AudioFormatManager fm;
    fm.registerFormat (new juce::WavAudioFormat(), true);
    fm.registerFormat (new juce::AiffAudioFormat(), false);
#if JUCE_USE_FLAC
    fm.registerFormat (new juce::FlacAudioFormat(), false);
#endif
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::unique_ptr<juce::InputStream> (in)));
    if (reader == nullptr)
        return false;

    const int n = (int) juce::jmin ((juce::int64) reader->lengthInSamples, (juce::int64) (reader->sampleRate * 4.0));
    if (n <= 8)
        return false;

    juce::AudioBuffer<float> tmp ((int) reader->numChannels, n);
    reader->read (&tmp, 0, n, 0, true, true);

    dest.nativeSr = reader->sampleRate > 1.0 ? reader->sampleRate : 44100.0;
    dest.mono.assign ((size_t) n, 0.0f);
    const int ch = tmp.getNumChannels();
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < ch; ++c)
            s += tmp.getSample (c, i);
        dest.mono[(size_t) i] = s / (float) juce::jmax (1, ch);
    }
    dest.n = n;
    dest.ready.store (1, std::memory_order_release);
    return true;
}

void SampleBank::scanUserFx()
{
    for (auto& u : user)
        u.ready.store (0, std::memory_order_release);

    int count = 0;
    auto dir = userFxDir();
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*");
    files.sort();
    for (auto& f : files)
    {
        if (count >= kMaxUser)
            break;
        if (! f.hasFileExtension ("wav;aif;aiff;flac"))
            continue;
        if (loadWavFile (user[(size_t) count], f))
            ++count;
    }
    userCountAtom.store (count, std::memory_order_release);
}

bool SampleBank::isReady (int slot) const noexcept
{
    if (slot < 0 || slot >= NumBundled)
        return false;
    return bundled[(size_t) slot].ready.load (std::memory_order_acquire) != 0;
}

void SampleBank::startVoice (const Buf* src, float gain, float rate, int bus) noexcept
{
    if (src == nullptr || src->ready.load (std::memory_order_acquire) == 0 || src->n <= 0)
        return;
    Voice& v = voices[(size_t) (voiceWrite++ & (kVoices - 1))];
    v.src = src;
    v.pos = 0.0;
    v.rate = juce::jlimit (0.25, 4.0, (double) rate * (src->nativeSr / juce::jmax (1.0, sampleRate)));
    v.gain = juce::jlimit (0.0f, 2.0f, gain);
    v.active = 1;
    v.bus = bus;
}

void SampleBank::play (int slot, float gain, float rate, int bus) noexcept
{
    if (slot < 0 || slot >= NumBundled)
        return;
    startVoice (&bundled[(size_t) slot], gain, rate, bus);
}

void SampleBank::playUser (int index, float gain, float rate, int bus) noexcept
{
    if (index < 0 || index >= userCountAtom.load (std::memory_order_relaxed))
        return;
    startVoice (&user[(size_t) index], gain, rate, bus);
}

void SampleBank::mix (int bus, float& l, float& r) noexcept
{
    float acc = 0.0f;
    for (auto& v : voices)
    {
        if (v.active == 0 || v.src == nullptr || v.bus != bus)
            continue;
        const int n = v.src->n;
        const double p = v.pos;
        const int i0 = (int) p;
        if (i0 < 0 || i0 >= n - 1)
        {
            v.active = 0;
            continue;
        }
        const float frac = (float) (p - (double) i0);
        const float a = v.src->mono[(size_t) i0];
        const float b = v.src->mono[(size_t) (i0 + 1)];
        acc += (a + (b - a) * frac) * v.gain;
        v.pos += v.rate;
        if (v.pos >= (double) (n - 1))
            v.active = 0;
    }
    l += acc;
    r += acc;
}
