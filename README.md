# GuitarSynthX

A native macOS guitar synthesizer. Plug in a guitar, and GuitarSynthX detects
what you play in real time and plays it back as a synth, with filter, envelopes,
LFO, drive, chorus, phaser, delay and reverb.

* **Native:** SwiftUI interface with a **Metal**-rendered oscilloscope and spectrum.
* **Real-time core in C++** on the **Core Audio HAL** (AUHAL). The audio thread
  never touches Swift, locks or the memory allocator.
* **Low latency:** a full-duplex single-device path, buffer sizes down to 16
  samples, and onset-synchronous pitch detection.
* **Mono or polyphonic:** mono mode for a normal guitar input; **Hex mode** gives
  true polyphony with a hexaphonic (GK-style, 6-channel) pickup, one tracker and
  voice per string.

## App icon

`Packaging/Icon/` contains the vector source (`AppIcon.svg`), a 1024 px PNG and the multi-resolution `AppIcon.icns` that `build_app.sh` puts into the bundle. The icon shows up when you launch the `.app`; `swift run` starts a bare executable, so it gets a generic icon.

## Requirements

* macOS 13 Ventura or later (Apple Silicon or Intel)
* Xcode 15+ **or** the Xcode Command Line Tools (Swift 5.9+)
* An audio interface. The built-in mic works for testing, but use a real
  instrument input for playing.

## Build & run

```bash
# Option A: proper .app bundle (recommended: own microphone permission, Dock icon)
./Packaging/build_app.sh            # universal app: Apple Silicon + Intel
./Packaging/build_app.sh native     # just this Mac's architecture (faster)
open build/GuitarSynthX.app

# Option B: run straight from the package
swift run -c release GuitarSynthX

# Option C: Xcode
open Package.swift       # then pick the GuitarSynthX scheme, set Release, ⌘R
```

macOS asks for microphone access on first launch. Without it, input is silent.
If you denied it, re-enable it under *System Settings ▸ Privacy & Security ▸ Microphone*.

### Engine tests / benchmark (no audio hardware needed)

```bash
./Tests/run_tests.sh
```

These run the pitch tracker against synthetic plucked strings (E2–C6 at 44.1/48/96 kHz).
They cover legato slides, re-picks over a ringing note, false retriggers, extreme
settings (NaN/overflow safety) and a CPU benchmark for mono and hex mode.

## Getting the lowest latency

1. In **Audio Settings** (⌘,), use the **same device for input and output**. That
   enables the full-duplex path: input is pulled inside the output callback, with
   no extra buffering. Two separate devices are bridged by a small, drift-controlled
   ring buffer. If you must use two devices, an *Aggregate Device* (Audio MIDI
   Setup) is the better option.
2. Choose a **buffer size** of 32–64 samples. If the *XRUNS* counter climbs, go up one step.
3. 48 kHz is the sweet spot. Higher rates lower the buffer latency but cost more CPU.
4. On the *Input & Tracking* tab, set **Gate** just above your guitar's noise
   floor, and set **Lowest Note** to the lowest note you actually play. A higher
   lowest note gives faster detection and less CPU.

The top bar shows the hardware round-trip latency, the DSP load (peak share of
the real-time budget), and dropouts.

### How fast is detection?

A pitch can't be known before about one to two periods of the waveform have
arrived. GuitarSynthX starts analysing right after the pick attack, using only
post-attack samples, with a window that grows as audio comes in. High notes lock
in after a few milliseconds; the low E string (82 Hz) takes about 20–25 ms.
Measured on the synthetic test set at 48 kHz: 45/45 notes correct, average
detection about 12 ms after the pick (including the 1.3 ms test block).
Hardware latency comes on top of that.

## Using it

| Area | What it does |
|---|---|
| **Presets** (left) | 20 factory sounds (leads, basses, pads, keys, FX). *Save* stores your own in `~/Library/Application Support/GuitarSynthX/Presets`. Presets change only the sound: your input, gate and tracking calibration stay as they are. |
| **Input & Tracking** | Mono/Hex input mode, input channel, gain, gate, output channel pair. Pitch range, sensitivity (YIN threshold), stability, analysis rate. **Pitch Mode**: *Track* follows bends and vibrato continuously; *Quantize* snaps to semitones; *Quantize+Glide* adds portamento. **Pick Retrigger** restarts envelopes on each pick; with it off, everything plays legato. **Dynamics** makes the synth volume follow your playing. |
| **Oscillators** | Osc A (up to 7-voice unison), Osc B (semi/fine), FM B→A, pulse width, sub-oscillator, noise. |
| **Filter** | 24 dB ladder, 12 dB LP/BP/HP state-variable filter, drive, key tracking, **Pick Dynamics** (picking harder opens the filter: an auto-wah), ADSR envelope. |
| **Amp & LFO** | Amp ADSR and velocity. LFO with fade-in for vibrato, filter, PWM and tremolo. |
| **Effects** | Drive → Chorus → Phaser → Delay → Reverb, then the limiter. |
| **Delay** | Character *Digital / Tape* (wow, flutter, saturated repeats) */ Analog* (dark BBD-style repeats). Free time or **tempo sync** (1/2, 1/4, 1/4 T, 1/8 D, 1/8, 1/8 T, 1/16 at any BPM). Feedback, mix, low-cut and high-cut in the feedback loop, modulation, ping-pong, **ducking** (echoes drop back while you play and swell in when you stop). |
| **Reverb** | *Room / Hall / Plate* types (level-matched). Size, decay up to 20 s, pre-delay, low-cut, high-cut, modulation, stereo width, ducking, and **Shimmer** (octave-up tail that climbs with every pass, built so it can never run away). |
| **Master** (right) | Synth level, **Dry Guitar** blend, master volume, limiter, **Dry > FX** (should the dry guitar go through the effects or stay clean), in/out meters. |

Knobs: drag vertically, ⇧-drag for fine control, double-click to reset.
⌘R starts/stops audio, ⌘. is panic.

## Architecture

```
Sources/
  GSXCore/                 C++17 real-time engine (C API in include/GSXCore.h)
    AudioIO.cpp            Core Audio HAL: AUHAL duplex or 2-device + SPSC ring
    Engine.cpp             graph: input → trackers → voices → FX → limiter
    PitchTracker.cpp       band-limit, decimate to ~24 kHz, onset detection
                           (level jump + HF pick noise), YIN via vDSP_conv,
                           adaptive post-onset window, octave-error guards
    Synth.cpp              PolyBLEP oscillators, ZDF ladder + TPT SVF, ADSR
    Effects.cpp            drive, chorus, phaser, delay, FDN reverb, limiter
    Params.cpp             parameter table (the UI is generated from it)
  GuitarSynthX/            SwiftUI app
    MetalVisualizer.swift  MTKView renderer + MSL shaders
    SynthController.swift  devices, engine lifecycle, presets, persistence
```

Real-time rules followed on the audio thread: no allocation, no locks, no
Objective-C/Swift runtime calls. Parameters cross threads as relaxed atomics,
with a version counter so the engine recomputes coefficients only when something
changed. Denormals are flushed (FTZ). A NaN/Inf guard mutes and resets the engine
rather than sending garbage to your speakers.

## Notes and limitations

* With a normal (mono) guitar input, the tracker is **monophonic**: play single
  notes. Chords need a hexaphonic pickup and Hex mode.
* A device that is unplugged while running stops the audio. Choose a device
  again in Audio Settings.
* The build script signs ad-hoc for local use. To distribute the app, sign with
  your Developer ID and notarize.
