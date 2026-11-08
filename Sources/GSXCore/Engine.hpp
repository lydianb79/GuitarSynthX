// Engine.hpp — the real-time DSP graph. Platform independent (no Core Audio),
// so it can be unit-tested and benchmarked off-device.
#pragma once
#include <atomic>
#include "Params.hpp"
#include "PitchTracker.hpp"
#include "Synth.hpp"
#include "Effects.hpp"

namespace gsx {

class Engine {
public:
    static constexpr int kMaxTrackers = 6;
    static constexpr int kMaxVoices = 12;
    static constexpr int kMaxBlock = 4096;
    static constexpr int kSubBlock = 16;      // control-rate granularity (samples)
    static constexpr int kScopeSize = 8192;

    Engine();
    /// Allocates everything. Must be called while audio is stopped.
    void prepare(double sampleRate, int numInputChannels);
    /// Real-time safe. `in` has numInputChannels channel pointers (may be null).
    void process(const float* const* in, int numIn, float* outL, float* outR, int frames);
    void requestPanic() { panic_.store(true, std::memory_order_release); }

    // parameters (thread safe)
    void  setParam(int id, float v);
    float getParam(int id) const { return params_[id].load(std::memory_order_relaxed); }

    // meters (written by audio thread, read by UI)
    std::atomic<float> inputPeak{0}, outputPeak{0};
    std::atomic<int>   activeVoices{0};
    std::atomic<int>   trackerCount{1};
    struct TrackerMeter {
        std::atomic<float> hz{0}, midi{0}, ap{1}, levelDb{-120};
        std::atomic<int> gate{0}, active{0};
    };
    TrackerMeter trackerMeters[kMaxTrackers];

    /// Copies the most recent `n` output samples (mono) into dst. UI thread.
    void readScope(float* dst, int n) const;
    double sampleRate() const { return fs_; }

    // for tests
    const PitchTracker& tracker(int i) const { return trackers_[i]; }

private:
    void updateParams();
    void handleNotes();
    void killAll();
    int  allocVoice();
    float lfoValue(int n);

    double fs_ = 48000.0;
    int numIn_ = 1;
    std::atomic<float> params_[P_COUNT];
    std::atomic<bool> panic_{false};

    PitchTracker trackers_[kMaxTrackers];
    Voice voices_[kMaxVoices];
    int trackerVoice_[kMaxTrackers];
    uint32_t trackerCounter_[kMaxTrackers];
    float qNote_[kMaxTrackers];
    float lastMidi_[kMaxTrackers];
    bool  hexMode_ = false;
    int   numTrackers_ = 1;

    VoiceParams vp_;
    ADSR ampProto_, fltProto_;
    TrackerSettings tset_;
    int pitchMode_ = 1;
    float transpose_ = 0;
    int inChannel_ = 0;
    float inGain_ = 1.0f;
    float synthGain_ = 0.5f, dryGain_ = 0, masterGain_ = 1;
    float synthGainS_ = 0.5f, dryGainS_ = 0, masterGainS_ = 1;
    bool drvOn_ = false, choOn_ = false, phaOn_ = false, dlyOn_ = false, revOn_ = false, limOn_ = true, dryFx_ = true;

    // LFO
    int lfoWave_ = 0;
    float lfoRate_ = 5, lfoPhase_ = 0, lfoSH_ = 0;
    Noise lfoNoise_;

    Drive drive_;
    Chorus chorus_;
    Phaser phaser_;
    StereoDelay delay_;
    Reverb reverb_;
    Limiter limiter_;

    float trkBuf_[kMaxTrackers][kSubBlock];
    float dryBuf_[kMaxBlock];
    uint64_t stamp_ = 0;

    float scope_[kScopeSize] = {};
    std::atomic<uint32_t> scopeW_{0};
    float inPeakHold_ = 0, outPeakHold_ = 0;
};

} // namespace gsx
