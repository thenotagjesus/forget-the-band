#pragma once

#include <JuceHeader.h>
#include "DSP/Biquad.h"
#include "DSP/SampleBank.h"
#include "DSP/DrumEngine.h"
#include <array>
#include <atomic>
#include <cstdint>

/** Synthesized follower trio: drums, bass, keys. Live-session pocket. RT-safe process(). */
class FollowerBand
{
public:
    enum class Style : int
    {
        Rock = 0,
        Blues,
        Metal,
        Funk,
        Jazz,
        NumStyles
    };

    enum class DrumKit : int
    {
        Acoustic = 0,
        Metal,
        Jazz,
        Funk,
        Electro,
        NumKits
    };

    enum class BassVoice : int
    {
        Finger = 0,
        Pick,
        Synth,
        Slap,
        NumVoices
    };

    enum class KeysVoice : int
    {
        Piano = 0,
        EP,
        Organ,
        Pad,
        Clav,
        NumVoices
    };

    enum class Form : int { Vamp = 0, Song, TwelveBar, Wander, NumForms };
    enum class Scale : int { Major = 0, Minor, Pentatonic, Blues, NumScales };
    enum class Feel : int { Grid = 0, Ahead, Behind, Swing, NumFeels };

    enum Degree : int
    {
        DegI = 0,
        DegIV,
        DegV,
        Degvi,
        DegbIII,
        DegbVI,
        DegbVII,
        Degii,
        NumDegrees
    };

    void prepare (double sampleRate, SampleBank* samples = nullptr);
    void reset() noexcept;

