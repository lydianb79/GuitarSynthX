// Synth.cpp
#include "Synth.hpp"

namespace gsx {

static inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
static inline float wrap01(float p) { return p - std::floor(p); }

void Voice::prepare(float fs) {
    fs_ = fs; invFs_ = 1.0f / fs;
    kill();
}

void Voice::kill() {
    active_ = gated_ = false;
    amp_.reset(); flt_.reset();
    s_[0] = s_[1] = s_[2] = s_[3] = 0;
    ic1_ = ic2_ = 0;
}

void Voice::setEnvelopes(const ADSR& a, const ADSR& f) {
    amp_.aCoef = a.aCoef; amp_.dCoef = a.dCoef; amp_.rCoef = a.rCoef; amp_.sustain = a.sustain;
    flt_.aCoef = f.aCoef; flt_.dCoef = f.dCoef; flt_.rCoef = f.rCoef; flt_.sustain = f.sustain;
    if (amp_.stage == ADSR::Sustain) amp_.stage = ADSR::Decay;   // follow sustain changes smoothly
    if (flt_.stage == ADSR::Sustain) flt_.stage = ADSR::Decay;
}

void Voice::noteOn(float midi, float velocity, float pan, float startMidi, bool legatoGlide) {
    const bool wasActive = active_;
    active_ = true; gated_ = true;
    velocity_ = velocity;
    pan_ = pan;
    targetMidi_ = midi;
    curMidi_ = legatoGlide ? startMidi : midi;
    dyn_ = dynSmooth_ = 1.0f;
    lfoFade_ = 0.0f;
    if (!wasActive) {
        s_[0] = s_[1] = s_[2] = s_[3] = 0; ic1_ = ic2_ = 0;
        amp_.value = 0; flt_.value = 0;
        for (int i = 0; i < kMaxUnison; ++i) phA_[i] = (noise_.next() + 1.0f) * 0.5f;
        phA_[0] = 0; phB_ = 0; phSub_ = 0; subSign_ = 1.0f;
    }
    amp_.gateOn(); flt_.gateOn();
    ++age_;
}

void Voice::release() {
    gated_ = false;
    amp_.gateOff(); flt_.gateOff();
}

inline float Voice::osc(int wave, float ph, float inc, float pw) const {
    switch (wave) {
        case 0: return std::sin(kTwoPi * ph);
        case 1: return 4.0f * std::fabs(ph - 0.5f) - 1.0f;
        case 2: return 2.0f * ph - 1.0f - polyBlep(ph, inc);
        case 3: pw = 0.5f; [[fallthrough]];
        default: {
            float v = ph < pw ? 1.0f : -1.0f;
            v += polyBlep(ph, inc);
            v -= polyBlep(wrap01(ph + 1.0f - pw), inc);
            return v;
        }
    }
}

void Voice::render(float* outL, float* outR, int n, const VoiceParams& p, float lfo, uint64_t stamp) {
    (void)stamp;
    if (!active_) return;
    const float nf = static_cast<float>(n);

    // --- control-rate updates (once per sub-block)
    if (p.glideMs > 0.05f) {
        const float c = 1.0f - std::exp(-nf / (p.glideMs * 0.001f * fs_));
        curMidi_ += c * (targetMidi_ - curMidi_);
    } else {
        curMidi_ = targetMidi_;
    }
    if (p.lfoDelayMs > 1.0f) lfoFade_ = std::min(1.0f, lfoFade_ + nf / (p.lfoDelayMs * 0.001f * fs_));
    else lfoFade_ = 1.0f;
    const float lfoV = lfo * lfoFade_;
    dynSmooth_ += 0.25f * (dyn_ - dynSmooth_);

    const float midi = curMidi_ + lfoV * p.lfoPitchCents * 0.01f;
    const float fA = midiToHz(midi + 12.0f * p.octaveA);
    const float fB = midiToHz(midi + p.semiB + p.fineB * 0.01f);
    const int nu = std::max(1, std::min(kMaxUnison, p.unison));
    float incA[kMaxUnison];
    for (int u = 0; u < nu; ++u) {
        const float spread = nu > 1 ? (2.0f * u / (nu - 1) - 1.0f) : 0.0f;
        incA[u] = std::min(0.45f, fA * std::exp2(spread * p.unisonDetune / 1200.0f) * invFs_);
    }
    const float uniGain = 1.0f / std::sqrt(static_cast<float>(nu));
    const float incB = std::min(0.45f, fB * invFs_);
    const float incSub = std::min(0.45f, fA * 0.5f * invFs_);
    const float pw = clampf(p.pw + lfoV * p.lfoPw, 0.05f, 0.95f);
    const float fmIdx = p.fm * 2.5f;
    const float mixA = 1.0f - p.mix, mixB = p.mix;

    // filter cutoff (octaves)
    const float dynLevel = dynSmooth_ * velocity_;
    float oct = std::log2(p.cutoff) + p.envAmt * flt_.value
              + p.keyTrack * (curMidi_ - 60.0f) / 12.0f
              + p.fltDyn * (dynLevel - 1.0f)
              + lfoV * p.lfoFilterOct;
    const float fc = clampf(std::exp2(oct), 20.0f, 0.45f * fs_);
    const float g = std::tan(kPi * fc * invFs_);
    const float drive = 1.0f + 3.0f * p.fltDrive;

    const float velGain = 1.0f - p.velSens + p.velSens * velocity_;
    const float dynGain = 1.0f - p.dynAmt + p.dynAmt * dynSmooth_;
    const float trem = 1.0f - p.lfoAmp * lfoFade_ * (0.5f + 0.5f * lfo);
    const float gain = 0.5f * velGain * dynGain * trem;
    const float pl = std::cos((pan_ + 1.0f) * kPi * 0.25f);
    const float pr = std::sin((pan_ + 1.0f) * kPi * 0.25f);

    // ladder coefficients
    const float G = g / (1.0f + g), beta = 1.0f / (1.0f + g);
    const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
    const float k = p.res * 3.95f;
    const float comp = 1.0f + 0.5f * k;
    // SVF coefficients
    const float ks = 2.0f - 1.96f * p.res;
    const float a1 = 1.0f / (1.0f + g * (g + ks)), a2 = g * a1, a3 = g * a2;

    for (int i = 0; i < n; ++i) {
        const float ae = amp_.next();
        flt_.next();

        // oscillators
        const float b = osc(p.waveB, phB_, incB, pw);
        phB_ += incB; if (phB_ >= 1.0f) phB_ -= 1.0f;

        float a = 0.0f;
        for (int u = 0; u < nu; ++u) {
            float ph = phA_[u];
            if (fmIdx > 0.0f) ph = wrap01(ph + fmIdx * b);
            a += osc(p.waveA, ph, incA[u], pw);
            phA_[u] += incA[u]; if (phA_[u] >= 1.0f) phA_[u] -= 1.0f;
        }
        a *= uniGain;

        float x = a * mixA + b * mixB;
        if (p.sub > 0.0f) {
            float sv = phSub_ < 0.5f ? 1.0f : -1.0f;
            sv += polyBlep(phSub_, incSub);
            sv -= polyBlep(wrap01(phSub_ + 0.5f), incSub);
            x += p.sub * sv;
        }
        phSub_ += incSub; if (phSub_ >= 1.0f) phSub_ -= 1.0f;
        if (p.noise > 0.0f) x += p.noise * noise_.next();

        // filter
        float y;
        if (p.filterType == 0) {
            const float xin = x * drive * comp * 0.5f;
            const float S = G3 * beta * s_[0] + G2 * beta * s_[1] + G * beta * s_[2] + beta * s_[3];
            const float y4 = (G4 * xin + S) / (1.0f + k * G4);
            const float u = fastTanh(xin - k * y4);
            float v = (u - s_[0]) * G;  const float y1 = v + s_[0]; s_[0] = y1 + v;
            v = (y1 - s_[1]) * G;       const float y2 = v + s_[1]; s_[1] = y2 + v;
            v = (y2 - s_[2]) * G;       const float y3 = v + s_[2]; s_[2] = y3 + v;
            v = (y3 - s_[3]) * G;       y = v + s_[3];              s_[3] = y + v;
            y *= 2.0f;
        } else {
            const float v0 = fastTanh(x * drive * 0.5f) * 2.0f;
            const float v3 = v0 - ic2_;
            const float v1 = a1 * ic1_ + a2 * v3;
            const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
            ic1_ = 2.0f * v1 - ic1_;
            ic2_ = 2.0f * v2 - ic2_;
            if (p.filterType == 1) y = v2;
            else if (p.filterType == 2) y = v1 * ks;
            else y = v0 - ks * v1 - v2;
        }

        const float out = y * ae * gain;
        outL[i] += out * pl;
        outR[i] += out * pr;
    }

    if (amp_.stage == ADSR::Idle) active_ = false;
}

} // namespace gsx
