#include "Daw/Project.h"
#include <algorithm>
#include "SessionSettings.h"

Project::Project (PluginHost& host)
{
    resetNew (host, "Untitled");
}

void Project::resetNew (PluginHost& host, const juce::String& n)
{
    const juce::ScopedLock sl (lock);
    name = n;
    folder = SessionSettings::projectsDir().getChildFile (n);
    bpm = 112.0f;
    loopStart = 0;
    loopEnd = (int64_t) (sampleRate * 60.0 / (double) bpm * 16.0); // 4 bars
    cycle = false;
    nextClipId = 1;
    undo.clear();
    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        auto& t = tracks[(size_t) i];
        t.id = i;
        t.name = Daw::defaultTrackName (i);
        t.kind = Daw::trackKind (i);
        t.level.store (i == Daw::kMasterIndex ? 0.90f : (i == Daw::kKeys ? 0.52f : 0.80f));
        t.pan.store (0.5f);
        t.mute.store (0);
        t.solo.store (0);
        t.arm.store (i == Daw::kGuitar || (i >= Daw::kDrums && i <= Daw::kKeys) ? 1 : 0);
        t.monitor.store (i == Daw::kGuitar || (i >= Daw::kDrums && i <= Daw::kKeys) ? 1 : 0);
        t.peak.store (0);
        t.clips.clear();
        t.inserts = std::make_unique<PluginRack> (host);
        t.inserts->setPersistSlots (false);
    }
}

void Project::prepare (double sr, int block)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    const int maxB = juce::jmax (block, 4096);
    for (auto& t : tracks)
    {
        t.work.setSize (2, maxB, false, true, true);
        if (t.inserts)
            t.inserts->prepare (sampleRate, maxB);
    }
    if (loopEnd <= 0)
        loopEnd = (int64_t) (sampleRate * 60.0 / (double) juce::jmax (40.0f, bpm) * 16.0);
}

void Project::ensureAudioFolder()
{
    if (! folder.isDirectory())
        folder.createDirectory();
    audioDir().createDirectory();
}

juce::File Project::audioDir() const
{
    return folder.getChildFile ("audio");
}

Daw::Clip* Project::findClip (int tr, int id)
{
    if (tr < 0 || tr >= Daw::kNumTracks)
        return nullptr;
    for (auto& c : tracks[(size_t) tr].clips)
        if (c && c->id == id)
            return c.get();
    return nullptr;
}

bool Project::anySolo() const noexcept
{
    for (int i = 0; i < Daw::kMasterIndex; ++i)
        if (tracks[(size_t) i].solo.load (std::memory_order_relaxed) != 0)
            return true;
    return false;
}

int64_t Project::endSamples() const
{
    int64_t e = loopEnd;
    for (int i = 0; i < Daw::kMasterIndex; ++i)
        for (auto& c : tracks[(size_t) i].clips)
            if (c)
                e = juce::jmax (e, c->startSamples + c->lengthSamples);
    return e;
}

bool Project::loadClipAudio (Daw::Clip& c)
{
    if (! c.file.existsAsFile())
        return false;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (c.file));
    if (r == nullptr)
        return false;
    const int n = (int) juce::jmin ((int64_t) juce::jmax (1, (int) r->lengthInSamples), (int64_t) r->lengthInSamples);
    c.audio.setSize (2, n, false, true, true);
    r->read (&c.audio, 0, n, 0, true, true);
    if (c.lengthSamples <= 0)
        c.lengthSamples = n;
    c.buildPeaks();
    c.ready.store (1, std::memory_order_release);
    return true;
}

void Project::pushMoveUndo (int tr, int clipId, int64_t from, int64_t to)
{
    Daw::UndoItem u;
    u.type = Daw::UndoItem::Move;
    u.track = tr;
    u.clipId = clipId;
    u.startA = from;
    u.startB = to;
    undo.push_back (std::move (u));
    if (undo.size() > 64)
        undo.erase (undo.begin());
}

void Project::pushDeleteUndo (int tr, std::unique_ptr<Daw::Clip> clip)
{
    Daw::UndoItem u;
    u.type = Daw::UndoItem::Delete;
    u.track = tr;
    u.clipId = clip ? clip->id : 0;
    u.snapshot = std::move (clip);
    undo.push_back (std::move (u));
    if (undo.size() > 64)
        undo.erase (undo.begin());
}

bool Project::undoLast()
{
    const juce::ScopedLock sl (lock);
    if (undo.empty())
        return false;
    auto u = std::move (undo.back());
    undo.pop_back();
    if (u.type == Daw::UndoItem::Move)
    {
        if (auto* c = findClip (u.track, u.clipId))
            c->startSamples = u.startA;
        return true;
    }
    if (u.type == Daw::UndoItem::Delete && u.snapshot)
    {
        tracks[(size_t) u.track].clips.push_back (std::move (u.snapshot));
        return true;
    }
    if (u.type == Daw::UndoItem::Add)
    {
        auto& clips = tracks[(size_t) u.track].clips;
        clips.erase (std::remove_if (clips.begin(), clips.end(),
                                     [&] (auto& c) { return c && c->id == u.clipId; }),
                     clips.end());
        return true;
    }
    return false;
}

