# Forget The Band

**Centrophy** — a live follower band for guitar.

Plug in, start a session, and a synthesized drums / bass / keys trio jams with you
in real time. This is not a song player and not a full amp-sim suite. It listens
to your playing (pitch, key, pulse, how busy you are) and follows.

Product: **Forget The Band** · Company: **Centrophy** · Bundle ID: `com.centrophy.forgettheband`

Window title is **Forget The Band**. Binary name on Windows is `ForgetTheBand.exe`.

Built with JUCE 8.0.8 (FetchContent, C++20). Windows-first desktop app. Version **2.0**.

## Features

- Live guitar path: input gain (0–+24 dB) → noise gate → modest drive + cabinet
  voicing (no IR files) → tempo-sync delay → hall-ish reverb send
- Tuner: nearest note + cents needle from the analysis thread
- Follower band: **Rock, Blues, Metal, Funk, Jazz** — original synthesized patterns
- Four lobby chairs: Drums, Bass, Keys, **FX** (sampler + SFX). Empty chairs are silent.
- Each seated member has a voice picker (kit / bass / keys / FX Auto·Hits·Risers·Foley), independent of jam Style
- Bar-aware drum fills every 8 bars (extra fills at high intensity) + crash on the next downbeat
- Intensity raises hat density, fill chance, bass motion, key velocity, and FX spice
- **Auto BPM follows the player until you press Lock Tempo.** It never auto-freezes mid-jam.
  Landing **Follow tempo** (was Slew) seeds BPM then keeps slewing. Key auto-lock can still engage.
- **Export MIDI** writes the named-note transcription as a Standard MIDI `.mid` file
- CC0 one-shots under `Assets/Samples/` (BushDrum, Kenney Impact/Sci-Fi, original DSP) with synth fallback
- Optional 4-beat count-in click on Start Session (band silent until it finishes; default on)
- Mixer: level / mute / solo / live peak meter for Guitar, Drums, Bass, Keys, FX, Master
- Stem recording: one WAV per bus plus stereo master (including `fx.wav`), 32-bit float; elapsed mm:ss + REC pulse
- Bar counter (`1.1`, `1.2`, …)
- Audio device picker (`AudioDeviceSelectorComponent`); WASAPI always; ASIO optional
- Persists last audio device + buffer, and UI (style, auto key/BPM, amp/FX knobs, bus levels)

## Architecture

```
 guitar in
    │
    ├── AubioEngine (audio thread, hop 512)
    │      onset / YIN Hz / RMS → atomics immediately
    │      lock-free ring → Basic Pitch worker (ONNX, 43844 @ 22050)
    │      worker: chroma / chord / key + IOI tempo lock
    │
    ├── gain → gate → AmpCab → delay → space ── guitar bus
    │
    └── FollowerBand + FxChair (audio thread, reads atomics)
           drums / bass / keys / fx  (silent during count-in)
                │
                ▼
           Mixer (mute/solo/level + per-bus peaks)
                │
                ├── StemWriter RT ring ──► disk thread ──► WAV stems
                └── stereo master out (+ count-in click)
```

See `Source/Analysis/README.md` for the dual-pipeline tracker, ONNX window
(43844, not 2048), thread rules, and CMake flags.

**Audio thread** runs aubio `_do` on preallocated objects (no `new_aubio_*`, no
`Ort::`) and never touches the filesystem. It copies input into a lock-free FIFO
for Basic Pitch, reads analysis atomics, renders the amp/FX and band, mixes, and
(if armed) pushes interleaved stem planes into a second lock-free FIFO. Delay and
reverb buffers are allocated in `prepare` only.

**Analysis worker** drains Basic Pitch results (chroma / chord / key), locks
tempo from aubio onset IOIs, and emits named-note MIDI. After a few bars of a
stable key the key auto-lock can engage. **BPM never auto-locks** — Auto BPM keeps
slewing from onset IOIs until **Lock Tempo** is on. Manual key/BPM overrides
bypass auto entirely. Homemade YIN remains as fallback if aubio is missing.

**Stem writer thread** drains the record FIFO and writes 32-bit float WAVs.
Start and stop are atomic across all buses: guitar, drums, bass, keys, fx, master.

