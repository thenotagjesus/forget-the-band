#include "Daw/DawEngine.h"
#include <cstring>
#include <vector>

DawEngine::DawEngine (PluginHost& h)
    : host (h), project (h)
{
}

void DawEngine::prepare (double sr, int block)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    maxBlock = juce::jmax (block, 4096);
    project.prepare (sampleRate, maxBlock);
    const int ring = juce::jmax (4096, (int) (sampleRate * 2.5));
    for (auto& c : rec)
    {
        c.fifo.setTotalSize (ring);
        c.ringL.assign ((size_t) ring, 0.0f);
        c.ringR.assign ((size_t) ring, 0.0f);
    }
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        pdcL[(size_t) t].assign ((size_t) kPdcSize, 0.0f);
        pdcR[(size_t) t].assign ((size_t) kPdcSize, 0.0f);
        pdcW[(size_t) t] = 0;
    }
}

void DawEngine::release()
{
    stopRecord();
    playing.store (0);
}

void DawEngine::play()
{
    playing.store (1, std::memory_order_relaxed);
}

void DawEngine::stop()
{
    playing.store (0, std::memory_order_relaxed);
    if (recording.load() != 0)
        stopRecord();
}

void DawEngine::returnToZero()
{
    position.store (0, std::memory_order_relaxed);
}

void DawEngine::setCycle (bool c) noexcept
{
    project.cycle = c;
}

float DawEngine::getTrackPeak (int t) const noexcept
{
    if (t < 0 || t >= Daw::kNumTracks)
        return 0.0f;
    return project.tracks[(size_t) t].peak.load (std::memory_order_relaxed);
}

void DawEngine::mixClips (int track, float* l, float* r, int64_t pos, int n) noexcept
{
    auto& tr = project.tracks[(size_t) track];
    for (auto& cp : tr.clips)
    {
        if (cp == nullptr || cp->ready.load (std::memory_order_acquire) == 0)
            continue;
        const int64_t cs = cp->startSamples;
        const int64_t ce = cs + cp->lengthSamples;
        if (pos + n <= cs || pos >= ce)
            continue;
        const int ch = cp->audio.getNumChannels();
        const int avail = cp->audio.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const int64_t abs = pos + i;
            if (abs < cs || abs >= ce)
                continue;
            const int local = (int) (abs - cs + cp->fileOffset);
            if (local < 0 || local >= avail)
                continue;
            const float sl = cp->audio.getSample (0, local);
            const float sr = ch > 1 ? cp->audio.getSample (1, local) : sl;
            l[i] += sl;
            r[i] += sr;
        }
    }
}

juce::String DawEngine::startRecord()
{
    stopRecord();
    project.ensureAudioFolder();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    juce::WavAudioFormat wav;
    recActive.store (0);
    for (int i = 0; i < Daw::kMasterIndex; ++i)
    {
        auto& c = rec[(size_t) i];
        c.writer.reset();
        c.samplesWritten = 0;
        c.firstWritePos = -1;
        c.fifo.reset();
        const bool on = project.tracks[(size_t) i].arm.load (std::memory_order_relaxed) != 0;
        c.armed.store (on ? 1 : 0);
        if (! on)
            continue;
        c.file = project.audioDir().getChildFile (
            "take-" + stamp + "-t" + juce::String (i) + ".wav");
        auto out = c.file.createOutputStream();
        if (out == nullptr)
            return "Failed to create take: " + c.file.getFullPathName();
        c.writer.reset (wav.createWriterFor (out.release(), sampleRate, 2u, 32, {}, 0));
        if (c.writer == nullptr)
            return "Failed to open WAV writer";
        recActive.fetch_add (1);
    }
    recordOrigin = position.load (std::memory_order_relaxed);
    recording.store (1, std::memory_order_relaxed);
    writer = std::make_unique<Worker> ("DawTakes", [this] { writerLoop(); });
    writer->startThread();
    return {};
}

void DawEngine::stopRecord()
{
    const bool was = recording.exchange (0) != 0;
    if (writer)
    {
        writer->signalThreadShouldExit();
        writer->stopThread (8000);
        writer.reset();
    }
    if (was)
        finishTakes();
}

