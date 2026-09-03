#include "Analysis/AubioEngine.h"
#include <cmath>
#include <cstdint>

bool AubioEngine::prepare (double sampleRate, int hopSize, int winSize)
{
    release();
    const uint32_t sr = (uint32_t) juce::jlimit (8000.0, 192000.0, sampleRate);
    hop = juce::jmax (64, hopSize);
    win = juce::jmax (hop * 2, winSize);
    fill = 0;
    hopFlag.store (0);
    onsetFlag.store (0);
    hz.store (0.0f);
    conf.store (0.0f);
    rmsLin.store (0.0f);

#if defined(FTB_HAS_AUBIO) && FTB_HAS_AUBIO
    hopVec.reset (new_fvec ((uint_t) hop));
    onsetOut.reset (new_fvec (1));
    pitchOut.reset (new_fvec (1));
    if (hopVec == nullptr || onsetOut == nullptr || pitchOut == nullptr)
    {
        release();
        return false;
    }

    onset.reset (new_aubio_onset ("specflux", (uint_t) win, (uint_t) hop, sr));
    if (onset == nullptr)
        onset.reset (new_aubio_onset ("hfc", (uint_t) win, (uint_t) hop, sr));
    if (onset == nullptr)
        onset.reset (new_aubio_onset ("default", (uint_t) win, (uint_t) hop, sr));

    pitch.reset (new_aubio_pitch ("yinfft", (uint_t) win, (uint_t) hop, sr));
    if (pitch == nullptr)
        pitch.reset (new_aubio_pitch ("yin", (uint_t) win, (uint_t) hop, sr));

    if (onset == nullptr || pitch == nullptr)
    {
        release();
        return false;
    }

    aubio_onset_set_threshold (onset.get(), 0.35f);
    aubio_onset_set_minioi_ms (onset.get(), 160.0f);

    aubio_pitch_set_unit (pitch.get(), "Hz");
    aubio_pitch_set_silence (pitch.get(), -50.0f);

    ready.store (1, std::memory_order_relaxed);
    return true;
#else
    juce::ignoreUnused (sr);
    ready.store (0);
    return false;
#endif
}

void AubioEngine::release()
{
    ready.store (0, std::memory_order_relaxed);
#if defined(FTB_HAS_AUBIO) && FTB_HAS_AUBIO
    pitch.reset();
    onset.reset();
    pitchOut.reset();
    onsetOut.reset();
    hopVec.reset();
#endif
    fill = 0;
}

void AubioEngine::process (const float* x, int n) noexcept
{
#if defined(FTB_HAS_AUBIO) && FTB_HAS_AUBIO
    if (ready.load (std::memory_order_relaxed) == 0 || x == nullptr || n <= 0)
        return;
    if (hopVec == nullptr || onset == nullptr || pitch == nullptr)
        return;

    bool anyHop = false;
    bool anyOnset = false;
    for (int i = 0; i < n; ++i)
    {
        hopVec->data[fill++] = x[i];
        if (fill < hop)
            continue;
        fill = 0;

        aubio_onset_do (onset.get(), hopVec.get(), onsetOut.get());
        aubio_pitch_do (pitch.get(), hopVec.get(), pitchOut.get());

        float sum2 = 0.0f;
        for (int k = 0; k < hop; ++k)
        {
            const float s = hopVec->data[k];
            sum2 += s * s;
        }
        const float rms = std::sqrt (sum2 / (float) juce::jmax (1, hop));

        const float pitchHz = pitchOut->data[0];
        const float pitchConf = aubio_pitch_get_confidence (pitch.get());
        const bool onsetHit = onsetOut->data[0] > 0.0f;

        hz.store (pitchHz, std::memory_order_relaxed);
        conf.store (juce::jlimit (0.0f, 1.0f, pitchConf), std::memory_order_relaxed);
        rmsLin.store (rms, std::memory_order_relaxed);
        anyHop = true;
        if (onsetHit)
            anyOnset = true;
    }

    if (anyHop)
    {
        onsetFlag.store (anyOnset ? 1 : 0, std::memory_order_relaxed);
        hopFlag.store (1, std::memory_order_relaxed);
    }
#else
    juce::ignoreUnused (x, n);
#endif
}
