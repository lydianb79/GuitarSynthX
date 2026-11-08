// Effects.cpp
#include "Effects.hpp"

namespace gsx {

// ------------------------------------------------------------------ Drive
void Drive::process(float* L, float* R, int n) {
    const float dry = 1.0f - mix_;
    for (int i = 0; i < n; ++i) {
        const float l = lpL_.process(fastTanh(L[i] * pre_) * post_ * 1.4f);
        const float r = lpR_.process(fastTanh(R[i] * pre_) * post_ * 1.4f);
        L[i] = L[i] * dry + l * mix_;
        R[i] = R[i] * dry + r * mix_;
    }
}

// ------------------------------------------------------------------ Chorus
void Chorus::prepare(float fs) {
    fs_ = fs;
    dl_.prepare(static_cast<int>(0.05f * fs));
    dr_.prepare(static_cast<int>(0.05f * fs));
}

void Chorus::process(float* L, float* R, int n) {
    const float inc = rate_ / fs_;
    const float base = 0.012f * fs_;
    const float dep = depth_ * 0.0055f * fs_;
    const float norm = 1.0f / (1.0f + 0.5f * mix_);
    for (int i = 0; i < n; ++i) {
        phase_ += inc; if (phase_ >= 1.0f) phase_ -= 1.0f;
        const float ml = std::sin(kTwoPi * phase_);
        const float mr = std::sin(kTwoPi * phase_ + kPi * 0.5f);
        dl_.push(L[i]); dr_.push(R[i]);
        const float wl = dl_.read(base + dep * ml);
        const float wr = dr_.read(base + dep * mr);
        L[i] = (L[i] + mix_ * (wl * 0.8f - wr * 0.2f)) * norm;
        R[i] = (R[i] + mix_ * (wr * 0.8f - wl * 0.2f)) * norm;
    }
}

// ------------------------------------------------------------------ Phaser
void Phaser::reset() {
    for (int s = 0; s < kStages; ++s) zL_[s] = zR_[s] = 0;
    fbL_ = fbR_ = 0;
}

void Phaser::process(float* L, float* R, int n) {
    const float inc = rate_ / fs_;
    float aL = 0, aR = 0;
    for (int i = 0; i < n; ++i) {
        if ((i & 15) == 0) {
            const float lfoL = 0.5f + 0.5f * std::sin(kTwoPi * phase_);
            const float lfoR = 0.5f + 0.5f * std::sin(kTwoPi * phase_ + kPi * 0.5f);
            const float fl = 180.0f * std::exp2(lfoL * depth_ * 4.8f);
            const float fr = 180.0f * std::exp2(lfoR * depth_ * 4.8f);
            const float tl = std::tan(kPi * std::min(fl, 0.45f * fs_) / fs_);
            const float tr = std::tan(kPi * std::min(fr, 0.45f * fs_) / fs_);
            aL = (tl - 1.0f) / (tl + 1.0f);
            aR = (tr - 1.0f) / (tr + 1.0f);
        }
        phase_ += inc; if (phase_ >= 1.0f) phase_ -= 1.0f;

        float xl = L[i] + fb_ * fbL_;
        float xr = R[i] + fb_ * fbR_;
        for (int s = 0; s < kStages; ++s) {
            const float yl = aL * xl + zL_[s]; zL_[s] = xl - aL * yl; xl = yl;
            const float yr = aR * xr + zR_[s]; zR_[s] = xr - aR * yr; xr = yr;
        }
        fbL_ = fastTanh(xl); fbR_ = fastTanh(xr);
        L[i] = L[i] * (1.0f - mix_) + xl * mix_;
        R[i] = R[i] * (1.0f - mix_) + xr * mix_;
    }
}

// ------------------------------------------------------------------ Delay
static constexpr float kMaxDelaySec = 3.1f;

void StereoDelay::prepare(float fs) {
    fs_ = fs;
    dl_.prepare(static_cast<int>(kMaxDelaySec * fs) + 1024);
    dr_.prepare(static_cast<int>(kMaxDelaySec * fs) + 1024);
    smooth_ = onePoleCoef(60.0f, fs);
    duckAtk_ = onePoleCoef(5.0f, fs);
    duckRel_ = onePoleCoef(45.0f, fs);
    cur_ = target_ = 0.35f * fs;
    set(s_);
}

void StereoDelay::reset() {
    dl_.reset(); dr_.reset(); dampL_.reset(); dampR_.reset(); hpL_.reset(); hpR_.reset();
    duckEnv_ = 0; cur_ = target_;
}

void StereoDelay::set(const DelaySettings& s) {
    s_ = s;
    target_ = clampf(s.timeMs * 0.001f * fs_, 1.0f, kMaxDelaySec * fs_);
    // Tape and analog (BBD) delays get darker with every repeat.
    float damp = s.dampHz;
    if (s.mode == 1) damp = std::min(damp, 6500.0f);
    if (s.mode == 2) damp = std::min(damp, 3200.0f);
    dampL_.setCutoff(damp, fs_); dampR_.setCutoff(damp, fs_);
    hpL_.setCutoff(s.lowCutHz, fs_); hpR_.setCutoff(s.lowCutHz, fs_);
}

void StereoDelay::process(float* L, float* R, int n) {
    const float invFs = 1.0f / fs_;
    const float fb = s_.feedback;
    const bool sat = s_.mode != 0;
    for (int i = 0; i < n; ++i) {
        // ---- ducking: follow the dry level, pull the echoes back while playing
        const float lvl = std::max(std::fabs(L[i]), std::fabs(R[i]));
        duckEnv_ += (lvl > duckEnv_ ? duckAtk_ : duckRel_) * (lvl - duckEnv_);
        if ((duckCount_++ & 15) == 0) duckGain_ = s_.duck > 0.0f ? duckGainFor(duckEnv_, s_.duck) : 1.0f;
        const float wet = s_.mix * duckGain_;

        // ---- time modulation per character
        modPh_ += 0.55f * invFs; if (modPh_ >= 1.0f) modPh_ -= 1.0f;
        float modSamples;
        if (s_.mode == 1) {                       // Tape: wow + flutter + scrape noise
            wow_ += 6.8f * invFs; if (wow_ >= 1.0f) wow_ -= 1.0f;
            flutterNoise_ += 0.002f * (noise_.next() - flutterNoise_);
            const float depth = 0.25f + s_.mod;
            modSamples = depth * fs_ * (0.0022f * std::sin(kTwoPi * modPh_)
                                        + 0.00025f * std::sin(kTwoPi * wow_)
                                        + 0.004f * flutterNoise_);
        } else if (s_.mode == 2) {                // Analog: gentle clock wobble
            modSamples = (0.08f + s_.mod) * 0.0018f * fs_ * std::sin(kTwoPi * modPh_);
        } else {                                  // Digital: optional chorus-like mod
            modSamples = s_.mod * 0.0025f * fs_ * std::sin(kTwoPi * modPh_);
        }
        cur_ += smooth_ * (target_ - cur_);
        const float d = std::max(1.0f, cur_ + modSamples);
        const float dR = std::max(1.0f, cur_ - modSamples * 0.7f);   // decorrelate sides
        const float dLv = dl_.read(d);
        const float dRv = dr_.read(dR);

        // ---- filtered (and for Tape/Analog saturated) feedback
        float fl = dampL_.process(dLv); fl -= hpL_.process(fl);
        float fr = dampR_.process(dRv); fr -= hpR_.process(fr);
        if (sat) { fl = fastTanh(fl * 1.3f) * 0.77f; fr = fastTanh(fr * 1.3f) * 0.77f; }
        fl *= fb; fr *= fb;

        if (s_.pingPong) {
            dl_.push(0.5f * (L[i] + R[i]) + fr);
            dr_.push(fl);
        } else {
            dl_.push(L[i] + fl);
            dr_.push(R[i] + fr);
        }
        L[i] += wet * dLv;
        R[i] += wet * dRv;
    }
}

// ------------------------------------------------------------------ Pitch shifter
void PitchShifter::prepare(float fs, float windowMs) {
    window_ = windowMs * 0.001f * fs;
    buf_.prepare(static_cast<int>(window_) + 16);
    phase_ = 0;
}

float PitchShifter::process(float x) {
    buf_.push(x);
    phase_ -= (ratio - 1.0f) / window_;
    if (phase_ < 0.0f) phase_ += 1.0f;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    float p2 = phase_ + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    const float g1 = 1.0f - std::fabs(2.0f * phase_ - 1.0f);   // zero where the head jumps
    const float g2 = 1.0f - std::fabs(2.0f * p2 - 1.0f);
    return buf_.read(phase_ * window_ + 2.0f) * g1 + buf_.read(p2 * window_ + 2.0f) * g2;
}

// ------------------------------------------------------------------ Reverb
static const float kLineMs[Reverb::kLines] = {31.7f, 37.3f, 41.9f, 47.1f, 53.3f, 59.9f, 67.1f, 73.7f};
static const float kApMs[4] = {4.77f, 3.59f, 12.73f, 9.31f};
//                                Room   Hall   Plate
static const float kTypeScale[3] = {0.45f, 1.00f, 0.62f};
static const float kTypeApG[3]   = {0.50f, 0.62f, 0.74f};
static const float kTypeBright[3]= {0.85f, 1.00f, 1.60f};
static const float kTypeMod[3]   = {0.50f, 1.00f, 0.40f};

void Reverb::prepare(float fs) {
    fs_ = fs;
    for (int i = 0; i < kLines; ++i) lines_[i].prepare(static_cast<int>(0.14f * fs) + 64);
    for (int i = 0; i < 4; ++i) {
        apLen_[i] = kApMs[i] * 0.001f * fs;
        ap_[i].prepare(static_cast<int>(apLen_[i]) + 4);
    }
    pre_.prepare(static_cast<int>(0.21f * fs));
    static const float kGrainMs[kShimmerLines] = {70.0f, 83.0f, 61.0f, 97.0f};
    for (int k = 0; k < kShimmerLines; ++k) {
        shifter_[k].prepare(fs, kGrainMs[k]);
        shimmerLP_[k].setCutoff(7000.0f, fs);
    }
    duckAtk_ = onePoleCoef(5.0f, fs);
    duckRel_ = onePoleCoef(60.0f, fs);
    set(s_);
}

void Reverb::reset() {
    for (auto& l : lines_) l.reset();
    for (auto& d : damp_) d.reset();
    for (auto& a : ap_) a.reset();
    pre_.reset();
    lowCut_.reset();
    for (auto& sh : shifter_) sh.reset();
    for (auto& lp : shimmerLP_) lp.reset();
    duckEnv_ = 0; duckGain_ = 1;
}

void Reverb::set(const ReverbSettings& s) {
    s_ = s;
    const int t = std::max(0, std::min(2, s.type));
    const float scale = (0.35f + 1.3f * s.size) * kTypeScale[t];
    const float damp = std::min(s.dampHz * kTypeBright[t], 0.45f * fs_);
    for (int i = 0; i < kLines; ++i) {
        len_[i] = kLineMs[i] * 0.001f * fs_ * scale;
        gain_[i] = std::pow(10.0f, -3.0f * len_[i] / (fs_ * std::max(0.05f, s.decayS)));
        damp_[i].setCutoff(damp, fs_);
    }
    apG_ = kTypeApG[t];
    level_ = std::sqrt(kTypeScale[t]);   // small rooms build up more energy: level-match the types
    modDepth_ = s.mod * kTypeMod[t] * 14.0f * fs_ / 48000.0f;
    predelay_ = std::max(1.0f, s.predelayMs * 0.001f * fs_);
    lowCut_.setHighpass(std::max(20.0f, s.lowCutHz), 0.707f, fs_);
}

void Reverb::process(float* L, float* R, int n) {
    const float lfoInc = 0.37f / fs_;
    const float mix = s_.mix;
    const float dryGain = 1.0f - 0.5f * mix;
    const float shimmerAmt = s_.shimmer * 0.55f;
    const float width = s_.width;
    constexpr float kH = 0.35355339f; // 1/sqrt(8)
    static const int kModLines[4] = {0, 2, 5, 7};
    for (int i = 0; i < n; ++i) {
        const float mono = 0.5f * (L[i] + R[i]);
        const float lvl = std::fabs(mono);
        duckEnv_ += (lvl > duckEnv_ ? duckAtk_ : duckRel_) * (lvl - duckEnv_);
        if ((duckCount_++ & 15) == 0) duckGain_ = s_.duck > 0.0f ? duckGainFor(duckEnv_, s_.duck) : 1.0f;
        const float wetGain = mix * duckGain_;

        pre_.push(lowCut_.process(mono));
        float d = pre_.read(predelay_);
        for (int a = 0; a < 4; ++a) {
            const float delayed = ap_[a].read(apLen_[a]);
            const float w = d - apG_ * delayed;
            ap_[a].push(w);
            d = delayed + apG_ * w;
        }

        lfoPhase_ += lfoInc; if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
        float o[kLines];
        for (int k = 0; k < kLines; ++k) {
            float len = len_[k];
            for (int m = 0; m < 4; ++m)
                if (kModLines[m] == k) len += modDepth_ * std::sin(kTwoPi * lfoPhase_ * (1.0f + 0.23f * m) + 1.7f * m);
            o[k] = damp_[k].process(lines_[k].read(len)) * gain_[k];
        }
        float h[kLines];
        for (int k = 0; k < kLines; ++k) h[k] = o[k];
        for (int len = 1; len < kLines; len <<= 1)
            for (int st = 0; st < kLines; st += len << 1)
                for (int j = st; j < st + len; ++j) {
                    const float a = h[j], b = h[j + len];
                    h[j] = a + b; h[j + len] = a - b;
                }
        // Shimmer: on four lines, part of the recirculating signal is replaced by
        // an octave-up copy of itself. A convex mix of two non-expansive paths,
        // so the tank can never gain energy, and every pass climbs an octave more.
        if (shimmerAmt > 0.0f) {
            static const int kShLines[kShimmerLines] = {0, 3, 4, 7};
            for (int m = 0; m < kShimmerLines; ++m) {
                const int k = kShLines[m];
                const float fbk = h[k] * kH;
                const float up = shimmerLP_[m].process(shifter_[m].process(fbk));
                h[k] = ((1.0f - shimmerAmt) * fbk + shimmerAmt * up) / kH;
            }
        }
        for (int k = 0; k < kLines; ++k)
            lines_[k].push(h[k] * kH + ((k & 1) ? -d : d) * 0.5f);

        float wl = (o[0] + o[2] + o[4] + o[6]) * 0.6f * level_;
        float wr = (o[1] + o[3] + o[5] + o[7]) * 0.6f * level_;

        const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * width;
        wl = m + sd; wr = m - sd;
        L[i] = L[i] * dryGain + wl * wetGain;
        R[i] = R[i] * dryGain + wr * wetGain;
    }
}

// ------------------------------------------------------------------ Limiter
void Limiter::process(float* L, float* R, int n, bool enabled) {
    constexpr float kCeil = 0.97f;
    for (int i = 0; i < n; ++i) {
        float l = L[i], r = R[i];
        if (enabled) {
            const float peak = std::max(std::fabs(l), std::fabs(r));
            const float target = peak > kCeil ? kCeil / peak : 1.0f;
            if (target < gain_) gain_ = target;
            else gain_ += rel_ * (target - gain_);
            l *= gain_; r *= gain_;
        }
        L[i] = clampf(l, -1.0f, 1.0f);
        R[i] = clampf(r, -1.0f, 1.0f);
    }
}

} // namespace gsx
