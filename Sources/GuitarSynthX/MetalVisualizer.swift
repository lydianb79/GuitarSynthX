import SwiftUI
import MetalKit
import simd
import GSXCore

/// GPU-rendered oscilloscope + log spectrum of the synth output (Metal).
struct MetalVisualizer: NSViewRepresentable {
    final class Coordinator { var renderer: VisualRenderer? }
    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.03, green: 0.035, blue: 0.05, alpha: 1)
        view.preferredFramesPerSecond = 60
        view.enableSetNeedsDisplay = false
        view.isPaused = false
        view.framebufferOnly = true
        view.layer?.isOpaque = true
        if let renderer = VisualRenderer(view: view) {
            context.coordinator.renderer = renderer   // MTKView.delegate is weak
            view.delegate = renderer
        }
        return view
    }

    func updateNSView(_ nsView: MTKView, context: Context) {}
}

// Layouts shared with the shader source below (must match byte-for-byte).
struct VisUniforms {
    var viewSize: SIMD2<Float>
    var time: Float
    var level: Float
    var colorA: SIMD4<Float>
    var colorB: SIMD4<Float>
}

struct LineVertex {
    var pos: SIMD2<Float>
    var side: Float
    var pad: Float = 0
}

struct BarUniforms {
    var count: Float
    var bottom: Float
    var top: Float
    var gap: Float
    var colorLow: SIMD4<Float>
    var colorHigh: SIMD4<Float>
}

final class VisualRenderer: NSObject, MTKViewDelegate {
    private let device: MTLDevice
    private let queue: MTLCommandQueue
    private let bgPipeline: MTLRenderPipelineState
    private let linePipeline: MTLRenderPipelineState
    private let barPipeline: MTLRenderPipelineState

    private static let scopeCount = 1024
    private static let bins = 96
    private var scope = [Float](repeating: 0, count: VisualRenderer.scopeCount)
    private var spectrum = [Float](repeating: 0, count: VisualRenderer.bins)
    private var smoothSpectrum = [Float](repeating: 0, count: VisualRenderer.bins)
    private var lineVerts = [LineVertex](repeating: LineVertex(pos: .zero, side: 0), count: VisualRenderer.scopeCount * 2)

    private let inFlight = DispatchSemaphore(value: 3)
    private var vertexBuffers: [MTLBuffer] = []
    private var frameIndex = 0
    private let start = CACurrentMediaTime()
    private var level: Float = 0

