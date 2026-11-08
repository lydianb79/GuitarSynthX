// Effects.hpp — stereo effect chain: drive, chorus, phaser, delay, FDN reverb, limiter.
#pragma once
#include "DSPUtil.hpp"

namespace gsx {

class Drive {
public:
    void prepare(float fs) { fs_ = fs; reset(); }
    void reset() { lpL_.reset(); lpR_.reset(); }
    void set(float amount, float toneHz, float mix) {
        pre_ = dbToLin(amount * 36.0f);
        post_ = 1.0f / std::sqrt(pre_);
        mix_ = mix;
        lpL_.setCutoff(toneHz, fs_); lpR_.setCutoff(toneHz, fs_);
    }
    void process(float* L, float* R, int n);
private:
    float fs_ = 48000, pre_ = 1, post_ = 1, mix_ = 1;
    OnePoleLP lpL_, lpR_;
};

class Chorus {
public:
    void prepare(float fs);
    void reset() { dl_.reset(); dr_.reset(); }
    void set(float rateHz, float depth, float mix) { rate_ = rateHz; depth_ = depth; mix_ = mix; }
    void process(float* L, float* R, int n);
private:
    float fs_ = 48000, rate_ = 0.5f, depth_ = 0.5f, mix_ = 0.5f, phase_ = 0;
    DelayLine dl_, dr_;
};

class Phaser {
public:
    static constexpr int kStages = 6;
    void prepare(float fs) { fs_ = fs; reset(); }
    void reset();
    void set(float rateHz, float depth, float fb, float mix) { rate_ = rateHz; depth_ = depth; fb_ = fb; mix_ = mix; }
    void process(float* L, float* R, int n);
private:
    float fs_ = 48000, rate_ = 0.3f, depth_ = 0.7f, fb_ = 0.5f, mix_ = 0.5f, phase_ = 0;
    float zL_[kStages] = {}, zR_[kStages] = {};
    float fbL_ = 0, fbR_ = 0;
};

struct DelaySettings {
    int   mode = 0;          // 0 Digital, 1 Tape, 2 Analog (BBD)
    float timeMs = 350;      // already resolved from tempo sync by the engine
    float feedback = 0.35f;
    float mix = 0.25f;
    bool  pingPong = true;
    float dampHz = 5000;     // high cut in the feedback loop
    float lowCutHz = 80;     // low cut in the feedback loop
    float mod = 0.0f;        // time modulation depth 0..1 (wow/flutter for Tape)
    float duck = 0.0f;       // 0..1 wet attenuation while you play
};

/// Stereo delay with Digital / Tape / Analog characters, filtered feedback,
/// modulation and input ducking.
class StereoDelay {
public:
    void prepare(float fs);
    void reset();
    void set(const DelaySettings& s);
    void process(float* L, float* R, int n);
private:
    float fs_ = 48000, target_ = 1000, cur_ = 1000, smooth_ = 0.001f;
    DelaySettings s_;
    DelayLine dl_, dr_;
    OnePoleLP dampL_, dampR_;
    OnePoleLP hpL_, hpR_;          // low cut = x - lowpass(x)
    float wow_ = 0, flutter_ = 0, modPh_ = 0;
    float duckEnv_ = 0, duckAtk_ = 0, duckRel_ = 0, duckGain_ = 1;
    int duckCount_ = 0;
    Noise noise_;
    float flutterNoise_ = 0;
};

struct ReverbSettings {
    int   type = 1;          // 0 Room, 1 Hall, 2 Plate
    float size = 0.6f;
    float decayS = 2.5f;
    float dampHz = 7000;
    float predelayMs = 20;
    float mix = 0.25f;
    float lowCutHz = 120;
    float mod = 0.3f;        // line modulation depth 0..1
    float shimmer = 0.0f;    // octave-up feedback amount 0..1
    float width = 1.0f;      // stereo width 0..1
    float duck = 0.0f;
};

/// Octave-up (or any ratio) granular pitch shifter: two crossfaded read heads.
/// Ducking amount from a peak envelope: starts at -50 dBFS, full at -20 dBFS.
inline float duckGainFor(float env, float amount) {
    const float t = clampf((linToDb(env) + 50.0f) / 30.0f, 0.0f, 1.0f);
    return 1.0f - amount * t;
}

class PitchShifter {
public:
    void prepare(float fs, float windowMs = 60.0f);
    void reset() { buf_.reset(); phase_ = 0; }
    float process(float x);
    float ratio = 2.0f;
private:
    DelayLine buf_;
    float window_ = 2880, phase_ = 0;
};

/// 8-line feedback delay network with input diffusion, modulated lines,
/// room/hall/plate voicings, shimmer and ducking.
class Reverb {
public:
    static constexpr int kLines = 8;
    void prepare(float fs);
    void reset();
    void set(const ReverbSettings& s);
    void process(float* L, float* R, int n);
private:
    float fs_ = 48000, predelay_ = 1;
    ReverbSettings s_;
    float len_[kLines] = {}, gain_[kLines] = {};
    DelayLine lines_[kLines];
    OnePoleLP damp_[kLines];
    DelayLine pre_;
    DelayLine ap_[4];
    float apLen_[4] = {};
    float apG_ = 0.62f, modDepth_ = 6, level_ = 1;
    float lfoPhase_ = 0;
    Biquad lowCut_;
    static constexpr int kShimmerLines = 4;
    PitchShifter shifter_[kShimmerLines];
    OnePoleLP shimmerLP_[kShimmerLines];
    float duckEnv_ = 0, duckAtk_ = 0, duckRel_ = 0, duckGain_ = 1;
    int duckCount_ = 0;
};

class Limiter {
public:
    void prepare(float fs) { rel_ = onePoleCoef(120.0f, fs); gain_ = 1.0f; }
    void process(float* L, float* R, int n, bool enabled);
    float gain() const { return gain_; }
private:
    float gain_ = 1.0f, rel_ = 0.001f;
};

} // namespace gsx
