import Foundation

struct Preset: Codable, Identifiable, Hashable {
    var id: String { category + "/" + name }
    var name: String
    var category: String
    var isFactory: Bool = false
    /// Overrides on top of the engine defaults (sound parameters only).
    var values: [String: Float]
}

enum FactoryPresets {
    /// Order in which categories appear in the preset browser.
    static let categoryOrder: [String] = ["Lead", "Bass", "Pad", "Strings", "Brass", "Keys", "Organ", "Pluck", "Bells & Mallets", "Ambient", "FX", "Retro"]

    /// Factory sounds, grouped by category and sorted by name. Every preset sets
    /// "amp.level" so that all of them play at the same loudness (-26 LUFS on a
    /// standard guitar test phrase, measured through the real engine).
    static let all: [Preset] = [
        // ------------------------------------------------------------ Lead (13)
        p("Ambient Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 9, "oscB.wave": 1, "oscB.semi": 12, "osc.mix": 0.3,
            "flt.cutoff": 2600, "flt.res": 0.2, "flt.envamt": 1.5, "trk.pitchmode": 2, "trk.glide": 35,
            "lfo.pitch": 10, "lfo.delay": 500, "aenv.a": 8, "aenv.s": 0.9, "aenv.r": 600, "dly.on": 1, "dly.sync": 4,
            "dly.fb": 0.55, "dly.mix": 0.4, "dly.duck": 0.75, "dly.mod": 0.2, "dly.lowcut": 200, "dly.damp": 4000,
            "rev.on": 1, "rev.type": 1, "rev.decay": 4.5, "rev.size": 0.85, "rev.mix": 0.35, "rev.duck": 0.5,
            "rev.lowcut": 180, "amp.level": 1.6]),
        p("Bend Sine (Track)", "Lead", [
            "oscA.wave": 0, "osc.mix": 0, "flt.type": 1, "flt.cutoff": 12000, "flt.envamt": 0, "flt.dyn": 0,
            "trk.pitchmode": 0, "trk.dynamics": 0.9, "aenv.a": 3, "aenv.s": 1, "aenv.r": 180, "rev.on": 1,
            "rev.mix": 0.22, "amp.level": -1.5]),
        p("Classic Mono Lead", "Lead", [
            "oscA.wave": 2, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.35, "osc.sub": 0.2,
            "flt.cutoff": 1800, "flt.res": 0.35, "flt.envamt": 2.5, "fenv.d": 350, "fenv.s": 0.35, "aenv.a": 2,
            "aenv.s": 0.9, "aenv.r": 150, "trk.pitchmode": 2, "trk.glide": 25, "lfo.pitch": 10, "lfo.delay": 500,
            "dly.on": 1, "dly.time": 320, "dly.mix": 0.18, "rev.on": 1, "rev.mix": 0.12, "amp.level": -0.7]),
        p("Fat Saw Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 18, "oscB.wave": 2, "oscB.fine": 7, "osc.mix": 0.3,
            "flt.cutoff": 3200, "flt.res": 0.25, "flt.envamt": 2.5, "fenv.d": 400, "fenv.s": 0.4, "aenv.a": 2,
            "aenv.d": 300, "aenv.s": 0.9, "aenv.r": 300, "trk.pitchmode": 2, "trk.glide": 40, "lfo.pitch": 12,
            "lfo.delay": 450, "cho.on": 1, "dly.on": 1, "dly.time": 380, "dly.fb": 0.3, "dly.mix": 0.2, "rev.on": 1,
            "rev.mix": 0.18, "amp.level": 2.2]),
        p("Fifth Lead", "Lead", [
            "oscA.wave": 2, "oscB.wave": 2, "oscB.semi": 7, "oscB.fine": 0, "osc.mix": 0.4, "flt.cutoff": 2600,
            "flt.res": 0.25, "flt.envamt": 2, "aenv.s": 0.9, "lfo.pitch": 8, "lfo.delay": 450, "drv.on": 1,
            "drv.amount": 0.25, "dly.on": 1, "dly.time": 400, "dly.mix": 0.2, "rev.on": 1, "rev.mix": 0.15,
            "amp.level": -5.8]),
        p("Glide Saw", "Lead", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 10, "oscB.wave": 2, "oscB.semi": -12, "osc.mix": 0.35,
            "flt.cutoff": 2200, "flt.res": 0.3, "flt.envamt": 2, "trk.pitchmode": 2, "trk.glide": 120, "dly.on": 1,
            "dly.time": 450, "dly.fb": 0.45, "rev.on": 1, "rev.mix": 0.2, "amp.level": 3.1]),
        p("Overdrive Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 8, "oscB.wave": 3, "osc.mix": 0.3, "flt.cutoff": 3200,
            "flt.res": 0.2, "flt.envamt": 1.5, "flt.drive": 0.4, "drv.on": 1, "drv.amount": 0.7, "drv.tone": 4200,
            "aenv.s": 0.95, "lfo.pitch": 14, "lfo.delay": 400, "dly.on": 1, "dly.mode": 2, "dly.time": 380,
            "dly.fb": 0.35, "dly.mix": 0.22, "rev.on": 1, "rev.mix": 0.15, "amp.level": -13.2]),
        p("Prog Lead", "Lead", [
            "oscA.wave": 2, "oscB.wave": 4, "osc.pw": 0.3, "oscB.fine": 12, "osc.mix": 0.45, "flt.cutoff": 2200,
            "flt.res": 0.45, "flt.envamt": 3, "fenv.a": 5, "fenv.d": 600, "fenv.s": 0.4, "trk.pitchmode": 2,
            "trk.glide": 80, "lfo.pitch": 12, "lfo.delay": 600, "dly.on": 1, "dly.mode": 1, "dly.sync": 4,
            "dly.fb": 0.45, "dly.mix": 0.28, "dly.mod": 0.3, "rev.on": 1, "rev.type": 1, "rev.decay": 3.5,
            "rev.mix": 0.22, "amp.level": 1.7]),
        p("Screaming Lead", "Lead", [
            "oscA.wave": 2, "oscB.wave": 4, "oscB.semi": 7, "osc.mix": 0.4, "osc.pw": 0.35, "flt.cutoff": 1800,
            "flt.res": 0.55, "flt.envamt": 3.5, "flt.drive": 0.6, "drv.on": 1, "drv.amount": 0.5, "drv.tone": 4500,
            "lfo.pitch": 18, "lfo.delay": 300, "dly.on": 1, "dly.time": 420, "dly.fb": 0.4, "dly.mix": 0.25,
            "rev.on": 1, "rev.mix": 0.2, "amp.level": -13.3]),
        p("Soft Sine Lead", "Lead", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.2, "flt.type": 1,
            "flt.cutoff": 8000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 8, "aenv.s": 1, "aenv.r": 300,
            "trk.pitchmode": 0, "trk.dynamics": 0.8, "cho.on": 1, "cho.mix": 0.25, "rev.on": 1, "rev.type": 1,
            "rev.mix": 0.28, "amp.level": 0.3]),
        p("Square Lead", "Lead", [
            "oscA.wave": 3, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.25, "flt.cutoff": 2800,
            "flt.res": 0.3, "flt.envamt": 1.5, "lfo.pitch": 15, "lfo.delay": 350, "dly.on": 1, "dly.time": 330,
            "dly.mix": 0.22, "rev.on": 1, "rev.mix": 0.15, "amp.level": -2.3]),
        p("Supersaw Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 7, "oscA.detune": 28, "oscB.wave": 2, "oscB.semi": 12, "oscB.fine": 0,
            "osc.mix": 0.25, "flt.cutoff": 4500, "flt.res": 0.1, "flt.envamt": 1, "aenv.a": 3, "aenv.s": 0.9,
            "aenv.r": 300, "cho.on": 1, "cho.mix": 0.3, "dly.on": 1, "dly.sync": 4, "dly.fb": 0.35, "dly.mix": 0.22,
            "rev.on": 1, "rev.type": 1, "rev.mix": 0.2, "amp.level": 1.3]),
        p("Whistle Lead", "Lead", [
            "oscA.wave": 0, "oscB.wave": 1, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.15, "osc.noise": 0.08,
            "flt.type": 1, "flt.cutoff": 5000, "flt.envamt": 0, "flt.dyn": 0.3, "aenv.a": 25, "aenv.s": 1,
            "aenv.r": 220, "lfo.pitch": 18, "lfo.rate": 5.5, "lfo.delay": 350, "trk.pitchmode": 2, "trk.glide": 60,
            "rev.on": 1, "rev.type": 2, "rev.mix": 0.25, "dly.on": 1, "dly.mix": 0.15, "amp.level": -3]),
        // ------------------------------------------------------------ Bass (9)
        p("Acid Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "osc.mix": 0, "flt.cutoff": 380, "flt.res": 0.8, "flt.envamt": 4.5,
            "fenv.a": 1, "fenv.d": 180, "fenv.s": 0, "flt.drive": 0.5, "drv.on": 1, "drv.amount": 0.35,
            "trk.pitchmode": 2, "trk.glide": 50, "aenv.s": 0.85, "aenv.r": 80, "dly.on": 1, "dly.sync": 4,
            "dly.bpm": 124, "dly.fb": 0.3, "dly.mix": 0.15, "dly.duck": 0.5, "amp.level": -11.3]),
        p("FM Bass", "Bass", [
            "oscA.wave": 0, "oscA.octave": -1, "oscB.wave": 0, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0,
            "osc.fm": 0.55, "flt.type": 1, "flt.cutoff": 3000, "flt.envamt": 0, "flt.dyn": 0.5, "aenv.a": 1,
            "aenv.d": 600, "aenv.s": 0.5, "aenv.r": 120, "amp.vel": 0.7, "amp.level": -2.2]),
        p("Ladder Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.4,
            "osc.sub": 0.3, "flt.cutoff": 450, "flt.res": 0.45, "flt.envamt": 3.2, "flt.dyn": 1.5, "fenv.a": 1,
            "fenv.d": 220, "fenv.s": 0.2, "fenv.r": 120, "aenv.a": 1, "aenv.d": 300, "aenv.s": 0.75, "aenv.r": 90,
            "trk.dynamics": 0.6, "amp.level": 6.6]),
        p("Pluck Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "osc.mix": 0, "flt.cutoff": 300, "flt.res": 0.3, "flt.envamt": 4,
            "flt.drive": 0.3, "fenv.d": 160, "fenv.s": 0, "aenv.d": 450, "aenv.s": 0, "aenv.r": 120,
            "amp.level": 4.7]),
        p("Reese Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "oscA.unison": 5, "oscA.detune": 22, "oscB.wave": 2, "oscB.semi": -12,
            "oscB.fine": -10, "osc.mix": 0.4, "flt.cutoff": 700, "flt.res": 0.2, "flt.envamt": 1, "aenv.s": 0.95,
            "aenv.r": 150, "drv.on": 1, "drv.amount": 0.3, "drv.tone": 3000, "drv.mix": 0.5, "amp.level": -3.6]),
        p("Square Bass", "Bass", [
            "oscA.wave": 3, "oscA.octave": -1, "oscB.wave": 4, "osc.pw": 0.25, "oscB.semi": -12, "oscB.fine": 0,
            "osc.mix": 0.3, "flt.type": 1, "flt.cutoff": 900, "flt.envamt": 1.5, "fenv.d": 300, "fenv.s": 0.3,
            "aenv.s": 0.8, "aenv.r": 100, "amp.level": -4.3]),
        p("Sub Bass", "Bass", [
            "oscA.wave": 0, "oscA.octave": -1, "oscB.wave": 1, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.3,
            "osc.sub": 0.4, "flt.cutoff": 300, "flt.res": 0.05, "flt.envamt": 0.5, "flt.dyn": 0, "aenv.r": 150,
            "trk.dynamics": 0.7, "amp.level": 1.8]),
        p("Synth Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.5,
            "osc.sub": 0.5, "flt.cutoff": 600, "flt.res": 0.35, "flt.envamt": 3, "flt.dyn": 1.5, "fenv.d": 250,
            "fenv.s": 0.15, "aenv.a": 1, "aenv.d": 400, "aenv.s": 0.7, "aenv.r": 120, "amp.level": 2.8]),
        p("Wobble Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.5,
            "osc.sub": 0.3, "flt.cutoff": 350, "flt.res": 0.55, "flt.envamt": 1, "flt.drive": 0.4, "lfo.wave": 0,
            "lfo.rate": 3, "lfo.delay": 0, "lfo.filter": 2.5, "drv.on": 1, "drv.amount": 0.35, "aenv.s": 1,
            "aenv.r": 120, "amp.level": -8.5]),
        // ------------------------------------------------------------ Pad (7)
        p("Airy Pad", "Pad", [
            "oscA.wave": 1, "oscB.wave": 2, "oscB.semi": 12, "osc.mix": 0.25, "osc.noise": 0.15, "flt.type": 2,
            "flt.cutoff": 1400, "flt.res": 0.35, "flt.envamt": 1, "lfo.filter": 0.6, "lfo.rate": 0.2, "lfo.delay": 0,
            "aenv.a": 900, "aenv.s": 0.9, "aenv.r": 3000, "cho.on": 1, "cho.depth": 0.7, "rev.on": 1, "rev.type": 1,
            "rev.decay": 7, "rev.mix": 0.5, "rev.shimmer": 0.25, "amp.level": 11]),
        p("Analog Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 12, "oscB.wave": 4, "osc.pw": 0.4, "oscB.fine": -9,
            "osc.mix": 0.5, "lfo.pw": 0.15, "lfo.rate": 0.6, "lfo.delay": 0, "flt.cutoff": 1500, "flt.res": 0.2,
            "flt.envamt": 1, "fenv.a": 600, "fenv.d": 1500, "fenv.s": 0.6, "aenv.a": 350, "aenv.s": 0.9,
            "aenv.r": 1600, "cho.on": 1, "cho.rate": 0.5, "cho.depth": 0.6, "cho.mix": 0.45, "rev.on": 1,
            "rev.type": 1, "rev.mix": 0.3, "amp.level": 1.7]),
        p("Dark Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 14, "oscB.wave": 2, "oscB.semi": -12, "oscB.fine": 0,
            "osc.mix": 0.4, "flt.cutoff": 600, "flt.res": 0.15, "flt.envamt": 0.8, "fenv.a": 1200, "fenv.d": 3000,
            "fenv.s": 0.5, "aenv.a": 700, "aenv.s": 0.9, "aenv.r": 2500, "trk.dynamics": 0.15, "rev.on": 1,
            "rev.type": 1, "rev.decay": 6, "rev.mix": 0.45, "rev.damp": 3500, "dly.on": 1, "dly.mode": 2,
            "dly.mix": 0.15, "amp.level": 1.9]),
        p("Glass Pad", "Pad", [
            "oscA.wave": 1, "oscA.unison": 3, "oscA.detune": 10, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": 0,
            "osc.mix": 0.3, "osc.fm": 0.12, "flt.type": 1, "flt.cutoff": 6000, "flt.envamt": 0, "aenv.a": 400,
            "aenv.d": 1500, "aenv.s": 0.85, "aenv.r": 2200, "trk.dynamics": 0.2, "cho.on": 1, "rev.on": 1,
            "rev.type": 2, "rev.decay": 4, "rev.mix": 0.4, "amp.level": -1]),
        p("Hollow Pulse Pad", "Pad", [
            "oscA.wave": 4, "osc.pw": 0.3, "lfo.pw": 0.2, "lfo.rate": 0.8, "lfo.delay": 0, "oscB.wave": 4,
            "oscB.fine": 12, "osc.mix": 0.5, "flt.type": 1, "flt.cutoff": 1800, "aenv.a": 200, "aenv.r": 900,
            "pha.on": 1, "rev.on": 1, "rev.mix": 0.35, "amp.level": 4]),
        p("Sweep Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 18, "oscB.wave": 4, "osc.pw": 0.35, "osc.mix": 0.4,
            "flt.cutoff": 300, "flt.res": 0.45, "flt.envamt": 5, "fenv.a": 2500, "fenv.d": 4000, "fenv.s": 0.3,
            "fenv.r": 3000, "aenv.a": 500, "aenv.s": 0.9, "aenv.r": 2500, "pha.on": 1, "pha.rate": 0.15,
            "pha.mix": 0.4, "rev.on": 1, "rev.type": 1, "rev.decay": 5, "rev.mix": 0.4, "amp.level": 5.3]),
        p("Warm Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 7, "oscA.detune": 22, "oscB.wave": 1, "oscB.semi": 12, "osc.mix": 0.3,
            "flt.cutoff": 1200, "flt.res": 0.1, "flt.envamt": 1.5, "flt.dyn": 0.3, "fenv.a": 800, "fenv.d": 2000,
            "fenv.s": 0.6, "aenv.a": 600, "aenv.d": 1000, "aenv.s": 0.9, "aenv.r": 2000, "trk.dynamics": 0.2,
            "cho.on": 1, "cho.depth": 0.7, "dly.on": 1, "dly.mix": 0.15, "rev.on": 1, "rev.type": 1, "rev.decay": 5,
            "rev.size": 0.9, "rev.mix": 0.45, "rev.mod": 0.6, "amp.level": 1.4]),
        // ------------------------------------------------------------ Strings (5)
        p("Cello Section", "Strings", [
            "oscA.wave": 2, "oscA.octave": -1, "oscA.unison": 3, "oscA.detune": 12, "oscB.wave": 2, "oscB.semi": -12,
            "oscB.fine": 6, "osc.mix": 0.4, "flt.type": 1, "flt.cutoff": 1400, "flt.envamt": 0.6, "fenv.a": 250,
            "aenv.a": 180, "aenv.s": 0.95, "aenv.r": 900, "lfo.pitch": 10, "lfo.rate": 5.2, "lfo.delay": 500,
            "cho.on": 1, "cho.mix": 0.35, "rev.on": 1, "rev.type": 1, "rev.mix": 0.3, "amp.level": 2.5]),
        p("Pizzicato", "Strings", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 10, "oscB.wave": 1, "oscB.semi": 12, "osc.mix": 0.2,
            "flt.type": 1, "flt.cutoff": 1200, "flt.envamt": 3, "fenv.a": 1, "fenv.d": 180, "fenv.s": 0, "aenv.a": 1,
            "aenv.d": 350, "aenv.s": 0, "aenv.r": 250, "amp.vel": 0.8, "rev.on": 1, "rev.type": 1, "rev.decay": 2.2,
            "rev.mix": 0.3, "amp.level": 9]),
        p("Slow Strings", "Strings", [
            "oscA.wave": 2, "oscA.unison": 7, "oscA.detune": 16, "oscB.wave": 2, "oscB.semi": 12, "oscB.fine": 5,
            "osc.mix": 0.3, "flt.type": 1, "flt.cutoff": 2600, "flt.res": 0.1, "flt.envamt": 0.5, "fenv.a": 800,
            "aenv.a": 800, "aenv.d": 1200, "aenv.s": 0.95, "aenv.r": 1800, "trk.dynamics": 0.25, "lfo.pitch": 6,
            "lfo.rate": 5, "lfo.delay": 800, "cho.on": 1, "cho.depth": 0.8, "cho.mix": 0.5, "rev.on": 1,
            "rev.type": 1, "rev.decay": 3.5, "rev.mix": 0.38, "amp.level": 1]),
        p("Solo Violin", "Strings", [
            "oscA.wave": 2, "oscB.wave": 2, "oscB.fine": 4, "osc.mix": 0.3, "flt.type": 1, "flt.cutoff": 3200,
            "flt.res": 0.2, "flt.envamt": 1, "fenv.a": 120, "fenv.d": 600, "fenv.s": 0.6, "aenv.a": 90,
            "aenv.s": 0.95, "aenv.r": 350, "lfo.pitch": 22, "lfo.rate": 5.6, "lfo.delay": 450, "trk.pitchmode": 2,
            "trk.glide": 45, "trk.dynamics": 0.7, "rev.on": 1, "rev.type": 1, "rev.decay": 2.6, "rev.mix": 0.3,
            "amp.level": 0.9]),
        p("String Ensemble", "Strings", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 15, "oscB.wave": 4, "osc.pw": 0.3, "oscB.semi": 12,
            "osc.mix": 0.35, "flt.type": 1, "flt.cutoff": 3000, "flt.envamt": 0.5, "aenv.a": 250, "aenv.s": 0.95,
            "aenv.r": 1200, "trk.dynamics": 0.3, "cho.on": 1, "cho.depth": 0.8, "cho.mix": 0.6, "rev.on": 1,
            "rev.decay": 3.5, "rev.mix": 0.35, "amp.level": 1]),
        // ------------------------------------------------------------ Brass (4)
        p("Brass Stabs", "Brass", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 12, "oscB.wave": 2, "oscB.semi": -12, "oscB.fine": 0,
            "osc.mix": 0.4, "flt.cutoff": 900, "flt.res": 0.2, "flt.envamt": 4, "fenv.a": 5, "fenv.d": 250,
            "fenv.s": 0.2, "aenv.a": 3, "aenv.d": 300, "aenv.s": 0.3, "aenv.r": 180, "amp.vel": 0.8, "rev.on": 1,
            "rev.type": 0, "rev.decay": 1.2, "rev.mix": 0.25, "amp.level": 6.8]),
        p("Soft Horns", "Brass", [
            "oscA.wave": 2, "oscB.wave": 1, "oscB.fine": 6, "osc.mix": 0.4, "flt.type": 1, "flt.cutoff": 600,
            "flt.res": 0.05, "flt.envamt": 2.2, "flt.dyn": 2, "fenv.a": 150, "fenv.d": 800, "fenv.s": 0.6,
            "aenv.a": 120, "aenv.s": 0.9, "aenv.r": 450, "trk.dynamics": 0.6, "rev.on": 1, "rev.type": 1,
            "rev.mix": 0.3, "amp.level": 1.7]),
        p("Synth Brass", "Brass", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 9, "oscB.wave": 2, "oscB.fine": -8, "osc.mix": 0.5,
            "flt.cutoff": 700, "flt.res": 0.15, "flt.envamt": 3.5, "flt.dyn": 1.5, "fenv.a": 60, "fenv.d": 500,
            "fenv.s": 0.5, "aenv.a": 25, "aenv.d": 400, "aenv.s": 0.85, "aenv.r": 250, "cho.on": 1, "cho.mix": 0.3,
            "rev.on": 1, "rev.type": 1, "rev.mix": 0.2, "amp.level": 2]),
        p("Trumpet Lead", "Brass", [
            "oscA.wave": 2, "oscB.wave": 4, "osc.pw": 0.3, "oscB.fine": 3, "osc.mix": 0.35, "flt.cutoff": 1100,
            "flt.res": 0.25, "flt.envamt": 3, "flt.dyn": 2.5, "fenv.a": 40, "fenv.d": 400, "fenv.s": 0.55,
            "aenv.a": 20, "aenv.s": 0.9, "aenv.r": 200, "lfo.pitch": 14, "lfo.rate": 5.8, "lfo.delay": 400,
            "trk.pitchmode": 2, "trk.glide": 30, "rev.on": 1, "rev.type": 2, "rev.mix": 0.2, "amp.level": 2.5]),
        // ------------------------------------------------------------ Keys (5)
        p("Clav", "Keys", [
            "oscA.wave": 4, "osc.pw": 0.15, "oscB.wave": 4, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.2,
            "flt.cutoff": 1600, "flt.res": 0.45, "flt.envamt": 3, "flt.dyn": 2.5, "fenv.d": 180, "fenv.s": 0.2,
            "aenv.a": 0.5, "aenv.d": 500, "aenv.s": 0.3, "aenv.r": 80, "amp.vel": 0.8, "drv.on": 1,
            "drv.amount": 0.25, "drv.tone": 6000, "pha.on": 1, "pha.rate": 0.4, "pha.mix": 0.3, "amp.level": -1.1]),
        p("Electric Piano", "Keys", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 3, "osc.mix": 0.1, "osc.fm": 0.25,
            "flt.type": 1, "flt.cutoff": 7000, "flt.envamt": 0, "aenv.d": 1200, "aenv.s": 0.25, "aenv.r": 400,
            "lfo.amp": 0.25, "lfo.rate": 4.5, "lfo.delay": 0, "cho.on": 1, "cho.rate": 0.4, "cho.depth": 0.4,
            "rev.on": 1, "rev.type": 2, "rev.decay": 1.8, "rev.mix": 0.22, "amp.level": 0.9]),
        p("Harpsichord", "Keys", [
            "oscA.wave": 4, "osc.pw": 0.2, "oscB.wave": 2, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.45,
            "flt.type": 3, "flt.cutoff": 220, "flt.envamt": 0, "flt.keytrack": 0, "flt.dyn": 0, "aenv.a": 0.5,
            "aenv.d": 900, "aenv.s": 0, "aenv.r": 400, "amp.vel": 0.3, "rev.on": 1, "rev.type": 0, "rev.decay": 1.4,
            "rev.mix": 0.25, "amp.level": 7.6]),
        p("Synth Piano", "Keys", [
            "oscA.wave": 1, "oscB.wave": 2, "oscB.semi": 12, "oscB.fine": 3, "osc.mix": 0.3, "flt.type": 1,
            "flt.cutoff": 2000, "flt.envamt": 2.5, "fenv.d": 700, "fenv.s": 0.2, "aenv.a": 1, "aenv.d": 1600,
            "aenv.s": 0.2, "aenv.r": 450, "amp.vel": 0.8, "cho.on": 1, "cho.mix": 0.3, "rev.on": 1, "rev.type": 1,
            "rev.mix": 0.22, "amp.level": 3.5]),
        p("Tine Piano", "Keys", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 24, "oscB.fine": 0, "osc.mix": 0, "osc.fm": 0.3,
            "flt.type": 1, "flt.cutoff": 9000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 1, "aenv.d": 2200,
            "aenv.s": 0.15, "aenv.r": 500, "amp.vel": 0.8, "lfo.amp": 0.2, "lfo.rate": 4, "lfo.delay": 0,
            "cho.on": 1, "cho.mix": 0.3, "rev.on": 1, "rev.type": 2, "rev.mix": 0.2, "amp.level": -2.9]),
        // ------------------------------------------------------------ Organ (4)
        p("Church Organ", "Organ", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.5, "osc.sub": 0.3,
            "flt.type": 1, "flt.cutoff": 5000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 60, "aenv.d": 10,
            "aenv.s": 1, "aenv.r": 900, "trk.dynamics": 0, "amp.vel": 0, "rev.on": 1, "rev.type": 1, "rev.size": 1,
            "rev.decay": 6, "rev.mix": 0.45, "rev.predelay": 40, "amp.level": -6.1]),
        p("Organ", "Organ", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.45, "osc.sub": 0.35,
            "flt.type": 1, "flt.cutoff": 6000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 3, "aenv.d": 10, "aenv.s": 1,
            "aenv.r": 40, "trk.dynamics": 0, "amp.vel": 0, "lfo.pitch": 6, "lfo.rate": 6.5, "lfo.delay": 0,
            "cho.on": 1, "cho.rate": 4.8, "cho.depth": 0.3, "cho.mix": 0.5, "rev.on": 1, "rev.type": 0,
            "rev.decay": 1.2, "rev.mix": 0.25, "amp.level": -4.5]),
        p("Reed Organ", "Organ", [
            "oscA.wave": 4, "osc.pw": 0.35, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.3,
            "flt.type": 1, "flt.cutoff": 2400, "flt.envamt": 0, "flt.dyn": 0.3, "aenv.a": 40, "aenv.s": 1,
            "aenv.r": 200, "trk.dynamics": 0.2, "amp.vel": 0.2, "lfo.amp": 0.12, "lfo.rate": 5, "lfo.delay": 0,
            "rev.on": 1, "rev.type": 0, "rev.decay": 1.5, "rev.mix": 0.25, "amp.level": -6.7]),
        p("Rock Organ", "Organ", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": 0, "osc.mix": 0.35, "osc.sub": 0.4,
            "flt.type": 1, "flt.cutoff": 6000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 2, "aenv.s": 1, "aenv.r": 60,
            "trk.dynamics": 0, "amp.vel": 0, "drv.on": 1, "drv.amount": 0.55, "drv.tone": 5000, "cho.on": 1,
            "cho.rate": 4.5, "cho.depth": 0.35, "cho.mix": 0.55, "rev.on": 1, "rev.type": 0, "rev.mix": 0.2,
            "amp.level": -13]),
        // ------------------------------------------------------------ Pluck (5)
        p("Digital Pluck", "Pluck", [
            "oscA.wave": 2, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.3, "flt.cutoff": 600,
            "flt.res": 0.3, "flt.envamt": 4.5, "fenv.a": 1, "fenv.d": 200, "fenv.s": 0, "fenv.r": 200, "aenv.a": 1,
            "aenv.d": 500, "aenv.s": 0, "aenv.r": 400, "dly.on": 1, "dly.sync": 4, "dly.fb": 0.4, "dly.mix": 0.3,
            "rev.on": 1, "rev.type": 1, "rev.mix": 0.25, "amp.level": 9.8]),
        p("Harp", "Pluck", [
            "oscA.wave": 1, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.25, "flt.type": 1,
            "flt.cutoff": 3500, "flt.envamt": 1.5, "fenv.d": 600, "fenv.s": 0, "aenv.a": 1, "aenv.d": 1800,
            "aenv.s": 0, "aenv.r": 1200, "amp.vel": 0.6, "rev.on": 1, "rev.type": 1, "rev.decay": 3.5,
            "rev.mix": 0.38, "amp.level": 2.6]),
        p("Kalimba", "Pluck", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": 0, "osc.mix": 0, "osc.fm": 0.2,
            "flt.type": 1, "flt.cutoff": 8000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 0.5, "aenv.d": 700,
            "aenv.s": 0, "aenv.r": 600, "amp.vel": 0.7, "rev.on": 1, "rev.type": 2, "rev.decay": 1.8,
            "rev.mix": 0.25, "amp.level": 0.2]),
        p("Koto", "Pluck", [
            "oscA.wave": 2, "oscB.wave": 4, "osc.pw": 0.2, "oscB.fine": 0, "osc.mix": 0.3, "flt.type": 1,
            "flt.cutoff": 2400, "flt.res": 0.3, "flt.envamt": 2.5, "fenv.d": 250, "fenv.s": 0.1, "aenv.a": 0.5,
            "aenv.d": 900, "aenv.s": 0, "aenv.r": 500, "trk.pitchmode": 0, "rev.on": 1, "rev.type": 2,
            "rev.mix": 0.25, "dly.on": 1, "dly.mix": 0.12, "amp.level": 8.1]),
        p("Space Echo Pluck", "Pluck", [
            "oscA.wave": 1, "oscB.wave": 2, "oscB.semi": 12, "osc.mix": 0.3, "flt.cutoff": 1500, "flt.envamt": 3.5,
            "fenv.d": 250, "fenv.s": 0, "aenv.d": 600, "aenv.s": 0, "aenv.r": 500, "dly.on": 1, "dly.mode": 1,
            "dly.mod": 0.35, "dly.time": 375, "dly.fb": 0.6, "dly.mix": 0.4, "dly.damp": 2500, "rev.on": 1,
            "rev.decay": 6, "rev.mix": 0.35, "amp.level": 8.2]),
        // ------------------------------------------------------------ Bells & Mallets (6)
        p("FM Bell", "Bells & Mallets", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": 0, "osc.mix": 0, "osc.fm": 0.5,
            "flt.type": 1, "flt.cutoff": 12000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 1, "aenv.d": 1800,
            "aenv.s": 0, "aenv.r": 1500, "amp.vel": 0.8, "rev.on": 1, "rev.decay": 3, "rev.mix": 0.3, "dly.on": 1,
            "dly.mix": 0.18, "amp.level": -4.1]),
        p("Glockenspiel", "Bells & Mallets", [
            "oscA.wave": 0, "oscA.octave": 1, "oscB.wave": 1, "oscB.semi": 24, "oscB.fine": 0, "osc.mix": 0.3,
            "osc.fm": 0.1, "flt.type": 1, "flt.cutoff": 12000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 0.5,
            "aenv.d": 1400, "aenv.s": 0, "aenv.r": 1200, "rev.on": 1, "rev.type": 2, "rev.mix": 0.3, "dly.on": 1,
            "dly.sync": 5, "dly.fb": 0.25, "dly.mix": 0.15, "amp.level": 1.4]),
        p("Marimba", "Bells & Mallets", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 24, "oscB.fine": 0, "osc.mix": 0.25, "flt.type": 1,
            "flt.cutoff": 4000, "flt.envamt": 2, "fenv.d": 100, "fenv.s": 0, "aenv.a": 0.5, "aenv.d": 450,
            "aenv.s": 0, "aenv.r": 300, "amp.vel": 0.8, "rev.on": 1, "rev.type": 0, "rev.decay": 1.3,
            "rev.mix": 0.25, "amp.level": 5.2]),
        p("Steel Drum", "Bells & Mallets", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": -20, "osc.mix": 0.2, "osc.fm": 0.25,
            "flt.type": 1, "flt.cutoff": 5000, "flt.envamt": 1, "fenv.d": 200, "fenv.s": 0, "aenv.a": 0.5,
            "aenv.d": 800, "aenv.s": 0, "aenv.r": 400, "amp.vel": 0.7, "rev.on": 1, "rev.type": 0, "rev.decay": 1.6,
            "rev.mix": 0.25, "amp.level": 1.3]),
        p("Tubular Bells", "Bells & Mallets", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 17, "oscB.fine": 0, "osc.mix": 0, "osc.fm": 0.45,
            "flt.type": 1, "flt.cutoff": 10000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 1, "aenv.d": 4000,
            "aenv.s": 0, "aenv.r": 3000, "amp.vel": 0.7, "rev.on": 1, "rev.type": 1, "rev.decay": 5, "rev.mix": 0.4,
            "amp.level": -5.5]),
        p("Vibraphone", "Bells & Mallets", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 24, "oscB.fine": 0, "osc.mix": 0.18, "flt.type": 1,
            "flt.cutoff": 6000, "flt.envamt": 0, "flt.dyn": 0, "aenv.a": 1, "aenv.d": 2500, "aenv.s": 0,
            "aenv.r": 1500, "lfo.amp": 0.35, "lfo.rate": 5.5, "lfo.delay": 0, "rev.on": 1, "rev.type": 1,
            "rev.mix": 0.3, "amp.level": 1.5]),
        // ------------------------------------------------------------ Ambient (4)
        p("Drone Field", "Ambient", [
            "oscA.wave": 2, "oscA.unison": 7, "oscA.detune": 20, "oscB.wave": 0, "oscB.semi": -12, "oscB.fine": 0,
            "osc.mix": 0.4, "flt.cutoff": 900, "flt.res": 0.25, "flt.envamt": 0, "lfo.filter": 1.2, "lfo.rate": 0.08,
            "lfo.delay": 0, "aenv.a": 1500, "aenv.s": 1, "aenv.r": 5000, "trk.dynamics": 0, "dly.on": 1,
            "dly.mode": 1, "dly.time": 700, "dly.fb": 0.6, "dly.mix": 0.3, "dly.mod": 0.4, "rev.on": 1,
            "rev.type": 1, "rev.size": 1, "rev.decay": 12, "rev.mix": 0.6, "rev.shimmer": 0.3, "rev.mod": 0.8,
            "amp.level": 2.2]),
        p("Frozen Shimmer", "Ambient", [
            "oscA.wave": 1, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.4, "flt.type": 1,
            "flt.cutoff": 3500, "flt.envamt": 0, "aenv.a": 300, "aenv.s": 0.8, "aenv.r": 4000, "trk.dynamics": 0.2,
            "rev.on": 1, "rev.type": 1, "rev.size": 1, "rev.decay": 15, "rev.mix": 0.6, "rev.shimmer": 0.75,
            "rev.lowcut": 300, "rev.mod": 0.6, "dly.on": 1, "dly.sync": 4, "dly.fb": 0.5, "dly.mix": 0.25,
            "dly.mode": 2, "dly.duck": 0.5, "amp.level": -0.9]),
        p("Shimmer Pad", "Ambient", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 16, "oscB.wave": 0, "oscB.semi": 12, "osc.mix": 0.4,
            "flt.type": 1, "flt.cutoff": 2400, "flt.envamt": 1, "flt.dyn": 0.2, "aenv.a": 900, "aenv.d": 1500,
            "aenv.s": 0.9, "aenv.r": 3000, "trk.dynamics": 0.15, "dly.on": 1, "dly.sync": 4, "dly.fb": 0.45,
            "dly.mix": 0.25, "dly.mode": 2, "dly.duck": 0.4, "rev.on": 1, "rev.type": 1, "rev.size": 1,
            "rev.decay": 9, "rev.mix": 0.55, "rev.shimmer": 0.6, "rev.mod": 0.7, "rev.lowcut": 250,
            "amp.level": -0.2]),
        p("Underwater", "Ambient", [
            "oscA.wave": 0, "oscB.wave": 1, "oscB.semi": 7, "oscB.fine": 0, "osc.mix": 0.35, "flt.cutoff": 500,
            "flt.res": 0.5, "flt.envamt": 1, "lfo.filter": 1.5, "lfo.pitch": 15, "lfo.rate": 0.35, "lfo.delay": 0,
            "aenv.a": 200, "aenv.s": 0.9, "aenv.r": 2500, "cho.on": 1, "cho.rate": 0.3, "cho.depth": 1,
            "cho.mix": 0.6, "pha.on": 1, "pha.rate": 0.1, "pha.mix": 0.4, "rev.on": 1, "rev.type": 1, "rev.decay": 6,
            "rev.mix": 0.5, "rev.damp": 2500, "amp.level": 5.7]),
        // ------------------------------------------------------------ FX (5)
        p("Metallic", "FX", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 6, "oscB.fine": 30, "osc.mix": 0.2, "osc.fm": 0.8,
            "flt.type": 1, "flt.cutoff": 7000, "flt.envamt": 0, "aenv.a": 1, "aenv.d": 1500, "aenv.s": 0.2,
            "aenv.r": 1200, "pha.on": 1, "pha.rate": 1.5, "pha.fb": 0.7, "pha.mix": 0.5, "dly.on": 1,
            "dly.time": 230, "dly.fb": 0.5, "dly.mix": 0.3, "rev.on": 1, "rev.type": 2, "rev.mix": 0.3,
            "amp.level": 2.3]),
        p("Noise Sweep", "FX", [
            "oscA.wave": 2, "osc.noise": 0.6, "flt.type": 2, "flt.cutoff": 800, "flt.res": 0.7, "lfo.filter": 2.5,
            "lfo.rate": 0.3, "lfo.delay": 0, "aenv.a": 50, "aenv.r": 1500, "rev.on": 1, "rev.decay": 8,
            "rev.mix": 0.5, "dly.on": 1, "dly.fb": 0.5, "amp.level": 3.7]),
        p("Robot Voice", "FX", [
            "oscA.wave": 4, "osc.pw": 0.12, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.3,
            "flt.type": 2, "flt.cutoff": 900, "flt.res": 0.7, "flt.envamt": 0, "flt.dyn": 3, "lfo.wave": 3,
            "lfo.rate": 9, "lfo.filter": 0.8, "lfo.delay": 0, "drv.on": 1, "drv.amount": 0.3, "aenv.s": 1,
            "aenv.r": 100, "trk.pitchmode": 1, "amp.level": -4.7]),
        p("Sample & Hold", "FX", [
            "oscA.wave": 2, "oscB.wave": 3, "oscB.fine": 12, "osc.mix": 0.4, "flt.cutoff": 900, "flt.res": 0.65,
            "flt.envamt": 1, "lfo.wave": 4, "lfo.rate": 8, "lfo.delay": 0, "lfo.filter": 2.5, "dly.on": 1,
            "dly.sync": 7, "dly.fb": 0.4, "dly.mix": 0.3, "rev.on": 1, "rev.mix": 0.2, "amp.level": 2.4]),
        p("Talking Wah", "FX", [
            "oscA.wave": 2, "oscA.unison": 3, "oscB.wave": 3, "osc.mix": 0.4, "flt.cutoff": 500, "flt.res": 0.65,
            "flt.dyn": 4, "flt.envamt": 1, "fenv.a": 10, "fenv.d": 200, "fenv.s": 0.2, "drv.on": 1,
            "drv.amount": 0.3, "amp.level": -2]),
        // ------------------------------------------------------------ Retro (3)
        p("80s Poly", "Retro", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 14, "oscB.wave": 4, "osc.pw": 0.4, "oscB.fine": -6,
            "osc.mix": 0.5, "flt.type": 1, "flt.cutoff": 2200, "flt.res": 0.2, "flt.envamt": 1.5, "fenv.d": 500,
            "fenv.s": 0.5, "aenv.a": 5, "aenv.d": 400, "aenv.s": 0.8, "aenv.r": 500, "cho.on": 1, "cho.rate": 0.45,
            "cho.depth": 0.8, "cho.mix": 0.55, "dly.on": 1, "dly.sync": 4, "dly.fb": 0.3, "dly.mix": 0.2,
            "rev.on": 1, "rev.type": 1, "rev.mix": 0.25, "amp.level": 0.5]),
        p("Chiptune", "Retro", [
            "oscA.wave": 4, "osc.pw": 0.25, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.2,
            "flt.type": 1, "flt.cutoff": 16000, "flt.res": 0, "flt.envamt": 0, "flt.keytrack": 0, "flt.dyn": 0,
            "aenv.a": 0.5, "aenv.d": 60, "aenv.s": 0.8, "aenv.r": 30, "amp.vel": 0, "trk.dynamics": 0.3,
            "amp.level": -6.5]),
        p("Synthwave Lead", "Retro", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 16, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0,
            "osc.mix": 0.25, "flt.cutoff": 3000, "flt.res": 0.2, "flt.envamt": 1, "aenv.s": 0.9, "aenv.r": 400,
            "trk.pitchmode": 2, "trk.glide": 50, "lfo.pitch": 10, "lfo.delay": 500, "cho.on": 1, "cho.mix": 0.35,
            "dly.on": 1, "dly.mode": 1, "dly.sync": 4, "dly.fb": 0.45, "dly.mix": 0.3, "dly.duck": 0.4, "rev.on": 1,
            "rev.type": 1, "rev.decay": 4, "rev.mix": 0.3, "amp.level": 2]),
    ]

    private static func p(_ name: String, _ cat: String, _ v: [String: Float]) -> Preset {
        Preset(name: name, category: cat, isFactory: true, values: v)
    }
}

/// User presets live in ~/Library/Application Support/GuitarSynthX/Presets.
enum PresetStore {
    static var directory: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let dir = base.appendingPathComponent("GuitarSynthX/Presets", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    static func loadUser() -> [Preset] {
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)) ?? []
        return files.filter { $0.pathExtension == "json" }.compactMap { url in
            guard let data = try? Data(contentsOf: url) else { return nil }
            return try? JSONDecoder().decode(Preset.self, from: data)
        }.sorted { $0.name < $1.name }
    }

    static func save(_ preset: Preset) throws {
        let safe = preset.name.replacingOccurrences(of: "/", with: "-")
        let url = directory.appendingPathComponent(safe + ".json")
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        try enc.encode(preset).write(to: url)
    }

    static func delete(_ preset: Preset) {
        let safe = preset.name.replacingOccurrences(of: "/", with: "-")
        try? FileManager.default.removeItem(at: directory.appendingPathComponent(safe + ".json"))
    }
}
