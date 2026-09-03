#pragma once

#include <JuceHeader.h>
#include "DSP/AmpCab.h"
#include "DSP/FollowerBand.h"
#include "DSP/Arrangement.h"
#include "DSP/GuitarFx.h"
#include "DSP/SampleBank.h"
#include "DSP/FxChair.h"
#include "Analysis/InputAnalyzer.h"
#include "Recording/StemWriter.h"
#include "Plugins/PluginHost.h"
#include "Plugins/PluginRack.h"
#include "Daw/DawEngine.h"
#include "Daw/DawModel.h"
#include "SessionSettings.h"
#include <array>
#include <atomic>
#include <vector>

class SessionProcessor
{
public:
    enum Bus : int
    {
        Guitar = 0,
        Drums,
        Bass,
        Keys,
        Fx,
        Master,
        NumBuses
    };

    SessionProcessor();
    void prepare (double sampleRate, int samplesPerBlock, int numOutChannels);
    void reset();
    void release();

    /** Mono in, stereo out. Allocation-free. Audio thread only.
        Safe only when the buffer already holds guitar on its input channels.
        Prefer processDuplex() for independent device in/out pointers. */
    void processBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    /** True duplex: read guitar from INPUT pointers only, write master to OUTPUT
        pointers. Never writes or clears inputChannelData. Audio thread only. */
    void processDuplex (const float* const* inChannels, int numIns,
                        float* const* outChannels, int numOuts,
                        int numSamples) noexcept;

    AmpCab&        getAmp()      noexcept { return amp; }
    GuitarFx&      getFx()       noexcept { return fx; }
    FxChair&       getFxChair()  noexcept { return fxChair; }
    SampleBank&    getSamples()  noexcept { return samples; }
    FollowerBand&  getBand()     noexcept { return band; }
    Arrangement&   getArrangement() noexcept { return arrangement; }
    InputAnalyzer& getAnalyzer() noexcept { return analyzer; }
    StemWriter&    getWriter()   noexcept { return writer; }
    PluginHost&    getPluginHost() noexcept { return host; }
    PluginRack&    getGuitarRack() noexcept { return guitarRack; }
    DawEngine&     getDaw()      noexcept { return daw; }

    void setBusLevel (int bus, float v) noexcept;
    void setBusMute  (int bus, bool m) noexcept;
    void setBusSolo  (int bus, bool s) noexcept;
    float getBusLevel (int bus) const noexcept;
    bool  getBusMute  (int bus) const noexcept;
    bool  getBusSolo  (int bus) const noexcept;
    float getBusPeak  (int bus) const noexcept;

    void setInputGainDb (float db) noexcept;
    float getInputGainDb() const noexcept { return inputGainDb.load (std::memory_order_relaxed); }
    void setGate (float v) noexcept;
    float getGate() const noexcept { return gateAmt.load (std::memory_order_relaxed); }

    void setDelayMix (float v) noexcept { fx.setDelayMix (v); }
    void setSpaceMix (float v) noexcept { fx.setSpaceMix (v); }
    float getDelayMix() const noexcept { return fx.getDelayMix(); }
    float getSpaceMix() const noexcept { return fx.getSpaceMix(); }