    void setStyle (Style s) noexcept { style.store ((int) s, std::memory_order_relaxed); }
    void setDrumKit (DrumKit k) noexcept { drumKit.store ((int) k, std::memory_order_relaxed); }
    void setBassVoice (BassVoice v) noexcept { bassVoice.store ((int) v, std::memory_order_relaxed); }
    void setKeysVoice (KeysVoice v) noexcept { keysVoice.store ((int) v, std::memory_order_relaxed); }
    void setForm (Form f) noexcept { form.store ((int) f, std::memory_order_relaxed); }
    void setScale (Scale s) noexcept { scaleKind.store ((int) s, std::memory_order_relaxed); }
    void setFeel (Feel f) noexcept { feel.store ((int) f, std::memory_order_relaxed); }
    void setPhraseBars (int n) noexcept
    {
        const int v = (n <= 4 ? 4 : (n <= 8 ? 8 : 16));
        phraseBars.store (v, std::memory_order_relaxed);
    }
    void setEnabled (bool e) noexcept { enabled.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed) != 0; }

    void setFollowedDegree (int deg) noexcept { followDeg.store (deg, std::memory_order_relaxed); }
    void setThinMask (int mask) noexcept { thinMask.store (mask, std::memory_order_relaxed); }
    void applyPhaseNudge (double deltaSamples) noexcept;
    /** Audio thread: strong guitar onset near a beat may reinforce kick. */
    void noteGuitarOnset (float strength) noexcept;
    double getStepAccum() const noexcept { return stepAccum; }
    double getSamplesPer16th() const noexcept { return samplesPer16th; }

    enum Member : int { MemberDrums = 0, MemberBass, MemberKeys, MemberFx, NumMembers };

    void setMemberEnabled (int member, bool e) noexcept
    {
        const int v = e ? 1 : 0;
        if (member == MemberDrums) drumsOn.store (v, std::memory_order_relaxed);
        else if (member == MemberBass) bassOn.store (v, std::memory_order_relaxed);
        else if (member == MemberKeys) keysOn.store (v, std::memory_order_relaxed);
        else if (member == MemberFx)   fxOn.store (v, std::memory_order_relaxed);
    }
    bool isMemberEnabled (int member) const noexcept
    {
        if (member == MemberDrums) return drumsOn.load (std::memory_order_relaxed) != 0;
        if (member == MemberBass)  return bassOn.load (std::memory_order_relaxed) != 0;
        if (member == MemberKeys)  return keysOn.load (std::memory_order_relaxed) != 0;
        if (member == MemberFx)    return fxOn.load (std::memory_order_relaxed) != 0;
        return false;
    }

    Style getStyle() const noexcept { return (Style) style.load (std::memory_order_relaxed); }
    DrumKit   getDrumKit()   const noexcept { return (DrumKit) drumKit.load (std::memory_order_relaxed); }
    BassVoice getBassVoice() const noexcept { return (BassVoice) bassVoice.load (std::memory_order_relaxed); }
    KeysVoice getKeysVoice() const noexcept { return (KeysVoice) keysVoice.load (std::memory_order_relaxed); }
    Form  getForm()  const noexcept { return (Form) form.load (std::memory_order_relaxed); }
    Scale getScale() const noexcept { return (Scale) scaleKind.load (std::memory_order_relaxed); }
    Feel  getFeel()  const noexcept { return (Feel) feel.load (std::memory_order_relaxed); }
    int   getPhraseBars() const noexcept { return phraseBars.load (std::memory_order_relaxed); }

    /** Audio thread. Reads key/bpm/intensity atomics passed in. Writes stereo buses. */
    void process (int keyPc,
                  float bpm,
                  float intensity,
                  float* drumsL, float* drumsR,
                  float* bassL,  float* bassR,
                  float* keysL,  float* keysR,
                  int numSamples) noexcept;

    int   getBarIndex()    const noexcept { return barIndexAtom.load (std::memory_order_relaxed); }
    int   getAbsBarIndex() const noexcept { return absBarAtom.load (std::memory_order_relaxed); }
    int   getStepInBar()   const noexcept { return stepAtom.load (std::memory_order_relaxed); }
    int   getBeatInBar()   const noexcept { return stepAtom.load (std::memory_order_relaxed) / 4; }
    float getBeatFraction() const noexcept
    {
        return (float) stepAtom.load (std::memory_order_relaxed) / 16.0f;
    }
    int   getChordDegree() const noexcept { return chordDegAtom.load (std::memory_order_relaxed); }
    int   getNextDegree()  const noexcept { return nextDegAtom.load (std::memory_order_relaxed); }
    int   getSoundingKey() const noexcept { return soundingKeyAtom.load (std::memory_order_relaxed); }
    float getSwing()       const noexcept { return lastSwing; }
    bool  isFillBar()      const noexcept { return fillAtom.load (std::memory_order_relaxed) != 0; }
    bool  isCrashDownbeat() const noexcept { return crashAtom.load (std::memory_order_relaxed) != 0; }
    bool  isChangeComing() const noexcept { return changeAtom.load (std::memory_order_relaxed) != 0; }

    juce::String chordName() const;
    juce::String nextChordName() const;
    juce::String romanName() const { return degreeName (getChordDegree()); }

    static const char* styleName (int i);
    static const char* drumKitName (int i);
    static const char* bassVoiceName (int i);
    static const char* keysVoiceName (int i);
    static void styleVoiceDefaults (int style, int& kit, int& bass, int& keys) noexcept;
    static const char* formName (int i);
    static const char* playerFormName (int i); // Jam / Radio / 12-Bar / Changes
    static const char* scaleName (int i);
    static const char* feelName (int i);
    static const char* degreeName (int deg);
    static juce::String namedChord (int keyPc, int deg, Scale sc, Style st);
    static bool scaleHas (Scale sc, int interval) noexcept;

