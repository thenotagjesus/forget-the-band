#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace Daw
{
    constexpr int kAudioTracks = 8;
    constexpr int kBandTracks  = 3;
    constexpr int kMasterIndex = 11;
    constexpr int kNumTracks   = 12; // 8 audio + drums/bass/keys + master

    constexpr int kGuitar = 0;
    constexpr int kDrums  = 8;
    constexpr int kBass   = 9;
    constexpr int kKeys   = 10;

    enum class Kind : int { Audio = 0, Band, Master };

    inline const char* defaultTrackName (int i)
    {
        static const char* n[] = {
            "Guitar", "Audio 2", "Audio 3", "Audio 4",
            "Audio 5", "Audio 6", "Audio 7", "Audio 8",
            "Drums", "Bass", "Keys", "Master"
        };
        if (i < 0 || i >= kNumTracks) return "Track";
        return n[i];
    }

    inline Kind trackKind (int i)
    {
        if (i == kMasterIndex) return Kind::Master;
        if (i >= kDrums && i <= kKeys) return Kind::Band;
        return Kind::Audio;
    }

    struct Clip
    {
        int id = 0;
        int64_t startSamples = 0;
        int64_t fileOffset = 0;
        int64_t lengthSamples = 0;
        juce::File file;
        juce::String name;
        juce::AudioBuffer<float> audio;
        std::vector<float> peaks;
        std::atomic<int> ready { 0 };

        void buildPeaks()
        {
            peaks.clear();
            if (audio.getNumSamples() <= 0)
                return;
            const int hop = 256;
            const int n = audio.getNumSamples();
            peaks.reserve ((size_t) (n / hop + 1));
            for (int i = 0; i < n; i += hop)
            {
                float m = 0.0f;
                const int last = juce::jmin (n, i + hop);
                for (int c = 0; c < audio.getNumChannels(); ++c)
                    for (int s = i; s < last; ++s)
                        m = juce::jmax (m, std::abs (audio.getSample (c, s)));
                peaks.push_back (m);
            }
        }
    };

    struct UndoItem
    {
        enum Type { Move, Delete, Add } type = Move;
        int track = 0;
        int clipId = 0;
        int64_t startA = 0, startB = 0;
        std::unique_ptr<Clip> snapshot;
    };

    /** Monophonic guitar transcription event. Times are quarter-notes from session/project zero. */
    struct NoteEvent
    {
        double startQuarter = 0.0;
        double durationQuarter = 0.0;
        int midi = -1;
        char name[8] {};      // "Eb3"
        float cents = 0.0f;
        float rms = 0.0f;
        float velocity = 0.0f;
        float confidence = 0.0f;

        void setNameFromMidi (int note)
        {
            static const char* spell[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            name[0] = 0;
            if (note < 0 || note > 127)
            {
                name[0] = '-'; name[1] = '-'; name[2] = 0;
                return;
            }
            const char* pc = spell[note % 12];
            const int oct = note / 12 - 1;
            int i = 0;
            while (pc[i] != 0 && i < 4) { name[i] = pc[i]; ++i; }
            if (oct < 0) { name[i++] = '-'; name[i++] = (char) ('0' + (-oct)); }
            else         { name[i++] = (char) ('0' + oct); }
            name[i] = 0;
        }
    };

    struct LiveNote
    {
        float startBeat = 0.0f;
        float durBeat = 0.0f;
        int midi = -1;
        float cents = 0.0f;
        float vel = 0.0f;
        int active = 0;
    };
}
