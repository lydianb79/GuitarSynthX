import SwiftUI

enum PanelTab: String, CaseIterable, Identifiable {
    case tracking = "Input & Tracking"
    case osc = "Oscillators"
    case filter = "Filter"
    case amp = "Amp & LFO"
    case fx = "Effects"
    var id: String { rawValue }
}

struct ContentView: View {
    @EnvironmentObject var synth: SynthController
    @StateObject private var status = StatusModel()
    @State private var tab: PanelTab = .osc
    @State private var showSettings = false

    var body: some View {
        HStack(spacing: 0) {
            PresetSidebar()
                .frame(width: 210)
            Divider().background(Theme.stroke)
            VStack(spacing: 0) {
                TopBar(status: status, showSettings: $showSettings)
                if let err = synth.errorMessage { ErrorBanner(text: err) }
                if synth.micPermission == .denied || synth.micPermission == .restricted { MicBanner() }
                ZStack(alignment: .topLeading) {
                    MetalVisualizer()
                    TunerHUD(status: status)
                        .padding(14)
                }
                .frame(minHeight: 230)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                .padding(.horizontal, 12)
                .padding(.top, 8)

                Picker("", selection: $tab) {
                    ForEach(PanelTab.allCases) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .labelsHidden()
                .padding(.horizontal, 12)
                .padding(.vertical, 10)

                ScrollView {
                    PanelView(tab: tab)
                        .padding(.horizontal, 12)
                        .padding(.bottom, 12)
                }
                .frame(minHeight: 250)
            }
            Divider().background(Theme.stroke)
            MasterStrip(status: status)
                .frame(width: 112)
        }
        .background(Theme.bg)
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showSettings) {
            AudioSettingsView().environmentObject(synth)
        }
        .onAppear { status.startPolling() }
    }
}

// MARK: - Top bar

struct TopBar: View {
    @EnvironmentObject var synth: SynthController
    @ObservedObject var status: StatusModel
    @Binding var showSettings: Bool

    var body: some View {
        let s = status.status
        HStack(spacing: 14) {
            Button(action: synth.toggleRunning) {
                HStack(spacing: 6) {
                    Image(systemName: "power")
                    Text(synth.running ? "Audio On" : "Audio Off")
                }
                .padding(.horizontal, 10).padding(.vertical, 5)
                .background(Capsule().fill(synth.running ? Theme.accent.opacity(0.25) : Color.white.opacity(0.08)))
                .overlay(Capsule().stroke(synth.running ? Theme.accent : Theme.stroke))
            }
            .buttonStyle(.plain)
            .help("Start/stop audio (⌘R)")

            VStack(alignment: .leading, spacing: 1) {
                Text(deviceSummary).font(.system(size: 11, weight: .medium)).lineLimit(1)
                Text(String(format: "%.1f kHz · %d smp · %@", s.sampleRate / 1000, s.bufferFrames, s.duplex != 0 ? "duplex" : "2 devices"))
                    .font(.system(size: 10, design: .monospaced)).foregroundColor(Theme.text2)
            }
            Spacer()
            Stat(label: "LATENCY", value: String(format: "%.1f ms", s.inputLatencyMs + s.outputLatencyMs),
                 help: "Hardware round trip (input + output, incl. buffers). Pitch detection adds ~5–25 ms depending on the note.")
            Stat(label: "DSP", value: String(format: "%.0f%%", s.cpuLoad * 100),
                 color: s.cpuLoad > 0.8 ? Theme.warn : .white, help: "Peak share of the real-time budget used by the audio callback.")
            Stat(label: "XRUNS", value: "\(s.overloads)", color: s.overloads > 0 ? Theme.warn : .white,
                 help: "Audio dropouts reported by the device. Increase the buffer size if this keeps rising.")
            Stat(label: "VOICES", value: "\(s.activeVoices)")
            Button { synth.panic() } label: { Image(systemName: "exclamationmark.octagon") }
                .help("Panic: silence all voices and effect tails (⌘.)")
            Button { synth.refreshDevices(); showSettings = true } label: { Image(systemName: "slider.horizontal.3") }
                .help("Audio settings (⌘,)")
                .keyboardShortcut(",", modifiers: [.command])
        }
        .padding(.horizontal, 14)
        .frame(height: 50)
        .background(Theme.panel)
    }

    private var deviceSummary: String {
        let i = synth.device(synth.inputDeviceID)?.name ?? "No input"
        let o = synth.device(synth.outputDeviceID)?.name ?? "No output"
        return i == o ? i : "\(i) → \(o)"
    }
}

