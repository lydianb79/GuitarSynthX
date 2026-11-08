// GSXCore.cpp — C API glue between Swift and the C++ engine.
#include "include/GSXCore.h"
#include "Engine.hpp"
#include "AudioIO.hpp"
#include <complex>
#include <cstdio>
#include <mutex>
#include <string>

using namespace gsx;

namespace {

Engine& engine() { static Engine e; return e; }
AudioIO& io() { static AudioIO a; return a; }
std::mutex& controlMutex() { static std::mutex m; return m; }

void copyStr(char* dst, size_t cap, const char* src) {
    std::snprintf(dst, cap, "%s", src ? src : "");
}

// ---- visual analysis (UI thread only)
constexpr int kFFT = 2048;

void fftRadix2(std::complex<float>* a, int n) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const float ang = -kTwoPi / static_cast<float>(len);
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                const std::complex<float> u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

} // namespace

extern "C" {

int32_t gsx_device_count(void) { return AudioIO::deviceCount(); }

int32_t gsx_device_info(int32_t index, GSXDeviceInfo* out) {
    if (!out) return 0;
    return AudioIO::deviceInfo(index, *out) ? 1 : 0;
}

int32_t gsx_device_sample_rates(uint32_t deviceID, double* outRates, int32_t maxRates) {
    if (!outRates || maxRates <= 0) return 0;
    return AudioIO::sampleRates(deviceID, outRates, maxRates);
}

int32_t gsx_device_buffer_range(uint32_t deviceID, int32_t* minFrames, int32_t* maxFrames) {
    int mn = 0, mx = 0;
    if (!AudioIO::bufferRange(deviceID, mn, mx)) return 0;
    if (minFrames) *minFrames = mn;
    if (maxFrames) *maxFrames = mx;
    return 1;
}

int32_t gsx_start(const GSXAudioConfig* config, char* errBuf, int32_t errLen) {
    std::lock_guard<std::mutex> lock(controlMutex());
    if (!config) return 0;
    std::string err;
    const bool ok = io().start(*config, engine(), err);
    if (!ok && errBuf && errLen > 0) copyStr(errBuf, static_cast<size_t>(errLen), err.c_str());
    return ok ? 1 : 0;
}

void gsx_stop(void) {
    std::lock_guard<std::mutex> lock(controlMutex());
    io().stop();
}

int32_t gsx_is_running(void) { return io().running() ? 1 : 0; }

void gsx_panic(void) { engine().requestPanic(); }

int32_t gsx_param_count(void) { return P_COUNT; }

int32_t gsx_param_info(int32_t id, GSXParamInfo* out) {
    if (!out || id < 0 || id >= P_COUNT) return 0;
    const ParamDef& d = paramDef(id);
    std::memset(out, 0, sizeof(*out));
    copyStr(out->key, sizeof(out->key), d.key);
    copyStr(out->name, sizeof(out->name), d.name);
    copyStr(out->group, sizeof(out->group), d.group);
    copyStr(out->unit, sizeof(out->unit), d.unit);
    copyStr(out->choices, sizeof(out->choices), d.choices);
    out->minValue = d.minV;
    out->maxValue = d.maxV;
    out->defaultValue = d.defV;
    out->type = d.type;
    out->scale = d.scale;
    return 1;
}

void gsx_set_param(int32_t id, float value) { engine().setParam(id, value); }

float gsx_get_param(int32_t id) {
    if (id < 0 || id >= P_COUNT) return 0.0f;
    return engine().getParam(id);
}

void gsx_get_status(GSXStatus* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    AudioIO& a = io();
    Engine& e = engine();
    out->running = a.running() ? 1 : 0;
    out->sampleRate = a.sampleRate();
    out->bufferFrames = a.bufferFrames();
    out->inputChannels = a.inputChannels();
    out->outputChannels = a.outputChannels();
    out->duplex = a.duplex() ? 1 : 0;
    out->inputPeak = e.inputPeak.load(std::memory_order_relaxed);
    out->outputPeak = e.outputPeak.load(std::memory_order_relaxed);
    out->cpuLoad = a.cpuLoad.load(std::memory_order_relaxed);
    out->overloads = a.overloads.load(std::memory_order_relaxed);
    out->inputLatencyMs = a.inputLatencyMs();
    out->outputLatencyMs = a.outputLatencyMs();
    out->activeVoices = e.activeVoices.load(std::memory_order_relaxed);
    out->trackerCount = e.trackerCount.load(std::memory_order_relaxed);
}

int32_t gsx_get_tracker(int32_t index, GSXTrackerStatus* out) {
    if (!out || index < 0 || index >= Engine::kMaxTrackers) return 0;
    const Engine::TrackerMeter& m = engine().trackerMeters[index];
    out->frequency = m.hz.load(std::memory_order_relaxed);
    out->midi = m.midi.load(std::memory_order_relaxed);
    out->aperiodicity = m.ap.load(std::memory_order_relaxed);
    out->levelDb = m.levelDb.load(std::memory_order_relaxed);
    out->gate = m.gate.load(std::memory_order_relaxed);
    out->noteActive = m.active.load(std::memory_order_relaxed);
    return 1;
}

int32_t gsx_read_visual(float* scope, int32_t scopeCount, float* spectrum, int32_t bins) {
    static float buf[4096];
    static float window[kFFT];
    static bool winInit = false;
    static std::complex<float> fft[kFFT];
    if (!winInit) {
        for (int i = 0; i < kFFT; ++i) window[i] = 0.5f - 0.5f * std::cos(kTwoPi * i / (kFFT - 1));
        winInit = true;
    }
    Engine& e = engine();
    e.readScope(buf, 4096);

    if (scope && scopeCount > 0) {
        const int n = std::min(scopeCount, 2048);
        // latest rising zero crossing that still leaves n samples after it
        int start = 4096 - n;
        for (int i = 4096 - n; i > 4096 - n - 1536 && i > 1; --i) {
            if (buf[i - 1] < 0.0f && buf[i] >= 0.0f) { start = i; break; }
        }
        for (int i = 0; i < n; ++i) scope[i] = buf[start + i];
        for (int i = n; i < scopeCount; ++i) scope[i] = 0.0f;
    }

    if (spectrum && bins > 0) {
        for (int i = 0; i < kFFT; ++i) fft[i] = std::complex<float>(buf[4096 - kFFT + i] * window[i], 0.0f);
        fftRadix2(fft, kFFT);
        const double fs = e.sampleRate() > 0 ? e.sampleRate() : 48000.0;
        const float fMin = 30.0f, fMax = static_cast<float>(std::min(20000.0, fs * 0.5));
        const float binHz = static_cast<float>(fs / kFFT);
        const float norm = 4.0f / kFFT;
        for (int b = 0; b < bins; ++b) {
            const float f0 = fMin * std::pow(fMax / fMin, static_cast<float>(b) / bins);
            const float f1 = fMin * std::pow(fMax / fMin, static_cast<float>(b + 1) / bins);
            int k0 = static_cast<int>(f0 / binHz), k1 = static_cast<int>(f1 / binHz);
            k0 = std::max(1, std::min(k0, kFFT / 2 - 1));
            k1 = std::max(k0, std::min(k1, kFFT / 2 - 1));
            float mag = 0.0f;
            for (int k = k0; k <= k1; ++k) mag = std::max(mag, std::abs(fft[k]) * norm);
            spectrum[b] = clampf((linToDb(mag) + 84.0f) / 84.0f, 0.0f, 1.0f);
        }
    }
    return 1;
}

} // extern "C"
