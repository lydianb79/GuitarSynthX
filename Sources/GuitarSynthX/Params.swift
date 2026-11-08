import Foundation
import GSXCore

/// Reads a fixed-size C char array (imported as a tuple) into a Swift String.
func cString<T>(_ value: T) -> String {
    withUnsafeBytes(of: value) { raw in
        String(decoding: raw.prefix(while: { $0 != 0 }), as: UTF8.self)
    }
}

enum ParamKind: Int32 { case continuous = 0, integer = 1, choice = 2, toggle = 3 }
enum ParamScale: Int32 { case linear = 0, log = 1, square = 2 }

/// One engine parameter. Generated from the C++ metadata table, so the UI never
/// drifts out of sync with the engine.
final class Param: ObservableObject, Identifiable {
    let id: Int32
    let key: String
    let name: String
    let group: String
    let unit: String
    let choices: [String]
    let minValue: Float
    let maxValue: Float
    let defaultValue: Float
    let kind: ParamKind
    let scale: ParamScale
    var onChange: (() -> Void)?

    @Published var value: Float {
        didSet {
            guard value != oldValue else { return }
            gsx_set_param(id, value)
            onChange?()
        }
    }

    init(id: Int32) {
        var info = GSXParamInfo()
        gsx_param_info(id, &info)
        self.id = id
        key = cString(info.key)
        name = cString(info.name)
        group = cString(info.group)
        unit = cString(info.unit)
        let c = cString(info.choices)
        choices = c.isEmpty ? [] : c.components(separatedBy: "|")
        minValue = info.minValue
        maxValue = info.maxValue
        defaultValue = info.defaultValue
        kind = ParamKind(rawValue: info.type) ?? .continuous
        scale = ParamScale(rawValue: info.scale) ?? .linear
        value = gsx_get_param(id)
    }

    func set(_ v: Float) {
        var x = min(max(v, minValue), maxValue)
        if kind != .continuous { x = x.rounded() }
        value = x
    }

    func reset() { set(defaultValue) }

    // MARK: normalised 0...1 mapping (for knobs)
    var normalized: Float {
        let v = min(max(value, minValue), maxValue)
        switch scale {
        case .linear: return (v - minValue) / (maxValue - minValue)
        case .log:    return log(v / minValue) / log(maxValue / minValue)
        case .square: return sqrt((v - minValue) / (maxValue - minValue))
        }
    }

    func setNormalized(_ n: Float) {
        let t = min(max(n, 0), 1)
        switch scale {
        case .linear: set(minValue + t * (maxValue - minValue))
        case .log:    set(minValue * pow(maxValue / minValue, t))
        case .square: set(minValue + t * t * (maxValue - minValue))
        }
    }

    var displayValue: String {
        switch kind {
        case .choice:
            let i = Int(value.rounded())
            return choices.indices.contains(i) ? choices[i] : "\(i)"
        case .toggle:
            return value > 0.5 ? "On" : "Off"
        case .integer:
            let i = Int(value.rounded())
            if unit == "st" || unit == "oct" { return (i > 0 ? "+" : "") + "\(i) \(unit)" }
            return "\(i)" + (unit.isEmpty ? "" : " \(unit)")
        case .continuous:
            break
        }
        switch unit {
        case "Hz":
            return value >= 1000 ? String(format: "%.2f kHz", value / 1000) : String(format: value < 10 ? "%.2f Hz" : "%.0f Hz", value)
        case "ms":
            return value >= 1000 ? String(format: "%.2f s", value / 1000) : String(format: value < 10 ? "%.1f ms" : "%.0f ms", value)
        case "dB":
            return value <= minValue + 0.05 && minValue <= -59 ? "Off" : String(format: "%+.1f dB", value)
        case "s":  return String(format: "%.1f s", value)
        case "BPM": return String(format: "%.0f BPM", value)
        case "ct": return String(format: "%+.0f ct", value)
        case "oct": return String(format: "%+.1f oct", value)
        default:   return String(format: "%.0f%%", (value - minValue) / (maxValue - minValue) * 100)
        }
    }
}

/// Parameters that define the *sound* (stored in presets). Input routing,
/// tracking calibration and master levels belong to the player's rig and are
/// deliberately left untouched when a preset is loaded.
func isSoundParam(_ key: String) -> Bool {
    if key.hasPrefix("in.") || key.hasPrefix("out.") || key.hasPrefix("mst.") { return false }
    let rigKeys: Set<String> = ["trk.lowest", "trk.highest", "trk.threshold", "trk.stability", "trk.hop", "trk.retrigger"]
    return !rigKeys.contains(key)
}
