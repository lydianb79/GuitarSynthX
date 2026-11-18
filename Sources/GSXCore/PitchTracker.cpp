// PitchTracker.cpp
#include "PitchTracker.hpp"

#if defined(__APPLE__)
  #include <Accelerate/Accelerate.h>
#endif

namespace gsx {

// Fraction of the available post-onset samples used as the maximum lag.
// 0.5 = classic YIN (window == max lag, needs 2 periods). Larger values let a
// note be recognised sooner with a shorter integration window.
static constexpr float kOnsetLagFraction = 0.56f;
// Minimum time after an onset before analysing (skips the pick transient).
static constexpr float kMinOnsetMs = 2.5f;

void PitchTracker::prepare(double sampleRate) {
    fs_ = sampleRate;
    decim_ = std::max(1, static_cast<int>(std::lround(sampleRate / 24000.0)));
    fsD_ = sampleRate / decim_;
    const float fs = static_cast<float>(fs_);
    fastAtk_ = onePoleCoef(0.25f, fs);
    fastRel_ = onePoleCoef(25.0f, fs);
    slowAtk_ = onePoleCoef(30.0f, fs);
    slowRel_ = onePoleCoef(250.0f, fs);
    hfSlowRel_ = onePoleCoef(60.0f, fs);
    gateHoldSamples_ = static_cast<int>(0.040 * fs_);
    refractorySamples_ = static_cast<int>(0.045 * fs_);
    setSettings(set_);
    reset();
}

void PitchTracker::reset() {
    hp_.reset(); lp1_.reset(); lp2_.reset(); hf_.reset();
    hfFast_ = hfSlow_ = 0;
    fastEnv_ = slowEnv_ = 0;
    gateOpen_ = false; gateCloseCount_ = 0; refractory_ = 0;
    std::memset(hist_, 0, sizeof(hist_));
    histW_ = 0; hopCount_ = static_cast<int>(hopPhase_ * hop_); decimPhase_ = 0;
    sinceOnset_ = 1 << 30; onsetWindow_ = false;
    noteActive_ = false; pendingRetrigger_ = false;
    candCount_ = 0; peakEnv_ = 0; dynNorm_ = dynSmooth_ = 0;
    lastAp_ = 1; lastEstHz_ = 0;
}

void PitchTracker::setSettings(const TrackerSettings& s) {
    set_ = s;
    const float fs = static_cast<float>(fs_);
    hp_.setHighpass(std::min(40.0f, s.lowestHz * 0.6f), 0.707f, fs);
    const float lpf = std::min(s.highestHz * 1.25f, static_cast<float>(fsD_) * 0.40f);
    lp1_.setLowpass(lpf, 0.5412f, fs);   // 4th-order Butterworth
    lp2_.setLowpass(lpf, 1.3066f, fs);
    hf_.setHighpass(std::min(2500.0f, fs * 0.2f), 0.707f, fs);   // pick-attack detector
    gateLin_ = dbToLin(s.gateDb);
    hop_ = std::max(4, static_cast<int>(std::lround(s.hopMs * 0.001 * fsD_)));
}

void PitchTracker::process(const float* x, int n) {
    for (int i = 0; i < n; ++i) {
        const float h = hp_.process(x[i]);
        const float a = std::fabs(h);
        fastEnv_ += (a > fastEnv_ ? fastAtk_ : fastRel_) * (a - fastEnv_);
        slowEnv_ += (a > slowEnv_ ? slowAtk_ : slowRel_) * (a - slowEnv_);
        if (fastEnv_ > peakEnv_) peakEnv_ = fastEnv_;
        const float hfa = std::fabs(hf_.process(h));
        hfFast_ += (hfa > hfFast_ ? fastAtk_ : fastRel_) * (hfa - hfFast_);
        hfSlow_ += (hfa > hfSlow_ ? slowAtk_ : hfSlowRel_) * (hfa - hfSlow_);
        if (refractory_ > 0) --refractory_;

        bool onset = false;
        if (!gateOpen_) {
            if (fastEnv_ > gateLin_) { gateOpen_ = true; gateCloseCount_ = 0; onset = true; }
        } else {
            if (fastEnv_ < gateLin_ * 0.5f) {
                if (++gateCloseCount_ >= gateHoldSamples_) {
                    gateOpen_ = false;
                    noteActive_ = false;
                    pendingRetrigger_ = false;
                    onsetWindow_ = false;
                    candCount_ = 0;
                }
            } else {
                gateCloseCount_ = 0;
            }
            // Onset = broadband level jump OR a burst of high-frequency pick noise
            // (catches soft re-picks while the previous note is still loud).
            if (refractory_ == 0 && fastEnv_ > gateLin_ * 1.5f &&
                (fastEnv_ > 2.0f * slowEnv_ ||
                 (hfFast_ > 3.0f * hfSlow_ && hfFast_ > gateLin_ * 0.3f && hfFast_ > 0.08f * fastEnv_)))
                onset = true;
        }
        if (onset && refractory_ == 0 && (set_.retrigger || !noteActive_)) {
            refractory_ = refractorySamples_;
            sinceOnset_ = 0;
            onsetWindow_ = true;
            pendingRetrigger_ = noteActive_;
            candCount_ = 0;
            peakEnv_ = fastEnv_;
            onsetHold_ = static_cast<int>(0.012 * fs_);   // let the attack peak settle
        }
        // Re-arm the onset detector: the reference envelopes jump to the attack
        // level so only a *new* transient can trigger the next onset.
        if (onsetHold_ > 0 && --onsetHold_ == 0) {
            slowEnv_ = std::max(slowEnv_, fastEnv_);
            hfSlow_ = std::max(hfSlow_, hfFast_);
        }

        const float l = lp2_.process(lp1_.process(h));
        if (++decimPhase_ >= decim_) {
            decimPhase_ = 0;
            hist_[histW_] = l;
            hist_[histW_ + kHist] = l;
            histW_ = (histW_ + 1) & (kHist - 1);
            if (sinceOnset_ < (1 << 30)) ++sinceOnset_;
            if (++hopCount_ >= hop_) { hopCount_ = 0; analyse(); }
        }
    }
    const float d = peakEnv_ > 1e-6f ? clampf(fastEnv_ / peakEnv_, 0.0f, 1.0f) : 0.0f;
    dynSmooth_ += 0.3f * (d - dynSmooth_);
    dynNorm_ = noteActive_ ? dynSmooth_ : 0.0f;
}

void PitchTracker::analyse() {
    if (!gateOpen_) { lastEstHz_ = 0; return; }
    const int maxTauFull = std::min(kMaxTau, static_cast<int>(std::ceil(fsD_ / set_.lowestHz)) + 2);
    const int minTau = std::max(2, static_cast<int>(std::floor(fsD_ / set_.highestHz)));
    int maxTau = maxTauFull;
    int window = maxTauFull;
    bool provisional = false;
    if (!onsetWindow_ && noteActive_ && !pendingRetrigger_ && currentHz_ > 0.0f && (++fullSweep_ & 7) != 0) {
        // Locked on a note: only lags up to ~9 semitones below the current pitch
        // are needed to follow bends/vibrato/slides; a full sweep every 8th hop
        // still catches large legato jumps downwards.
        const int lim = static_cast<int>(std::ceil(fsD_ / currentHz_ * 1.7f)) + 4;
        if (lim < maxTauFull) { maxTau = window = std::max(lim, minTau + 6); }
    }
    if (onsetWindow_) {
        const int avail = sinceOnset_;
        if (avail >= 2 * maxTauFull) {
            onsetWindow_ = false;
        } else {
            if (avail < static_cast<int>(kMinOnsetMs * 0.001 * fsD_)) return;
            maxTau = static_cast<int>(avail * kOnsetLagFraction);
            if (maxTau > maxTauFull) maxTau = maxTauFull;
            window = avail - maxTau;
            if (maxTau < minTau + 3 || window < 8) return;
            provisional = true;
        }
    }
    ++analyses_;

    const int L = window + maxTau;
    const float* xp = &hist_[histW_ + kHist - L];

    // Cross-correlation r(tau) = sum_{j<W} x[j] x[j+tau]
#if defined(__APPLE__)
    vDSP_conv(xp, 1, xp, 1, corr_, 1, static_cast<vDSP_Length>(maxTau + 1), static_cast<vDSP_Length>(window));
#else
    for (int tau = 0; tau <= maxTau; ++tau) {
        // 8 independent accumulators so the compiler can vectorise
        float acc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        const float* b = xp + tau;
        int j = 0;
        for (; j + 8 <= window; j += 8)
            for (int k = 0; k < 8; ++k) acc[k] += xp[j + k] * b[j + k];
        float s = ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7]));
        for (; j < window; ++j) s += xp[j] * b[j];
        corr_[tau] = s;
    }