Default device hint: 44.1 kHz, buffer **128** samples (falls back to **256**).
Last device and buffer size are restored from
`user app data/Centrophy/ForgetTheBand/audio.xml` on launch.

## Guitar path

Mono in, stereo out. Order is fixed and real-time safe:

1. **Input gain** — `SmoothedValue` 0 to +24 dB
2. **Tuner tap** — YIN on the hot pre-gate signal (analysis thread)
3. **Noise gate** — 1-pole abs envelope, ~2 ms open / ~100 ms close, −40 / −46 dB hysteresis, 5 ms gain ramp
4. **VST Slot 1 PreAmp**
5. **AmpCab** *or* **VST Slot 2 Amp** (`isVstAmpActive` skips AmpCab)
6. **VST Slot 3 Post**
7. **VST Slot 4**
8. **Delay** — 1/4, 1/8, 1/8., 1/16; 1.5 s buffers; 50 ms equal-power crossfade on BPM/division change
9. **Space** — `juce::Reverb` room 0.30–0.45, damp 0.60–0.75, wet cap 0.35, dry 1.0, width 0.3
10. Mix with the follower band / DAW

VST3 host: scan on a worker thread, dead-man's pedal, `suspendProcessing` before destroy, never swap the instance on the audio thread.

## Live band + arrange

Two views in one window:

- **Live band** — set Style, Form (Vamp / Song / 12-Bar / Wander), Scale, Tempo, Feel (Grid / Ahead / Behind / Swing), then **Start**. Count-in (optional) then groove. **Auto BPM** follows until **Lock Tempo**. **Keep Groove** (default on) holds a floor so rests do not kill the kit. Dual meters: You vs Band (band lags). Chord name + Roman + next-chord telegraph + 2D neck. **Export MIDI** saves the notes you played as a `.mid`.
- **Arrange** — 8 audio tracks + Drums/Bass/Keys + Master. Play / Stop / Record / RTZ / Cycle. Record writes 32-bit float takes into `Documents/Centrophy/ForgetTheBand/projects/<name>/`. New / Open / Save / Bounce. Per-track 4 VST3 inserts. Undo for clip move/delete.

Projects: `Documents/Centrophy/ForgetTheBand/projects/<name>/project.xml` + `audio/`.

## Band styles

| Style | Drums | Bass | Keys | Form |
|-------|--------|------|------|------|
| Rock  | 2-and-4, 8th hats, extra kicks when busy | root / fifth / approach | triad + 7th pad | I – bVII – IV – I |
| Blues | shuffle hats, light ride | walking 8ths | dominant 7ths | 12-bar I / IV / V |
| Metal | 8th/16th kicks, dense hats | palm-root 8ths, octave at high intensity | fifths / octaves | i – bVI – bIII – bVII |
| Funk  | 16th hats, kick on 1 and &-of-2, ghost snare | muted 16ths with pluck | sparse 7/9 stabs | I – IV – I – V |
| Jazz  | swing ride (spang-a-lang), light kick | walking quarters + approach tones | 7th/9th voicings, 2-and-4 comp | ii – V – I – vi |

Every 8th bar (and extra 4-bar fills at high intensity) the kit plays a one-bar
tom/snare fill and crashes on the next downbeat.

## Windows (Visual Studio 2022 Build Tools + CMake)

