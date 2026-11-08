// DSPUtil.hpp — small, allocation-free DSP building blocks.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

#if defined(__aarch64__)
  #include <arm_neon.h>
#elif defined(__x86_64__) || defined(__i386__)
  #include <xmmintrin.h>
  #include <pmmintrin.h>
#endif

namespace gsx {

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float linToDb(float l)  { return 20.0f * std::log10(std::max(l, 1e-9f)); }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float midiToHz(float m) { return 440.0f * std::exp2((m - 69.0f) * (1.0f / 12.0f)); }
inline float hzToMidi(float f) { return 69.0f + 12.0f * std::log2(std::max(f, 1e-3f) / 440.0f); }

/// Rational tanh approximation, accurate to ~1e-3 and monotonic, clamps at |x|>=4.97.
inline float fastTanh(float x) {
    x = clampf(x, -4.97f, 4.97f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/// Coefficient for a one-pole smoother reaching ~63% in `ms` milliseconds.
inline float onePoleCoef(float ms, float fs) {
    if (ms <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-1.0f / (ms * 0.001f * fs));
}

/// RAII flush-to-zero / denormals-are-zero for the audio thread.
class ScopedNoDenormals {
public:
    ScopedNoDenormals() {
#if defined(__aarch64__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        old_ = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#elif defined(__x86_64__) || defined(__i386__)
        old_ = _mm_getcsr();
        _mm_setcsr(static_cast<unsigned>(old_) | 0x8040);
#endif
    }
    ~ScopedNoDenormals() {
#if defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(old_));
#elif defined(__x86_64__) || defined(__i386__)
        _mm_setcsr(static_cast<unsigned>(old_));
#endif
    }
private:
    uint64_t old_ = 0;
};

/// Direct-form-I-transposed biquad (RBJ cookbook designs).
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void setLowpass(float fc, float q, float fs) {
        const float w = kTwoPi * clampf(fc, 10.0f, 0.49f * fs) / fs;
        const float c = std::cos(w), s = std::sin(w), alpha = s / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0;
        a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
    }
    void setHighpass(float fc, float q, float fs) {
        const float w = kTwoPi * clampf(fc, 10.0f, 0.49f * fs) / fs;
        const float c = std::cos(w), s = std::sin(w), alpha = s / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0;
        a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
    }
    inline float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/// Simple one-pole low-pass (also used for damping in feedback loops).
struct OnePoleLP {
    float a = 1.0f, z = 0.0f;
    void setCutoff(float fc, float fs) { a = 1.0f - std::exp(-kTwoPi * clampf(fc, 1.0f, 0.49f * fs) / fs); }
    inline float process(float x) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
};

/// Fractional delay line with power-of-two buffer. Memory is allocated in
/// prepare() only (never on the audio thread).
class DelayLine {
public:
    void prepare(int maxSamples) {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf_.assign(size, 0.0f);
        mask_ = size - 1;
        w_ = 0;
    }
    void reset() { std::fill(buf_.begin(), buf_.end(), 0.0f); }
    inline void push(float x) { buf_[w_] = x; w_ = (w_ + 1) & mask_; }
    /// Read `d` samples behind the most recently pushed sample (d >= 1).
    inline float read(float d) const {
        const float fi = std::floor(d);
        const int i = static_cast<int>(fi);
        const float frac = d - fi;
        const float a = buf_[(w_ - i) & mask_];
        const float b = buf_[(w_ - i - 1) & mask_];
        return a + (b - a) * frac;
    }
    inline float readInt(int d) const { return buf_[(w_ - d) & mask_]; }
    int capacity() const { return mask_ - 3; }
private:
    std::vector<float> buf_;
    int mask_ = 0;
    int w_ = 0;
};

/// xorshift32 white noise in [-1, 1).
struct Noise {
    uint32_t s = 0x12345678u;
    inline float next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return static_cast<float>(static_cast<int32_t>(s)) * (1.0f / 2147483648.0f);
    }
};

} // namespace gsx