private:
    float noise() noexcept;
    float oscSaw (float& phase, float freq) noexcept;
    float oscSin (float& phase, float freq) noexcept;
    void  applyTimbre (DrumKit kit, BassVoice bv, KeysVoice kv) noexcept;
    void  hitKeys (KeysVoice kv, float vel) noexcept;
    float midiToHz (float midi) const noexcept;
    int   degreeSemitones (int deg) const noexcept;
    bool  degreeIsMinor (Style st, int deg) const noexcept;
    int   progressionLength() const noexcept;
    int   progressionDegree (int bar) const noexcept;
    void  triggerStep (int step, Style st, float inten, int deg, int upcoming, int keyPc) noexcept;
    int   pickBass (Style st, int step, float inten, int deg, int upcoming, int keyPc) noexcept;
    void  setKeyVoicing (Style st, int deg, int keyPc) noexcept;
    void  decideFill (Style st, float inten) noexcept;

    std::atomic<int> style { (int) Style::Rock };
    std::atomic<int> drumKit { (int) DrumKit::Acoustic };
    std::atomic<int> bassVoice { (int) BassVoice::Finger };
    std::atomic<int> keysVoice { (int) KeysVoice::Piano };
    std::atomic<int> form { (int) Form::Song };
    std::atomic<int> scaleKind { (int) Scale::Pentatonic };
    std::atomic<int> feel { (int) Feel::Grid };
    std::atomic<int> phraseBars { 8 };
    std::atomic<int> enabled { 0 };
    std::atomic<int> drumsOn { 1 };
    std::atomic<int> bassOn { 0 };
    std::atomic<int> keysOn { 0 };
    std::atomic<int> fxOn { 0 };
    std::atomic<int> followDeg { -1 };
    std::atomic<int> thinMask { 0x1 };
    std::atomic<int> barIndexAtom { 0 };
    std::atomic<int> absBarAtom { 0 };
    std::atomic<int> stepAtom { 0 };
    std::atomic<int> chordDegAtom { (int) DegI };
    std::atomic<int> nextDegAtom { (int) DegIV };
    std::atomic<int> soundingKeyAtom { 4 };
    std::atomic<int> fillAtom { 0 };
    std::atomic<int> crashAtom { 0 };
    std::atomic<int> changeAtom { 0 };

    double sampleRate = 44100.0;
    double stepAccum = 0.0;
    bool fireImmediate = true;
    double samplesPer16th = 5512.5;
    double lastStepLen = 5512.5;
    double stepHumanize = 1.0;
    float bpmApplied = 96.0f;
    int step = 0;
    int barIndex = 0;
    int absBar = 0;
    int currentDeg = DegI;
    int nextDeg = DegIV;
    float lastSwing = 0.0f;
    float fade = 0.0f;

    float kickEnv = 0, snareEnv = 0, hatEnv = 0, rideEnv = 0, crashEnv = 0, tomEnv = 0;
    float kickPhase = 0, snareTonePhase = 0, hatPhase = 0, ridePhase = 0, tomPhase = 0;
    float kickHz = 60.0f;
    float tomHz = 140.0f;
    float hatDecayUse = 0.9982f;
    float kickClick = 0;
    float kickPitchRate = 0.99915f;
    float kickFloorHz = 42.0f;
    float kickNoiseAmt = 0.10f;
    float kickDecayUse = 0.9994f;
    float snareDecayUse = 0.9991f;
    float snareToneAmt = 0.28f;
    float snareToneHz = 188.0f;
    float hatGain = 1.0f;
    float rideGain = 1.0f;
    bool hatOpen = false;
    bool fillThisBar = false;
    bool pendingCrash = false;
    int  fillVariant = 0; // 0 tom run, 1 snare build, 2 crash accent
    std::atomic<int> pendingOnsetKick { 0 }; // 0 idle, 1 this beat, 2 next downbeat
    float pendingOnsetStr = 0.0f;

    float bassPhase = 0, bassEnv = 0, bassHz = 82.41f;
    float subPhase = 0;
    float bassPluck = 0;
    float bassPop = 0;
    float bassSawAmt = 1.0f;
    float bassSqrAmt = 0.0f;
    float bassSubAmt = 0.35f;
    float bassPluckAmt = 0.22f;
    float bassPluckDecay = 0.9945f;
    float bassPopAmt = 0.0f;

    std::array<float, 4> keyPhase {};
    std::array<float, 4> keyHz {};
    float keyEnv = 0;
    float keyTargetEnv = 0;
    float keyLag = 0.0004f;
    float keyHarmonic = 0.08f;
    float keyTine = 0.0f;
    float keyDetune = 0.0f;
    float keyPercEnv = 0;
    float keyPercDecay = 0.996f;
    float keyTargetDecay = 0.9995f;
    bool keyStab = false;
    int lastTimbreStamp = -1;

    int lastBassMidi = 40;
    int keyShift = 0;
    uint32_t rng = 0xC3A10Fu;

    Biquad hatHp, snareBp, bassLp, keyLp, tomBp, kickHp;

    SampleBank* samples = nullptr;
    DrumEngine drumEngine;
};
