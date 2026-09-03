#include "Analysis/BasicPitchWorker.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(FTB_HAS_ONNX) && FTB_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

#if defined(FTB_HAS_EMBEDDED_PITCH) && FTB_HAS_EMBEDDED_PITCH
 #if __has_include("SessionModel.h")
  #include "SessionModel.h"
  #define FTB_PITCH_EMBEDDED 1
 #endif
#endif
#ifndef FTB_PITCH_EMBEDDED
 #define FTB_PITCH_EMBEDDED 0
#endif

namespace
{
    constexpr float kNoteThresh = 0.40f;

    // Krumhansl-Kessler key profiles
    constexpr float kMajProf[12] = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    constexpr float kMinProf[12] = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    float scoreTemplate (const float chroma[12], int root, const int* iv, int nIv, const float* w)
    {
        float v = 0.0f;
        for (int i = 0; i < nIv; ++i)
            v += chroma[(root + iv[i]) % 12] * w[i];
        return v;
    }
}

#if defined(FTB_HAS_ONNX) && FTB_HAS_ONNX
struct BasicPitchWorker::OrtState
{
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "ForgetTheBand" };
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo mem { Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault) };
};
#else
struct BasicPitchWorker::OrtState { int unused = 0; };
#endif

BasicPitchWorker::BasicPitchWorker()
    : juce::Thread ("BasicPitch")
{
    for (auto& c : chromaAtom)
        c.store (0.0f);
}

BasicPitchWorker::~BasicPitchWorker()
{
    release();
}

juce::File BasicPitchWorker::findModelFile()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                         .getParentDirectory();
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("Centrophy")
                          .getChildFile ("ForgetTheBand")
                          .getChildFile ("models")
                          .getChildFile ("basic_pitch.onnx");

    const juce::File candidates[] = {
        exe.getChildFile ("basic_pitch.onnx"),
        exe.getChildFile ("Assets").getChildFile ("Models").getChildFile ("basic_pitch.onnx"),
        docs
    };
    for (const auto& f : candidates)
        if (f.existsAsFile() && f.getSize() > 1024)
            return f;
    return {};
}

void BasicPitchWorker::prepare (double deviceSampleRate)
{
    release();
    deviceSr = deviceSampleRate > 1.0 ? deviceSampleRate : 44100.0;

    const int ringSize = juce::jmax (8192, (int) std::lround (deviceSr * 4.0));
    fifo.setTotalSize (ringSize);
    ring.assign ((size_t) ringSize, 0.0f);
    srcStaging.assign ((size_t) juce::jmax (8192, (int) std::lround (deviceSr * 3.0)), 0.0f);
    srcFill = 0;
    fracPos = 0.0;
    window.assign ((size_t) kAudioNSamples, 0.0f);
    windowFill = 0;
    inputFlat.assign ((size_t) kAudioNSamples, 0.0f);
    chromaSmooth.fill (0.0f);
    available.store (0);
    readyFlag.store (0);

    startThread (juce::Thread::Priority::low);
}

void BasicPitchWorker::release()
{
    signalThreadShouldExit();
    stopThread (4000);
    ort.reset();
    available.store (0);
}

void BasicPitchWorker::pushSamples (const float* x, int n) noexcept
{
    if (x == nullptr || n <= 0)
        return;
    if (fifo.getFreeSpace() < n)
        return;

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToWrite (n, s1, n1, s2, n2);
    if (n1 > 0)
        std::memcpy (ring.data() + s1, x, (size_t) n1 * sizeof (float));
    if (n2 > 0)
        std::memcpy (ring.data() + s2, x + n1, (size_t) n2 * sizeof (float));
    fifo.finishedWrite (n1 + n2);
}

void BasicPitchWorker::copyChroma (float out[12]) const noexcept
{
    for (int i = 0; i < 12; ++i)
        out[i] = chromaAtom[(size_t) i].load (std::memory_order_relaxed);
}

