# Changelog

All notable changes to GuitarSynthX. Versions follow `CFBundleShortVersionString`
in `Packaging/Info.plist`; `Packaging/make_release.sh` copies the section for the
version being released into the GitHub release notes.

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
