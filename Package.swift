// swift-tools-version:5.9
// GuitarSynthX — native macOS guitar-to-synth with a C++ real-time core.
//
// Build & run:   swift run -c release GuitarSynthX
// App bundle:    ./Packaging/build_app.sh      (creates build/GuitarSynthX.app)
// Or open this folder in Xcode (File ▸ Open… ▸ Package.swift) and press Run.
import PackageDescription

let package = Package(
    name: "GuitarSynthX",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "GuitarSynthX", targets: ["GuitarSynthX"])
    ],
    targets: [
        // Real-time engine: Core Audio HAL I/O, pitch tracking, synth, effects.
        .target(
            name: "GSXCore",
            path: "Sources/GSXCore",
            publicHeadersPath: "include",
            cxxSettings: [
                .unsafeFlags(["-O3", "-fno-math-errno"], .when(configuration: .release)),
                .define("NDEBUG", .when(configuration: .release))
            ],
            linkerSettings: [
                .linkedFramework("CoreAudio"),
                .linkedFramework("AudioToolbox"),
                .linkedFramework("Accelerate"),
                .linkedFramework("CoreFoundation")
            ]
        ),
        // SwiftUI + Metal user interface.
        .executableTarget(
            name: "GuitarSynthX",
            dependencies: ["GSXCore"],
            path: "Sources/GuitarSynthX",
            swiftSettings: [
                .unsafeFlags(["-Ounchecked"], .when(configuration: .release))
            ],
            linkerSettings: [
                .linkedFramework("Metal"),
                .linkedFramework("MetalKit"),
                .linkedFramework("AVFoundation")
            ]
        )
    ],
    cxxLanguageStandard: .cxx17
)
