#include "DSP/FollowerBand.h"
#include <initializer_list>
#include <cmath>

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

    // Keep Groove floors ~0.22 (rest/light). Layers:
    // 0 rest sparse, 1 pocket backbeat, 2 push 8ths, 3 fire busy.
    inline int densityLayer (float inten) noexcept
    {
        return inten < 0.28f ? 0
             : inten < 0.55f ? 1
             : inten < 0.78f ? 2
             : 3;
    }
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

const char* FollowerBand::drumKitName (int i)
{
    static const char* names[] = { "Acoustic", "Metal", "Jazz", "Funk", "Electro" };
    if (i < 0 || i >= (int) DrumKit::NumKits) return "Acoustic";
    return names[i];
}

const char* FollowerBand::bassVoiceName (int i)
{
    static const char* names[] = { "Finger", "Pick", "Synth", "Slap" };
    if (i < 0 || i >= (int) BassVoice::NumVoices) return "Finger";
    return names[i];
}

const char* FollowerBand::keysVoiceName (int i)
{
    static const char* names[] = { "Piano", "EP", "Organ", "Pad", "Clav" };
    if (i < 0 || i >= (int) KeysVoice::NumVoices) return "Piano";
    return names[i];
}

void FollowerBand::styleVoiceDefaults (int style, int& kit, int& bass, int& keys) noexcept
{
    switch (style)
    {
        case (int) Style::Blues: kit = (int) DrumKit::Acoustic; bass = (int) BassVoice::Finger; keys = (int) KeysVoice::EP;     break;
        case (int) Style::Metal: kit = (int) DrumKit::Metal;    bass = (int) BassVoice::Pick;   keys = (int) KeysVoice::Organ;  break;
        case (int) Style::Funk:  kit = (int) DrumKit::Funk;     bass = (int) BassVoice::Slap;   keys = (int) KeysVoice::Clav;   break;
        case (int) Style::Jazz:  kit = (int) DrumKit::Jazz;     bass = (int) BassVoice::Finger; keys = (int) KeysVoice::EP;     break;
        default:                 kit = (int) DrumKit::Acoustic; bass = (int) BassVoice::Finger; keys = (int) KeysVoice::Piano;  break;
    }
}

const char* FollowerBand::degreeName (int deg)
{
    static const char* names[] = { "I", "IV", "V", "vi", "bIII", "bVI", "bVII", "ii" };
    if (deg < 0 || deg >= NumDegrees) return "I";
    return names[deg];
}

void FollowerBand::prepare (double sr, SampleBank* bank)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    samples = bank;
    const float fsr = (float) sampleRate;
    hatHp.setHighPass (fsr, 6200.0f, 0.70f);
    snareBp.setPeaking (fsr, 1900.0f, 0.75f, 6.0f);
    bassLp.setLowPass (fsr, 420.0f, 0.70f);
    keyLp.setLowPass (fsr, 2400.0f, 0.65f);
    tomBp.setPeaking (fsr, 220.0f, 0.85f, 5.0f);
    kickHp.setHighPass (fsr, 28.0f, 0.70f);
    drumEngine.prepare (sampleRate, samples);
    reset();
}