    init?(view: MTKView) {
        guard let device = view.device, let queue = device.makeCommandQueue() else { return nil }
        self.device = device
        self.queue = queue
        do {
            let library = try device.makeLibrary(source: VisualRenderer.shaderSource, options: nil)
            func pipeline(_ v: String, _ f: String, additive: Bool?) throws -> MTLRenderPipelineState {
                let d = MTLRenderPipelineDescriptor()
                d.vertexFunction = library.makeFunction(name: v)
                d.fragmentFunction = library.makeFunction(name: f)
                let ca = d.colorAttachments[0]!
                ca.pixelFormat = view.colorPixelFormat
                if let additive = additive {
                    ca.isBlendingEnabled = true
                    ca.rgbBlendOperation = .add
                    ca.alphaBlendOperation = .add
                    ca.sourceRGBBlendFactor = .one
                    ca.sourceAlphaBlendFactor = .one
                    ca.destinationRGBBlendFactor = additive ? .one : .oneMinusSourceAlpha
                    ca.destinationAlphaBlendFactor = additive ? .one : .oneMinusSourceAlpha
                }
                return try device.makeRenderPipelineState(descriptor: d)
            }
            bgPipeline = try pipeline("bg_vertex", "bg_fragment", additive: nil)
            linePipeline = try pipeline("line_vertex", "line_fragment", additive: true)
            barPipeline = try pipeline("bar_vertex", "bar_fragment", additive: false)
        } catch {
            NSLog("GuitarSynthX: Metal pipeline error: \(error)")
            return nil
        }
        let length = MemoryLayout<LineVertex>.stride * VisualRenderer.scopeCount * 2 * 2
        for _ in 0..<3 {
            guard let b = device.makeBuffer(length: length, options: .storageModeShared) else { return nil }
            vertexBuffers.append(b)
        }
        super.init()
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func draw(in view: MTKView) {
        let size = view.drawableSize
        guard size.width > 1, size.height > 1 else { return }
        inFlight.wait()

        // ---- pull audio visuals from the engine (UI thread, lock-free read)
        gsx_read_visual(&scope, Int32(scope.count), &spectrum, Int32(spectrum.count))
        var peak: Float = 0
        for v in scope { peak = max(peak, abs(v)) }
        level += (peak - level) * (peak > level ? 0.5 : 0.08)
        for i in 0..<spectrum.count {
            let s = spectrum[i], o = smoothSpectrum[i]
            smoothSpectrum[i] = s > o ? o + (s - o) * 0.6 : o + (s - o) * 0.12
        }

        guard let pass = view.currentRenderPassDescriptor,
              let drawable = view.currentDrawable,
              let cmd = queue.makeCommandBuffer(),
              let enc = cmd.makeRenderCommandEncoder(descriptor: pass) else {
            inFlight.signal()
            return
        }

        let w = Float(size.width), h = Float(size.height)
        let scale = Float(view.window?.backingScaleFactor ?? 2)
        var uniforms = VisUniforms(viewSize: SIMD2(w, h),
                                   time: Float(CACurrentMediaTime() - start),
                                   level: min(level * 2.5, 1),
                                   colorA: SIMD4(0.22, 0.86, 0.82, 1),
                                   colorB: SIMD4(0.62, 0.42, 1.0, 1))

        // ---- background
        enc.setRenderPipelineState(bgPipeline)
        enc.setFragmentBytes(&uniforms, length: MemoryLayout<VisUniforms>.stride, index: 0)
        enc.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)

        // ---- spectrum bars (bottom 38%)
        var bars = BarUniforms(count: Float(VisualRenderer.bins), bottom: -0.97, top: -0.25, gap: 0.0025,
                               colorLow: SIMD4(0.62, 0.42, 1.0, 0.55), colorHigh: SIMD4(0.22, 0.86, 0.82, 0.95))
        enc.setRenderPipelineState(barPipeline)
        smoothSpectrum.withUnsafeBytes { raw in
            enc.setVertexBytes(raw.baseAddress!, length: raw.count, index: 0)
        }
        enc.setVertexBytes(&bars, length: MemoryLayout<BarUniforms>.stride, index: 1)
        enc.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4, instanceCount: VisualRenderer.bins)

