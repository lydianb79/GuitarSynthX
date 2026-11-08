// PitchTracker.hpp — low-latency monophonic pitch & note tracker for one string
// (or for a whole guitar in mono mode).
//
// Design for latency:
//  * Band-limit + decimate to ~24 kHz so the YIN difference function is cheap
//    enough to evaluate every 0.5–2 ms.
//  * Onset-synchronous adaptive window: after a pick attack the analysis window
//    only contains samples *after* the attack and grows with time, so high notes
//    are recognised after ~2 periods instead of waiting for a fixed long window.
//  * Correlation via vDSP_conv (Accelerate) on Apple platforms.
#pragma once
#include "DSPUtil.hpp"

namespace gsx {

struct TrackerSettings {
    float lowestHz   = 75.0f;
    float highestHz  = 1400.0f;
    float threshold  = 0.15f;  // YIN aperiodicity threshold
    int   stability  = 2;      // consecutive agreeing estimates before a note change
    float hopMs      = 1.0f;
    float gateDb     = -54.0f;
    bool  retrigger  = true;
};

class PitchTracker {
public:
    void prepare(double sampleRate);
    void reset();
    void setSettings(const TrackerSettings& s);
    /// Offsets the analysis hop so several trackers don't analyse in the same block.
    void setHopPhase(float fraction) { hopPhase_ = fraction; hopCount_ = static_cast<int>(fraction * hop_); }

    /// Feed `n` input samples (full sample rate). Updates the public state.
    void process(const float* x, int n);

    // ---- state read by the engine after each process() call
    bool  noteActive() const      { return noteActive_; }
    uint32_t noteCounter() const  { return noteCounter_; } // increments on every note-on/retrigger
    float frequency() const       { return currentHz_; }   // tracked fundamental (Hz)
    float velocity() const        { return velocity_; }    // 0..1 from pick strength
    float dynamics() const        { return dynNorm_; }     // envelope relative to attack peak 0..~1.2
    float levelDb() const         { return linToDb(fastEnv_); }
    bool  gateOpen() const        { return gateOpen_; }
    float lastAperiodicity() const{ return lastAp_; }
    float lastEstimateHz() const  { return lastEstHz_; }

    // statistics for testing
    uint64_t analyses() const { return analyses_; }

private:
    void analyse();
    bool yin(int maxTau, float& outHz, float& outAp);
    void onEstimate(bool valid, float hz);
    void noteOn(float hz);

    double fs_ = 48000.0;
    double fsD_ = 24000.0;
    int decim_ = 2;
    int decimPhase_ = 0;
    TrackerSettings set_;

    // filters
    Biquad hp_, lp1_, lp2_, hf_;
    float hfFast_ = 0, hfSlow_ = 0, hfSlowRel_ = 0;
    float hopPhase_ = 0;
    int   onsetHold_ = 0;
    uint32_t fullSweep_ = 0;
    // envelope
    float fastEnv_ = 0, slowEnv_ = 0;
    float fastAtk_ = 0, fastRel_ = 0, slowAtk_ = 0, slowRel_ = 0;
    float gateLin_ = 0;
    bool  gateOpen_ = false;
    int   gateCloseCount_ = 0, gateHoldSamples_ = 0;
    int   refractory_ = 0, refractorySamples_ = 0;

    // decimated history (mirrored ring for contiguous reads)
    static constexpr int kHist = 4096;
    float hist_[kHist * 2] = {};
    int   histW_ = 0;
    int   hop_ = 24, hopCount_ = 0;
    int   sinceOnset_ = 1 << 30;   // decimated samples since last onset
    bool  onsetWindow_ = false;

    // YIN scratch (fixed size, no allocation)
    static constexpr int kMaxTau = 1600;
    float corr_[kMaxTau + 2] = {};
    float cmnd_[kMaxTau + 2] = {};
    float energy_[kMaxTau * 2 + 4] = {};

    // note state
    bool  noteActive_ = false;
    uint32_t noteCounter_ = 0;
    float currentHz_ = 0, currentMidi_ = 0;
    float candMidi_ = 0; int candCount_ = 0;
    bool  pendingRetrigger_ = false;
    float peakEnv_ = 0;
    float velocity_ = 0, dynNorm_ = 0, dynSmooth_ = 0;
    float lastAp_ = 1, lastEstHz_ = 0;
    uint64_t analyses_ = 0;
};

} // namespace gsx
