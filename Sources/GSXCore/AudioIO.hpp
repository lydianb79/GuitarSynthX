// AudioIO.hpp — Core Audio HAL (AUHAL) I/O for macOS.
//
// Two paths:
//  * Same input & output device → ONE AUHAL unit in full duplex. Input is pulled
//    inside the output render callback, so the round trip is exactly
//    hardware latency + 2 × buffer. This is the lowest-latency configuration.
//  * Different devices → two AUHAL units bridged by a lock-free SPSC ring with
//    bounded-latency drift control (clocks of two devices are never in sync).
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "include/GSXCore.h"

namespace gsx {

class Engine;

class AudioIO {
public:
    ~AudioIO() { stop(); }
    bool start(const GSXAudioConfig& cfg, Engine& engine, std::string& error);
    void stop();
    bool running() const { return running_; }

    double sampleRate() const { return fs_; }
    int bufferFrames() const { return buffer_; }
    int inputChannels() const { return inCh_; }
    int outputChannels() const { return outCh_; }
    bool duplex() const { return duplex_; }
    float inputLatencyMs() const { return inLatMs_; }
    float outputLatencyMs() const { return outLatMs_; }

    std::atomic<float> cpuLoad{0};
    std::atomic<uint32_t> overloads{0};

    // device helpers (static, main thread)
    static int deviceCount();
    static bool deviceInfo(int index, GSXDeviceInfo& out);
    static int sampleRates(uint32_t dev, double* out, int maxCount);
    static bool bufferRange(uint32_t dev, int& minF, int& maxF);

#if defined(__APPLE__)
    // callbacks need access; public for the C trampolines
    void* outUnit_ = nullptr;
    void* inUnit_ = nullptr;
    void renderDuplex(void* flags, const void* ts, uint32_t frames, void* ioData);
    void renderOutput(void* flags, const void* ts, uint32_t frames, void* ioData);
    void captureInput(void* flags, const void* ts, uint32_t frames);
#endif

private:
    void processAndWrite(const float* const* in, int numIn, uint32_t frames, void* ioData);

    Engine* engine_ = nullptr;
    bool running_ = false;
    bool duplex_ = true;
    double fs_ = 48000;
    int buffer_ = 64;
    int inCh_ = 0, outCh_ = 0;
    float inLatMs_ = 0, outLatMs_ = 0;
    uint32_t inDev_ = 0, outDev_ = 0;

    // input buffers
    std::vector<float> inStore_;
    std::vector<const float*> inPtrs_;
    void* inABL_ = nullptr;          // AudioBufferList*
    std::vector<float> scratchL_, scratchR_;

    // SPSC ring (separate devices)
    static constexpr int kRingFrames = 16384;
    std::vector<float> ring_;        // [channel][kRingFrames]
    std::atomic<uint64_t> ringW_{0}, ringR_{0};
    std::vector<float> ringOut_;
    std::vector<const float*> ringPtrs_;
    bool primed_ = false;

    double ticksToNs_ = 1.0;
    float cpuPeak_ = 0;
};

} // namespace gsx
