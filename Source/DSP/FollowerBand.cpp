#include "DSP/FollowerBand.h"
#include <initializer_list>

namespace
{
    // Original 4-bar rock: mixolydian I – bVII – IV – I
    constexpr int kRock[]  = { FollowerBand::DegI, FollowerBand::DegbVII, FollowerBand::DegIV, FollowerBand::DegI };
    // 12-bar blues (public-domain form)
    constexpr int kBlues[] = {
        FollowerBand::DegI,  FollowerBand::DegI,  FollowerBand::DegI,  FollowerBand::DegI,
        FollowerBand::DegIV, FollowerBand::DegIV, FollowerBand::DegI,  FollowerBand::DegI,
        FollowerBand::DegV,  FollowerBand::DegIV, FollowerBand::DegI,  FollowerBand::DegV
    };
    // Aeolian metal: i – bVI – bIII – bVII
    constexpr int kMetal[] = { FollowerBand::DegI, FollowerBand::DegbVI, FollowerBand::DegbIII, FollowerBand::DegbVII };
    // Funk: I – IV – I – V (dominant flavour)
    constexpr int kFunk[]  = { FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegbVII };
    // Jazz 12-bar: I I I I | IV IV | I I | ii V | I V
    constexpr int kJazz[]  = {
        FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegI,
        FollowerBand::DegIV, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegI,
        FollowerBand::Degii, FollowerBand::DegV, FollowerBand::DegI, FollowerBand::DegV
    };
    constexpr int kSongRock[]  = {
        FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegI,
        FollowerBand::DegV, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegI
    };
    constexpr int kSongFunk[]  = {
        FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegI, FollowerBand::DegbVII,
        FollowerBand::DegI, FollowerBand::DegIV, FollowerBand::DegV, FollowerBand::DegI
    };
    constexpr int kSongMetal[] = {
        FollowerBand::DegI, FollowerBand::DegI, FollowerBand::DegbVI, FollowerBand::DegI,
        FollowerBand::DegbIII, FollowerBand::DegbVII, FollowerBand::DegI, FollowerBand::DegI
    };
}

const char* FollowerBand::formName (int i)
{
    static const char* n[] = { "Vamp", "Song", "12-Bar", "Wander" };
    if (i < 0 || i >= (int) Form::NumForms) return "Song";
    return n[i];
}

const char* FollowerBand::playerFormName (int i)
{
    static const char* n[] = { "Jam", "Radio", "12-Bar", "Changes" };
    if (i < 0 || i >= (int) Form::NumForms) return "Radio";
    return n[i];
}

const char* FollowerBand::scaleName (int i)
{
    static const char* n[] = { "Major", "Minor", "Pentatonic", "Blues" };
    if (i < 0 || i >= (int) Scale::NumScales) return "Pentatonic";
    return n[i];
}

const char* FollowerBand::feelName (int i)
{
    static const char* n[] = { "Grid", "Ahead", "Behind", "Swing" };
    if (i < 0 || i >= (int) Feel::NumFeels) return "Grid";
    return n[i];
}

bool FollowerBand::scaleHas (Scale sc, int interval) noexcept
{
    interval = ((interval % 12) + 12) % 12;
    switch (sc)
    {
        case Scale::Major:      return interval == 0 || interval == 2 || interval == 4
                                    || interval == 5 || interval == 7 || interval == 9 || interval == 11;
        case Scale::Minor:      return interval == 0 || interval == 2 || interval == 3
                                    || interval == 5 || interval == 7 || interval == 8 || interval == 10;
        case Scale::Pentatonic: return interval == 0 || interval == 3 || interval == 5
                                    || interval == 7 || interval == 10;
        case Scale::Blues:      return interval == 0 || interval == 3 || interval == 5
                                    || interval == 6 || interval == 7 || interval == 10;
        default: return true;
    }
}

