// Engine.cpp
#include "Engine.hpp"

namespace gsx {

static std::atomic<uint32_t> gParamVersion{1};
static uint32_t gSeenVersion = 0;

Engine::Engine() {
    for (int i = 0; i < P_COUNT; ++i) params_[i].store(paramDef(i).defV);
    for (int t = 0; t < kMaxTrackers; ++t) {
        trackerVoice_[t] = -1; trackerCounter_[t] = 0; qNote_[t] = 0; lastMidi_[t] = 0;
    }
}

void Engine::setParam(int id, float v) {
    if (id < 0 || id >= P_COUNT) return;
    const ParamDef& d = paramDef(id);
    v = clampf(v, d.minV, d.maxV);
    if (d.type != GSX_PARAM_CONTINUOUS) v = std::round(v);
    params_[id].store(v, std::memory_order_relaxed);
    gParamVersion.fetch_add(1, std::memory_order_release);
}

void Engine::prepare(double sampleRate, int numInputChannels) {
    fs_ = sampleRate;
    numIn_ = std::max(1, std::min(numInputChannels, 64));
    const float fs = static_cast<float>(sampleRate);
    for (int t = 0; t < kMaxTrackers; ++t) {
        trackers_[t].prepare(sampleRate);
        trackers_[t].setHopPhase(static_cast<float>(t) / kMaxTrackers);
    }
    for (auto& v : voices_) v.prepare(fs);
    drive_.prepare(fs); chorus_.prepare(fs); phaser_.prepare(fs);
    delay_.prepare(fs); reverb_.prepare(fs); limiter_.prepare(fs);
    gSeenVersion = 0;
    updateParams();
    synthGainS_ = synthGain_; dryGainS_ = dryGain_; masterGainS_ = masterGain_;
    killAll();
    std::memset(scope_, 0, sizeof(scope_));
}

void Engine::killAll() {
    for (auto& v : voices_) v.kill();
    for (int t = 0; t < kMaxTrackers; ++t) { trackerVoice_[t] = -1; trackers_[t].reset(); }
    drive_.reset(); chorus_.reset(); phaser_.reset(); delay_.reset(); reverb_.reset();
}

void Engine::updateParams() {
    const uint32_t ver = gParamVersion.load(std::memory_order_acquire);
    if (ver == gSeenVersion) return;
    gSeenVersion = ver;
    auto P = [this](int id) { return params_[id].load(std::memory_order_relaxed); };
    const float fs = static_cast<float>(fs_);

    const bool hex = P(P_IN_MODE) > 0.5f;
    if (hex != hexMode_) {
        for (int t = 0; t < kMaxTrackers; ++t) {
            if (trackerVoice_[t] >= 0) voices_[trackerVoice_[t]].release();
            trackerVoice_[t] = -1;
            trackers_[t].reset();
        }
        hexMode_ = hex;
    }
    numTrackers_ = hex ? kMaxTrackers : 1;
    trackerCount.store(numTrackers_, std::memory_order_relaxed);
    inChannel_ = static_cast<int>(P(P_IN_CHANNEL)) - 1;
    inGain_ = dbToLin(P(P_IN_GAIN));

    static const float kHops[3] = {0.5f, 1.0f, 2.0f};
    tset_.lowestHz = P(P_TRK_LOWEST);
    tset_.highestHz = std::max(P(P_TRK_HIGHEST), tset_.lowestHz * 4.0f);
    tset_.threshold = P(P_TRK_THRESHOLD);
    tset_.stability = static_cast<int>(P(P_TRK_STABILITY));
    tset_.hopMs = kHops[static_cast<int>(clampf(P(P_TRK_HOP), 0, 2))];
    tset_.gateDb = P(P_IN_GATE);
    tset_.retrigger = P(P_TRK_RETRIGGER) > 0.5f;
    for (int t = 0; t < kMaxTrackers; ++t) {
        trackers_[t].setSettings(tset_);
        trackers_[t].setHopPhase(static_cast<float>(t) / kMaxTrackers);
    }

    pitchMode_ = static_cast<int>(P(P_TRK_PITCHMODE));
    transpose_ = P(P_TRK_TRANSPOSE);
    vp_.glideMs = pitchMode_ == 0 ? 4.0f : (pitchMode_ == 1 ? 0.0f : P(P_TRK_GLIDE));
    vp_.dynAmt = P(P_TRK_DYNAMICS);

    vp_.waveA = static_cast<int>(P(P_OSCA_WAVE));
    vp_.octaveA = P(P_OSCA_OCTAVE);
    vp_.unison = static_cast<int>(P(P_OSCA_UNISON));
    vp_.unisonDetune = P(P_OSCA_DETUNE);
    vp_.waveB = static_cast<int>(P(P_OSCB_WAVE));
    vp_.semiB = P(P_OSCB_SEMI);
    vp_.fineB = P(P_OSCB_FINE);
    vp_.mix = P(P_OSC_MIX);
    vp_.pw = P(P_OSC_PW);
    vp_.fm = P(P_OSC_FM);
    vp_.sub = P(P_OSC_SUB);
    vp_.noise = P(P_OSC_NOISE);

    vp_.filterType = static_cast<int>(P(P_FLT_TYPE));
    vp_.cutoff = P(P_FLT_CUTOFF);
    vp_.res = P(P_FLT_RES);
    vp_.envAmt = P(P_FLT_ENVAMT);
    vp_.keyTrack = P(P_FLT_KEYTRACK);
    vp_.fltDyn = P(P_FLT_DYN);
    vp_.fltDrive = P(P_FLT_DRIVE);
    fltProto_.set(P(P_FENV_A), P(P_FENV_D), P(P_FENV_S), P(P_FENV_R), fs);
    ampProto_.set(P(P_AENV_A), P(P_AENV_D), P(P_AENV_S), P(P_AENV_R), fs);
    vp_.velSens = P(P_AMP_VEL);
    for (auto& v : voices_) if (v.active()) v.setEnvelopes(ampProto_, fltProto_);

    lfoWave_ = static_cast<int>(P(P_LFO_WAVE));
    lfoRate_ = P(P_LFO_RATE);
    vp_.lfoDelayMs = P(P_LFO_DELAY);
    vp_.lfoPitchCents = P(P_LFO_PITCH);
    vp_.lfoFilterOct = P(P_LFO_FILTER);
    vp_.lfoPw = P(P_LFO_PW);
    vp_.lfoAmp = P(P_LFO_AMP);

    auto on = [&](int id) { return P(id) > 0.5f; };
    if (on(P_DRV_ON) && !drvOn_) drive_.reset();
    if (on(P_CHO_ON) && !choOn_) chorus_.reset();
    if (on(P_PHA_ON) && !phaOn_) phaser_.reset();
    if (on(P_DLY_ON) && !dlyOn_) delay_.reset();
    if (on(P_REV_ON) && !revOn_) reverb_.reset();
    drvOn_ = on(P_DRV_ON); choOn_ = on(P_CHO_ON); phaOn_ = on(P_PHA_ON);
    dlyOn_ = on(P_DLY_ON); revOn_ = on(P_REV_ON); limOn_ = on(P_MST_LIMITER);
    dryFx_ = on(P_MST_DRYFX);
    drive_.set(P(P_DRV_AMOUNT), P(P_DRV_TONE), P(P_DRV_MIX));
    chorus_.set(P(P_CHO_RATE), P(P_CHO_DEPTH), P(P_CHO_MIX));
    phaser_.set(P(P_PHA_RATE), P(P_PHA_DEPTH), P(P_PHA_FB), P(P_PHA_MIX));
    {
        // Tempo sync: beats per division (Free uses the Time knob).
        static const float kBeats[8] = {0.0f, 2.0f, 1.0f, 2.0f / 3.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f};
        const int sync = static_cast<int>(clampf(P(P_DLY_SYNC), 0, 7));
        DelaySettings ds;
        ds.mode = static_cast<int>(P(P_DLY_MODE));
        ds.timeMs = sync == 0 ? P(P_DLY_TIME) : 60000.0f / P(P_DLY_BPM) * kBeats[sync];
        ds.feedback = P(P_DLY_FB);
        ds.mix = P(P_DLY_MIX);
        ds.pingPong = on(P_DLY_PINGPONG);
        ds.dampHz = P(P_DLY_DAMP);
        ds.lowCutHz = P(P_DLY_LOWCUT);
        ds.mod = P(P_DLY_MOD);
        ds.duck = P(P_DLY_DUCK);
        delay_.set(ds);

        ReverbSettings rs;
        rs.type = static_cast<int>(P(P_REV_TYPE));
        rs.size = P(P_REV_SIZE);
        rs.decayS = P(P_REV_DECAY);
        rs.dampHz = P(P_REV_DAMP);
        rs.predelayMs = P(P_REV_PREDELAY);
        rs.mix = P(P_REV_MIX);
        rs.lowCutHz = P(P_REV_LOWCUT);
        rs.mod = P(P_REV_MOD);
        rs.shimmer = P(P_REV_SHIMMER);
        rs.width = P(P_REV_WIDTH);
        rs.duck = P(P_REV_DUCK);
        reverb_.set(rs);
    }

    synthGain_ = P(P_MST_SYNTH) <= -59.9f ? 0.0f : dbToLin(P(P_MST_SYNTH));
    dryGain_ = P(P_MST_DRY) <= -59.9f ? 0.0f : dbToLin(P(P_MST_DRY));
    masterGain_ = P(P_MST_VOLUME) <= -59.9f ? 0.0f : dbToLin(P(P_MST_VOLUME));
}

int Engine::allocVoice() {
    for (int i = 0; i < kMaxVoices; ++i) if (!voices_[i].active()) return i;
    int best = -1; float lvl = 1e9f;
    for (int i = 0; i < kMaxVoices; ++i)
        if (!voices_[i].gated() && voices_[i].level() < lvl) { lvl = voices_[i].level(); best = i; }
    if (best < 0) best = 0;
    voices_[best].kill();
    return best;
}

float Engine::lfoValue(int n) {
    const float ph = lfoPhase_;
    float v;
    switch (lfoWave_) {
        case 0: v = std::sin(kTwoPi * ph); break;
        case 1: v = 4.0f * std::fabs(ph - 0.5f) - 1.0f; break;
        case 2: v = 2.0f * ph - 1.0f; break;
        case 3: v = ph < 0.5f ? 1.0f : -1.0f; break;
        default: v = lfoSH_; break;
    }
    lfoPhase_ += lfoRate_ * static_cast<float>(n) / static_cast<float>(fs_);
    if (lfoPhase_ >= 1.0f) { lfoPhase_ -= std::floor(lfoPhase_); lfoSH_ = lfoNoise_.next(); }
    return v;
}

void Engine::handleNotes() {
    const bool quant = pitchMode_ != 0;
    for (int t = 0; t < numTrackers_; ++t) {
        const PitchTracker& tr = trackers_[t];
        int v = trackerVoice_[t];
        if (tr.noteActive()) {
            const float m = hzToMidi(tr.frequency()) + transpose_;
            const bool newNote = tr.noteCounter() != trackerCounter_[t];
            float target = m;
            if (quant) {
                if (newNote || std::fabs(m - qNote_[t]) > 0.6f) qNote_[t] = std::round(m);
                target = qNote_[t];
            }
            if (newNote) {
                trackerCounter_[t] = tr.noteCounter();
                const float prev = v >= 0 ? voices_[v].currentMidi() : lastMidi_[t];
                const bool glide = pitchMode_ == 2 && prev > 0.0f;
                if (v >= 0) voices_[v].release();
                v = allocVoice();
                const float pan = hexMode_ ? (-0.6f + 1.2f * static_cast<float>(t) / (kMaxTrackers - 1)) : 0.0f;
                voices_[v].setEnvelopes(ampProto_, fltProto_);
                voices_[v].noteOn(target, tr.velocity(), pan, prev, glide);
                trackerVoice_[t] = v;
            } else if (v >= 0) {
                voices_[v].setTarget(target);
            }
            if (v >= 0) voices_[v].setDynamics(tr.dynamics());
            lastMidi_[t] = target;
        } else {
            trackerCounter_[t] = tr.noteCounter();
            if (v >= 0) { voices_[v].release(); trackerVoice_[t] = -1; }
        }
    }
}

void Engine::process(const float* const* in, int numIn, float* outL, float* outR, int frames) {
    while (frames > kMaxBlock) {   // never expected, but stay safe
        process(in, numIn, outL, outR, kMaxBlock);
        // advance channel pointers is not possible generically; drop input for the rest
        outL += kMaxBlock; outR += kMaxBlock; frames -= kMaxBlock; in = nullptr; numIn = 0;
    }
    ScopedNoDenormals noDenormals;
    if (panic_.exchange(false, std::memory_order_acq_rel)) killAll();
    updateParams();

    std::memset(outL, 0, sizeof(float) * frames);
    std::memset(outR, 0, sizeof(float) * frames);

    float inPeak = 0.0f;
    const int chans = in ? std::min(numIn, numIn_) : 0;
    for (int off = 0; off < frames; off += kSubBlock) {
        const int n = std::min(kSubBlock, frames - off);
        // ---- gather input
        if (hexMode_) {
            for (int i = 0; i < n; ++i) dryBuf_[off + i] = 0.0f;
            for (int t = 0; t < kMaxTrackers; ++t) {
                const int ch = inChannel_ + t;
                const float* src = (ch >= 0 && ch < chans && in[ch]) ? in[ch] + off : nullptr;
                for (int i = 0; i < n; ++i) {
                    const float s = src ? src[i] * inGain_ : 0.0f;
                    trkBuf_[t][i] = s;
                    dryBuf_[off + i] += s * 0.4f;
                    inPeak = std::max(inPeak, std::fabs(s));
                }
            }
        } else {
            const int ch = std::min(std::max(inChannel_, 0), std::max(chans - 1, 0));
            const float* src = (chans > 0 && in[ch]) ? in[ch] + off : nullptr;
            for (int i = 0; i < n; ++i) {
                const float s = src ? src[i] * inGain_ : 0.0f;
                trkBuf_[0][i] = s;
                dryBuf_[off + i] = s;
                inPeak = std::max(inPeak, std::fabs(s));
            }
        }
        // ---- analysis -> note events
        for (int t = 0; t < numTrackers_; ++t) trackers_[t].process(trkBuf_[t], n);
        handleNotes();
        // ---- synthesis
        const float lfo = lfoValue(n);
        ++stamp_;
        for (auto& v : voices_) if (v.active()) v.render(outL + off, outR + off, n, vp_, lfo, stamp_);
    }

    // ---- synth/dry mix with gain ramps
    {
        const float invN = 1.0f / static_cast<float>(frames);
        const float sStep = (synthGain_ - synthGainS_) * invN;
        const float dStep = (dryGain_ - dryGainS_) * invN;
        float sg = synthGainS_, dg = dryGainS_;
        for (int i = 0; i < frames; ++i) {
            sg += sStep; dg += dStep;
            dryBuf_[i] *= dg;                       // keep the scaled dry signal
            const float dryIn = dryFx_ ? dryBuf_[i] : 0.0f;
            outL[i] = outL[i] * sg + dryIn;
            outR[i] = outR[i] * sg + dryIn;
        }
        synthGainS_ = synthGain_; dryGainS_ = dryGain_;
    }

    // ---- effects
    if (drvOn_) drive_.process(outL, outR, frames);
    if (choOn_) chorus_.process(outL, outR, frames);
    if (phaOn_) phaser_.process(outL, outR, frames);
    if (dlyOn_) delay_.process(outL, outR, frames);
    if (revOn_) reverb_.process(outL, outR, frames);

    // ---- dry guitar that bypasses the effects
    if (!dryFx_) for (int i = 0; i < frames; ++i) { outL[i] += dryBuf_[i]; outR[i] += dryBuf_[i]; }

    // ---- master + limiter
    {
        const float step = (masterGain_ - masterGainS_) / static_cast<float>(frames);
        float g = masterGainS_;
        for (int i = 0; i < frames; ++i) { g += step; outL[i] *= g; outR[i] *= g; }
        masterGainS_ = masterGain_;
    }
    limiter_.process(outL, outR, frames, limOn_);

    // ---- safety: never let NaN/Inf reach the DAC
    float outPeak = 0.0f;
    bool bad = false;
    for (int i = 0; i < frames; ++i) {
        const float a = std::fabs(outL[i]) + std::fabs(outR[i]);
        if (!(a < 1e6f)) { bad = true; break; }
        outPeak = std::max(outPeak, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
    }
    if (bad) {
        std::memset(outL, 0, sizeof(float) * frames);
        std::memset(outR, 0, sizeof(float) * frames);
        killAll();
        outPeak = 0.0f;
    }

    // ---- scope + meters
    uint32_t w = scopeW_.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i) {
        scope_[w & (kScopeSize - 1)] = 0.5f * (outL[i] + outR[i]);
        ++w;
    }
    scopeW_.store(w, std::memory_order_release);

    const float decay = std::exp(-static_cast<float>(frames) / (0.3f * static_cast<float>(fs_)));
    inPeakHold_ = std::max(inPeak, inPeakHold_ * decay);
    outPeakHold_ = std::max(outPeak, outPeakHold_ * decay);
    inputPeak.store(inPeakHold_, std::memory_order_relaxed);
    outputPeak.store(outPeakHold_, std::memory_order_relaxed);
    int nv = 0;
    for (auto& v : voices_) nv += v.active() ? 1 : 0;
    activeVoices.store(nv, std::memory_order_relaxed);
    for (int t = 0; t < kMaxTrackers; ++t) {
        TrackerMeter& m = trackerMeters[t];
        if (t < numTrackers_) {
            const PitchTracker& tr = trackers_[t];
            const float hz = tr.noteActive() ? tr.frequency() : tr.lastEstimateHz();
            m.hz.store(hz, std::memory_order_relaxed);
            m.midi.store(hz > 0 ? hzToMidi(hz) : 0.0f, std::memory_order_relaxed);
            m.ap.store(tr.lastAperiodicity(), std::memory_order_relaxed);
            m.levelDb.store(tr.levelDb(), std::memory_order_relaxed);
            m.gate.store(tr.gateOpen() ? 1 : 0, std::memory_order_relaxed);
            m.active.store(tr.noteActive() ? 1 : 0, std::memory_order_relaxed);
        } else {
            m.hz.store(0); m.gate.store(0); m.active.store(0); m.levelDb.store(-120);
        }
    }
}

void Engine::readScope(float* dst, int n) const {
    n = std::min(n, kScopeSize);
    const uint32_t w = scopeW_.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dst[i] = scope_[(w - static_cast<uint32_t>(n) + static_cast<uint32_t>(i)) & (kScopeSize - 1)];
}

} // namespace gsx