        // ---- oscilloscope (top area), drawn twice: wide glow + sharp core
        let buffer = vertexBuffers[frameIndex % 3]
        frameIndex += 1
        let n = VisualRenderer.scopeCount
        let centerY: Float = 0.36, amp: Float = 0.55
        let stride = MemoryLayout<LineVertex>.stride
        enc.setRenderPipelineState(linePipeline)
        for (layer, (thickPx, color)) in [(14.0 * scale, SIMD4<Float>(0.22, 0.86, 0.82, 0.16)),
                                          (2.2 * scale, SIMD4<Float>(0.75, 1.0, 0.98, 0.95))].enumerated() {
            buildStrip(count: n, centerY: centerY, amp: amp, thicknessPx: thickPx, w: w, h: h)
            let offset = layer * n * 2 * stride
            lineVerts.withUnsafeBytes { raw in
                (buffer.contents() + offset).copyMemory(from: raw.baseAddress!, byteCount: raw.count)
            }
            var c = color
            enc.setVertexBuffer(buffer, offset: offset, index: 0)
            enc.setVertexBytes(&c, length: MemoryLayout<SIMD4<Float>>.stride, index: 1)
            enc.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: n * 2)
        }

        enc.endEncoding()
        cmd.present(drawable)
        let sem = inFlight
        cmd.addCompletedHandler { _ in sem.signal() }
        cmd.commit()
    }

    /// Expands the scope polyline into a screen-space thick triangle strip.
    private func buildStrip(count n: Int, centerY: Float, amp: Float, thicknessPx: Float, w: Float, h: Float) {
        let half = thicknessPx * 0.5
        func point(_ i: Int) -> SIMD2<Float> {
            let x = -0.98 + 1.96 * Float(i) / Float(n - 1)
            let y = centerY + amp * max(-1, min(1, scope[i]))
            return SIMD2(x, y)
        }
        for i in 0..<n {
            let p = point(i)
            let a = point(max(i - 1, 0)), b = point(min(i + 1, n - 1))
            // tangent in pixel space
            var t = SIMD2<Float>((b.x - a.x) * w, (b.y - a.y) * h)
            let len = max(simd_length(t), 1e-4)
            t /= len
            let normalPx = SIMD2<Float>(-t.y, t.x) * half
            let off = SIMD2<Float>(normalPx.x * 2 / w, normalPx.y * 2 / h)
            lineVerts[2 * i] = LineVertex(pos: p + off, side: 1)
            lineVerts[2 * i + 1] = LineVertex(pos: p - off, side: -1)
        }
    }

    static let shaderSource = """
    #include <metal_stdlib>
    using namespace metal;

    struct Uniforms {
        float2 viewSize;
        float  time;
        float  level;
        float4 colorA;
        float4 colorB;
    };

    struct LineVertex {
        float2 pos;
        float  side;
        float  pad;
    };

    struct BarUniforms {
        float  count;
        float  bottom;
        float  top;
        float  gap;
        float4 colorLow;
        float4 colorHigh;
    };

    struct VOut {
        float4 position [[position]];
        float4 color;
        float2 uv;
    };

    vertex VOut bg_vertex(uint vid [[vertex_id]]) {
        float2 p[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };
        VOut o;
        o.position = float4(p[vid], 0, 1);
        o.uv = p[vid] * 0.5 + 0.5;
        o.color = float4(1);
        return o;
    }

    fragment float4 bg_fragment(VOut in [[stage_in]], constant Uniforms& u [[buffer(0)]]) {
        float2 uv = in.uv;
        float2 px = uv * u.viewSize;
        float3 col = mix(float3(0.030, 0.034, 0.050), float3(0.055, 0.045, 0.090), uv.y);
        // grid (distance to nearest line in pixels)
        float2 g = abs(fract(px / 48.0 - 0.5) - 0.5) * 48.0;
        float grid = 1.0 - smoothstep(0.0, 1.2, min(g.x, g.y));
        col += grid * 0.03;
        // scope baseline
        float base = 1.0 - smoothstep(0.0, 1.5, abs(uv.y - 0.68) * u.viewSize.y);
        col += base * 0.06;
        // level-reactive glow
        float2 d = (uv - float2(0.5, 0.68)) * float2(1.0, 2.2);
        float glow = exp(-dot(d, d) * 3.0);
        float pulse = 0.85 + 0.15 * sin(u.time * 2.0);
        col += mix(u.colorB.rgb, u.colorA.rgb, u.level) * glow * u.level * 0.35 * pulse;
        // vignette
        float2 v = uv - 0.5;
        col *= 1.0 - 0.9 * dot(v, v);
        return float4(col, 1.0);
    }

    vertex VOut line_vertex(uint vid [[vertex_id]],
                            const device LineVertex* verts [[buffer(0)]],
                            constant float4& color [[buffer(1)]]) {
        LineVertex v = verts[vid];
        VOut o;
        o.position = float4(v.pos, 0, 1);
        o.color = color;
        o.uv = float2(0, v.side);
        return o;
    }

    fragment float4 line_fragment(VOut in [[stage_in]]) {
        float a = 1.0 - smoothstep(0.35, 1.0, abs(in.uv.y));
        float k = in.color.a * a;
        return float4(in.color.rgb * k, k);   // premultiplied, additive blend
    }

    vertex VOut bar_vertex(uint vid [[vertex_id]], uint iid [[instance_id]],
                           constant float* heights [[buffer(0)]],
                           constant BarUniforms& b [[buffer(1)]]) {
        float h = clamp(heights[iid], 0.0, 1.0);
        float x0 = -0.98 + 1.96 * (float(iid) / b.count) + b.gap;
        float x1 = -0.98 + 1.96 * (float(iid + 1) / b.count) - b.gap;
        float top = b.bottom + max(h, 0.004) * (b.top - b.bottom);
        float2 corner = float2((vid & 1) ? x1 : x0, (vid & 2) ? top : b.bottom);
        VOut o;
        o.position = float4(corner, 0, 1);
        float t = (vid & 2) ? h : 0.0;
        o.color = mix(b.colorLow, b.colorHigh, t);
        o.uv = float2(float(vid & 1), (vid & 2) ? 1.0 : 0.0);
        return o;
    }

    fragment float4 bar_fragment(VOut in [[stage_in]]) {
        float edge = smoothstep(0.0, 0.08, in.uv.x) * (1.0 - smoothstep(0.92, 1.0, in.uv.x));
        float cap = 1.0 + 0.6 * smoothstep(0.9, 1.0, in.uv.y);
        float a = in.color.a * (0.65 + 0.35 * edge);
        return float4(in.color.rgb * cap * a, a);   // premultiplied
    }
    """
}
