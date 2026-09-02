#include "Recording/StemWriter.h"
#include <cstring>

const char* StemWriter::stemFileName (int s)
{
    static const char* n[] = { "guitar.wav", "drums.wav", "bass.wav", "keys.wav", "master.wav" };
    if (s < 0 || s >= NumStems) return "unknown.wav";
    return n[s];
}

StemWriter::StemWriter() = default;

StemWriter::~StemWriter()
{
    stop();
}

void StemWriter::prepare (double sr)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    maxSamples = (int) std::llround (sampleRate * (double) kMaxSeconds);
    recRingSize = juce::jmax (4096, (int) (sampleRate * 2.5)); // ~2.5 s RT ring

    recFifo.setTotalSize (recRingSize);
    for (auto& p : recRing)
        p.assign ((size_t) recRingSize, 0.0f);
    for (auto& p : writeScratch)
        p.assign (2048, 0.0f);

    recordedLength.store (0);
    recSamplesPushed.store (0);
    dropped.store (0);
}

void StemWriter::reset()
{
    stop();
    recFifo.reset();
    recordedLength.store (0);
    recSamplesPushed.store (0);
    dropped.store (0);
}

float StemWriter::getRecordedSeconds() const noexcept
{
    return (float) recordedLength.load (std::memory_order_relaxed)
         / (float) juce::jmax (1.0, sampleRate);
}

void StemWriter::closeWriters()
{
    for (auto& w : wavWriters)
        w.reset();
}

bool StemWriter::openWriters()
{
    closeWriters();
    juce::WavAudioFormat wav;

    for (int i = 0; i < NumStems; ++i)
    {
        auto f = sessionDir.getChildFile (stemFileName (i));
        auto out = f.createOutputStream();
        if (out == nullptr)
            return false;

        // 32-bit IEEE float WAV, stereo.
        wavWriters[(size_t) i].reset (wav.createWriterFor (out.release(),
                                                           sampleRate,
                                                           2u,
                                                           32,
                                                           {},
                                                           0));
        if (wavWriters[(size_t) i] == nullptr)
            return false;
    }
    return true;
}

juce::String StemWriter::beginRecording()
{
    stop();

    sessionDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Centrophy")
                     .getChildFile ("FtheBand")
                     .getChildFile ("stems")
                     .getChildFile ("session-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S"));

    if (! sessionDir.createDirectory())
        return "Failed to create stem folder: " + sessionDir.getFullPathName();

    if (! openWriters())
        return "Failed to create stem WAV writers.";

    recFifo.reset();
    recordedLength.store (0);
    recSamplesPushed.store (0);
    dropped.store (0);
    recording.store (1);

    writer = std::make_unique<Worker> ("SessionStems", [this] { writerLoop(); });
    writer->startThread();
    return {};
}

void StemWriter::stop()
{
    const bool wasRec = recording.exchange (0) != 0;

    if (writer != nullptr)
    {
        writer->signalThreadShouldExit();
        writer->stopThread (6000);
        writer.reset();
    }

    closeWriters();

    if (wasRec)
        writeSidecar();
}

void StemWriter::writeSidecar()
{
    if (! sessionDir.isDirectory())
        return;

    juce::String txt;
    txt << "F#$*ktheband stems\n";
    txt << "sampleRate=" << sampleRate << "\n";
    txt << "lengthSamples=" << recordedLength.load() << "\n";
    txt << "lengthSeconds=" << getRecordedSeconds() << "\n";
    txt << "format=32-bit float WAV stereo\n";
    txt << "stems=guitar,drums,bass,keys,master\n";
    txt << "style=" << metaStyle << "\n";
    txt << "key=" << metaKey << "\n";
    txt << "bpm=" << metaBpm << "\n";
    txt << "droppedBuffers=" << dropped.load() << "\n";
    sessionDir.getChildFile ("session.txt").replaceWithText (txt);
}

void StemWriter::writerLoop()
{
    while (! juce::Thread::getCurrentThread()->threadShouldExit()
           || recFifo.getNumReady() > 0)
    {
        const int ready = recFifo.getNumReady();
        if (ready <= 0)
        {
            if (recording.load() == 0)
                break;
            juce::Thread::sleep (2);
            continue;
        }

        const int n = juce::jmin (ready, 2048);
        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        recFifo.prepareToRead (n, s1, n1, s2, n2);

        auto copyPlanes = [&] (int start, int count, int destOff)
        {
            for (int p = 0; p < kPlanes; ++p)
                for (int i = 0; i < count; ++i)
                    writeScratch[(size_t) p][(size_t) (destOff + i)] = recRing[(size_t) p][(size_t) (start + i)];
        };
        copyPlanes (s1, n1, 0);
        copyPlanes (s2, n2, n1);
        recFifo.finishedRead (n1 + n2);

        const int total = n1 + n2;
        if (total <= 0)
            continue;

        for (int stem = 0; stem < NumStems; ++stem)
        {
            const float* ch[2] = {
                writeScratch[(size_t) (stem * 2)].data(),
                writeScratch[(size_t) (stem * 2 + 1)].data()
            };
            if (wavWriters[(size_t) stem])
                wavWriters[(size_t) stem]->writeFromFloatArrays (ch, 2, total);
        }

        recordedLength.fetch_add (total, std::memory_order_relaxed);
    }
}

void StemWriter::push (const float* guitarL, const float* guitarR,
                       const float* drumsL,  const float* drumsR,
                       const float* bassL,   const float* bassR,
                       const float* keysL,   const float* keysR,
                       const float* masterL, const float* masterR,
                       int numSamples) noexcept
{
    if (recording.load (std::memory_order_relaxed) == 0 || numSamples <= 0)
        return;

    const int pushed = recSamplesPushed.load (std::memory_order_relaxed);
    if (pushed >= maxSamples)
    {
        recording.store (0, std::memory_order_relaxed);
        return;
    }

    if (recFifo.getFreeSpace() < numSamples)
    {
        dropped.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const float* src[kPlanes] = {
        guitarL, guitarR, drumsL, drumsR, bassL, bassR, keysL, keysR, masterL, masterR
    };

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    recFifo.prepareToWrite (numSamples, s1, n1, s2, n2);

    auto store = [&] (int start, int count, int srcOff)
    {
        for (int p = 0; p < kPlanes; ++p)
        {
            const float* in = src[p];
            float* dest = recRing[(size_t) p].data() + start;
            if (in != nullptr)
                std::memcpy (dest, in + srcOff, (size_t) count * sizeof (float));
            else
                std::memset (dest, 0, (size_t) count * sizeof (float));
        }
    };
    store (s1, n1, 0);
    store (s2, n2, n1);
    recFifo.finishedWrite (n1 + n2);
    recSamplesPushed.fetch_add (n1 + n2, std::memory_order_relaxed);
}