void FollowerBand::reset() noexcept
{
    hatHp.reset(); snareBp.reset(); bassLp.reset(); keyLp.reset(); tomBp.reset(); kickHp.reset();
    drumEngine.reset();
    kickEnv = snareEnv = hatEnv = rideEnv = crashEnv = tomEnv = 0;
    kickPhase = snareTonePhase = hatPhase = ridePhase = tomPhase = 0;
    kickClick = 0;
    bassPhase = bassEnv = subPhase = bassPluck = bassPop = 0;
    keyPhase.fill (0); keyHz.fill (220.0f);
    keyEnv = keyTargetEnv = 0;
    keyLag = 0.0004f;
    keyHarmonic = 0.08f;
    keyTine = 0;
    keyDetune = 0;
    keyPercEnv = 0;
    keyStab = false;
    lastTimbreStamp = -1;
    hatOpen = false;
    hatDecayUse = 0.9982f;
    fillThisBar = false;
    pendingCrash = false;
    fillVariant = 0;
    pendingOnsetKick.store (0, std::memory_order_relaxed);
    pendingOnsetStr = 0.0f;
    playerOnsetRate.store (0.0f, std::memory_order_relaxed);
    playerLiveAtom.store (0, std::memory_order_relaxed);
    phrasePeakInten = 0.0f;
    phraseSpiked = false;
    phrasePeakBar = -1;
    followDeg.store (-1, std::memory_order_relaxed);
    thinMask.store (0x1, std::memory_order_relaxed);
    step = 0;
    stepAccum = 0.0;
    fireImmediate = true; // one downbeat, not a catch-up flood
    stepHumanize = 1.0;
    lastStepLen = 5512.5;
    bpmApplied = 96.0f;
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
    crashAtom.store (0);
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

void FollowerBand::applyTimbre (DrumKit kit, BassVoice bv, KeysVoice kv) noexcept
{
    const float fsr = (float) sampleRate;
    switch (kit)
    {
        case DrumKit::Metal:
            // Industrial: tighter kick, 2–3 kHz snare crack, thin metallic hats.
            hatHp.setHighPass (fsr, 8800.0f, 0.78f);
            snareBp.setPeaking (fsr, 2650.0f, 0.95f, 10.0f);
            kickHp.setHighPass (fsr, 95.0f, 0.75f);
            kickPitchRate = 0.99775f;
            kickFloorHz = 62.0f;
            kickNoiseAmt = 0.38f;
            kickDecayUse = 0.99715f;
            snareDecayUse = 0.99775f;
            snareToneAmt = 0.10f;
            snareToneHz = 280.0f;
            hatGain = 1.02f;
            rideGain = 0.42f;
            break;
        case DrumKit::Jazz:
            hatHp.setHighPass (fsr, 4800.0f, 0.65f);
            kickHp.setHighPass (fsr, 28.0f, 0.70f);
            snareBp.setPeaking (fsr, 1250.0f, 0.70f, 3.5f);
            kickPitchRate = 0.99935f;
            kickFloorHz = 40.0f;
            kickNoiseAmt = 0.04f;
            kickDecayUse = 0.9989f;
            snareDecayUse = 0.9989f;
            snareToneAmt = 0.08f;
            snareToneHz = 160.0f;
            hatGain = 0.55f;
            rideGain = 1.55f;
            break;
        case DrumKit::Funk:
            hatHp.setHighPass (fsr, 7000.0f, 0.72f);
            kickHp.setHighPass (fsr, 28.0f, 0.70f);
            snareBp.setPeaking (fsr, 2100.0f, 0.85f, 5.5f);
            kickPitchRate = 0.9988f;
            kickFloorHz = 52.0f;
            kickNoiseAmt = 0.07f;
            kickDecayUse = 0.99815f;
            snareDecayUse = 0.9976f;
            snareToneAmt = 0.22f;
            snareToneHz = 200.0f;
            hatGain = 1.05f;
            rideGain = 0.55f;
            break;
        case DrumKit::Electro:
            hatHp.setHighPass (fsr, 9200.0f, 0.80f);
            kickHp.setHighPass (fsr, 28.0f, 0.70f);
            snareBp.setPeaking (fsr, 1800.0f, 0.90f, 7.0f);
            kickPitchRate = 0.99958f;
            kickFloorHz = 38.0f;
            kickNoiseAmt = 0.0f;
            kickDecayUse = 0.99978f;
            snareDecayUse = 0.9974f;
            snareToneAmt = 0.0f;
            snareToneHz = 188.0f;
            hatGain = 0.85f;
            rideGain = 0.20f;
            break;
        default: // Acoustic
            hatHp.setHighPass (fsr, 6200.0f, 0.70f);
            kickHp.setHighPass (fsr, 28.0f, 0.70f);
            snareBp.setPeaking (fsr, 1900.0f, 0.75f, 6.0f);
            kickPitchRate = 0.99915f;
            kickFloorHz = 42.0f;
            kickNoiseAmt = 0.10f;
            kickDecayUse = 0.9994f;
            snareDecayUse = 0.9991f;
            snareToneAmt = 0.28f;
            snareToneHz = 188.0f;
            hatGain = 1.0f;
            rideGain = 1.0f;
            break;
    }

    switch (bv)
    {
        case BassVoice::Pick:
            // Mid grind so pick bass sits under SAWVI, not a clean DI.
            bassLp.setLowPass (fsr, 1680.0f, 0.62f);
            bassSawAmt = 1.05f; bassSqrAmt = 0.28f; bassSubAmt = 0.12f;
            bassPluckAmt = 0.58f; bassPluckDecay = 0.9890f; bassPopAmt = 0.14f;
            break;
        case BassVoice::Synth:
            bassLp.setLowPass (fsr, 210.0f, 0.80f);
            bassSawAmt = 0.45f; bassSqrAmt = 0.50f; bassSubAmt = 0.55f;
            bassPluckAmt = 0.06f; bassPluckDecay = 0.9968f; bassPopAmt = 0.0f;
            break;
        case BassVoice::Slap:
            bassLp.setLowPass (fsr, 780.0f, 0.72f);
            bassSawAmt = 0.22f; bassSqrAmt = 0.0f; bassSubAmt = 0.15f;
            bassPluckAmt = 0.70f; bassPluckDecay = 0.9885f; bassPopAmt = 0.55f;
            break;
        default: // Finger
            bassLp.setLowPass (fsr, 340.0f, 0.70f);
            bassSawAmt = 0.42f; bassSqrAmt = 0.0f; bassSubAmt = 0.55f;
            bassPluckAmt = 0.14f; bassPluckDecay = 0.9962f; bassPopAmt = 0.0f;
            break;
    }

    switch (kv)
    {
        case KeysVoice::EP:
            keyLp.setLowPass (fsr, 3100.0f, 0.65f);
            break;
        case KeysVoice::Organ:
            keyLp.setLowPass (fsr, 2600.0f, 0.70f);
            break;
        case KeysVoice::Pad:
            keyLp.setLowPass (fsr, 1180.0f, 0.55f); // dark synth-strings
            break;
        case KeysVoice::Clav:
            keyLp.setPeaking (fsr, 1120.0f, 0.85f, 7.0f);
            break;
        default: // Piano
            keyLp.setLowPass (fsr, 4200.0f, 0.65f);
            break;
    }
}

void FollowerBand::hitKeys (KeysVoice kv, float vel) noexcept
{
    switch (kv)
    {
        case KeysVoice::EP:
            keyEnv = vel * 0.90f;
            keyTargetEnv = vel * 0.62f;
            keyLag = 0.018f;
            keyHarmonic = 0.28f;
            keyTine = 0.22f;
            keyDetune = 0.0012f;
            keyPercEnv = juce::jmax (keyPercEnv, vel * 0.45f);
            keyPercDecay = 0.9965f;
            keyTargetDecay = 0.99958f;
            keyStab = false;
            break;
        case KeysVoice::Organ:
            keyTargetEnv = vel;
            keyLag = 0.010f;
            keyHarmonic = 0.42f;
            keyTine = 0.12f;
            keyDetune = 0.0004f;
            keyPercEnv = juce::jmax (keyPercEnv, vel * 0.75f);
            keyPercDecay = 0.9935f;
            keyTargetDecay = 0.99992f;
            keyStab = false;
            break;
        case KeysVoice::Pad:
            // Slow bow, 200–400 Hz body, almost no hammer.
            keyTargetEnv = vel * 0.70f;
            keyLag = 0.00007f;
            keyHarmonic = 0.20f;
            keyTine = 0.0f;
            keyDetune = 0.0060f;
            keyPercEnv = 0;
            keyPercDecay = 0.999f;
            keyTargetDecay = 0.99993f;
            keyStab = false;
            break;
        case KeysVoice::Clav:
            keyEnv = vel;
            keyTargetEnv = 0.0f;
            keyLag = 0.070f;
            keyHarmonic = 0.18f;
            keyTine = 0.0f;
            keyDetune = 0.0f;
            keyPercEnv = juce::jmax (keyPercEnv, vel * 0.55f);
            keyPercDecay = 0.991f;
            keyTargetDecay = 0.9968f;
            keyStab = true;
            break;
        default: // Piano
            keyEnv = vel;
            keyTargetEnv = vel * 0.42f;
            keyLag = 0.10f;
            keyHarmonic = 0.22f;
            keyTine = 0.04f;
            keyDetune = 0.0003f;
            keyPercEnv = juce::jmax (keyPercEnv, vel * 0.85f);
            keyPercDecay = 0.9955f;
            keyTargetDecay = 0.99942f;
            keyStab = false;
            break;
    }
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
    const bool minor = degreeIsMinor (st, deg);
    const int third = minor ? 3 : 4;
    const int layer = densityLayer (inten);
    auto approach = [&]() -> int
    {
        const int prev = lastBassMidi;
        if (prev < nxt) return juce::jlimit (28, 64, nxt - 1);
        return juce::jlimit (28, 64, nxt + 1);
    };

    // Pocket+ never silent on 0 / 4 / 8 / 12 (root, fifth, root, fifth).
    if (layer >= 1)
    {
        if (step16 == 0 || step16 == 8)
            return chord;
        if (step16 == 4 || step16 == 12)
            return chord + 7;
    }
    else
    {
        if (step16 == 0)
            return chord;
        return -1;
    }

    // fire: approach into the next chord on 14/15
    if (layer >= 3 && (step16 == 14 || step16 == 15))
        return approach();

    // push+: busier 8ths on the chord
    if (layer >= 2 && (step16 % 2) == 0)
        return chord;

    // Funk/jazz extra syncopation at push/fire only.
    if (layer >= 2 && st == Style::Funk)
    {
        if (step16 == 3) return chord;
        if (step16 == 5) return chord + 7;
        if (step16 == 7 && layer >= 3) return chord + 10;
        if (step16 == 9 && layer >= 3) return chord + 14;
        if (step16 == 11) return chord + third;
    }
    if (layer >= 2 && st == Style::Jazz)
    {
        if (step16 == 2 || step16 == 10) return chord + third;
        if (layer >= 3 && (step16 == 6 || step16 == 11)) return chord + 7;
    }
    return -1;
}

void FollowerBand::setPlayerActivity (float onsetRate, bool playerLive) noexcept
{
    playerOnsetRate.store (juce::jlimit (0.0f, 8.0f, onsetRate), std::memory_order_relaxed);
    playerLiveAtom.store (playerLive ? 1 : 0, std::memory_order_relaxed);
}

void FollowerBand::noteGuitarOnset (float strength) noexcept
{
    strength = juce::jlimit (0.0f, 1.0f, strength);
    // Onset = downbeat candidate: main interaction, not a rare garnish.
    if (strength < 0.28f)
        return;
    const int s = stepAtom.load (std::memory_order_relaxed);
    const int intoBeat = s % 4;
    // Near a beat → reinforce this beat's kick; else snap to next downbeat.
    if (intoBeat <= 1)
        pendingOnsetKick.store (1, std::memory_order_relaxed);
    else
        pendingOnsetKick.store (2, std::memory_order_relaxed);
    pendingOnsetStr = juce::jmax (pendingOnsetStr * 0.35f, strength);
    playerLiveAtom.store (1, std::memory_order_relaxed);
}

void FollowerBand::decideFill (Style /*st*/, float inten) noexcept
{
    const int layer = densityLayer (inten);
    const int ph = juce::jmax (4, phraseBars.load (std::memory_order_relaxed));
    const bool phraseEnd = ((absBar + 1) % ph) == 0;
    const int phraseIdx = absBar / ph;
    if (phraseIdx != phrasePeakBar)
    {
        phrasePeakBar = phraseIdx;
        phrasePeakInten = inten;
        phraseSpiked = false;
    }
    else
    {
        if (inten > phrasePeakInten + 0.12f && inten >= 0.62f)
            phraseSpiked = true;
        phrasePeakInten = juce::jmax (phrasePeakInten, inten);
    }

    const bool afterFill = fillThisBar;
    // Fill ONLY when intensity spiked this phrase — not every 8 bars on autopilot.
    const bool live = playerLiveAtom.load (std::memory_order_relaxed) != 0;
    fillThisBar = live && phraseSpiked && phraseEnd && layer >= 2;
    if (afterFill)
        pendingCrash = layer >= 1 && live;
    else if (fillThisBar)
        pendingCrash = false;
    else
        pendingCrash = false;
    if (fillThisBar)
    {
        phraseSpiked = false; // consume spike
        const uint32_t seed = (uint32_t) absBar * 2654435761u ^ (uint32_t) (inten * 1000.0f);
        if (layer >= 3)
            fillVariant = (int) (seed % 3);
        else if (layer >= 2)
            fillVariant = (int) (seed % 2);
        else
            fillVariant = 0;
    }
    fillAtom.store (fillThisBar ? 1 : 0, std::memory_order_relaxed);
}

void FollowerBand::triggerStep (int step16, Style st, float inten, int deg, int upcoming, int keyPc) noexcept
{
    const bool even8 = (step16 % 2) == 0;
    const bool beat  = (step16 % 4) == 0;
    const int  beatN = step16 / 4;

    const auto kit = (DrumKit) juce::jlimit (0, (int) DrumKit::NumKits - 1,
                                             drumKit.load (std::memory_order_relaxed));
    const auto bv  = (BassVoice) juce::jlimit (0, (int) BassVoice::NumVoices - 1,
                                              bassVoice.load (std::memory_order_relaxed));
    const auto kv  = (KeysVoice) juce::jlimit (0, (int) KeysVoice::NumVoices - 1,
                                              keysVoice.load (std::memory_order_relaxed));

    const int thin = thinMask.load (std::memory_order_relaxed);
    const bool drumsLive = drumsOn.load (std::memory_order_relaxed) != 0 && (thin & 0x1) != 0;
    const bool bassLive  = bassOn.load (std::memory_order_relaxed) != 0 && (thin & 0x2) != 0;
    const bool keysLive  = keysOn.load (std::memory_order_relaxed) != 0 && (thin & 0x4) != 0;

    crashAtom.store (0, std::memory_order_relaxed);
    if (drumsLive && step16 == 0 && pendingCrash)
    {
        drumEngine.trigCrash (0.88f);
        crashAtom.store (1, std::memory_order_relaxed);
        pendingCrash = false;
    }

    // Continuous velocity from player intensity (ghosts stay quiet).
    const float velK = 0.55f + 0.45f * inten;
    const float velS = 0.50f + 0.50f * inten;
    const float velG = juce::jlimit (0.10f, 0.28f, 0.12f + 0.10f * inten); // ghosts quiet
    const float velH = 0.42f + 0.40f * inten;
    const float velR = 0.50f + 0.35f * inten;
    const float velO = 0.48f + 0.38f * inten;
    const float hatJ = 0.90f + 0.20f * (0.5f + 0.5f * noise()); // ±10%

    const int layer = densityLayer (inten);
    const auto fl = getFeel();
    const bool swingFeel = (fl == Feel::Swing) || (st == Style::Blues);

    // Onset = downbeat candidate: snaps density feel and reinforces kick.
    {
        const int pk = pendingOnsetKick.load (std::memory_order_relaxed);
        if (drumsLive && pk != 0)
        {
            const bool fireNow = (pk == 1 && beat) || (pk == 2 && step16 == 0);
            if (fireNow)
            {
                drumEngine.trigKick (juce::jlimit (0.55f, 1.0f, velK * (0.85f + 0.15f * pendingOnsetStr)));
                pendingOnsetKick.store (0, std::memory_order_relaxed);
                pendingOnsetStr *= 0.5f;
            }
        }
    }

    const bool playerLive = playerLiveAtom.load (std::memory_order_relaxed) != 0;
    const float onsetRate = playerOnsetRate.load (std::memory_order_relaxed);
    // Fast onset rate → busy hats. Never thin to half-time pulse without updating BPM.
    const bool hatsBusy = playerLive && onsetRate > 2.8f;
    // Style colours only when pocket+ AND player is actually playing.
    const bool styleColour = layer >= 1 && playerLive;

    if (drumsLive && fillThisBar)
    {
        if (step16 == 0 || step16 == 8)
            drumEngine.trigKick (velK);
        if (step16 == 4)
            drumEngine.trigSnare (velS);
        if (even8 && step16 < 12)
        {
            if (st == Style::Jazz)
                drumEngine.trigRide (velR);
            else
                drumEngine.trigHat (velH * hatJ, false);
        }
        if (fillVariant == 0)
        {
            if (step16 == 12) drumEngine.trigTom (0, velK);
            if (step16 == 13) drumEngine.trigTom (1, velK * 0.95f);
            if (step16 == 14) drumEngine.trigTom (2, velK * 0.90f);
            if (step16 == 15) drumEngine.trigSnare (velS * 0.85f);
        }
        else if (fillVariant == 1)
        {
            if (step16 == 12) drumEngine.trigSnare (velG, true);
            if (step16 == 13) drumEngine.trigSnare (velS * 0.70f);
            if (step16 == 14) { drumEngine.trigSnare (velS * 0.85f); drumEngine.trigTom (1, velK * 0.70f); }
            if (step16 == 15) drumEngine.trigSnare (velS);
        }
        else
        {
            if (step16 == 12) { drumEngine.trigTom (0, velK); drumEngine.trigCrash (0.55f); }
            if (step16 == 13) drumEngine.trigTom (1, velK * 0.92f);
            if (step16 == 14) drumEngine.trigTom (2, velK * 0.88f);
            if (step16 == 15) drumEngine.trigSnare (velS);
        }
    }
    else if (drumsLive)
    {
        if (layer == 0 || ! playerLive)
        {
            // Rest / Keep Groove floor: sparse kick or light hat — NOT a busy metronome.
            // Keep Groove lands ~0.22 (layer 0): kick on 1 + whisper hat on 1/3.
            if (step16 == 0)
                drumEngine.trigKick (velK * 0.70f);
            if (step16 == 0 || step16 == 8)
                drumEngine.trigHat (velH * 0.32f, false);
        }
        else if (layer == 1)
        {
            // Pocket without recent busy playing: ONLY backbeat + light hat.
            // Density layers drive whether 8ths/16ths exist — not a canned 16th grid.
            if (step16 == 0 || step16 == 8)
                drumEngine.trigKick (velK);
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS);
            if (step16 == 0 || step16 == 8)
                drumEngine.trigHat (velH * 0.50f * hatJ, false);
        }
        else if (styleColour && st == Style::Metal)
        {
            if (layer >= 3)
            {
                if (step16 == 0 || step16 == 3 || step16 == 6 || step16 == 8
                    || step16 == 11 || step16 == 14)
                    drumEngine.trigKick (velK * (step16 == 0 || step16 == 8 ? 1.0f : 0.82f));
            }
            else
            {
                if (step16 == 0 || step16 == 8)
                    drumEngine.trigKick (velK);
                if (layer >= 2 && (step16 == 6 || step16 == 10))
                    drumEngine.trigKick (velK * 0.78f);
            }
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS * 1.05f);
            if (even8)
                drumEngine.trigHat (velH * 0.85f * hatJ, false);
            if ((layer >= 3 || hatsBusy) && ! even8)
                drumEngine.trigHat (velH * 0.55f * hatJ, false);
            if (layer >= 2 && step16 == 14)
                drumEngine.trigHat (velO * 0.70f * hatJ, true);
        }
        else if (styleColour && st == Style::Jazz)
        {
            if (step16 == 0)
                drumEngine.trigKick (velK * 0.55f);
            if (step16 == 6)
                drumEngine.trigKick (velK * 0.40f);
            if (step16 == 0 || step16 == 2 || step16 == 4 || step16 == 6
                || step16 == 8 || step16 == 10 || step16 == 12 || step16 == 14)
                drumEngine.trigRide (velR * (beat ? 1.0f : 0.72f));
            if (step16 == 5 || step16 == 13)
                drumEngine.trigSnare (velG * 1.15f, true);
            if (layer >= 2 && (step16 == 4 || step16 == 12))
                drumEngine.trigSnare (velS * 0.55f);
            if (layer >= 3 && (step16 == 7 || step16 == 15))
                drumEngine.trigSnare (velG, true);
        }
        else if (styleColour && st == Style::Funk)
        {
            if (step16 == 0 || (layer >= 2 && (step16 == 6 || step16 == 10)))
                drumEngine.trigKick (velK * (step16 == 0 ? 1.0f : 0.80f));
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS);
            if (layer >= 2)
            {
                if (step16 == 1 || step16 == 2 || step16 == 3
                    || step16 == 9 || step16 == 10 || step16 == 11)
                    drumEngine.trigSnare (velG, true);
            }
            if (even8)
                drumEngine.trigHat (velH * hatJ, false);
            if ((layer >= 2 || hatsBusy) && step16 == 14)
                drumEngine.trigHat (velO * hatJ, true);
            if (layer >= 3 && step16 == 7)
                drumEngine.trigSnare (velG * 0.85f, true);
        }
        else if (styleColour && (st == Style::Blues || swingFeel))
        {
            if (step16 == 0 || step16 == 6)
                drumEngine.trigKick (velK * (step16 == 0 ? 1.0f : 0.82f));
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS);
            if (even8)
                drumEngine.trigHat (velH * hatJ, false);
            if (layer >= 2 && step16 == 10)
                drumEngine.trigKick (velK * 0.70f);
            if (layer >= 2 && (step16 == 3 || step16 == 11))
                drumEngine.trigSnare (velG, true);
            if (layer >= 2 && step16 == 14)
                drumEngine.trigHat (velO * hatJ, true);
        }
        else if (styleColour)
        {
            // Rock push/fire: 8ths (and 16ths when busy) driven by density + onset rate.
            if (step16 == 0 || step16 == 8)
                drumEngine.trigKick (velK);
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS);
            if (layer >= 2 && step16 == 10)
                drumEngine.trigKick (velK * 0.78f);
            if (layer >= 2 && (step16 == 3 || step16 == 11))
                drumEngine.trigSnare (velG, true);
            if (layer >= 3 && (step16 == 7 || step16 == 15))
                drumEngine.trigSnare (velG * 0.85f, true);
            if (even8)
                drumEngine.trigHat (velH * hatJ, false);
            if ((layer >= 3 || hatsBusy) && ! even8)
                drumEngine.trigHat (velH * 0.65f * hatJ, false);
            if (layer >= 2 && step16 == 14)
                drumEngine.trigHat (velO * hatJ, true);
        }
        else
        {
            // Player quiet but energy still pocket: backbeat only.
            if (step16 == 0 || step16 == 8)
                drumEngine.trigKick (velK * 0.90f);
            if (step16 == 4 || step16 == 12)
                drumEngine.trigSnare (velS * 0.90f);
            if (step16 == 0 || step16 == 8)
                drumEngine.trigHat (velH * 0.45f * hatJ, false);
        }

        if (layer >= 3 && playerLive && step16 == 0 && (absBar % 8) == 0)
            drumEngine.trigCrash (0.70f);
    }

    juce::ignoreUnused (beatN, kit);


    const int midi = bassLive ? pickBass (st, step16, inten, deg, upcoming, keyPc) : -1;
    if (midi >= 0)
    {
        lastBassMidi = midi;
        bassHz = midiToHz ((float) midi);
        bassEnv = (st == Style::Funk ? 0.55f : 0.75f) * (0.7f + 0.3f * inten);
        if (st == Style::Metal) bassEnv = 0.95f;
        if (bv == BassVoice::Synth) bassEnv = juce::jmax (bassEnv, 0.70f);
        if (bv == BassVoice::Slap)  bassEnv *= 0.92f;
        bassPluck = juce::jmax (bassPluck, bassPluckAmt > 0.01f ? 0.80f : 0.20f);
        if (bv == BassVoice::Slap)
            bassPop = juce::jmax (bassPop, 0.90f);
        if (samples != nullptr && samples->isReady (SampleBank::BassPluck)
            && (bv == BassVoice::Finger || bv == BassVoice::Pick || bv == BassVoice::Slap))
        {
            const float rate = std::pow (2.0f, ((float) midi - 40.0f) / 12.0f);
            samples->play (SampleBank::BassPluck, bassEnv * 0.85f, rate, 2);
            bassEnv *= 0.28f; // synth fallback stays quiet under the pluck
        }
    }

    bool keyHit = false;
    if (! keysLive)
        keyHit = false;
    else if (layer == 0)
        keyHit = (step16 == 0);
    else if (layer == 1)
        keyHit = (step16 == 0 || step16 == 8);
    else if (layer == 2)
        keyHit = (step16 == 0 || step16 == 4 || step16 == 8 || step16 == 12);
    else
    {
        keyHit = (step16 == 0 || step16 == 4 || step16 == 8 || step16 == 12);
        const int extra = (st == Style::Rock || st == Style::Metal) ? 10 : 6;
        if (step16 == extra)
            keyHit = true;
    }

    if (keyHit)
    {
        setKeyVoicing (st, deg, keyPc);
        const float vel = 0.47f + 0.31f * inten;
        hitKeys (kv, vel);
        if (samples != nullptr)
        {
            const int keySlot = (kv == KeysVoice::Pad && samples->isReady (SampleBank::KeysStrings))
                                    ? SampleBank::KeysStrings
                                    : ((kv == KeysVoice::Piano || kv == KeysVoice::EP)
                                           && samples->isReady (SampleBank::KeysHammer)
                                           ? SampleBank::KeysHammer : -1);
            if (keySlot >= 0)
            {
                for (int v = 0; v < 4; ++v)
                {
                    const float hz = keyHz[(size_t) v];
                    if (hz < 20.0f) continue;
                    const float midiF = 69.0f + 12.0f * std::log2 (hz / 440.0f);
                    const float rate = std::pow (2.0f, (midiF - 60.0f) / 12.0f);
                    const float g = (kv == KeysVoice::Pad)
                                        ? vel * (v == 0 ? 0.48f : 0.22f)
                                        : vel * (v == 0 ? 0.55f : 0.28f);
                    samples->play (keySlot, g, rate, 3);
                }
                keyEnv *= (kv == KeysVoice::Pad ? 0.48f : 0.35f);
                keyTargetEnv *= (kv == KeysVoice::Pad ? 0.48f : 0.35f);
            }
        }
    }
}

