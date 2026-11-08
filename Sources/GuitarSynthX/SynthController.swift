import Foundation
import AVFoundation
import AppKit
import GSXCore

struct AudioDevice: Identifiable, Hashable {
    let id: UInt32
    let name: String
    let uid: String
    let inputChannels: Int
    let outputChannels: Int
    let isDefaultInput: Bool
    let isDefaultOutput: Bool
}

/// Owns the engine lifecycle, the audio configuration, parameters and presets.
final class SynthController: ObservableObject {
    // devices / configuration
    @Published private(set) var devices: [AudioDevice] = []
    @Published var inputDeviceID: UInt32 = 0
    @Published var outputDeviceID: UInt32 = 0
    @Published var sampleRate: Double = 48000
    @Published var bufferFrames: Int = 64
    @Published private(set) var running = false
    @Published var errorMessage: String?
    @Published private(set) var micPermission: AVAuthorizationStatus = .notDetermined

    // parameters
    let params: [Param]
    private let byKey: [String: Param]

    // presets
    @Published private(set) var userPresets: [Preset] = []
    @Published var currentPresetName: String = "Init"

    private let defaults = UserDefaults.standard
    private var saveWork: DispatchWorkItem?

    init() {
        let count = gsx_param_count()
        params = (0..<count).map { Param(id: $0) }
        var map: [String: Param] = [:]
        for p in params { map[p.key] = p }
        byKey = map
        for p in params { p.onChange = { [weak self] in self?.scheduleStateSave() } }

        restoreState()
        userPresets = PresetStore.loadUser()
        refreshDevices()
        restoreAudioConfig()
    }

    func param(_ key: String) -> Param? { byKey[key] }
    func params(inGroup group: String) -> [Param] { params.filter { $0.group == group } }

    // MARK: - Microphone permission + startup

    private var booted = false