void DawEngine::writerLoop()
{
    std::array<float, 2048> sl {}, sr {};
    while (! juce::Thread::getCurrentThread()->threadShouldExit() || recActive.load() > 0)
    {
        bool any = false;
        for (int t = 0; t < Daw::kMasterIndex; ++t)
        {
            auto& c = rec[(size_t) t];
            if (c.writer == nullptr)
                continue;
            const int ready = c.fifo.getNumReady();
            if (ready <= 0)
                continue;
            any = true;
            const int n = juce::jmin (ready, 2048);
            int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
            c.fifo.prepareToRead (n, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i)
            {
                sl[(size_t) i] = c.ringL[(size_t) (s1 + i)];
                sr[(size_t) i] = c.ringR[(size_t) (s1 + i)];
            }
            for (int i = 0; i < n2; ++i)
            {
                sl[(size_t) (n1 + i)] = c.ringL[(size_t) (s2 + i)];
                sr[(size_t) (n1 + i)] = c.ringR[(size_t) (s2 + i)];
            }
            c.fifo.finishedRead (n1 + n2);
            const float* ch[2] = { sl.data(), sr.data() };
            c.writer->writeFromFloatArrays (ch, 2, n1 + n2);
            c.samplesWritten += n1 + n2;
        }
        if (! any)
        {
            if (recording.load() == 0)
                break;
            juce::Thread::sleep (2);
        }
    }
}

void DawEngine::finishTakes()
{
    const juce::ScopedLock sl (project.lock);
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        auto& c = rec[(size_t) t];
        c.writer.reset();
        if (c.samplesWritten <= 0 || c.armed.load() == 0)
            continue;
        auto clip = std::make_unique<Daw::Clip>();
        clip->id = project.nextClipId++;
        const int64_t origin = (c.firstWritePos >= 0) ? c.firstWritePos : recordOrigin;
        clip->startSamples = juce::jmax ((int64_t) 0, origin - (int64_t) pdcSamples (t));
        clip->lengthSamples = c.samplesWritten;
        clip->file = c.file;
        clip->name = c.file.getFileNameWithoutExtension();
        if (project.cycle && project.loopEnd > project.loopStart)
            punchTrim (t, project.loopStart, project.loopEnd);
        project.loadClipAudio (*clip);
        Daw::UndoItem u;
        u.type = Daw::UndoItem::Add;
        u.track = t;
        u.clipId = clip->id;
        project.undo.push_back (std::move (u));
        project.tracks[(size_t) t].clips.push_back (std::move (clip));
        c.armed.store (0);
        c.samplesWritten = 0;
    }
    recActive.store (0);
}