juce::String FollowerBand::namedChord (int keyPc, int deg, Scale sc, Style st)
{
    static const char* pcN[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int stn = (deg == DegI ? 0 : deg == DegIV ? 5 : deg == DegV ? 7
                   : deg == Degvi ? 9 : deg == DegbIII ? 3 : deg == DegbVI ? 8
                   : deg == DegbVII ? 10 : deg == Degii ? 2 : 0);
    const int root = ((keyPc + stn) % 12 + 12) % 12;
    juce::String s (pcN[root]);
    const bool minor = (sc == Scale::Minor || sc == Scale::Pentatonic)
                       && (deg == DegI || deg == Degvi || deg == Degii || deg == DegbIII);
    const bool dom = (sc == Scale::Blues) || st == Style::Blues || st == Style::Funk
                     || (st == Style::Jazz && (deg == DegV));
    if (sc == Scale::Pentatonic && deg == DegI && ! dom)
        s << "5";
    else if (minor)
        s << (dom ? "m7" : "m");
    else if (dom)
        s << "7";
    return s;
}

juce::String FollowerBand::chordName() const
{
    return namedChord (getSoundingKey(), getChordDegree(), getScale(), getStyle());
}

juce::String FollowerBand::nextChordName() const
{
    return namedChord (getSoundingKey(), getNextDegree(), getScale(), getStyle());
}

const char* FollowerBand::styleName (int i)
{
    static const char* names[] = { "Rock", "Blues", "Metal", "Funk", "Jazz" };
    if (i < 0 || i >= (int) Style::NumStyles) return "Rock";
    return names[i];
}

const char* FollowerBand::degreeName (int deg)
{
    static const char* names[] = { "I", "IV", "V", "vi", "bIII", "bVI", "bVII", "ii" };
    if (deg < 0 || deg >= NumDegrees) return "I";
    return names[deg];
}

void FollowerBand::prepare (double sr)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    const float fsr = (float) sampleRate;
    hatHp.setHighPass (fsr, 6200.0f, 0.70f);
    snareBp.setPeaking (fsr, 1900.0f, 0.75f, 6.0f);
    bassLp.setLowPass (fsr, 420.0f, 0.70f);
    keyLp.setLowPass (fsr, 2400.0f, 0.65f);
    tomBp.setPeaking (fsr, 220.0f, 0.85f, 5.0f);
    reset();
}

void FollowerBand::reset() noexcept
{
    hatHp.reset(); snareBp.reset(); bassLp.reset(); keyLp.reset(); tomBp.reset();
    kickEnv = snareEnv = hatEnv = rideEnv = crashEnv = tomEnv = 0;
    kickPhase = snareTonePhase = hatPhase = ridePhase = tomPhase = 0;
    bassPhase = bassEnv = subPhase = bassPluck = 0;
    keyPhase.fill (0); keyHz.fill (220.0f);
    keyEnv = keyTargetEnv = 0;
    keyLag = 0.0004f;
    keyHarmonic = 0.08f;
    keyStab = false;
    hatOpen = false;
    hatDecayUse = 0.9982f;
    fillThisBar = false;
    pendingCrash = false;
    step = 0;
    stepAccum = 0.0;
    fireImmediate = true; // one downbeat, not a catch-up flood
    barIndex = 0;
    absBar = 0;
    currentDeg = DegI;
    nextDeg = DegbVII;
    fade = 0;
    bassHz = 82.41f;
    lastBassMidi = 40;
    keyShift = 0;
    kickHz = 60.0f;
    tomHz = 140.0f;
    barIndexAtom.store (0);
    absBarAtom.store (0);
    stepAtom.store (0);
    chordDegAtom.store ((int) DegI);
    nextDegAtom.store ((int) DegI);
    soundingKeyAtom.store (4);
    fillAtom.store (0);
    changeAtom.store (0);
}

float FollowerBand::noise() noexcept
{
    rng = rng * 1664525u + 1013904223u;
    return (float) ((int) (rng >> 8) & 0xffff) / 32768.0f - 1.0f;
}

float FollowerBand::oscSaw (float& phase, float freq) noexcept
{
    phase += freq / (float) sampleRate;
    phase -= std::floor (phase);
    return 2.0f * phase - 1.0f;
}

float FollowerBand::oscSin (float& phase, float freq) noexcept
{
    phase += freq / (float) sampleRate;
    phase -= std::floor (phase);
    return std::sin (juce::MathConstants<float>::twoPi * phase);
}

