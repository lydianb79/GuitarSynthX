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
    static let all: [Preset] = [
        // ------------------------------------------------------------ Leads
        p("Fat Saw Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 18, "oscB.wave": 2, "oscB.fine": 7, "osc.mix": 0.3,
            "flt.cutoff": 3200, "flt.res": 0.25, "flt.envamt": 2.5, "fenv.d": 400, "fenv.s": 0.4,
            "aenv.a": 2, "aenv.d": 300, "aenv.s": 0.9, "aenv.r": 300,
            "trk.pitchmode": 2, "trk.glide": 40, "lfo.pitch": 12, "lfo.delay": 450,
            "cho.on": 1, "dly.on": 1, "dly.time": 380, "dly.fb": 0.3, "dly.mix": 0.2, "rev.on": 1, "rev.mix": 0.18]),
        p("Square Lead", "Lead", [
            "oscA.wave": 3, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.25,
            "flt.cutoff": 2800, "flt.res": 0.3, "flt.envamt": 1.5, "lfo.pitch": 15, "lfo.delay": 350,
            "dly.on": 1, "dly.time": 330, "dly.mix": 0.22, "rev.on": 1, "rev.mix": 0.15]),
        p("Screaming Lead", "Lead", [
            "oscA.wave": 2, "oscB.wave": 4, "oscB.semi": 7, "osc.mix": 0.4, "osc.pw": 0.35,
            "flt.cutoff": 1800, "flt.res": 0.55, "flt.envamt": 3.5, "flt.drive": 0.6,
            "drv.on": 1, "drv.amount": 0.5, "drv.tone": 4500, "lfo.pitch": 18, "lfo.delay": 300,
            "dly.on": 1, "dly.time": 420, "dly.fb": 0.4, "dly.mix": 0.25, "rev.on": 1, "rev.mix": 0.2]),
        p("Glide Saw", "Lead", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 10, "oscB.wave": 2, "oscB.semi": -12, "osc.mix": 0.35,
            "flt.cutoff": 2200, "flt.res": 0.3, "flt.envamt": 2, "trk.pitchmode": 2, "trk.glide": 120,
            "dly.on": 1, "dly.time": 450, "dly.fb": 0.45, "rev.on": 1, "rev.mix": 0.2]),
        p("Bend Sine (Track)", "Lead", [
            "oscA.wave": 0, "osc.mix": 0, "flt.type": 1, "flt.cutoff": 12000, "flt.envamt": 0, "flt.dyn": 0,
            "trk.pitchmode": 0, "trk.dynamics": 0.9, "aenv.a": 3, "aenv.s": 1, "aenv.r": 180,
            "rev.on": 1, "rev.mix": 0.22]),
        p("Ambient Lead", "Lead", [
            "oscA.wave": 2, "oscA.unison": 3, "oscA.detune": 9, "oscB.wave": 1, "oscB.semi": 12, "osc.mix": 0.3,
            "flt.cutoff": 2600, "flt.res": 0.2, "flt.envamt": 1.5, "trk.pitchmode": 2, "trk.glide": 35,
            "lfo.pitch": 10, "lfo.delay": 500, "aenv.a": 8, "aenv.s": 0.9, "aenv.r": 600,
            "dly.on": 1, "dly.sync": 4, "dly.fb": 0.55, "dly.mix": 0.4, "dly.duck": 0.75, "dly.mod": 0.2,
            "dly.lowcut": 200, "dly.damp": 4000,
            "rev.on": 1, "rev.type": 1, "rev.decay": 4.5, "rev.size": 0.85, "rev.mix": 0.35, "rev.duck": 0.5,
            "rev.lowcut": 180]),
        p("Chiptune", "Lead", [
            "oscA.wave": 4, "osc.pw": 0.25, "oscB.wave": 3, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.2,
            "flt.type": 1, "flt.cutoff": 16000, "flt.res": 0, "flt.envamt": 0, "flt.keytrack": 0, "flt.dyn": 0,
            "aenv.a": 0.5, "aenv.d": 60, "aenv.s": 0.8, "aenv.r": 30, "amp.vel": 0, "trk.dynamics": 0.3]),

        // ------------------------------------------------------------ Bass
        p("Synth Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "oscB.wave": 3, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.5,
            "osc.sub": 0.5, "flt.cutoff": 600, "flt.res": 0.35, "flt.envamt": 3, "flt.dyn": 1.5,
            "fenv.d": 250, "fenv.s": 0.15, "aenv.a": 1, "aenv.d": 400, "aenv.s": 0.7, "aenv.r": 120]),
        p("Acid Bass", "Bass", [
            "oscA.wave": 2, "oscA.octave": -1, "osc.mix": 0, "flt.cutoff": 380, "flt.res": 0.8, "flt.envamt": 4.5,
            "fenv.a": 1, "fenv.d": 180, "fenv.s": 0, "flt.drive": 0.5, "drv.on": 1, "drv.amount": 0.35,
            "trk.pitchmode": 2, "trk.glide": 50, "aenv.s": 0.85, "aenv.r": 80,
            "dly.on": 1, "dly.sync": 4, "dly.bpm": 124, "dly.fb": 0.3, "dly.mix": 0.15, "dly.duck": 0.5]),
        p("Sub Bass", "Bass", [
            "oscA.wave": 0, "oscA.octave": -1, "oscB.wave": 1, "oscB.semi": -12, "oscB.fine": 0, "osc.mix": 0.3,
            "osc.sub": 0.4, "flt.cutoff": 300, "flt.res": 0.05, "flt.envamt": 0.5, "flt.dyn": 0,
            "aenv.r": 150, "trk.dynamics": 0.7]),

        // ------------------------------------------------------------ Pads & strings
        p("Warm Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 7, "oscA.detune": 22, "oscB.wave": 1, "oscB.semi": 12, "osc.mix": 0.3,
            "flt.cutoff": 1200, "flt.res": 0.1, "flt.envamt": 1.5, "flt.dyn": 0.3,
            "fenv.a": 800, "fenv.d": 2000, "fenv.s": 0.6, "aenv.a": 600, "aenv.d": 1000, "aenv.s": 0.9, "aenv.r": 2000,
            "trk.dynamics": 0.2, "cho.on": 1, "cho.depth": 0.7, "dly.on": 1, "dly.mix": 0.15,
            "rev.on": 1, "rev.type": 1, "rev.decay": 5, "rev.size": 0.9, "rev.mix": 0.45, "rev.mod": 0.6]),
        p("Shimmer Pad", "Pad", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 16, "oscB.wave": 0, "oscB.semi": 12, "osc.mix": 0.4,
            "flt.type": 1, "flt.cutoff": 2400, "flt.envamt": 1, "flt.dyn": 0.2,
            "aenv.a": 900, "aenv.d": 1500, "aenv.s": 0.9, "aenv.r": 3000, "trk.dynamics": 0.15,
            "dly.on": 1, "dly.sync": 4, "dly.fb": 0.45, "dly.mix": 0.25, "dly.mode": 2, "dly.duck": 0.4,
            "rev.on": 1, "rev.type": 1, "rev.size": 1.0, "rev.decay": 9, "rev.mix": 0.55,
            "rev.shimmer": 0.6, "rev.mod": 0.7, "rev.lowcut": 250]),
        p("String Ensemble", "Pad", [
            "oscA.wave": 2, "oscA.unison": 5, "oscA.detune": 15, "oscB.wave": 4, "osc.pw": 0.3, "oscB.semi": 12, "osc.mix": 0.35,
            "flt.type": 1, "flt.cutoff": 3000, "flt.envamt": 0.5, "aenv.a": 250, "aenv.s": 0.95, "aenv.r": 1200,
            "trk.dynamics": 0.3, "cho.on": 1, "cho.depth": 0.8, "cho.mix": 0.6, "rev.on": 1, "rev.decay": 3.5, "rev.mix": 0.35]),
        p("Hollow Pulse Pad", "Pad", [
            "oscA.wave": 4, "osc.pw": 0.3, "lfo.pw": 0.2, "lfo.rate": 0.8, "lfo.delay": 0, "oscB.wave": 4, "oscB.fine": 12,
            "osc.mix": 0.5, "flt.type": 1, "flt.cutoff": 1800, "aenv.a": 200, "aenv.r": 900,
            "pha.on": 1, "rev.on": 1, "rev.mix": 0.35]),

        // ------------------------------------------------------------ Keys
        p("FM Bell", "Keys", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 19, "oscB.fine": 0, "osc.mix": 0, "osc.fm": 0.5,
            "flt.type": 1, "flt.cutoff": 12000, "flt.envamt": 0, "flt.dyn": 0,
            "aenv.a": 1, "aenv.d": 1800, "aenv.s": 0, "aenv.r": 1500, "amp.vel": 0.8,
            "rev.on": 1, "rev.decay": 3, "rev.mix": 0.3, "dly.on": 1, "dly.mix": 0.18]),
        p("Electric Piano", "Keys", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 3, "osc.mix": 0.1, "osc.fm": 0.25,
            "flt.type": 1, "flt.cutoff": 7000, "flt.envamt": 0, "aenv.d": 1200, "aenv.s": 0.25, "aenv.r": 400,
            "lfo.amp": 0.25, "lfo.rate": 4.5, "lfo.delay": 0, "cho.on": 1, "cho.rate": 0.4, "cho.depth": 0.4,
            "rev.on": 1, "rev.type": 2, "rev.decay": 1.8, "rev.mix": 0.22]),
        p("Organ", "Keys", [
            "oscA.wave": 0, "oscB.wave": 0, "oscB.semi": 12, "oscB.fine": 0, "osc.mix": 0.45, "osc.sub": 0.35,
            "flt.type": 1, "flt.cutoff": 6000, "flt.envamt": 0, "flt.dyn": 0,
            "aenv.a": 3, "aenv.d": 10, "aenv.s": 1, "aenv.r": 40, "trk.dynamics": 0, "amp.vel": 0,
            "lfo.pitch": 6, "lfo.rate": 6.5, "lfo.delay": 0, "cho.on": 1, "cho.rate": 5.8, "cho.depth": 0.3, "cho.mix": 0.5,
            "rev.on": 1, "rev.type": 0, "rev.decay": 1.2, "rev.mix": 0.25]),

        // ------------------------------------------------------------ FX
        p("Talking Wah", "FX", [
            "oscA.wave": 2, "oscA.unison": 3, "oscB.wave": 3, "osc.mix": 0.4, "flt.cutoff": 500, "flt.res": 0.65,
            "flt.dyn": 4, "flt.envamt": 1, "fenv.a": 10, "fenv.d": 200, "fenv.s": 0.2,
            "drv.on": 1, "drv.amount": 0.3]),
        p("Noise Sweep", "FX", [
            "oscA.wave": 2, "osc.noise": 0.6, "flt.type": 2, "flt.cutoff": 800, "flt.res": 0.7,
            "lfo.filter": 2.5, "lfo.rate": 0.3, "lfo.delay": 0, "aenv.a": 50, "aenv.r": 1500,
            "rev.on": 1, "rev.decay": 8, "rev.mix": 0.5, "dly.on": 1, "dly.fb": 0.5]),
        p("Space Echo Pluck", "FX", [
            "oscA.wave": 1, "oscB.wave": 2, "oscB.semi": 12, "osc.mix": 0.3, "flt.cutoff": 1500, "flt.envamt": 3.5,
            "fenv.d": 250, "fenv.s": 0, "aenv.d": 600, "aenv.s": 0.0, "aenv.r": 500,
            "dly.on": 1, "dly.mode": 1, "dly.mod": 0.35, "dly.time": 375, "dly.fb": 0.6, "dly.mix": 0.4, "dly.damp": 2500,
            "rev.on": 1, "rev.decay": 6, "rev.mix": 0.35]),
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
