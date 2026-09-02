#pragma once

#include "Daw/DawModel.h"
#include "Plugins/PluginRack.h"
#include <array>

class Project
{
public:
    struct Track
    {
        int id = 0;
        juce::String name;
        Daw::Kind kind = Daw::Kind::Audio;
        std::atomic<float> level { 0.85f };
        std::atomic<float> pan { 0.5f };
        std::atomic<int> mute { 0 }, solo { 0 }, arm { 0 }, monitor { 0 };
        std::atomic<float> peak { 0 };
        std::vector<std::unique_ptr<Daw::Clip>> clips;
        std::unique_ptr<PluginRack> inserts;
        juce::AudioBuffer<float> work;
    };

    explicit Project (PluginHost& host);
    void resetNew (PluginHost& host, const juce::String& name);
    juce::String save (PluginRack* guitarRack = nullptr);
    juce::String saveAs (const juce::File& folder, PluginRack* guitarRack = nullptr);
    juce::String load (const juce::File& folder, PluginHost& host, PluginRack* guitarRack = nullptr);
    void prepare (double sampleRate, int block);
    void ensureAudioFolder();

    juce::CriticalSection lock;
    juce::String name { "Untitled" };
    juce::File folder;
    double sampleRate = 44100.0;
    float bpm = 112.0f;
    int64_t loopStart = 0;
    int64_t loopEnd = 0;
    bool cycle = false;
    int nextClipId = 1;
    std::array<Track, Daw::kNumTracks> tracks;
    std::vector<Daw::UndoItem> undo;
    std::vector<Daw::NoteEvent> notes;

    Track& track (int i) { return tracks[(size_t) juce::jlimit (0, Daw::kNumTracks - 1, i)]; }
    const Track& track (int i) const { return tracks[(size_t) juce::jlimit (0, Daw::kNumTracks - 1, i)]; }

    Daw::Clip* findClip (int track, int id);
    void pushMoveUndo (int track, int clipId, int64_t from, int64_t to);
    void pushDeleteUndo (int track, std::unique_ptr<Daw::Clip> clip);
    bool undoLast();
    bool loadClipAudio (Daw::Clip& c);
    juce::File audioDir() const;
    int64_t endSamples() const;
    bool anySolo() const noexcept;
};