void DawEngine::process (float* guitarL, float* guitarR,
                         float* drumsL, float* drumsR,
                         float* bassL, float* bassR,
                         float* keysL, float* keysR,
                         float* masterL, float* masterR,
                         int n,
                         bool sessionLive,
                         const juce::MidiBuffer& midi) noexcept
{
    if (n <= 0 || masterL == nullptr)
        return;

    if (project.tracks[(size_t) Daw::kMasterIndex].work.getNumSamples() < n)
    {
        std::memset (masterL, 0, (size_t) n * sizeof (float));
        if (masterR != nullptr)
            std::memset (masterR, 0, (size_t) n * sizeof (float));
        return;
    }

    const bool run = playing.load (std::memory_order_relaxed) != 0
                  || recording.load (std::memory_order_relaxed) != 0;
    int64_t pos = position.load (std::memory_order_relaxed);
    if (project.cycle && project.loopEnd > project.loopStart && pos >= project.loopEnd)
        pos = project.loopStart;

    const juce::CriticalSection::ScopedTryLockType sl (project.lock);
    const bool locked = sl.isLocked();

    const bool anySolo = project.anySolo();
    float* liveL[4] = { guitarL, drumsL, bassL, keysL };
    float* liveR[4] = { guitarR, drumsR, bassR, keysR };
    const int liveMap[4] = { Daw::kGuitar, Daw::kDrums, Daw::kBass, Daw::kKeys };

    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        auto& tr = project.tracks[(size_t) t];
        float* wl = tr.work.getWritePointer (0);
        float* wr = tr.work.getWritePointer (1);
        std::memset (wl, 0, (size_t) n * sizeof (float));
        std::memset (wr, 0, (size_t) n * sizeof (float));

        if (run && locked)
            mixClips (t, wl, wr, pos, n);

        for (int k = 0; k < 4; ++k)
        {
            if (liveMap[k] != t || liveL[k] == nullptr)
                continue;
            const bool mon = tr.monitor.load (std::memory_order_relaxed) != 0;
            const bool bandLive = (t >= Daw::kDrums && sessionLive);
            if (mon || bandLive)
            {
                for (int i = 0; i < n; ++i)
                {
                    wl[i] += liveL[k][i];
                    wr[i] += liveR[k][i];
                }
            }
        }

        if (tr.mute.load (std::memory_order_relaxed) != 0
            || (anySolo && tr.solo.load (std::memory_order_relaxed) == 0))
        {
            std::memset (wl, 0, (size_t) n * sizeof (float));
            std::memset (wr, 0, (size_t) n * sizeof (float));
        }

        if (tr.inserts != nullptr && tr.inserts->hasReadySlot())
            tr.inserts->processChain (wl, wr, n, midi);

        const float g = tr.level.load (std::memory_order_relaxed);
        const float pan = tr.pan.load (std::memory_order_relaxed);
        const float gl = g * std::cos (pan * juce::MathConstants<float>::halfPi);
        const float gr = g * std::sin (pan * juce::MathConstants<float>::halfPi);
        float pk = 0;
        for (int i = 0; i < n; ++i)
        {
            wl[i] *= gl;
            wr[i] *= gr;
            pk = juce::jmax (pk, std::abs (wl[i]), std::abs (wr[i]));
        }
        tr.peak.store (tr.peak.load (std::memory_order_relaxed) * 0.6f + pk * 0.4f,
                       std::memory_order_relaxed);
    }

    int lat[Daw::kMasterIndex] = {};
    int maxLat = 0;
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        int l = 0;
        if (auto* ins = project.tracks[(size_t) t].inserts.get())
            l += ins->getLatencySamples();
        if (t == Daw::kGuitar)
            l += guitarRackLatency.load (std::memory_order_relaxed);
        lat[t] = l;
        maxLat = juce::jmax (maxLat, l);
    }
    maxLat = juce::jmin (maxLat, kPdcSize - 1);
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        const int d = juce::jmax (0, maxLat - lat[t]);
        auto& tr = project.tracks[(size_t) t];
        float* wl = tr.work.getWritePointer (0);
        float* wr = tr.work.getWritePointer (1);
        auto& dl = pdcL[(size_t) t];
        auto& dr = pdcR[(size_t) t];
        int w = pdcW[(size_t) t];
        if ((int) dl.size() < kPdcSize)
            continue; // not prepared; skip delay
        for (int i = 0; i < n; ++i)
        {
            dl[(size_t) w] = wl[i];
            dr[(size_t) w] = wr[i];
            const int r = (w - d + kPdcSize) % kPdcSize;
            wl[i] = dl[(size_t) r];
            wr[i] = dr[(size_t) r];
            w = (w + 1) % kPdcSize;
        }
        pdcW[(size_t) t] = w;
    }

    auto& mas = project.tracks[(size_t) Daw::kMasterIndex];
    float* ml = mas.work.getWritePointer (0);
    float* mr = mas.work.getWritePointer (1);
    std::memset (ml, 0, (size_t) n * sizeof (float));
    std::memset (mr, 0, (size_t) n * sizeof (float));
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        const float* wl = project.tracks[(size_t) t].work.getReadPointer (0);
        const float* wr = project.tracks[(size_t) t].work.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            ml[i] += wl[i];
            mr[i] += wr[i];
        }
    }

    if (mas.inserts != nullptr && mas.inserts->hasReadySlot())
        mas.inserts->processChain (ml, mr, n, midi);

    const float mg = mas.mute.load() ? 0.0f : mas.level.load (std::memory_order_relaxed);
    float pk = 0;
    for (int i = 0; i < n; ++i)
    {
        masterL[i] = ml[i] * mg;
        masterR[i] = mr[i] * mg;
        pk = juce::jmax (pk, std::abs (masterL[i]), std::abs (masterR[i]));
    }
    mas.peak.store (mas.peak.load() * 0.6f + pk * 0.4f, std::memory_order_relaxed);

    if (recording.load (std::memory_order_relaxed) != 0)
    {
        for (int t = 0; t < Daw::kMasterIndex; ++t)
        {
            auto& c = rec[(size_t) t];
            if (c.armed.load (std::memory_order_relaxed) == 0)
                continue;

            // INPUT ONLY. Never the post-mixClips work buffer (that rebakes old takes).
            const float* srcL = guitarL;
            const float* srcR = guitarR;
            if (t == Daw::kDrums) { srcL = drumsL; srcR = drumsR; }
            else if (t == Daw::kBass) { srcL = bassL; srcR = bassR; }
            else if (t == Daw::kKeys) { srcL = keysL; srcR = keysR; }
            if (srcL == nullptr)
                continue;
            if (srcR == nullptr)
                srcR = srcL;

            int i = 0;
            while (i < n)
            {
                if (! inPunchWindow (pos + i))
                {
                    ++i;
                    continue;
                }
                int j = i + 1;
                while (j < n && inPunchWindow (pos + j))
                    ++j;
                const int len = j - i;
                if (c.firstWritePos < 0)
                    c.firstWritePos = pos + i;
                const int room = c.fifo.getFreeSpace();
                const int take = juce::jmin (len, room);
                if (take <= 0)
                    break;
                int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
                c.fifo.prepareToWrite (take, s1, n1, s2, n2);
                for (int k = 0; k < n1; ++k)
                {
                    c.ringL[(size_t) (s1 + k)] = srcL[i + k];
                    c.ringR[(size_t) (s1 + k)] = srcR[i + k];
                }
                for (int k = 0; k < n2; ++k)
                {
                    c.ringL[(size_t) (s2 + k)] = srcL[i + n1 + k];
                    c.ringR[(size_t) (s2 + k)] = srcR[i + n1 + k];
                }
                c.fifo.finishedWrite (n1 + n2);
                i = j;
            }
        }
    }

    if (run)
    {
        pos += n;
        if (project.cycle && project.loopEnd > project.loopStart && pos >= project.loopEnd)
            pos = project.loopStart + (pos - project.loopEnd);
        position.store (pos, std::memory_order_relaxed);
    }
}

