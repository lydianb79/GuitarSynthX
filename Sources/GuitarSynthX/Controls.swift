import SwiftUI
import AppKit

enum Theme {
    static let bg = Color(red: 0.055, green: 0.06, blue: 0.08)
    static let panel = Color(red: 0.085, green: 0.09, blue: 0.12)
    static let card = Color(red: 0.11, green: 0.115, blue: 0.15)
    static let stroke = Color.white.opacity(0.07)
    static let accent = Color(red: 0.22, green: 0.86, blue: 0.82)
    static let accent2 = Color(red: 0.62, green: 0.42, blue: 1.0)
    static let warn = Color(red: 1.0, green: 0.62, blue: 0.25)
    static let text2 = Color.white.opacity(0.55)
}

/// Rotary knob: vertical drag (⇧ for fine), double-click to reset.
struct Knob: View {
    @ObservedObject var param: Param
    var size: CGFloat = 44
    var tint: Color = Theme.accent
    @State private var dragStart: Float?
    @State private var hovering = false

    var body: some View {
        let n = CGFloat(param.normalized)
        VStack(spacing: 3) {
            ZStack {
                Circle()
                    .fill(RadialGradient(colors: [Color(white: 0.22), Color(white: 0.10)],
                                         center: .init(x: 0.4, y: 0.3), startRadius: 1, endRadius: size * 0.7))
                    .frame(width: size * 0.74, height: size * 0.74)
                    .shadow(color: .black.opacity(0.6), radius: 3, y: 2)
                Circle()
                    .trim(from: 0, to: 0.75)
                    .stroke(Color.white.opacity(0.09), style: StrokeStyle(lineWidth: 3.5, lineCap: .round))
                    .rotationEffect(.degrees(135))
                    .frame(width: size, height: size)
                Circle()
                    .trim(from: 0, to: 0.75 * n)
                    .stroke(tint, style: StrokeStyle(lineWidth: 3.5, lineCap: .round))
                    .rotationEffect(.degrees(135))
                    .frame(width: size, height: size)
                    .shadow(color: tint.opacity(hovering ? 0.8 : 0.45), radius: 4)
                Capsule()
                    .fill(Color.white.opacity(0.9))
                    .frame(width: 2.5, height: size * 0.2)
                    .offset(y: -size * 0.2)
                    .rotationEffect(.degrees(-135 + 270 * Double(n)))
            }
            .frame(width: size + 6, height: size + 6)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 1)
                    .onChanged { g in
                        if dragStart == nil { dragStart = param.normalized }
                        let fine = NSEvent.modifierFlags.contains(.shift)
                        let delta = Float(-g.translation.height / (fine ? 900 : 180))
                        param.setNormalized((dragStart ?? 0) + delta)
                    }
                    .onEnded { _ in dragStart = nil }
            )
            .onTapGesture(count: 2) { param.reset() }
            .onHover { hovering = $0 }

            Text(param.name)
                .font(.system(size: 10, weight: .medium))
                .foregroundColor(Theme.text2)
                .lineLimit(1)
                .minimumScaleFactor(0.8)
            Text(param.displayValue)
                .font(.system(size: 10, weight: .regular, design: .monospaced))
                .foregroundColor(.white.opacity(0.85))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
        }
        .frame(width: 74)
        .help("\(param.name): drag to change, ⇧-drag for fine, double-click to reset")
    }
}

/// Chooses the right control for a parameter type.
struct ParamControl: View {
    @ObservedObject var param: Param
    var tint: Color = Theme.accent

    var body: some View {
        switch param.kind {
        case .choice:
            VStack(spacing: 4) {
                Picker("", selection: Binding(get: { Int(param.value.rounded()) }, set: { param.set(Float($0)) })) {
                    ForEach(Array(param.choices.enumerated()), id: \.offset) { i, label in
                        Text(label).tag(i)
                    }
                }
                .labelsHidden()
                .pickerStyle(.menu)
                .frame(width: 128)
                Text(param.name).font(.system(size: 10, weight: .medium)).foregroundColor(Theme.text2)
            }
            .frame(height: 76)
        case .toggle:
            VStack(spacing: 6) {
                LEDToggle(param: param, tint: tint)
                Text(param.name).font(.system(size: 10, weight: .medium)).foregroundColor(Theme.text2)
            }
            .frame(width: 74, height: 76)
        default:
            Knob(param: param, tint: tint)
        }
    }
}

struct LEDToggle: View {
    @ObservedObject var param: Param
    var tint: Color = Theme.accent
    var body: some View {
        let on = param.value > 0.5
        Button {
            param.set(on ? 0 : 1)
        } label: {
            Circle()
                .fill(on ? tint : Color(white: 0.2))
                .frame(width: 12, height: 12)
                .shadow(color: on ? tint : .clear, radius: 5)
                .overlay(Circle().stroke(Color.white.opacity(0.15), lineWidth: 1))
                .padding(6)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .help(on ? "Turn off" : "Turn on")
    }
}

/// Rounded panel with a title and optional bypass LED.
struct Card<Content: View>: View {
    let title: String
    var toggle: Param? = nil
    var tint: Color = Theme.accent
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                if let t = toggle { LEDToggle(param: t, tint: tint).padding(-6) }
                Text(title.uppercased())
                    .font(.system(size: 10, weight: .bold))
                    .tracking(1.2)
                    .foregroundColor(.white.opacity(0.7))
                Spacer()
            }
            content
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 10).fill(Theme.card))
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(Theme.stroke))
    }
}

/// Vertical peak meter (linear amplitude in, dB scale out).
struct PeakMeter: View {
    var level: Float
    var body: some View {
        GeometryReader { geo in
            let db = 20 * log10(max(level, 1e-5))
            let frac = CGFloat(min(max((db + 60) / 60, 0), 1))
            ZStack(alignment: .bottom) {
                RoundedRectangle(cornerRadius: 2).fill(Color.white.opacity(0.06))
                RoundedRectangle(cornerRadius: 2)
                    .fill(LinearGradient(colors: [Theme.accent, .yellow, .red], startPoint: .bottom, endPoint: .top))
                    .frame(height: geo.size.height * frac)
            }
        }
        .frame(width: 8)
    }
}