struct Stat: View {
    let label: String
    let value: String
    var color: Color = .white
    var help: String = ""
    var body: some View {
        VStack(spacing: 1) {
            Text(label).font(.system(size: 8, weight: .bold)).tracking(1).foregroundColor(Theme.text2)
            Text(value).font(.system(size: 12, weight: .semibold, design: .monospaced)).foregroundColor(color)
        }
        .frame(minWidth: 52)
        .help(help)
    }
}

struct ErrorBanner: View {
    @EnvironmentObject var synth: SynthController
    let text: String
    var body: some View {
        HStack {
            Image(systemName: "exclamationmark.triangle.fill").foregroundColor(Theme.warn)
            Text(text).font(.system(size: 12))
            Spacer()
            Button("Dismiss") { synth.errorMessage = nil }
        }
        .padding(8)
        .background(Theme.warn.opacity(0.15))
    }
}

struct MicBanner: View {
    @EnvironmentObject var synth: SynthController
    var body: some View {
        HStack {
            Image(systemName: "mic.slash.fill").foregroundColor(Theme.warn)
            Text("Microphone access is off — GuitarSynthX can't hear your guitar.").font(.system(size: 12))
            Spacer()
            Button("Open Privacy Settings") { synth.openMicrophoneSettings() }
        }
        .padding(8)
        .background(Theme.warn.opacity(0.15))
    }
}

// MARK: - Tuner / note HUD

struct TunerHUD: View {
    @EnvironmentObject var synth: SynthController
    @ObservedObject var status: StatusModel

    var body: some View {
        let count = max(1, Int(status.status.trackerCount))
        let t0 = status.trackers[0]
        VStack(alignment: .leading, spacing: 6) {
            if count == 1 {
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    let note = NoteName.of(midi: t0.midi)
                    Text(t0.frequency > 0 ? note.name : "–")
                        .font(.system(size: 44, weight: .heavy, design: .rounded))
                        .foregroundColor(t0.noteActive != 0 ? Theme.accent : .white.opacity(0.35))
                    if t0.frequency > 0 {
                        Text("\(note.octave)").font(.system(size: 18, weight: .bold, design: .rounded)).foregroundColor(Theme.text2)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(String(format: "%.1f Hz", t0.frequency)).font(.system(size: 11, design: .monospaced))
                            CentsBar(cents: note.cents).frame(width: 120, height: 6)
                        }
                    }
                }
            } else {
                HStack(spacing: 8) {
                    ForEach(0..<6, id: \.self) { i in
                        let t = status.trackers[i]
                        let note = NoteName.of(midi: t.midi)
                        VStack(spacing: 2) {
                            Text("S\(6 - i)").font(.system(size: 9, weight: .bold)).foregroundColor(Theme.text2)
                            Text(t.noteActive != 0 ? "\(note.name)\(note.octave)" : "–")
                                .font(.system(size: 16, weight: .bold, design: .rounded))
                                .foregroundColor(t.noteActive != 0 ? Theme.accent : .white.opacity(0.3))
                        }
                        .frame(width: 44)
                        .padding(.vertical, 4)
                        .background(RoundedRectangle(cornerRadius: 6).fill(Color.black.opacity(0.35)))
                    }
                }
            }
            HStack(spacing: 6) {
                Circle().fill(t0.gate != 0 ? Theme.accent : Color.white.opacity(0.15)).frame(width: 7, height: 7)
                Text(t0.gate != 0 ? "GATE OPEN" : "GATE").font(.system(size: 9, weight: .bold)).tracking(1)
                    .foregroundColor(Theme.text2)
                Text(String(format: "%.0f dBFS", max(t0.levelDb, -99))).font(.system(size: 9, design: .monospaced))
                    .foregroundColor(Theme.text2)
                Text("· \(synth.currentPresetName)").font(.system(size: 10, weight: .semibold)).foregroundColor(.white.opacity(0.7))
            }
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 10).fill(Color.black.opacity(0.35)))
    }
}

struct CentsBar: View {
    let cents: Float
    var body: some View {
        GeometryReader { g in
            let x = CGFloat((cents + 50) / 100) * g.size.width
            ZStack(alignment: .leading) {
                Capsule().fill(Color.white.opacity(0.1))
                Rectangle().fill(Color.white.opacity(0.4)).frame(width: 1).offset(x: g.size.width / 2)
                Circle().fill(abs(cents) < 8 ? Theme.accent : Theme.warn).frame(width: 8, height: 8).offset(x: x - 4)
            }
        }
    }
}