void BasicPitchWorker::run()
{
#if defined(FTB_HAS_ONNX) && FTB_HAS_ONNX
    try
    {
        ort = std::make_unique<OrtState>();
        ort->opts.SetIntraOpNumThreads (1);
        ort->opts.SetInterOpNumThreads (1);
        ort->opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Prefer a sidecar so the user can drop a newer nmp.onnx; else embedded bytes.
        const juce::File model = findModelFile();
        if (model.existsAsFile())
        {
#ifdef JUCE_WINDOWS
            ort->session = std::make_unique<Ort::Session> (ort->env,
                                                           model.getFullPathName().toWideCharPointer(),
                                                           ort->opts);
#else
            ort->session = std::make_unique<Ort::Session> (ort->env,
                                                           model.getFullPathName().toRawUTF8(),
                                                           ort->opts);
#endif
            available.store (1, std::memory_order_relaxed);
        }
#if FTB_PITCH_EMBEDDED
        else
        {
            int sz = 0;
            if (const char* data = SessionModel::getNamedResource ("basic_pitch_onnx", sz))
            {
                if (sz > 1024 && data != nullptr)
                {
                    ort->session = std::make_unique<Ort::Session> (ort->env,
                                                                   (const void*) data,
                                                                   (size_t) sz,
                                                                   ort->opts);
                    available.store (1, std::memory_order_relaxed);
                }
            }
        }
#endif
    }
    catch (...)
    {
        available.store (0, std::memory_order_relaxed);
        ort.reset();
    }
#endif

    while (! threadShouldExit())
    {
        drainAndMaybeInfer();
        wait (8);
    }

    ort.reset();
}

void BasicPitchWorker::drainAndMaybeInfer()
{
    const int ready = fifo.getNumReady();
    if (ready <= 0)
        return;

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToRead (ready, s1, n1, s2, n2);

    auto take = [this] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (srcFill >= (int) srcStaging.size())
                break;
            srcStaging[(size_t) srcFill++] = ring[(size_t) (start + i)];
        }
    };
    take (s1, n1);
    take (s2, n2);
    fifo.finishedRead (n1 + n2);

    resampleIntoWindow();

#if defined(FTB_HAS_ONNX) && FTB_HAS_ONNX
    if (available.load (std::memory_order_relaxed) == 0 || ort == nullptr || ort->session == nullptr)
        return;
    if (windowFill < kAudioNSamples)
        return;

    std::memcpy (inputFlat.data(), window.data(), (size_t) kAudioNSamples * sizeof (float));

    const int64_t shape[3] = { 1, (int64_t) kAudioNSamples, 1 };
    const char* inputNames[] = { "serving_default_input_2:0" };
    const char* outputNames[] = {
        "StatefulPartitionedCall:1", // note
        "StatefulPartitionedCall:2", // onset
        "StatefulPartitionedCall:0"  // contour
    };

    try
    {
        Ort::Value inputTensor = Ort::Value::CreateTensor<float> (
            ort->mem, inputFlat.data(), (size_t) kAudioNSamples, shape, 3);

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto outputs = ort->session->Run (Ort::RunOptions {},
                                     inputNames, &inputTensor, 1,
                                     outputNames, 3);
        lastInferMs_.store ((float) (juce::Time::getMillisecondCounterHiRes() - t0),
                            std::memory_order_relaxed);

        if (! outputs.empty() && outputs[0].IsTensor())
        {
            const auto info = outputs[0].GetTensorTypeAndShapeInfo();
            const auto dims = info.GetShape();
            int frames = kAnnotNFrames;
            int bins = kNFreqBinsNotes;
            if (dims.size() >= 3)
            {
                frames = (int) dims[dims.size() - 2];
                bins = (int) dims[dims.size() - 1];
            }
            const float* note = outputs[0].GetTensorData<float>();
            decodeAndPublish (note, frames, bins);
        }
    }
    catch (...)
    {
        // keep running; skip this window
    }

    const int keep = kAudioNSamples - kAdvanceSamples;
    if (keep > 0 && keep < kAudioNSamples)
    {
        std::memmove (window.data(), window.data() + kAdvanceSamples, (size_t) keep * sizeof (float));
        windowFill = keep;
    }
    else
    {
        windowFill = 0;
    }
#else
    if (windowFill >= kAudioNSamples)
        windowFill = juce::jmax (0, kAudioNSamples - kAdvanceSamples);
#endif
}