1. Install [CMake](https://cmake.org/) 3.22+ and VS 2022 Build Tools with the
   **Desktop development with C++** workload.
2. Open an **x64 Native Tools Command Prompt for VS 2022** (or Developer PowerShell):

```bat
cd path\to\forget-the-band
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -j
```

Binary: `build/Session_artefacts/Release/ForgetTheBand.exe`

Audio: **WASAPI** (`Windows Audio` / `Windows Audio Exclusive`) is always
available. Prefer 44.1 kHz, buffer 128–256, 1 in / 2 out. Open **Audio** in the
app for the full `AudioDeviceSelectorComponent`.

### Optional ASIO (Steinberg ASIO SDK)

ASIO is **off by default**. The Steinberg ASIO SDK is **not** bundled.

1. Download the ASIO SDK from Steinberg (account required).
2. Point CMake at the SDK root (folder that contains `common/asio.h`) **or**
   at the `common` folder itself (folder that contains `asio.h`):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DJUCE_ASIO=ON ^
  -DJUCE_ASIO_SDK_PATH="C:/path/to/asiosdk"
cmake --build build --config Release -j
```

If the path is valid, CMake sets `JUCE_ASIO=1` and adds `common` (and
`host/pc` when present) to the include path so JUCE can see `asio.h` /
`iasiodrv.h`. If the path is empty or `asio.h` is missing, the build continues
**without** ASIO and WASAPI still works.

**ASIO4ALL** is a separate third-party driver install. It is not the ASIO SDK
and is not shipped with this project. Install ASIO4ALL (or a hardware ASIO
driver) on the machine that will run Session; enable the SDK at compile time
as above so the ASIO device type appears in the picker.

To share a JUCE tree already cloned elsewhere:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DFETCHCONTENT_SOURCE_DIR_JUCE=C:\path\to\JUCE
```

## Linux (optional compile check)

JUCE 8.0.8 is fetched by CMake. This project is Windows-first; a Linux build
is only a source/compile sanity check.

```bash
sudo apt install cmake g++ pkg-config libasound2-dev libfreetype6-dev \
  libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev libxcomposite-dev \
  libgl1-mesa-dev libglu1-mesa-dev libjack-jackd2-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Binary: `build/Session_artefacts/Release/Session`

## Stem recordings

Press **Record**. Session creates a timestamped folder and writes five WAVs
with the same start and stop:

| File          | Contents              |
|---------------|-----------------------|
| `guitar.wav`  | Amp/FX guitar (stereo) |
| `drums.wav`   | Drum bus              |
| `bass.wav`    | Bass bus              |
| `keys.wav`    | Harmonic pad / keys   |
| `fx.wav`      | Sampler / SFX chair   |
| `master.wav`  | Stereo mix            |

Format: **32-bit float WAV** at the device sample rate. A `session.txt` sidecar
records sample rate, length, style, key, and BPM.

Default location:

```
Documents/Centrophy/ForgetTheBand/stems/session-YYYYMMDD-HHMMSS/
```

The path, elapsed `mm:ss`, and a pulsing REC indicator are shown while armed.

## Playing

1. Connect a guitar (or any line/mic input). Open **Audio** and pick the device.
2. Seat Drums / Bass / Keys / FX in the lobby. Leave **Follow tempo** on so Auto BPM tracks you; turn it off (or press **Lock Tempo** in the session) to freeze BPM.
3. Set Gain / Gate so the tuner locks a note; tweak Drive / Tone / Delay / Space.
4. **Start Session** — optional 4-beat count-in, then the band enters on the next downbeat.
5. Play. Intensity (hats, fills, bass motion, FX spice) tracks how busy/loud you are.
6. **Export MIDI** to save the named-note transcription as a `.mid`. Drop extra one-shots in `Documents/Centrophy/ForgetTheBand/samples/fx` and hit **Load FX**.
7. **Record** if you want stems (elapsed time is on the transport). **Stop** ends the jam.

## Samples (CC0)

Bundled one-shots live in `Assets/Samples/` (about 1 MB). See `Assets/Samples/LICENSE.txt`
for source URLs. All files are **CC0 / public domain**:

- BushDrum LinnDrum kit (funk/metal hits + crash) — https://github.com/EwonRael/BushDrum
- Kenney Impact Sounds + Sci-Fi Sounds — https://kenney.nl/assets/impact-sounds , https://kenney.nl/assets/sci-fi-sounds
- Original DSP acoustic kick/snare/hat, bass pluck, piano hammer — CC0 by Centrophy

User FX drops: `Documents/Centrophy/ForgetTheBand/samples/fx/*.wav` (scanned on Start Session and **Load FX**). Missing files fall back to the synthesized voices. Other notes are pitch-shifted by simple resampling (no 88-key pianos).

## License

Application sources are licensed under the **GNU GPL v3** (see `LICENSE`), matching
JUCE 8 used under its GPL/AGPL option. JUCE is fetched at configure time
(tag **8.0.8**) and is copyright Raw Material Software Limited — see the JUCE
license in the fetched tree.

If you hold a JUCE commercial license you may relicense *this application* under
that agreement; this repo itself stays GPL-3.0 as published.

ASIO is a trademark of Steinberg Media Technologies GmbH. The ASIO SDK is not
redistributed. ASIO4ALL is a third-party product and is not bundled.