    void setAmpBypass (bool e) noexcept { ampBypass.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isAmpBypass() const noexcept { return ampBypass.load (std::memory_order_relaxed) != 0; }
    bool isVstAmpActive() const noexcept { return guitarRack.isVstAmpActive(); }

    void setCountInEnabled (bool e) noexcept { countInEnabled.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isCountInEnabled() const noexcept { return countInEnabled.load (std::memory_order_relaxed) != 0; }
    bool isCountingIn() const noexcept { return countingIn.load (std::memory_order_relaxed) != 0; }
    int  getCountInBeat() const noexcept { return countInBeatAtom.load (std::memory_order_relaxed); }

    void startSession();
    void stopSession();
    void setBandRoster (bool drums, bool bass, bool keys, bool fx = true) noexcept;
    void applyJamSetup (const SessionSettings::Setup& setup) noexcept;
    bool isSessionRunning() const noexcept { return sessionRunning.load (std::memory_order_relaxed) != 0; }
    bool isWaitingForNotes() const noexcept { return waitingNotes.load (std::memory_order_relaxed) != 0; }

    void setGrooveFloor (bool e) noexcept { grooveFloor.store (e ? 1 : 0, std::memory_order_relaxed); }
    void setFadeOnSilence (bool e) noexcept { fadeSilence.store (e ? 1 : 0, std::memory_order_relaxed); }
    bool isGrooveFloor() const noexcept { return grooveFloor.load (std::memory_order_relaxed) != 0; }
    bool isFadeOnSilence() const noexcept { return fadeSilence.load (std::memory_order_relaxed) != 0; }
    float getBandEnergy() const noexcept { return bandEnergy.load (std::memory_order_relaxed); }
    float getPlayerEnergy() const noexcept { return analyzer.getPlayerEnergy(); }

    juce::String startRecording();
    void stopRecording();
    void setDeviceLatency (int inSamples, int outSamples) noexcept
    {
        daw.setDeviceLatency (inSamples, outSamples);
    }
    bool isRecording() const noexcept { return writer.isRecording() || daw.isRecording(); }

    float getInputPeak()  const noexcept { return inputPeak.load  (std::memory_order_relaxed); }
    float getOutputPeak() const noexcept { return outputPeak.load (std::memory_order_relaxed); }

    static const char* busName (int bus);

    /** Load NAM/VoLum then ChowCentaur into empty insert slots (never AmpReplace).
        No-op if any guitar slot already has a plugin. Returns how many loaded. */
    int seedStarterGuitarVsts();

private:
    bool guitarRackHasAnyPlugin() const;
    float busGain (int bus) const noexcept;
    void  beginCountIn() noexcept;
    void  finishCountIn() noexcept;
    void  renderClick (float* left, float* right, int n) noexcept;

    PluginHost host;
    PluginRack guitarRack { host };
    DawEngine daw { host };
    AmpCab amp;
    GuitarFx fx;
    SampleBank samples;
    FxChair fxChair;
    FollowerBand band;
    Arrangement arrangement;
    InputAnalyzer analyzer;
    StemWriter writer;
    juce::MidiBuffer midiScratch;

    std::array<std::atomic<float>, NumBuses> level {};
    std::array<std::atomic<int>,   NumBuses> mute {};
    std::array<std::atomic<int>,   NumBuses> solo {};
    std::array<std::atomic<float>, NumBuses> busPeak {};
    std::atomic<int> sessionRunning { 0 };
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> gateAmt { 0.0f };  // off until the user sets it (quiet UX2 guitar)
    std::atomic<int> ampBypass { 0 };
    std::atomic<int> countInEnabled { 1 };
    std::atomic<int> countInRequest { 0 };
    std::atomic<int> goBand { 0 };
    std::atomic<int> countingIn { 0 };
    std::atomic<int> countInBeatAtom { 0 };
    std::atomic<int> waitingNotes { 0 };
    std::atomic<int> grooveFloor { 1 };
    std::atomic<int> fadeSilence { 0 };
    std::atomic<float> bandEnergy { 0.50f };

    double sampleRate = 44100.0;
    int maxBlock = 512;

    std::vector<float> inMono;
    std::vector<float> gL, gR, dL, dR, bL, bR, kL, kR, fL, fR, mL, mR;

    std::atomic<float> inputPeak  { 0 };
    std::atomic<float> outputPeak { 0 };

    float gateEnv = 0.0f;
    float gateOpen = 0.01f;
    float gateClose = 0.005f;
    float gateAtk = 0.02f;
    float gateRel = 0.001f;
    bool gateOpenState = false;
    juce::SmoothedValue<float> inputGainSmooth;
    juce::SmoothedValue<float> gateGainSmooth;

    bool countInActive = false;
    int countInBeatsDone = 0;
    double countInSample = 0.0;
    float clickEnv = 0.0f;
    float clickPhase = 0.0f;
    float clickHz = 1000.0f;
};