float FollowerBand::midiToHz (float midi) const noexcept
{
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

int FollowerBand::degreeSemitones (int deg) const noexcept
{
    switch (deg)
    {
        case DegI:    return 0;
        case DegIV:   return 5;
        case DegV:    return 7;
        case Degvi:   return 9;
        case DegbIII: return 3;
        case DegbVI:  return 8;
        case DegbVII: return 10;
        case Degii:   return 2;
        default:      return 0;
    }
}

bool FollowerBand::degreeIsMinor (Style st, int deg) const noexcept
{
    if (st == Style::Metal)
        return deg == DegI || deg == Degvi || deg == DegbIII || deg == Degii;
    if (st == Style::Blues || st == Style::Funk)
        return false; // dominant-flavour I/IV/V
    if (st == Style::Jazz)
        return deg == Degii || deg == Degvi;
    return deg == Degvi || deg == Degii;
}

int FollowerBand::progressionLength() const noexcept
{
    const auto f = getForm();
    if (f == Form::Vamp) return 1;
    if (f == Form::TwelveBar) return 12;
    return 8;
}

int FollowerBand::progressionDegree (int bar) const noexcept
{
    const auto f = getForm();
    const auto st = getStyle();
    if (f == Form::Vamp)
        return DegI;
    if (f == Form::TwelveBar)
    {
        const int i = ((bar % 12) + 12) % 12;
        return (st == Style::Jazz) ? kJazz[i] : kBlues[i];
    }
    const int i = ((bar % 8) + 8) % 8;
    if (st == Style::Funk)  return kSongFunk[i];
    if (st == Style::Metal) return kSongMetal[i];
    if (st == Style::Jazz)
    {
        static const int k[] = { DegI, Degvi, Degii, DegV, DegI, DegIV, DegV, DegI };
        return k[i];
    }
    return kSongRock[i];
}

void FollowerBand::setKeyVoicing (Style st, int deg, int keyPc) noexcept
{
    const int rootMidi = 52 + ((keyPc % 12) + 12) % 12 + degreeSemitones (deg);
    const bool minor = degreeIsMinor (st, deg);
    const int third = minor ? 3 : 4;
    const int fifth = 7;
    const int seventh = (st == Style::Blues || st == Style::Funk || st == Style::Jazz)
                        ? 10
                        : (minor ? 10 : 11);
    const int ninth = 14;

    if (st == Style::Jazz)
    {
        keyHz[0] = midiToHz ((float) (rootMidi + third));
        keyHz[1] = midiToHz ((float) (rootMidi + seventh));
        keyHz[2] = midiToHz ((float) (rootMidi + ninth));
        keyHz[3] = midiToHz ((float) (rootMidi + fifth + 12));
        keyHarmonic = 0.22f;
        keyStab = false;
        keyLag = 0.0025f;
    }
    else if (st == Style::Funk)
    {
        keyHz[0] = midiToHz ((float) (rootMidi + third));
        keyHz[1] = midiToHz ((float) (rootMidi + seventh));
        keyHz[2] = midiToHz ((float) (rootMidi + ninth));
        keyHz[3] = midiToHz ((float) rootMidi);
        keyHarmonic = 0.12f;
        keyStab = true;
        keyLag = 0.08f;
    }
    else if (st == Style::Metal)
    {
        keyHz[0] = midiToHz ((float) rootMidi);
        keyHz[1] = midiToHz ((float) (rootMidi + fifth));
        keyHz[2] = midiToHz ((float) (rootMidi + 12));
        keyHz[3] = midiToHz ((float) (rootMidi + fifth + 12));
        keyHarmonic = 0.05f;
        keyStab = false;
        keyLag = 0.00035f;
    }
    else
    {
        keyHz[0] = midiToHz ((float) rootMidi);
        keyHz[1] = midiToHz ((float) (rootMidi + third));
        keyHz[2] = midiToHz ((float) (rootMidi + fifth));
        keyHz[3] = midiToHz ((float) (rootMidi + seventh));
        keyHarmonic = 0.08f;
        keyStab = false;
        keyLag = 0.0004f;
    }
}

int FollowerBand::pickBass (Style st, int step16, float inten, int deg, int upcoming, int keyPc) noexcept
{
    const int root = 40 + ((keyPc % 12) + 12) % 12;
    const int chord = root + degreeSemitones (deg);
    const int nxt   = root + degreeSemitones (upcoming);
    const int beat = step16 / 4;
    const bool minor = degreeIsMinor (st, deg);
    const int third = minor ? 3 : 4;
    auto approach = [&]() -> int
    {
        const int prev = lastBassMidi;
        if (prev < nxt) return juce::jlimit (28, 64, nxt - 1);
        return juce::jlimit (28, 64, nxt + 1);
    };

    if (st == Style::Funk)
    {
        if (step16 == 0) return chord;
        if (step16 == 6 || step16 == 12) return chord + 7;
        if (step16 == 7 && inten >= 0.6f) return chord + 10;
        if (step16 == 9 && inten >= 0.6f) return chord + 14;
        if (step16 == 14 || (step16 == 15 && inten >= 0.9f)) return approach();
        if (step16 == 3 && inten >= 0.3f) return chord;
        if (step16 == 5 && inten >= 0.3f) return chord + 7;
        if (step16 == 10 && inten >= 0.3f) return chord;
        return -1;
    }
    if (st == Style::Jazz)
    {
        if (step16 % 4 != 0 && ! (inten >= 0.9f && (step16 == 2 || step16 == 10)))
            return -1;
        if (beat == 0) return chord;
        if (beat == 1) return chord + (inten >= 0.6f ? third : 7);
        if (beat == 2) return chord + (upcoming == DegV ? 9 : 7);
        const float r = 0.5f + 0.5f * noise();
        if (r < 0.70f) return juce::jlimit (28, 64, nxt - 1);
        if (r < 0.90f) return juce::jlimit (28, 64, nxt + 1);
        return chord + (r < 0.95f ? 2 : 11);
    }
    if (st == Style::Rock)
    {
        if (step16 == 0 || step16 == 8) return chord;
        if (step16 == 4 || step16 == 12) return chord + 7;
        if (inten >= 0.6f && (step16 % 2) == 0) return chord;
        return -1;
    }
    if (st == Style::Blues)
    {
        if (step16 % 4 != 0 && ! (inten >= 0.9f && (barIndex % 4 == 3) && (step16 == 14)))
            return (step16 == 14 && inten >= 0.9f) ? approach() : -1;
        if (beat == 0) return chord;
        if (beat == 1) return chord + third;
        if (beat == 2) return chord + 7;
        return approach();
    }
    // Metal
    if ((step16 % 4) == 0) return chord;
    if ((step16 % 2) == 0) return (step16 / 2) % 2 ? chord + 7 : chord;
    if (inten >= 0.9f && step16 == 15) return approach();
    return -1;
}

void FollowerBand::decideFill (Style /*st*/, float /*inten*/) noexcept
{
    const int ph = juce::jmax (4, phraseBars.load (std::memory_order_relaxed));
    const bool phraseEnd = ((absBar + 1) % ph) == 0;
    fillThisBar = (absBar % 8 == 7) || phraseEnd;
    pendingCrash = (absBar % 8 == 0 && absBar > 0) || (absBar > 0 && (absBar % ph) == 0);
    fillAtom.store (fillThisBar ? 1 : 0, std::memory_order_relaxed);
}

void FollowerBand::triggerStep (int step16, Style st, float inten, int deg, int upcoming, int keyPc) noexcept
{
    const bool even8 = (step16 % 2) == 0;
    const bool beat  = (step16 % 4) == 0;
    const int  beatN = step16 / 4;

    auto trigKick = [&] (float vel)
    {
        kickEnv = juce::jmax (kickEnv, vel);
        kickHz = 96.0f;
        kickPhase = 0;
    };
    auto trigSnare = [&] (float vel)
    {
        snareEnv = juce::jmax (snareEnv, vel);
        snareTonePhase = 0;
    };
    auto trigHat = [&] (float vel, bool open)
    {
        hatEnv = juce::jmax (hatEnv, vel * (0.82f + 0.18f * (0.5f + 0.5f * noise())));
        hatOpen = open;
        hatDecayUse = open ? 0.99935f : 0.9974f;
    };
    auto trigRide = [&] (float vel)
    {
        rideEnv = juce::jmax (rideEnv, vel);
        ridePhase = 0;
    };
    auto trigTom = [&] (float vel, float hz)
    {
        tomEnv = juce::jmax (tomEnv, vel);
        tomHz = hz;
        tomPhase = 0;
    };

    const bool drumsLive = drumsOn.load (std::memory_order_relaxed) != 0;
    const bool bassLive  = bassOn.load (std::memory_order_relaxed) != 0;
    const bool keysLive  = keysOn.load (std::memory_order_relaxed) != 0;

    if (drumsLive && step16 == 0 && pendingCrash)
    {
        crashEnv = 0.88f;
        pendingCrash = false;
    }

    const float velK = 0.87f, velS = 0.87f, velG = 0.28f, velH = 0.63f, velR = 0.63f, velO = 0.63f;

    if (drumsLive && fillThisBar)
    {
        const int seed = absBar / 8;
        const int var = seed & 1;
        const bool low = inten < 0.35f, med = inten >= 0.35f && inten <= 0.70f, high = inten > 0.70f;
        auto hitK = [&] (std::initializer_list<int> xs) { for (int x : xs) if (step16 == x) trigKick (velK); };
        auto hitS = [&] (std::initializer_list<int> xs, float v) { for (int x : xs) if (step16 == x) trigSnare (v); };
        if (low && var == 0) { hitK ({0,10}); hitS ({12,13,14,15}, velS); if (even8) trigHat (velH * 0.7f, false); }
        else if (low) { hitK ({0,8,12}); hitS ({4,14}, velS); if (even8) trigHat (velH * 0.7f, false); }
        else if (med && var == 0) { hitK ({8,14}); hitS ({0,1,3,4,5,11}, velG); hitS ({2,6,10,12,13,15}, velS); }
        else if (med) { hitK ({0,3,6,9,12,15}); hitS ({1,4,7,10,13,14}, velS); }
        else if (high && var == 0) { hitK ({6,12}); hitS ({1,3}, velG); hitS ({0,2,4,5,7,9,11,13,14,15}, velS); }
        else { hitK ({0,4,9,11,15}); hitS ({2,7}, velG); hitS ({1,3,6,8,10,12,13,14}, velS); }

        if (st == Style::Funk && med && (step16 == 7 || step16 == 15))
            trigHat (velO, true);
        if (st == Style::Jazz)
        {
            if (low && (step16 == 0 || step16 == 4 || step16 == 8 || step16 == 12))
                trigRide (velR);
            if (med && step16 <= 8 && (step16 % 4) == 0) trigRide (velR);
            if (high && step16 >= 8 && step16 <= 11)
                trigSnare ((step16 & 1) ? velS : velG);
            if (high && step16 >= 12) trigSnare (velS);
        }
        if (st == Style::Metal && high && (step16 % 2) == 0)
            trigKick (velK);
        if (even8 && st != Style::Jazz) trigHat (0.22f + 0.2f * inten, false);
    }
    else if (drumsLive && st == Style::Funk)
    {
        bool kick[16] = {1,0,0,1,0,0,1,0,0,0,1,0,0,1,0,1};
        bool gsn[16]  = {0,1,0,1,0,1,0,1,0,1,0,1,0,0,1,1};
        bool acc[16]  = {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0};
        if (inten < 0.3f) { kick[3]=kick[10]=kick[13]=kick[15]=0; gsn[1]=gsn[5]=gsn[9]=0; }
        if (inten >= 0.6f) { kick[6]=1; gsn[11]=1; }
        if (inten >= 0.9f) kick[1]=1;
        if (kick[step16]) trigKick (velK);
        if (acc[step16]) trigSnare (velS);
        else if (gsn[step16]) trigSnare (velG);
        const bool eighths = inten < 0.3f;
        const bool hatOn = eighths ? even8 : true;
        if (hatOn) trigHat ((step16 % 4 == 0) ? velH + 0.03f : velH, step16 == 7 || step16 == 15);
    }
    else if (drumsLive && st == Style::Jazz)
    {
        if (step16 == 0) trigKick (velK * 0.55f);
        if (step16 == 14 && inten >= 0.3f) trigKick (velK * 0.45f);
        if (step16 == 7 && inten >= 0.3f) trigKick (0.28f);
        if (step16 == 8 && inten >= 0.6f) trigKick (0.40f);
        if (inten >= 0.9f && (step16 == 7 || step16 == 14)) trigKick (0.35f);
        if (inten >= 0.3f)
        {
            if (step16 == 6) trigSnare (velS * 0.5f);
            if (step16 == 3) trigSnare (velG);
            if (inten >= 0.3f && (step16 == 9 || step16 == 14)) trigSnare (velG);
        }
        if (step16 == 4 || step16 == 12) trigHat (velH, false);
        if (step16 == 0 || step16 == 4 || step16 == 8 || step16 == 12)
            trigRide ((step16 == 4 || step16 == 12) ? velR * 0.85f : velR);
        if (inten >= 0.3f && (step16 == 3 || step16 == 7 || step16 == 11 || step16 == 15))
            trigRide (velR * 0.55f);
    }
    else if (drumsLive && st == Style::Rock)
    {
        if (step16 == 0 || step16 == 8) trigKick (velK);
        if (inten >= 0.6f && step16 == 10) trigKick (velK * 0.7f);
        if (inten >= 0.9f && (step16 == 6 || step16 == 14)) trigKick (velK * 0.65f);
        if (step16 == 4 || step16 == 12) trigSnare (velS);
        if (inten >= 0.9f && step16 == 3) trigSnare (velG);
        if (inten >= 0.3f && even8) trigHat (velH, false);
        if (inten >= 0.6f && ! even8) trigHat (velH * 0.7f, false);
        if (inten > 0.85f && (absBar % 8) == 0 && step16 == 0)
            crashEnv = juce::jmax (crashEnv, 0.70f);
    }
    else if (drumsLive && st == Style::Blues)
    {
        if (step16 == 0) trigKick (velK);
        if (step16 == 6) trigKick (velK * 0.75f);
        if (inten >= 0.6f && step16 == 14) trigKick (velK * 0.6f);
        if (step16 == 4 || step16 == 12) trigSnare (velS);
        if (inten >= 0.6f && (step16 == 3 || step16 == 7 || step16 == 11 || step16 == 15))
            trigSnare (velG);
        if (even8) trigHat (velH, false);
        if (even8) trigRide (velR * 0.5f);
        if (inten >= 0.9f && (barIndex % 12) == 0 && step16 == 0)
            crashEnv = juce::jmax (crashEnv, 0.55f);
    }
    else if (drumsLive) // Metal
    {
        if (inten < 0.3f)
        {
            if (step16 == 0 || step16 == 8) trigKick (velK);
        }
        else
        {
            if (beat) trigKick (velK);
            if (inten >= 0.6f && even8) trigKick (velK * 0.7f);
            if (inten >= 0.9f && step16 == 15) trigKick (velK * 0.6f);
        }
        if (step16 == 4 || step16 == 12) trigSnare (velS);
        if (inten >= 0.9f && step16 == 14) trigSnare (velS * 0.7f);
        if (even8) trigHat (velH + 0.2f * inten, inten >= 0.6f && step16 == 14);
        if (inten >= 0.6f && (barIndex % 4) == 0 && step16 == 0)
            crashEnv = juce::jmax (crashEnv, 0.70f);
        if (inten >= 0.9f && step16 == 0)
            crashEnv = juce::jmax (crashEnv, 0.75f);
    }

    juce::ignoreUnused (beatN, trigTom);

    const int midi = bassLive ? pickBass (st, step16, inten, deg, upcoming, keyPc) : -1;
    if (midi >= 0)
    {
        lastBassMidi = midi;
        bassHz = midiToHz ((float) midi);
        bassEnv = (st == Style::Funk ? 0.55f : 0.75f) * (0.7f + 0.3f * inten);
        if (st == Style::Metal) bassEnv = 0.95f;
        bassPluck = juce::jmax (bassPluck, 0.80f);
    }

    bool keyHit = false;
    if (! keysLive)
        keyHit = false;
    else if (st == Style::Funk)
    {
        keyHit = (step16 == 0) || (inten >= 0.3f && (step16 == 6 || step16 == 10 || step16 == 14));
        if (inten < 0.3f) keyHit = (step16 == 0);
        if (inten >= 0.6f && step16 == 8) keyHit = true;
        if (inten >= 0.9f && step16 == 12) keyHit = true;
    }
    else if (st == Style::Jazz)
    {
        if (inten < 0.3f) keyHit = (step16 == 0);
        else if (inten < 0.6f) keyHit = (step16 == 0 || step16 == 8);
        else if (inten < 0.9f) keyHit = (step16 == 0 || step16 == 6 || step16 == 12);
        else keyHit = (step16 == 0 || step16 == 4 || step16 == 8 || step16 == 12);
    }
    else
        keyHit = (step16 == 0) || (st == Style::Rock && inten >= 0.6f && step16 == 0);

    if (keyHit)
    {
        setKeyVoicing (st, deg, keyPc);
        const float vel = 0.47f + 0.31f * inten;
        if (st == Style::Funk) { keyTargetEnv = vel; keyEnv = vel; keyStab = true; }
        else { keyTargetEnv = vel; keyStab = (st == Style::Jazz); }
    }
}

void FollowerBand::process (int keyPc,
                            float bpmIn,
                            float intensity,
                            float* drumsL, float* drumsR,
                            float* bassL,  float* bassR,
                            float* keysL,  float* keysR,
                            int numSamples) noexcept
{
    if (drumsL == nullptr || numSamples <= 0)
        return;

    const bool run = enabled.load (std::memory_order_relaxed) != 0;
    const Style st = (Style) juce::jlimit (0, (int) Style::NumStyles - 1,
                                           style.load (std::memory_order_relaxed));
    const float bpmClamped = juce::jlimit (60.0f, 180.0f, bpmIn);
    const float inten = juce::jlimit (0.0f, 1.0f, intensity);
    const auto fl = getFeel();
    float swing = 0.0f;
    if (fl == Feel::Swing)
        swing = 0.62f;
    lastSwing = swing;

    samplesPer16th = (60.0 / (double) bpmClamped) * sampleRate / 4.0;
    double feelBias = 1.0;
    if (fl == Feel::Ahead)  feelBias = 0.97;
    if (fl == Feel::Behind) feelBias = 1.03;

    keyPc = ((keyPc + keyShift) % 12 + 12) % 12;
    currentDeg = progressionDegree (barIndex);
    nextDeg = progressionDegree (barIndex + 1);
    chordDegAtom.store (currentDeg, std::memory_order_relaxed);
    nextDegAtom.store (nextDeg, std::memory_order_relaxed);
    soundingKeyAtom.store (keyPc, std::memory_order_relaxed);
    barIndexAtom.store (barIndex, std::memory_order_relaxed);
    absBarAtom.store (absBar, std::memory_order_relaxed);
    stepAtom.store (step, std::memory_order_relaxed);
    changeAtom.store ((nextDeg != currentDeg && step >= 12) ? 1 : 0, std::memory_order_relaxed);

    const float drumsGain = drumsOn.load (std::memory_order_relaxed) != 0 ? 1.0f : 0.0f;
    const float bassGain  = bassOn.load (std::memory_order_relaxed) != 0 ? 1.0f : 0.0f;
    const float keysGain  = keysOn.load (std::memory_order_relaxed) != 0 ? 1.0f : 0.0f;

    const float kickDecay  = (st == Style::Metal) ? 0.9992f : 0.9994f;
    const float snareDecay = 0.9991f;
    const float bassDecay  = (st == Style::Funk)  ? 0.9968f
                           : (st == Style::Metal) ? 0.9990f
                           : (st == Style::Jazz)  ? 0.99962f
                           : 0.99955f;

    for (int i = 0; i < numSamples; ++i)
    {
        fade += ((run ? 1.0f : 0.0f) - fade) * 0.0008f; // ~25 ms at 44.1k

        double stepLen = samplesPer16th * feelBias;
        if (swing > 0.0f)
        {
            if ((step & 1) == 0) stepLen = samplesPer16th * feelBias * (1.0 + 0.5 * (double) swing);
            else                 stepLen = samplesPer16th * feelBias * (1.0 - 0.5 * (double) swing);
        }
        stepLen *= 1.0 + 0.012 * (double) noise(); // slight humanize
        stepLen = juce::jmax (64.0, stepLen);

        bool due = fireImmediate;
        if (run)
            stepAccum += 1.0;
        if (run && (due || stepAccum >= stepLen))
        {
            fireImmediate = false;
            if (due)
                stepAccum = 0.0;
            else
            {
                stepAccum -= stepLen;
                if (stepAccum >= stepLen)
                    stepAccum = 0.0; // never catch-up multiple 16ths in one sample
            }
            if (step == 0)
                decideFill (st, inten);
            triggerStep (step, st, inten, currentDeg, nextDeg, keyPc);
            step = (step + 1) & 15;
            stepAtom.store (step, std::memory_order_relaxed);
            if (step == 0)
            {
                const int plen = progressionLength();
                barIndex = (barIndex + 1) % plen;
                absBar += 1;
                const int ph = juce::jmax (4, phraseBars.load (std::memory_order_relaxed));
                if (getForm() == Form::Wander && (absBar % ph) == 0)
                    keyShift = (keyShift + 7) % 12;
                currentDeg = progressionDegree (barIndex);
                nextDeg = progressionDegree (barIndex + 1);
                chordDegAtom.store (currentDeg);
                nextDegAtom.store (nextDeg);
                barIndexAtom.store (barIndex);
                absBarAtom.store (absBar);
            }
        }

        // --- drums ---
        kickHz *= 0.99915f;
        if (kickHz < 42.0f) kickHz = 42.0f;
        const float kick = oscSin (kickPhase, kickHz) * kickEnv
                         + 0.10f * noise() * kickEnv * kickEnv;
        kickEnv *= kickDecay;

        float sn = snareBp.process (noise()) * snareEnv;
        sn += 0.28f * oscSin (snareTonePhase, 188.0f) * snareEnv;
        snareEnv *= snareDecay;

        const float hatNoise = hatHp.process (noise());
        float hat = hatNoise * hatEnv * (hatOpen ? 1.15f : 1.0f);
        hatEnv *= hatDecayUse;
        crashEnv *= 0.99972f;
        const float crash = hatNoise * crashEnv + 0.08f * oscSin (hatPhase, 620.0f) * crashEnv;

        rideEnv *= 0.99885f;
        const float rFund = oscSin (ridePhase, 1540.0f);
        const float ride = (0.42f * rFund
                          + 0.22f * std::sin (juce::MathConstants<float>::twoPi * ridePhase * 1.5f)
                          + 0.18f * hatNoise) * rideEnv;

        tomHz *= 0.9994f;
        if (tomHz < 80.0f) tomHz = 80.0f;
        const float tom = (oscSin (tomPhase, tomHz) * 0.85f + tomBp.process (noise()) * 0.18f) * tomEnv;
        tomEnv *= 0.99905f;

        kickEnv  = juce::jmin (kickEnv,  1.0f);
        snareEnv = juce::jmin (snareEnv, 1.0f);
        hatEnv   = juce::jmin (hatEnv,   0.85f);
        crashEnv = juce::jmin (crashEnv, 0.70f);
        rideEnv  = juce::jmin (rideEnv,  0.80f);
        tomEnv   = juce::jmin (tomEnv,   1.0f);
        float dL = kick * 0.95f + sn * 0.62f + hat * 0.14f + crash * 0.18f + ride * 0.22f + tom * 0.55f;
        float dR = kick * 0.90f + sn * 0.68f + hat * 0.16f + crash * 0.20f + ride * 0.24f + tom * 0.50f;

        // --- bass ---
        float saw = oscSaw (bassPhase, bassHz);
        float bs = saw + 0.35f * oscSin (subPhase, bassHz * 0.5f);
        bs = bassLp.process (bs) * bassEnv;
        bs += 0.22f * saw * bassPluck; // pluck click
        bassEnv *= bassDecay;
        bassPluck *= 0.9945f;
        bs = bs / (1.0f + std::abs (bs) * 0.4f);

        // --- keys ---
        if (keyStab)
        {
            keyEnv += (keyTargetEnv - keyEnv) * juce::jmin (1.0f, keyLag);
            keyTargetEnv *= 0.99925f;
        }
        else
        {
            keyEnv += (keyTargetEnv - keyEnv) * keyLag;
        }

        float kMix = 0.0f;
        float kMixR = 0.0f;
        for (int v = 0; v < 4; ++v)
        {
            float& ph = keyPhase[(size_t) v];
            const float fund = std::sin (juce::MathConstants<float>::twoPi * ph);
            const float harm = std::sin (juce::MathConstants<float>::twoPi * ph * 2.0f);
            ph += keyHz[(size_t) v] / (float) sampleRate;
            ph -= std::floor (ph);
            const float s = fund + keyHarmonic * harm;
            const float w = (v == 0 ? 0.34f : 0.22f);
            const float pan = (v == 1 || v == 3) ? 0.22f : -0.18f;
            kMix  += s * w * (1.0f - pan);
            kMixR += s * w * (1.0f + pan);
        }
        kMix  = keyLp.process (kMix)  * keyEnv;
        kMixR = kMixR * keyEnv;

        const float g = fade;
        const float gd = g * drumsGain;
        const float gb = g * bassGain;
        const float gk = g * keysGain;
        drumsL[i] = dL * gd;
        drumsR[i] = dR * gd;
        bassL[i]  = bs * gb;
        bassR[i]  = bs * gb * 0.92f;
        keysL[i]  = kMix * gk;
        keysR[i]  = kMixR * gk;
    }
}
