#include "DSP/Arrangement.h"
#include <cmath>
#include <cstring>
#include <initializer_list>

void Arrangement::resetDefault() noexcept
{
    nSections = 4;

    auto fill = [] (Section& s, const char* n, int bars, int diff, std::initializer_list<int> ch)
    {
        std::memset (s.name, 0, sizeof (s.name));
        std::strncpy (s.name, n, sizeof (s.name) - 1);
        s.bars = bars;
        s.difficulty = diff;
        s.nChords = 0;
        for (int c : ch)
        {
            if (s.nChords >= kMaxChords)
                break;
            s.chords[s.nChords++] = c;
        }
    };

    fill (sections[0], "Intro", 4, 2,
          { FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegI });
    fill (sections[1], "Jam", 8, 2,
          { FollowerBand::DegI, FollowerBand::DegbVII, FollowerBand::DegIV, FollowerBand::DegI,
            FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegV, FollowerBand::DegI });
    fill (sections[2], "Break", 4, 1,
          { FollowerBand::DegIV, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegI });
    fill (sections[3], "Outro", 4, 1,
          { FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegI });

    followDeg.store (0);
    followConf.store (0.0f);
    phaseNudge.store (0.0);
    thin.store (0x7);
    pendingDeg = committedDeg = 0;
    pendingHops = 0;
    committedScore = 0.0f;
}

void Arrangement::prepare (double sr) noexcept
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
}

int Arrangement::currentSection (int absBar) const noexcept
{
    if (nSections <= 0)
        return 0;
    int span = 0;
    for (int i = 0; i < nSections; ++i)
        span += juce::jmax (1, sections[(size_t) i].bars);
    if (span <= 0)
        return 0;
    int b = ((absBar % span) + span) % span;
    for (int i = 0; i < nSections; ++i)
    {
        const int n = juce::jmax (1, sections[(size_t) i].bars);
        if (b < n)
            return i;
        b -= n;
    }
    return 0;
}

int Arrangement::sectionChord (int absBar) const noexcept
{
    const int si = currentSection (absBar);
    const auto& s = sections[(size_t) si];
    if (s.nChords <= 0)
        return -1;
    int span = 0;
    for (int i = 0; i < nSections; ++i)
        span += juce::jmax (1, sections[(size_t) i].bars);
    int b = ((absBar % juce::jmax (1, span)) + span) % span;
    for (int i = 0; i < si; ++i)
        b -= juce::jmax (1, sections[(size_t) i].bars);
    const int idx = ((b % s.nChords) + s.nChords) % s.nChords;
    return s.chords[idx];
}

int Arrangement::getDifficulty (int absBar) const noexcept
{
    return sections[(size_t) currentSection (absBar)].difficulty;
}

int Arrangement::scoreTemplates (const float chroma[12], int keyPc, int style,
                                 int& bestDeg, bool& bestMinor) const noexcept
{
    struct Deg { int deg; int st; bool minorHint; };
    const Deg table[] = {
        { FollowerBand::DegI,    0,  false },
        { FollowerBand::DegIV,   5,  false },
        { FollowerBand::DegV,    7,  false },
        { FollowerBand::Degvi,   9,  true  },
        { FollowerBand::DegbIII, 3,  true  },
        { FollowerBand::DegbVI,  8,  false },
        { FollowerBand::DegbVII, 10, false },
        { FollowerBand::Degii,   2,  true  }
    };

    float energy = 1.0e-8f;
    for (int i = 0; i < 12; ++i)
        energy += juce::jmax (0.0f, chroma[i]);

    bestDeg = FollowerBand::DegI;
    bestMinor = false;
    float best = -1.0f;

    const bool metal = (style == (int) FollowerBand::Style::Metal);
    const bool bluesy = (style == (int) FollowerBand::Style::Blues)
                     || (style == (int) FollowerBand::Style::Funk);

    for (const auto& d : table)
    {
        const int root = ((keyPc + d.st) % 12 + 12) % 12;
        const bool minor = metal ? (d.deg == FollowerBand::DegI || d.minorHint)
                                 : d.minorHint;
        const int third = (root + (minor ? 3 : 4)) % 12;
        const int fifth = (root + 7) % 12;
        const int seventh = (root + (bluesy || style == (int) FollowerBand::Style::Jazz ? 10
                                    : (minor ? 10 : 11))) % 12;

        float v = chroma[root] * 1.55f
                + chroma[third] * 1.15f
                + chroma[fifth] * 0.90f
                + chroma[seventh] * 0.55f;
        // Penalize competing roots a little.
        v -= chroma[(root + 1) % 12] * 0.15f;
        v /= energy;
        if (v > best)
        {
            best = v;
            bestDeg = d.deg;
            bestMinor = minor;
        }
    }
    juce::ignoreUnused (bestMinor);
    return bestDeg;
}

void Arrangement::tick (const float chroma[12],
                        bool onset,
                        int keyPc,
                        int style,
                        int /*form*/,
                        int absBar,
                        int stepInBar,
                        float intensity,
                        double stepAccum,
                        double samplesPer16th) noexcept
{
    int bestDeg = 0;
    bool minor = false;
    scoreTemplates (chroma, keyPc, style, bestDeg, minor);

    float energy = 0.0f;
    for (int i = 0; i < 12; ++i)
        energy += juce::jmax (0.0f, chroma[i]);
    const float conf = juce::jlimit (0.0f, 1.0f, energy * 0.35f);

    // Hysteresis: do not panic-modulate. Hold until a run of hops agrees,
    // and require the new chord to beat the committed one.
    if (bestDeg == pendingDeg)
        ++pendingHops;
    else
    {
        pendingDeg = bestDeg;
        pendingHops = 0;
    }

    const bool barLock = (stepInBar <= 1 || stepInBar >= 14);
    if (conf > 0.12f && pendingHops >= 8
        && (pendingDeg != committedDeg)
        && (conf > committedScore * 1.12f + 0.04f || pendingHops >= 24))
    {
        committedDeg = pendingDeg;
        committedScore = conf;
    }
    if (barLock && conf > 0.18f && pendingHops >= 4)
    {
        committedDeg = pendingDeg;
        committedScore = conf;
    }

    // Player sitting on IV must win over a canned I–bVII–IV bar of I.
    const int chart = sectionChord (absBar);
    int outDeg = committedDeg;
    float outConf = committedScore;
    if (outConf < 0.20f && chart >= 0)
    {
        outDeg = chart;
        outConf = 0.20f;
    }

    followDeg.store (outDeg, std::memory_order_relaxed);
    followConf.store (outConf, std::memory_order_relaxed);

    // Timing: lock 16th phase to player onsets. Elastic stepAccum, max slew
    // applied by FollowerBand::applyPhaseNudge. Never replace BPM here.
    // Real pick only — hiss/false aubio onsets must not yank the 16th grid.
    if (onset && intensity > 0.28f && samplesPer16th > 32.0)
    {
        const double toPrev = stepAccum;
        const double toNext = stepAccum - samplesPer16th;
        const double nearest = (std::abs (toPrev) < std::abs (toNext)) ? toPrev : toNext;
        // Onset should land on the grid. Pull accum toward 0 or samplesPer16th.
        phaseNudge.store (-nearest, std::memory_order_relaxed);
    }

    // User lobby chairs already gate drums/bass/keys via setMemberEnabled.
    // Difficulty/intensity are a hint only — never mute seated members.
    thin.store (0x7, std::memory_order_relaxed);
}