// MARK: - Panels

struct PanelView: View {
    @EnvironmentObject var synth: SynthController
    let tab: PanelTab

    private let columns = [GridItem(.adaptive(minimum: 260), spacing: 10, alignment: .top)]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            LazyVGrid(columns: columns, alignment: .leading, spacing: 10) {
                switch tab {
                case .tracking:
                    card("Input", keys: ["in.mode", "in.channel", "in.gain", "in.gate", "out.channel"])
                    card("Pitch Detection", keys: ["trk.lowest", "trk.highest", "trk.threshold", "trk.stability", "trk.hop"])
                    card("Note Behaviour", keys: ["trk.pitchmode", "trk.glide", "trk.transpose", "trk.retrigger", "trk.dynamics"])
                case .osc:
                    card("Oscillator A", keys: ["oscA.wave", "oscA.octave", "oscA.unison", "oscA.detune"])
                    card("Oscillator B", keys: ["oscB.wave", "oscB.semi", "oscB.fine", "osc.fm"], tint: Theme.accent2)
                    card("Mixer", keys: ["osc.mix", "osc.pw", "osc.sub", "osc.noise"])
                case .filter:
                    card("Filter", keys: ["flt.type", "flt.cutoff", "flt.res", "flt.drive", "flt.keytrack", "flt.dyn"])
                    card("Filter Envelope", keys: ["flt.envamt", "fenv.a", "fenv.d", "fenv.s", "fenv.r"], tint: Theme.accent2)
                case .amp:
                    card("Amp Envelope", keys: ["aenv.a", "aenv.d", "aenv.s", "aenv.r", "amp.vel"])
                    card("LFO", keys: ["lfo.wave", "lfo.rate", "lfo.delay", "lfo.pitch", "lfo.filter", "lfo.pw", "lfo.amp"], tint: Theme.accent2)
                case .fx:
                    card("Drive", toggle: "drv.on", keys: ["drv.amount", "drv.tone", "drv.mix"], tint: Theme.warn)
                    card("Chorus", toggle: "cho.on", keys: ["cho.rate", "cho.depth", "cho.mix"])
                    card("Phaser", toggle: "pha.on", keys: ["pha.rate", "pha.depth", "pha.fb", "pha.mix"], tint: Theme.accent2)
                }
            }
            if tab == .fx {
                // Delay and reverb get full-width cards: they have the most controls.
                card("Delay", toggle: "dly.on",
                     keys: ["dly.mode", "dly.sync", "dly.time", "dly.bpm", "dly.fb", "dly.mix",
                            "dly.lowcut", "dly.damp", "dly.mod", "dly.duck", "dly.pingpong"])
                card("Reverb", toggle: "rev.on",
                     keys: ["rev.type", "rev.size", "rev.decay", "rev.predelay", "rev.mix",
                            "rev.lowcut", "rev.damp", "rev.mod", "rev.shimmer", "rev.width", "rev.duck"],
                     tint: Theme.accent2)
            }
        }
    }

    @ViewBuilder
    private func card(_ title: String, toggle: String? = nil, keys: [String], tint: Color = Theme.accent) -> some View {
        Card(title: title, toggle: toggle.flatMap { synth.param($0) }, tint: tint) {
            FlowRow(spacing: 4) {
                ForEach(keys.compactMap { synth.param($0) }) { p in
                    if p.key == "dly.time" || p.key == "dly.bpm", let sync = synth.param("dly.sync") {
                        // Time is used in Free mode, Tempo when synced: dim the inactive one.
                        SyncAware(param: p, sync: sync, activeWhenSynced: p.key == "dly.bpm", tint: tint)
                    } else {
                        ParamControl(param: p, tint: tint)
                    }
                }
            }
        }
    }
}

/// Simple wrapping row layout (macOS 13 Layout protocol).
struct FlowRow: Layout {
    var spacing: CGFloat = 4

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let maxW = proposal.width ?? 600
        var x: CGFloat = 0, y: CGFloat = 0, rowH: CGFloat = 0, widest: CGFloat = 0
        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if x > 0 && x + s.width > maxW { y += rowH + spacing; x = 0; rowH = 0 }
            x += s.width + spacing
            rowH = max(rowH, s.height)
            widest = max(widest, x)
        }
        return CGSize(width: min(widest, maxW), height: y + rowH)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX, y = bounds.minY, rowH: CGFloat = 0
        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if x > bounds.minX && x + s.width > bounds.maxX { y += rowH + spacing; x = bounds.minX; rowH = 0 }
            v.place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(s))
            x += s.width + spacing
            rowH = max(rowH, s.height)
        }
    }
}

