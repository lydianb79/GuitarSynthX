# Changelog

All notable changes to GuitarSynthX. Versions follow `CFBundleShortVersionString`
in `Packaging/Info.plist`; `Packaging/make_release.sh` copies the section for the
version being released into the GitHub release notes.

## [1.2.0] - 2026-11-18

### Changed
- **Notes stop when you stop.** Muting a string now ends the note within about
  10 ms (high strings) to 45 ms (low E). Before, it took 140–160 ms, and with
  background noise or sound leaking from the speakers into the microphone the
  note never ended. Notes also end when no clear pitch is left for 80 ms.
- After a note has ended, only a new pick starts the next one, so hum or
  speaker sound can't restart it.
- **Fast playing:** picks are detected against the last few milliseconds
  instead of the previous note's attack, and a softer pick that brings a
  clearly new pitch also counts. 16th notes at 150–180 BPM on one string:
  45–47 of 48 notes now retrigger correctly (before: 27–28).
- **Snappier response:** a single very clean pitch reading is accepted
  immediately, and analysis starts 1 ms earlier after the pick.

### Documentation
- README: built-in microphone and speakers add about 60 ms of latency, so use
  an interface. New "Stopping notes" and "Fast playing" sections.

## [1.1.2] - 2026-11-18

### Changed
- The tuner (note, frequency, cents, gate level and preset name) moved out of
  the visualizer into its own bar above it, so it no longer covers the
  waveform. The layout stays put as notes change, and in Hex mode the six
  strings appear in the same bar.

### Documentation
- New README screenshot showing the tuner bar.

## [1.1.1] - 2026-11-18

### Fixed
- After updating from 1.0.x, the loaded preset wasn't highlighted in the
  preset list and its category didn't open. The app now finds it by name.

### Documentation
- Screenshot in the README.

## [1.1.0] - 2026-11-18

### Added
- **50 new factory presets**, for 70 in total, in 12 categories: Lead, Bass,
  Pad, Strings, Brass, Keys, Organ, Pluck, Bells & Mallets, Ambient, FX and Retro.
- **Level-matched presets:** every factory preset now plays at the same loudness,
  so switching sounds doesn't change your volume. Before, they were up to 29 dB
  apart.
- **Patch Level** control (Amp & LFO tab): each preset's own volume. It is
  applied after the effects, so it changes loudness only, not the tone.
- **Preset browser:** categories in a fixed order, sounds sorted
  alphabetically, collapsible categories with counts, and a search box.
- **Previous / next preset:** ◀ ▶ buttons, a new Presets menu, and ⌘[ / ⌘].
- `Tests/check_presets.py`: checks every factory preset for valid settings,
  clean output and matched loudness.

### Changed
- Some of the original sounds moved to better-fitting categories (for example
  Electric Piano to Keys, Shimmer Pad to Ambient, FM Bell to Bells & Mallets).

### Fixed
- The original **Organ** preset's chorus speed was above the allowed maximum
  and was silently capped; it now uses a valid setting.

## [1.0.0] - 2026-11-08

First release.

- Real-time guitar pitch tracking with a low-latency, onset-synchronous
  detector; Hex mode for polyphonic tracking with a 6-channel hexaphonic pickup.
- Synth with unison oscillators, FM, sub-oscillator, noise, ladder and
  state-variable filters, envelopes and an LFO.
- Effects: drive, chorus, phaser, delay (Digital / Tape / Analog, tempo sync,
  ducking), reverb (Room / Hall / Plate, shimmer), limiter.
- 20 factory presets plus user presets.
- Metal-rendered oscilloscope, spectrum and tuner display.
- Core Audio engine with buffer sizes down to 16 samples.
- Separate apps for Apple Silicon and Intel Macs.
