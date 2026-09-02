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
}
