import SwiftUI
import AppKit
import GSXCore

@main
struct GuitarSynthXApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var synth = SynthController()

    var body: some Scene {
        WindowGroup("GuitarSynthX") {
            ContentView()
                .environmentObject(synth)
                .frame(minWidth: 1120, minHeight: 760)
                .onAppear {
                    appDelegate.synth = synth
                    synth.boot()
                }
        }
        .windowStyle(.hiddenTitleBar)
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandMenu("Presets") {
                Button("Next Preset") { synth.stepPreset(1) }
                    .keyboardShortcut("]", modifiers: [.command])
                Button("Previous Preset") { synth.stepPreset(-1) }
                    .keyboardShortcut("[", modifiers: [.command])
                Divider()
                Button("Init Sound") { synth.initPatch() }
            }
            CommandMenu("Audio") {
                Button(synth.running ? "Stop Audio" : "Start Audio") { synth.toggleRunning() }
                    .keyboardShortcut("r", modifiers: [.command])
                Button("Panic (All Notes Off)") { synth.panic() }
                    .keyboardShortcut(".", modifiers: [.command])
                Divider()
                Button(synth.recording ? "Stop Recording Input" : "Record Input…") { synth.toggleRecording() }
                    .keyboardShortcut("r", modifiers: [.command, .shift])
                    .disabled(!synth.running && !synth.recording)
                Button("Show Recordings in Finder") { synth.revealRecordings() }
            }
        }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    weak var synth: SynthController?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Launched as a bare executable (swift run) the process would otherwise
        // stay a background app without a Dock icon or key window.
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    func applicationWillTerminate(_ notification: Notification) {
        synth?.saveState()
        gsx_stop()
    }
}