#endif
    // Prefix energies
    energy_[0] = 0.0f;
    for (int i = 0; i < L; ++i) energy_[i + 1] = energy_[i] + xp[i] * xp[i];
    const float e0 = energy_[window];
    if (e0 < 1e-10f) { lastEstHz_ = 0; return; }

    // Cumulative mean normalised difference
    cmnd_[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= maxTau; ++tau) {
        const float d = std::max(0.0f, e0 + (energy_[tau + window] - energy_[tau]) - 2.0f * corr_[tau]);
        running += d;
        cmnd_[tau] = running > 0.0f ? d * static_cast<float>(tau) / running : 1.0f;
    }

    // Provisional (short-window) estimates must be cleaner to be trusted.
    const float thr = provisional ? set_.threshold * 0.7f : set_.threshold;
    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau) {
        if (cmnd_[tau] < thr) {
            while (tau + 1 < maxTau && cmnd_[tau + 1] < cmnd_[tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0) {
        float mn = 1e9f;
        for (int tau = minTau; tau < maxTau; ++tau)
            if (cmnd_[tau] < mn) { mn = cmnd_[tau]; best = tau; }
    }
    if (best <= 0 || best >= maxTau) { lastEstHz_ = 0; onEstimate(false, 0); return; }
    // Sub-harmonic check: if the signal is much more periodic at 2x/3x/4x the lag,
    // the first dip was a strong overtone (weak fundamental, typical for pickups).
    for (int k = 2; k <= 4 && !provisional && cmnd_[best] > 0.04f; ++k) {
        const int c = best * k;
        if (c + 2 >= maxTau) continue;
        int bi = c; float bv = cmnd_[c];
        for (int t = c - 2; t <= c + 2; ++t) if (cmnd_[t] < bv) { bv = cmnd_[t]; bi = t; }
        if (bv < cmnd_[best] * 0.25f) {
            while (bi + 1 < maxTau && cmnd_[bi + 1] < cmnd_[bi]) ++bi;
            while (bi - 1 > minTau && cmnd_[bi - 1] < cmnd_[bi]) --bi;
            best = bi;
            break;
        }
    }
    // Short post-onset window: guard against the classic YIN octave-low error by
    // accepting a clean dip at half the lag (normal threshold) if there is one.
    if (provisional && best / 2 >= minTau + 2) {
        const int h = best / 2;
        int bi = h; float bv = cmnd_[h];
        for (int t = h - 2; t <= h + 2; ++t) if (cmnd_[t] < bv) { bv = cmnd_[t]; bi = t; }
        if (bv < set_.threshold) {
            while (bi + 1 < maxTau && cmnd_[bi + 1] < cmnd_[bi]) ++bi;
            while (bi - 1 > minTau && cmnd_[bi - 1] < cmnd_[bi]) --bi;
            best = bi;
        }
    }
    // In the growing post-onset window, a dip at the very edge is not trustworthy.
    if (provisional && best > static_cast<int>(maxTau * 0.92f)) return;

    const float a = cmnd_[best - 1], b = cmnd_[best], c = cmnd_[best + 1];
    float t = static_cast<float>(best);
    const float den = a - 2.0f * b + c;
    if (den > 1e-9f) t += clampf(0.5f * (a - c) / den, -0.5f, 0.5f);
    const float hz = static_cast<float>(fsD_) / t;
    lastAp_ = b;
    lastEstHz_ = hz;
    onEstimate(b < thr, hz);
}

void PitchTracker::onEstimate(bool valid, float hz) {
    if (!valid) return;
    const float m = hzToMidi(hz);
    if (candCount_ > 0 && std::fabs(m - candMidi_) < 0.5f) {
        ++candCount_;
    } else {
        candCount_ = 1;
    }
    candMidi_ = m;
    const bool stable = candCount_ >= set_.stability;

    if (!noteActive_) { if (stable) noteOn(hz); return; }
    if (pendingRetrigger_) { if (stable) noteOn(hz); return; }
    if (std::fabs(m - currentMidi_) < 0.7f || stable) {
        currentHz_ = hz;          // bends / vibrato / slides / legato jumps
        currentMidi_ = m;
    }
}

void PitchTracker::noteOn(float hz) {
    noteActive_ = true;
    pendingRetrigger_ = false;
    ++noteCounter_;
    currentHz_ = hz;
    currentMidi_ = hzToMidi(hz);
    velocity_ = clampf((linToDb(peakEnv_) - set_.gateDb) / 36.0f, 0.05f, 1.0f);
    dynSmooth_ = 1.0f;
}

} // namespace gsx