struct SyncAware: View {
    @ObservedObject var param: Param
    @ObservedObject var sync: Param
    let activeWhenSynced: Bool
    var tint: Color
    var body: some View {
        let synced = sync.value > 0.5
        ParamControl(param: param, tint: tint)
            .opacity(synced == activeWhenSynced ? 1 : 0.35)
    }
}

// MARK: - Master strip

struct MasterStrip: View {
    @EnvironmentObject var synth: SynthController
    @ObservedObject var status: StatusModel

    var body: some View {
        VStack(spacing: 10) {
            Text("MASTER").font(.system(size: 10, weight: .bold)).tracking(1.2).foregroundColor(.white.opacity(0.7))
                .padding(.top, 14)
            ForEach(["mst.synth", "mst.dry", "mst.volume"], id: \.self) { k in
                if let p = synth.param(k) { Knob(param: p) }
            }
            HStack(spacing: 0) {
                if let lim = synth.param("mst.limiter") { ParamControl(param: lim) }
            }
            if let dfx = synth.param("mst.dryfx") {
                ParamControl(param: dfx)
                    .help("On: the dry guitar also goes through drive, chorus, phaser, delay and reverb. Off: dry guitar stays clean.")
            }
            HStack(spacing: 10) {
                VStack(spacing: 4) {
                    PeakMeter(level: status.status.inputPeak)
                    Text("IN").font(.system(size: 8, weight: .bold)).foregroundColor(Theme.text2)
                }
                VStack(spacing: 4) {
                    PeakMeter(level: status.status.outputPeak)
                    Text("OUT").font(.system(size: 8, weight: .bold)).foregroundColor(Theme.text2)
                }
            }
            .frame(maxHeight: .infinity)
            .padding(.bottom, 14)
        }
        .frame(maxWidth: .infinity)
        .background(Theme.panel)
    }
}

// MARK: - Preset browser

struct PresetSidebar: View {
    @EnvironmentObject var synth: SynthController
    @State private var newName = ""
    @State private var showSave = false

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("GuitarSynth").font(.system(size: 15, weight: .heavy, design: .rounded))
                Text("X").font(.system(size: 15, weight: .heavy, design: .rounded)).foregroundColor(Theme.accent)
                Spacer()
            }
            .padding(.horizontal, 14)
            .frame(height: 50)
            .background(Theme.panel)

            List {
                ForEach(categories, id: \.self) { cat in
                    Section(cat) {
                        ForEach(synth.allPresets.filter { $0.category == cat }) { preset in
                            PresetRow(preset: preset, selected: preset.name == synth.currentPresetName)
                                .contentShape(Rectangle())
                                .onTapGesture { synth.apply(preset) }
                                .contextMenu {
                                    if !preset.isFactory { Button("Delete", role: .destructive) { synth.delete(preset) } }
                                }
                        }
                    }
                }
            }
            .listStyle(.sidebar)
            .scrollContentBackground(.hidden)

            HStack {
                Button("Init") { synth.initPatch() }
                Spacer()
                Button {
                    newName = synth.currentPresetName == "Init" ? "My Sound" : synth.currentPresetName + " 2"
                    showSave = true
                } label: { Label("Save", systemImage: "square.and.arrow.down") }
            }
            .padding(10)
        }
        .background(Theme.panel.opacity(0.6))
        .alert("Save Preset", isPresented: $showSave) {
            TextField("Name", text: $newName)
            Button("Save") { if !newName.trimmingCharacters(in: .whitespaces).isEmpty { synth.saveCurrentAsPreset(named: newName) } }
            Button("Cancel", role: .cancel) {}
        }
    }

    private var categories: [String] {
        var seen: [String] = []
        for p in synth.allPresets where !seen.contains(p.category) { seen.append(p.category) }
        return seen
    }
}

struct PresetRow: View {
    let preset: Preset
    let selected: Bool
    var body: some View {
        HStack {
            Circle().fill(selected ? Theme.accent : Color.clear).frame(width: 6, height: 6)
            Text(preset.name).font(.system(size: 12, weight: selected ? .semibold : .regular))
                .foregroundColor(selected ? .white : .white.opacity(0.75))
            Spacer()
        }
        .padding(.vertical, 2)
    }
}
