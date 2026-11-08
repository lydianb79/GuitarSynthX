// Synth.hpp — subtractive/FM voice: unison PolyBLEP oscillators, sub, noise,
// ZDF ladder + SVF filter, ADSR envelopes. All per-voice state is POD.
#pragma once
#include "DSPUtil.hpp"

namespace gsx {

struct ADSR {
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0;
    float aCoef = 0, dCoef = 0, rCoef = 0, sustain = 1;

    void set(float aMs, float dMs, float s, float rMs, float fs) {
        // attack aims at 1.3 so the curve is convex but reaches 1 in ~aMs
        aCoef = 1.0f - std::exp(-1.47f / std::max(1.0f, aMs * 0.001f * fs));
        dCoef = 1.0f - std::exp(-4.6f / std::max(1.0f, dMs * 0.001f * fs));
        rCoef = 1.0f - std::exp(-4.6f / std::max(1.0f, rMs * 0.001f * fs));
        sustain = s;
    }
    void gateOn()  { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; value = 0; }
    inline float next() {
        switch (stage) {
            case Attack:
                value += aCoef * (1.3f - value);
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value += dCoef * (sustain - value);
                if (std::fabs(value - sustain) < 1e-4f) { value = sustain; stage = Sustain; }
                break;
            case Sustain: value = sustain; break;
            case Release:
                value += rCoef * (0.0f - value);
                if (value < 1e-5f) { value = 0; stage = Idle; }
                break;
            case Idle: break;
        }
        return value;
    }
};

/// Snapshot of synth parameters, built once per audio block by the engine.
struct VoiceParams {
    int   waveA = 2, waveB = 3;
    float octaveA = 0;
    int   unison = 1;
    float unisonDetune = 12;
    float semiB = 0, fineB = 0;
    float mix = 0.5f, pw = 0.5f, fm = 0, sub = 0, noise = 0;
    int   filterType = 0;
    float cutoff = 2500, res = 0.2f, envAmt = 2, keyTrack = 0.5f, fltDyn = 1, fltDrive = 0.1f;
    float velSens = 0.6f, dynAmt = 0.5f;
    float glideMs = 0;
    float lfoPitchCents = 0, lfoFilterOct = 0, lfoPw = 0, lfoAmp = 0, lfoDelayMs = 0;
};

class Voice {
public:
    static constexpr int kMaxUnison = 7;

    void prepare(float fs);
    void noteOn(float midi, float velocity, float pan, float startMidi, bool legatoGlide);
    void release();
    void kill();
    void setTarget(float midi) { targetMidi_ = midi; }
    void setDynamics(float d) { dyn_ = d; }
    void setEnvelopes(const ADSR& amp, const ADSR& flt);

    bool active() const { return active_; }
    bool gated() const  { return gated_; }
    float level() const { return amp_.value; }
    float currentMidi() const { return curMidi_; }
    uint64_t age() const { return age_; }
    float pan() const { return pan_; }

    /// Renders n (<= 64) samples and *adds* them to outL/outR.
    void render(float* outL, float* outR, int n, const VoiceParams& p, float lfo, uint64_t stamp);

private:
    inline float osc(int wave, float ph, float inc, float pw) const;

    float fs_ = 48000, invFs_ = 1.0f / 48000;
    bool active_ = false, gated_ = false;
    float velocity_ = 1, pan_ = 0, dyn_ = 1, dynSmooth_ = 1;
    float curMidi_ = 60, targetMidi_ = 60;
    float phA_[kMaxUnison] = {}, phB_ = 0, phSub_ = 0;
    ADSR amp_, flt_;
    float ampAttackMs_ = 1;
    float lfoFade_ = 0;
    // filter state
    float s_[4] = {};
    float ic1_ = 0, ic2_ = 0;
    float subSign_ = 1.0f;
    Noise noise_;
    uint64_t age_ = 0;
};

} // namespace gsx