void BasicPitchWorker::resampleIntoWindow()
{
    if (srcFill < 2 || windowFill >= kAudioNSamples)
        return;

    const double ratio = deviceSr / (double) kAudioSampleRate;
    if (ratio <= 0.0)
        return;

    while (windowFill < kAudioNSamples)
    {
        const int i0 = (int) fracPos;
        const int i1 = i0 + 1;
        if (i1 >= srcFill)
            break;
        const double frac = fracPos - (double) i0;
        const float a = srcStaging[(size_t) i0];
        const float b = srcStaging[(size_t) i1];
        window[(size_t) windowFill++] = (float) ((1.0 - frac) * (double) a + frac * (double) b);
        fracPos += ratio;
    }

    const int drop = juce::jmax (0, (int) fracPos);
    if (drop > 0 && drop < srcFill)
    {
        const int remain = srcFill - drop;
        std::memmove (srcStaging.data(), srcStaging.data() + drop, (size_t) remain * sizeof (float));
        srcFill = remain;
        fracPos -= (double) drop;
    }
    else if (drop >= srcFill)
    {
        srcFill = 0;
        fracPos = 0.0;
    }
}

void BasicPitchWorker::decodeAndPublish (const float* note, int frames, int bins)
{
    if (note == nullptr || frames <= 0 || bins <= 0)
        return;

    bins = juce::jmin (bins, kNFreqBinsNotes);
    float chroma[12] = {};
    uint64_t lo = 0, hi = 0;

    for (int b = 0; b < bins; ++b)
    {
        float mx = 0.0f;
        for (int t = 0; t < frames; ++t)
            mx = juce::jmax (mx, note[t * bins + b]);
        if (mx > kNoteThresh)
        {
            if (b < 64)
                lo |= (uint64_t) 1 << b;
            else if (b < 88)
                hi |= (uint64_t) 1 << (b - 64);
            chroma[(kMidiA0 + b) % 12] += mx;
        }
    }

    float sum = 0.0f;
    for (float c : chroma)
        sum += c;
    if (sum > 1.0e-6f)
        for (float& c : chroma)
            c /= sum;

    for (int i = 0; i < 12; ++i)
    {
        chromaSmooth[(size_t) i] = chromaSmooth[(size_t) i] * 0.80f + chroma[i] * 0.20f;
        chromaAtom[(size_t) i].store (chromaSmooth[(size_t) i], std::memory_order_relaxed);
    }

    // Template-match major / minor / 7 / 5
    const int majIv[] = { 0, 4, 7 };
    const float majW[] = { 1.45f, 1.05f, 0.85f };
    const int minIv[] = { 0, 3, 7 };
    const float minW[] = { 1.45f, 1.05f, 0.85f };
    const int sevIv[] = { 0, 4, 7, 10 };
    const float sevW[] = { 1.35f, 0.95f, 0.75f, 0.70f };
    const int pwrIv[] = { 0, 7 };
    const float pwrW[] = { 1.50f, 1.10f };

    int bestRoot = 0, bestQ = Maj;
    float bestV = -1.0f;
    auto consider = [&] (int q, const int* iv, int nIv, const float* w)
    {
        for (int r = 0; r < 12; ++r)
        {
            const float v = scoreTemplate (chromaSmooth.data(), r, iv, nIv, w);
            if (v > bestV)
            {
                bestV = v;
                bestRoot = r;
                bestQ = q;
            }
        }
    };
    consider (Maj, majIv, 3, majW);
    consider (Min, minIv, 3, minW);
    consider (Dom7, sevIv, 4, sevW);
    consider (Fifth, pwrIv, 2, pwrW);

    chordRoot_.store (bestRoot, std::memory_order_relaxed);
    chordQuality_.store (bestQ, std::memory_order_relaxed);

    int bestKey = 0;
    float bestKeyV = -1.0e9f;
    for (int k = 0; k < 12; ++k)
    {
        float maj = 0.0f, minr = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            maj += chromaSmooth[(size_t) ((k + i) % 12)] * kMajProf[i];
            minr += chromaSmooth[(size_t) ((k + i) % 12)] * kMinProf[i];
        }
        const float v = juce::jmax (maj, minr);
        if (v > bestKeyV)
        {
            bestKeyV = v;
            bestKey = k;
        }
    }
    keyPc_.store (bestKey, std::memory_order_relaxed);
    polyLo_.store (lo, std::memory_order_relaxed);
    polyHi_.store (hi, std::memory_order_relaxed);
    readyFlag.store (1, std::memory_order_relaxed);
}