juce::String Project::save()
{
    if (name.isEmpty())
        name = "Untitled";
    if (! folder.isDirectory())
        folder = SessionSettings::projectsDir().getChildFile (name);
    return saveAs (folder);
}

juce::String Project::saveAs (const juce::File& dest)
{
    folder = dest;
    ensureAudioFolder();
    juce::XmlElement xml ("SessionProject");
    xml.setAttribute ("name", name);
    xml.setAttribute ("bpm", bpm);
    xml.setAttribute ("sr", sampleRate);
    xml.setAttribute ("loopStart", (double) loopStart);
    xml.setAttribute ("loopEnd", (double) loopEnd);
    xml.setAttribute ("cycle", cycle ? 1 : 0);
    xml.setAttribute ("nextClipId", nextClipId);
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        auto& t = tracks[(size_t) i];
        auto* tx = xml.createNewChildElement ("Track");
        tx->setAttribute ("id", t.id);
        tx->setAttribute ("name", t.name);
        tx->setAttribute ("kind", (int) t.kind);
        tx->setAttribute ("level", t.level.load());
        tx->setAttribute ("pan", t.pan.load());
        tx->setAttribute ("mute", t.mute.load());
        tx->setAttribute ("solo", t.solo.load());
        tx->setAttribute ("arm", t.arm.load());
        tx->setAttribute ("monitor", t.monitor.load());
        for (auto& c : t.clips)
        {
            if (! c) continue;
            auto* cx = tx->createNewChildElement ("Clip");
            cx->setAttribute ("id", c->id);
            cx->setAttribute ("start", (double) c->startSamples);
            cx->setAttribute ("offset", (double) c->fileOffset);
            cx->setAttribute ("length", (double) c->lengthSamples);
            cx->setAttribute ("file", c->file.getRelativePathFrom (folder));
            cx->setAttribute ("name", c->name);
        }
    }
    if (! xml.writeTo (folder.getChildFile ("project.xml")))
        return "Failed to write project.xml";
    return {};
}

juce::String Project::load (const juce::File& dest, PluginHost& host)
{
    auto xf = dest.getChildFile ("project.xml");
    auto xml = juce::XmlDocument::parse (xf);
    if (xml == nullptr)
        return "Missing project.xml";
    const juce::ScopedLock sl (lock);
    folder = dest;
    name = xml->getStringAttribute ("name", dest.getFileName());
    bpm = (float) xml->getDoubleAttribute ("bpm", 112.0);
    sampleRate = xml->getDoubleAttribute ("sr", 44100.0);
    loopStart = xml->getIntAttribute ("loopStart", 0);
    loopEnd = (int64_t) xml->getDoubleAttribute ("loopEnd", 0);
    cycle = xml->getIntAttribute ("cycle", 0) != 0;
    nextClipId = xml->getIntAttribute ("nextClipId", 1);
    undo.clear();
    for (int i = 0; i < Daw::kNumTracks; ++i)
    {
        tracks[(size_t) i].clips.clear();
        if (tracks[(size_t) i].inserts == nullptr)
        {
            tracks[(size_t) i].inserts = std::make_unique<PluginRack> (host);
            tracks[(size_t) i].inserts->setPersistSlots (false);
        }
    }
    for (auto* tx = xml->getFirstChildElement(); tx != nullptr; tx = tx->getNextElement())
    {
        if (tx->getTagName() != "Track")
            continue;
        const int id = tx->getIntAttribute ("id", 0);
        if (id < 0 || id >= Daw::kNumTracks)
            continue;
        auto& t = tracks[(size_t) id];
        t.name = tx->getStringAttribute ("name", Daw::defaultTrackName (id));
        t.level.store ((float) tx->getDoubleAttribute ("level", 0.85));
        t.pan.store ((float) tx->getDoubleAttribute ("pan", 0.5));
        t.mute.store (tx->getIntAttribute ("mute", 0));
        t.solo.store (tx->getIntAttribute ("solo", 0));
        t.arm.store (tx->getIntAttribute ("arm", 0));
        const int monDef = (id == Daw::kGuitar || (id >= Daw::kDrums && id <= Daw::kKeys)) ? 1 : 0;
        t.monitor.store (tx->getIntAttribute ("monitor", monDef));
        for (auto* cx = tx->getFirstChildElement(); cx != nullptr; cx = cx->getNextElement())
        {
            if (cx->getTagName() != "Clip")
                continue;
            auto c = std::make_unique<Daw::Clip>();
            c->id = cx->getIntAttribute ("id", nextClipId++);
            c->startSamples = (int64_t) cx->getDoubleAttribute ("start", 0);
            c->fileOffset = (int64_t) cx->getDoubleAttribute ("offset", 0);
            c->lengthSamples = (int64_t) cx->getDoubleAttribute ("length", 0);
            c->name = cx->getStringAttribute ("name");
            auto rel = cx->getStringAttribute ("file");
            c->file = rel.isNotEmpty() ? folder.getChildFile (rel) : juce::File();
            loadClipAudio (*c);
            t.clips.push_back (std::move (c));
        }
    }
    return {};
}
