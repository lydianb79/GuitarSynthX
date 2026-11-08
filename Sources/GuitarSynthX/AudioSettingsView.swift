import SwiftUI

struct AudioSettingsView: View {
    @EnvironmentObject var synth: SynthController
    @Environment(\.dismiss) private var dismiss

    @State private var inputID: UInt32 = 0
    @State private var outputID: UInt32 = 0
    @State private var rate: Double = 48000
    @State private var buffer: Int = 64

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("Audio Settings").font(.title2.bold())
                Spacer()
                Button { synth.refreshDevices() } label: { Label("Refresh", systemImage: "arrow.clockwise") }
            }

            Form {
                Picker("Input device", selection: $inputID) {
                    Text("None (output only)").tag(UInt32(0))
                    ForEach(synth.inputDevices) { d in
                        Text("\(d.name)  (\(d.inputChannels) in)").tag(d.id)
                    }
                }
                Picker("Output device", selection: $outputID) {
                    ForEach(synth.outputDevices) { d in
                        Text("\(d.name)  (\(d.outputChannels) out)").tag(d.id)
                    }
                }
                Picker("Sample rate", selection: $rate) {
                    ForEach(rates, id: \.self) { r in
                        Text(String(format: "%.1f kHz", r / 1000)).tag(r)
                    }
                }
                Picker("Buffer size", selection: $buffer) {
                    ForEach(buffers, id: \.self) { b in
                        Text(String(format: "%ld samples  (%.2f ms)", b, Double(b) / rate * 1000)).tag(b)
                    }
                }
            }
            .formStyle(.grouped)

            VStack(alignment: .leading, spacing: 6) {
                Label(sameDevice ? "Full-duplex on one device — lowest possible latency."
                                 : "Two separate devices are bridged with a small safety buffer. For the lowest latency use one interface for input and output, or create an Aggregate Device in Audio MIDI Setup.",
                      systemImage: sameDevice ? "bolt.fill" : "info.circle")
                    .font(.callout)
                    .foregroundColor(sameDevice ? Theme.accent : Theme.text2)
                Text(String(format: "Buffer contribution to round trip: ≈ %.1f ms (2 × buffer). Use 32–64 samples on a modern Mac with a USB/Thunderbolt interface; raise it if XRUNS climb.",
                            2.0 * Double(buffer) / rate * 1000))
                    .font(.caption)
                    .foregroundColor(Theme.text2)
                Text("Hexaphonic pickups (e.g. GK-style, 6 separate channels) give true polyphonic tracking: set Input Mode to “Hex” on the Input & Tracking tab.")
                    .font(.caption)
                    .foregroundColor(Theme.text2)
            }

            HStack {
                Spacer()
                Button("Cancel") { dismiss() }.keyboardShortcut(.cancelAction)
                Button("Apply") {
                    synth.inputDeviceID = inputID
                    synth.outputDeviceID = outputID
                    synth.sampleRate = rate
                    synth.bufferFrames = buffer
                    synth.start()
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(outputID == 0)
            }
        }
        .padding(20)
        .frame(width: 560)
        .onAppear {
            inputID = synth.inputDeviceID
            outputID = synth.outputDeviceID
            rate = synth.sampleRate
            buffer = synth.bufferFrames
            normalise()
        }
        .onChange(of: outputID) { _ in normalise() }
        .onChange(of: inputID) { _ in normalise() }
    }

    private var sameDevice: Bool { inputID != 0 && inputID == outputID }

    private var rates: [Double] {
        let out = synth.supportedSampleRates(for: outputID)
        guard inputID != 0, inputID != outputID else { return out }
        let inp = Set(synth.supportedSampleRates(for: inputID))
        let common = out.filter { inp.contains($0) }
        return common.isEmpty ? out : common
    }

    private var buffers: [Int] {
        let b = synth.supportedBufferSizes(for: outputID)
        return b.isEmpty ? [32, 64, 128, 256, 512] : b
    }

    /// Keep the selected rate/buffer valid for the chosen devices.
    private func normalise() {
        if !rates.contains(rate) { rate = rates.contains(48000) ? 48000 : (rates.first ?? 48000) }
        if !buffers.contains(buffer) { buffer = buffers.min(by: { abs($0 - 64) < abs($1 - 64) }) ?? 64 }
    }
}
