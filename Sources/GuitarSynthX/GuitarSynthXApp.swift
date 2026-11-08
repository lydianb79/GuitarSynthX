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
            CommandMenu("Audio") {
                Button(synth.running ? "Stop Audio" : "Start Audio") { synth.toggleRunning() }
                    .keyboardShortcut("r", modifiers: [.command])
                Button("Panic (All Notes Off)") { synth.panic() }
                    .keyboardShortcut(".", modifiers: [.command])
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
