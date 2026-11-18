import Foundation
import AppKit
import GSXCore

/// Records what the pitch tracker hears (the selected input channel after
/// Input Gain; all six channels in hex mode) to a 32-bit float WAV file, with
/// a text file of the settings next to it. Used to tune note detection on a
/// real guitar and interface.
extension SynthController {
    static let maxRecordSeconds = 300.0

    var recordingsFolder: URL {
        let music = FileManager.default.urls(for: .musicDirectory, in: .userDomainMask).first
            ?? FileManager.default.homeDirectoryForCurrentUser
        return music.appendingPathComponent("GuitarSynthX Recordings", isDirectory: true)
    }

    func toggleRecording() { recording ? stopRecording() : startRecording() }

    func startRecording() {
        guard running, !recording else { return }
        gsx_record_start(Self.maxRecordSeconds)
        recordingStarted = Date()
        recording = true
    }

    func stopRecording() {
        guard recording else { return }
        recording = false
        let frames = Int(gsx_record_stop())
        guard frames > 0 else { return }
        var channels: Int32 = 1
        var rate: Double = 48000
        var samples = [Float](repeating: 0, count: frames * 6)
        let n = Int(gsx_record_copy(&samples, Int32(samples.count), &channels, &rate))
        samples.removeSubrange(n...)
        do {
            try FileManager.default.createDirectory(at: recordingsFolder, withIntermediateDirectories: true)
            let stamp = Self.fileStamp.string(from: recordingStarted)
            let wav = recordingsFolder.appendingPathComponent("Input \(stamp).wav")
            try Self.wavData(samples, channels: Int(channels), sampleRate: rate).write(to: wav)
            try settingsText(channels: Int(channels), sampleRate: rate, frames: n / Int(channels))
                .write(to: wav.deletingPathExtension().appendingPathExtension("txt"), atomically: true, encoding: .utf8)
            NSWorkspace.shared.activateFileViewerSelecting([wav])
        } catch {
            errorMessage = "Could not save the recording: \(error.localizedDescription)"
        }
    }

    func revealRecordings() {
        try? FileManager.default.createDirectory(at: recordingsFolder, withIntermediateDirectories: true)
        NSWorkspace.shared.open(recordingsFolder)
    }

    private static let fileStamp: DateFormatter = {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "yyyy-MM-dd HH.mm.ss"
        return f
    }()

    private func settingsText(channels: Int, sampleRate: Double, frames: Int) -> String {
        let info = Bundle.main.infoDictionary
        var lines = [
            "GuitarSynthX input recording",
            "app: \(info?["CFBundleShortVersionString"] as? String ?? "dev") (\(info?["CFBundleVersion"] as? String ?? "-"))",
            "date: \(ISO8601DateFormatter().string(from: recordingStarted))",
            "input: \(device(inputDeviceID)?.name ?? "none")",
            "output: \(device(outputDeviceID)?.name ?? "none")",
            String(format: "sampleRate: %.0f", sampleRate),
            "buffer: \(bufferFrames)",
            "channels: \(channels)",
            String(format: "seconds: %.2f", Double(frames) / sampleRate),
            "preset: \(currentPresetName)",
            "",
            "[params]",
        ]
        for p in params { lines.append("\(p.key) = \(p.value)") }
        return lines.joined(separator: "\n") + "\n"
    }

    /// 32-bit float WAV (WAVE_FORMAT_IEEE_FLOAT), interleaved.
    static func wavData(_ samples: [Float], channels: Int, sampleRate: Double) -> Data {
        var d = Data()
        func u32(_ v: UInt32) { withUnsafeBytes(of: v.littleEndian) { d.append(contentsOf: $0) } }
        func u16(_ v: UInt16) { withUnsafeBytes(of: v.littleEndian) { d.append(contentsOf: $0) } }
        let dataBytes = UInt32(samples.count * 4)
        d.append(contentsOf: Array("RIFF".utf8)); u32(36 + dataBytes)
        d.append(contentsOf: Array("WAVE".utf8))
        d.append(contentsOf: Array("fmt ".utf8)); u32(16)
        u16(3)                                   // IEEE float
        u16(UInt16(channels))
        u32(UInt32(sampleRate))
        u32(UInt32(sampleRate) * UInt32(channels) * 4)
        u16(UInt16(channels * 4))
        u16(32)
        d.append(contentsOf: Array("data".utf8)); u32(dataBytes)
        samples.withUnsafeBufferPointer { d.append(UnsafeBufferPointer(start: $0.baseAddress, count: $0.count)) }
        return d
    }
}