bool DawEngine::inPunchWindow (int64_t abs) const noexcept
{
    if (! project.cycle || project.loopEnd <= project.loopStart)
        return true;
    return abs >= project.loopStart && abs < project.loopEnd;
}

int DawEngine::pdcSamples (int track) const noexcept
{
    int lat = inputLatency.load (std::memory_order_relaxed)
            + outputLatency.load (std::memory_order_relaxed);
    if (track == Daw::kGuitar)
        lat += guitarRackLatency.load (std::memory_order_relaxed);
    if (track >= 0 && track < Daw::kNumTracks)
    {
        auto& tr = project.tracks[(size_t) track];
        if (tr.inserts != nullptr)
            lat += tr.inserts->getLatencySamples();
    }
    return juce::jmax (0, lat);
}

void DawEngine::punchTrim (int track, int64_t a, int64_t b)
{
    if (track < 0 || track >= Daw::kMasterIndex || b <= a)
        return;
    auto& clips = project.tracks[(size_t) track].clips;
    std::vector<std::unique_ptr<Daw::Clip>> keep;
    keep.reserve (clips.size() + 1);
    for (auto& cp : clips)
    {
        if (! cp)
            continue;
        const int64_t cs = cp->startSamples;
        const int64_t ce = cs + cp->lengthSamples;
        if (ce <= a || cs >= b)
        {
            keep.push_back (std::move (cp));
            continue;
        }
        auto cloneBody = [] (Daw::Clip& dst, const Daw::Clip& src)
        {
            dst.file = src.file;
            dst.name = src.name;
            dst.audio = src.audio;
            dst.peaks = src.peaks;
            dst.ready.store (src.ready.load (std::memory_order_relaxed));
        };
        if (cs < a)
        {
            auto left = std::make_unique<Daw::Clip>();
            left->id = project.nextClipId++;
            left->startSamples = cs;
            left->fileOffset = cp->fileOffset;
            left->lengthSamples = a - cs;
            cloneBody (*left, *cp);
            keep.push_back (std::move (left));
        }
        if (ce > b)
        {
            auto right = std::make_unique<Daw::Clip>();
            right->id = project.nextClipId++;
            right->startSamples = b;
            right->fileOffset = cp->fileOffset + (b - cs);
            right->lengthSamples = ce - b;
            cloneBody (*right, *cp);
            keep.push_back (std::move (right));
        }
    }
    clips.swap (keep);
}

juce::String DawEngine::bounceMixdown (const juce::File& dest)
{
    const juce::ScopedLock sl (project.lock);
    const int64_t total = juce::jmax ((int64_t) 1, project.endSamples());
    juce::AudioBuffer<float> mix (2, (int) juce::jmin (total, (int64_t) (sampleRate * 60 * 45)));
    mix.clear();
    const int N = mix.getNumSamples();
    for (int t = 0; t < Daw::kMasterIndex; ++t)
    {
        auto& tr = project.tracks[(size_t) t];
        if (tr.mute.load() != 0)
            continue;
        const float g = tr.level.load();
        const float pan = tr.pan.load();
        const float gl = g * std::cos (pan * juce::MathConstants<float>::halfPi);
        const float gr = g * std::sin (pan * juce::MathConstants<float>::halfPi);
        for (auto& cp : tr.clips)
        {
            if (! cp || cp->ready.load() == 0)
                continue;
            const int ch = cp->audio.getNumChannels();
            const int avail = cp->audio.getNumSamples();
            for (int i = 0; i < avail; ++i)
            {
                const int destI = (int) (cp->startSamples + i);
                if (destI < 0 || destI >= N)
                    continue;
                const float sl = cp->audio.getSample (0, i);
                const float sr = ch > 1 ? cp->audio.getSample (1, i) : sl;
                mix.addSample (0, destI, sl * gl);
                mix.addSample (1, destI, sr * gr);
            }
        }
    }
    dest.getParentDirectory().createDirectory();
    juce::WavAudioFormat wav;
    auto out = dest.createOutputStream();
    if (out == nullptr)
        return "Could not create mixdown file";
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (out.release(), sampleRate, 2u, 32, {}, 0));
    if (w == nullptr)
        return "Could not write mixdown WAV";
    w->writeFromAudioSampleBuffer (mix, 0, N);
    return {};
}