    func boot() {
        guard !booted else { return }
        booted = true
        micPermission = AVCaptureDevice.authorizationStatus(for: .audio)
        switch micPermission {
        case .authorized:
            start()
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .audio) { [weak self] _ in
                DispatchQueue.main.async {
                    self?.micPermission = AVCaptureDevice.authorizationStatus(for: .audio)
                    self?.start()
                }
            }
        default:
            start()   // output still works; input will be silent until access is granted
        }
    }

    func openMicrophoneSettings() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone") {
            NSWorkspace.shared.open(url)
        }
    }

    // MARK: - Devices

    func refreshDevices() {
        let n = gsx_device_count()
        var list: [AudioDevice] = []
        for i in 0..<n {
            var info = GSXDeviceInfo()
            guard gsx_device_info(i, &info) == 1 else { continue }
            list.append(AudioDevice(id: info.deviceID, name: cString(info.name), uid: cString(info.uid),
                                    inputChannels: Int(info.inputChannels), outputChannels: Int(info.outputChannels),
                                    isDefaultInput: info.isDefaultInput != 0, isDefaultOutput: info.isDefaultOutput != 0))
        }
        devices = list
    }

    var inputDevices: [AudioDevice] { devices.filter { $0.inputChannels > 0 } }
    var outputDevices: [AudioDevice] { devices.filter { $0.outputChannels > 0 } }

    func device(_ id: UInt32) -> AudioDevice? { devices.first { $0.id == id } }

    func supportedSampleRates(for id: UInt32) -> [Double] {
        var rates = [Double](repeating: 0, count: 16)
        let n = Int(gsx_device_sample_rates(id, &rates, Int32(rates.count)))
        return n > 0 ? Array(rates.prefix(n)) : [44100, 48000, 96000]
    }

    func supportedBufferSizes(for id: UInt32) -> [Int] {
        var mn: Int32 = 16, mx: Int32 = 4096
        _ = gsx_device_buffer_range(id, &mn, &mx)
        return [16, 32, 48, 64, 96, 128, 192, 256, 512, 1024, 2048].filter { $0 >= Int(mn) && $0 <= Int(mx) }
    }

    // MARK: - Engine lifecycle

    func start() {
        gsx_stop()
        var cfg = GSXAudioConfig(inputDeviceID: inputDeviceID, outputDeviceID: outputDeviceID,
                                 sampleRate: sampleRate, bufferFrames: Int32(bufferFrames))
        var err = [CChar](repeating: 0, count: 512)
        let ok = gsx_start(&cfg, &err, Int32(err.count)) == 1
        running = ok
        if ok {
            errorMessage = nil
            saveAudioConfig()
        } else {
            errorMessage = err.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
        }
    }

    func stop() {
        gsx_stop()
        running = false
    }

    func toggleRunning() { running ? stop() : start() }

    func panic() { gsx_panic() }

    // MARK: - Presets

    var allPresets: [Preset] { FactoryPresets.all + userPresets }

    func apply(_ preset: Preset) {
        for p in params where isSoundParam(p.key) {
            p.set(preset.values[p.key] ?? p.defaultValue)
        }
        currentPresetName = preset.name
        defaults.set(preset.name, forKey: "presetName")
    }

    func initPatch() {
        apply(Preset(name: "Init", category: "", values: [:]))
    }

    func saveCurrentAsPreset(named name: String) {
        var values: [String: Float] = [:]
        for p in params where isSoundParam(p.key) && p.value != p.defaultValue { values[p.key] = p.value }
        let preset = Preset(name: name, category: "User", isFactory: false, values: values)
        do {
            try PresetStore.save(preset)
            userPresets = PresetStore.loadUser()
            currentPresetName = name
        } catch {
            errorMessage = "Could not save preset: \(error.localizedDescription)"
        }
    }

    func delete(_ preset: Preset) {
        guard !preset.isFactory else { return }
        PresetStore.delete(preset)
        userPresets = PresetStore.loadUser()
    }

    // MARK: - Persistence

    private func scheduleStateSave() {
        saveWork?.cancel()
        let work = DispatchWorkItem { [weak self] in self?.saveState() }
        saveWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5, execute: work)
    }

    func saveState() {
        var dict: [String: Float] = [:]
        for p in params { dict[p.key] = p.value }
        defaults.set(dict, forKey: "paramState")
    }

    private func restoreState() {
        if let dict = defaults.dictionary(forKey: "paramState") as? [String: Float] {
            for p in params { if let v = dict[p.key] { p.set(v) } }
        } else if let first = FactoryPresets.all.first {
            apply(first)
        }
        currentPresetName = defaults.string(forKey: "presetName") ?? currentPresetName
    }

    private func saveAudioConfig() {
        defaults.set(device(inputDeviceID)?.uid ?? "", forKey: "inputUID")
        defaults.set(device(outputDeviceID)?.uid ?? "", forKey: "outputUID")
        defaults.set(sampleRate, forKey: "sampleRate")
        defaults.set(bufferFrames, forKey: "bufferFrames")
    }

    private func restoreAudioConfig() {
        let inUID = defaults.string(forKey: "inputUID") ?? ""
        let outUID = defaults.string(forKey: "outputUID") ?? ""
        inputDeviceID = devices.first { $0.uid == inUID && $0.inputChannels > 0 }?.id
            ?? devices.first { $0.isDefaultInput }?.id ?? 0
        outputDeviceID = devices.first { $0.uid == outUID && $0.outputChannels > 0 }?.id
            ?? devices.first { $0.isDefaultOutput }?.id ?? 0
        let sr = defaults.double(forKey: "sampleRate")
        sampleRate = sr > 0 ? sr : 48000
        let bf = defaults.integer(forKey: "bufferFrames")
        bufferFrames = bf > 0 ? bf : 64
    }
}

/// Meters & tracker state, polled at display rate. Kept separate from the
/// controller so the knob panels don't re-render 30× per second.
final class StatusModel: ObservableObject {
    @Published private(set) var status = GSXStatus()
    @Published private(set) var trackers: [GSXTrackerStatus] = Array(repeating: GSXTrackerStatus(), count: 6)
    private var timer: Timer?

    func startPolling() {
        guard timer == nil else { return }
        let t = Timer(timeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in self?.poll() }
        RunLoop.main.add(t, forMode: .common)
        timer = t
    }

    private func poll() {
        var s = GSXStatus()
        gsx_get_status(&s)
        var tr = trackers
        for i in 0..<6 {
            var t = GSXTrackerStatus()
            gsx_get_tracker(Int32(i), &t)
            tr[i] = t
        }
        status = s
        trackers = tr
    }
}

enum NoteName {
    static let names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"]
    static func of(midi: Float) -> (name: String, octave: Int, cents: Float) {
        let n = Int(midi.rounded())
        let cents = (midi - Float(n)) * 100
        let idx = ((n % 12) + 12) % 12
        return (names[idx], n / 12 - 1, cents)
    }
}