void FollowerBand::applyPhaseNudge (double deltaSamples) noexcept
{
    if (samplesPer16th <= 1.0)
        return;
    // Arrangement already ignores raw |delta| < 12% of a 16th before capping
    // the request at 8%. Apply at most 6%; drop empty stores.
    if (std::abs (deltaSamples) <= 0.0)
        return;
    const double maxSlew = samplesPer16th * 0.06; // max 6% of a 16th
    const double d = juce::jlimit (-maxSlew, maxSlew, deltaSamples);
    const double len = juce::jmax (64.0, lastStepLen > 1.0 ? lastStepLen : samplesPer16th);
    double next = stepAccum + d;
    // Never push stepAccum across stepLen (no extra trigger from a nudge).
    if (stepAccum < len && next >= len)
        next = len * (1.0 - 1.0e-9);
    if (next < 0.0)
        next = 0.0;
    stepAccum = next;
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
    const auto kit = (DrumKit) juce::jlimit (0, (int) DrumKit::NumKits - 1,
                                             drumKit.load (std::memory_order_relaxed));
    const auto bv  = (BassVoice) juce::jlimit (0, (int) BassVoice::NumVoices - 1,
                                              bassVoice.load (std::memory_order_relaxed));
    const auto kv  = (KeysVoice) juce::jlimit (0, (int) KeysVoice::NumVoices - 1,
                                              keysVoice.load (std::memory_order_relaxed));
    const int timbreStamp = ((int) kit) | ((int) bv << 8) | ((int) kv << 16);
    if (timbreStamp != lastTimbreStamp)
    {
        applyTimbre (kit, bv, kv);
        drumEngine.setKit ((int) kit);
        lastTimbreStamp = timbreStamp;
    }
    const float bpmTarget = juce::jlimit (60.0f, 180.0f, bpmIn);
    // Clamp |dBPM| applied per block so the UI/engine cannot jump the clock.
    bpmApplied += juce::jlimit (-0.4f, 0.4f, bpmTarget - bpmApplied);
    const float inten = juce::jlimit (0.0f, 1.0f, intensity);
    const auto fl = getFeel();
    float swing = 0.0f;
    if (fl == Feel::Swing)
        swing = 0.62f;
    // Blues style swings off-8ths harder even on Grid feel.
    if (st == Style::Blues)
        swing = juce::jmax (swing, 0.72f);
    else if (fl == Feel::Swing && st == Style::Blues)
        swing = 0.78f;
    lastSwing = swing;

    const double newSp16 = (60.0 / (double) bpmApplied) * sampleRate / 4.0;
    if (samplesPer16th > 1.0 && newSp16 > 1.0)
        stepAccum *= newSp16 / samplesPer16th;
    samplesPer16th = newSp16;
    double feelBias = 1.0;
    if (fl == Feel::Ahead)  feelBias = 0.97;
    if (fl == Feel::Behind) feelBias = 1.03;

    keyPc = ((keyPc + keyShift) % 12 + 12) % 12;
    {
        const int canned = progressionDegree (barIndex);
        const int f = followDeg.load (std::memory_order_relaxed);
        currentDeg = (f >= 0) ? f : canned;
    }
    nextDeg = progressionDegree (barIndex + 1);
    chordDegAtom.store (currentDeg, std::memory_order_relaxed);
    nextDegAtom.store (nextDeg, std::memory_order_relaxed);
    soundingKeyAtom.store (keyPc, std::memory_order_relaxed);
    barIndexAtom.store (barIndex, std::memory_order_relaxed);
    absBarAtom.store (absBar, std::memory_order_relaxed);
    stepAtom.store (step, std::memory_order_relaxed);
    changeAtom.store ((nextDeg != currentDeg && step >= 12) ? 1 : 0, std::memory_order_relaxed);

    const int thinG = thinMask.load (std::memory_order_relaxed);
    const float drumsGain = (drumsOn.load (std::memory_order_relaxed) != 0 && (thinG & 0x1)) ? 1.0f : 0.0f;
    const float bassGain  = (bassOn.load (std::memory_order_relaxed) != 0 && (thinG & 0x2)) ? 1.0f : 0.0f;
    const float keysGain  = (keysOn.load (std::memory_order_relaxed) != 0 && (thinG & 0x4)) ? 1.0f : 0.0f;

    float bassDecay  = (st == Style::Funk)  ? 0.9968f
                     : (st == Style::Metal) ? 0.9990f
                     : (st == Style::Jazz)  ? 0.99962f
                     : 0.99955f;
    if (bv == BassVoice::Synth) bassDecay = juce::jmax (bassDecay, 0.99955f);
    if (bv == BassVoice::Slap)  bassDecay = juce::jmin (bassDecay, 0.9972f);

    for (int i = 0; i < numSamples; ++i)
    {
        fade += ((run ? 1.0f : 0.0f) - fade) * 0.0008f; // ~25 ms at 44.1k

        double stepLen = samplesPer16th * feelBias;
        if (swing > 0.0f)
        {
            if ((step & 1) == 0) stepLen = samplesPer16th * feelBias * (1.0 + 0.5 * (double) swing);
            else                 stepLen = samplesPer16th * feelBias * (1.0 - 0.5 * (double) swing);
        }
        stepLen *= stepHumanize; // once per triggered step, not every sample
        stepLen = juce::jmax (64.0, stepLen);
        lastStepLen = stepLen;

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
            stepHumanize = 1.0 + 0.012 * (double) noise();
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
                {
                    const int canned = progressionDegree (barIndex);
                    const int f = followDeg.load (std::memory_order_relaxed);
                    currentDeg = (f >= 0) ? f : canned;
                }
                nextDeg = progressionDegree (barIndex + 1);
                chordDegAtom.store (currentDeg);
                nextDegAtom.store (nextDeg);
                barIndexAtom.store (barIndex);
                absBarAtom.store (absBar);
            }
        }

        // --- drums (DrumEngine: body + sample layer + bus) ---
        float dL = 0.0f, dR = 0.0f;
        drumEngine.render (dL, dR);

                // --- bass ---
        float saw = oscSaw (bassPhase, bassHz);
        const float sqr = saw >= 0.0f ? 1.0f : -1.0f;
        float sub = oscSin (subPhase, bassHz * 0.5f);
        float bs = bassSawAmt * saw + bassSqrAmt * sqr + bassSubAmt * sub;
        if (bv == BassVoice::Slap)
            bs = 0.85f * sub * 1.15f + 0.35f * saw; // thump + a little edge
        if (bv == BassVoice::Pick)
            bs += 0.22f * saw * std::fabs (saw); // even mid grind under SAWVI
        bs = bassLp.process (bs) * bassEnv;
        bs += bassPluckAmt * saw * bassPluck;
        bs += bassPop * noise() * bassPopAmt;
        bassEnv *= bassDecay;
        bassPluck *= bassPluckDecay;
        bassPop *= 0.955f;
        bs = bs / (1.0f + std::abs (bs) * 0.4f);

        // --- keys ---
        const float lag = keyStab ? juce::jmin (1.0f, keyLag) : keyLag;
        keyEnv += (keyTargetEnv - keyEnv) * lag;
        keyTargetEnv *= keyTargetDecay;
        keyPercEnv *= keyPercDecay;

        float kMix = 0.0f;
        float kMixR = 0.0f;
        for (int v = 0; v < 4; ++v)
        {
            float& ph = keyPhase[(size_t) v];
            const float det = 1.0f + keyDetune * (v == 1 ? 1.0f : (v == 3 ? -1.15f : (v == 2 ? 0.35f : -0.25f)));
            const float fund = std::sin (juce::MathConstants<float>::twoPi * ph);
            const float harm = std::sin (juce::MathConstants<float>::twoPi * ph * 2.0f);
            const float odd  = std::sin (juce::MathConstants<float>::twoPi * ph * 3.0f);
            const float tine = std::sin (juce::MathConstants<float>::twoPi * ph * 6.17f);
            float s = fund + keyHarmonic * harm;
            if (kv == KeysVoice::Organ)
                s = fund + 0.55f * harm + 0.32f * odd + 0.18f * std::sin (juce::MathConstants<float>::twoPi * ph * 4.0f);
            else if (kv == KeysVoice::Pad)
                s = fund + 0.28f * harm + 0.08f * odd; // dark string, no hammer
            else if (kv == KeysVoice::Clav)
                s = fund + 0.42f * odd + 0.18f * std::sin (juce::MathConstants<float>::twoPi * ph * 5.0f);
            else if (kv == KeysVoice::EP)
                s = fund + keyHarmonic * harm + keyTine * tine;
            s += keyPercEnv * 0.28f * std::sin (juce::MathConstants<float>::twoPi * ph * 8.0f);
            ph += keyHz[(size_t) v] * det / (float) sampleRate;
            ph -= std::floor (ph);
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
        if (samples != nullptr)
        {
            float bS = 0, kS = 0, z = 0;
            samples->mix (2, bS, z);
            z = 0;
            samples->mix (3, kS, z);
            bs += bS;
            kMix += kS; kMixR += kS * 0.92f;
        }
        drumsL[i] = dL * gd;
        drumsR[i] = dR * gd;
        bassL[i]  = bs * gb;
        bassR[i]  = bs * gb * 0.92f;
        keysL[i]  = kMix * gk;
        keysR[i]  = kMixR * gk;
    }
}
