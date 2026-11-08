// AudioIO.cpp — Core Audio HAL implementation (macOS only).
#include "AudioIO.hpp"
#include "Engine.hpp"
#include "Params.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <algorithm>

#if defined(__APPLE__)
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <mach/mach_time.h>
#include <unistd.h>

namespace gsx {

// ------------------------------------------------------------ HAL helpers
static AudioObjectPropertyAddress addr(AudioObjectPropertySelector s,
                                       AudioObjectPropertyScope sc = kAudioObjectPropertyScopeGlobal) {
    return AudioObjectPropertyAddress{s, sc, kAudioObjectPropertyElementMain};
}

static std::vector<AudioDeviceID> allDevices() {
    std::vector<AudioDeviceID> ids;
    auto a = addr(kAudioHardwarePropertyDevices);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size) != noErr) return ids;
    ids.resize(size / sizeof(AudioDeviceID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data()) != noErr) ids.clear();
    return ids;
}

static int channelCount(AudioDeviceID dev, bool input) {
    auto a = addr(kAudioDevicePropertyStreamConfiguration,
                  input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, nullptr, &size) != noErr || size == 0) return 0;
    std::vector<uint8_t> mem(size);
    auto* abl = reinterpret_cast<AudioBufferList*>(mem.data());
    if (AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, abl) != noErr) return 0;
    int n = 0;
    for (UInt32 i = 0; i < abl->mNumberBuffers; ++i) n += static_cast<int>(abl->mBuffers[i].mNumberChannels);
    return n;
}

static void cfToC(CFStringRef s, char* out, int len) {
    out[0] = 0;
    if (!s) return;
    CFStringGetCString(s, out, len, kCFStringEncodingUTF8);
}

static double nominalRate(AudioDeviceID dev) {
    auto a = addr(kAudioDevicePropertyNominalSampleRate);
    Float64 r = 0; UInt32 size = sizeof(r);
    AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, &r);
    return r;
}

static AudioDeviceID defaultDevice(bool input) {
    auto a = addr(input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice);
    AudioDeviceID d = 0; UInt32 size = sizeof(d);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, &d);
    return d;
}

static bool setNominalRate(AudioDeviceID dev, double rate) {
    if (std::fabs(nominalRate(dev) - rate) < 0.5) return true;
    auto a = addr(kAudioDevicePropertyNominalSampleRate);
    Float64 r = rate;
    if (AudioObjectSetPropertyData(dev, &a, 0, nullptr, sizeof(r), &r) != noErr) return false;
    for (int i = 0; i < 100; ++i) {           // the change is asynchronous
        if (std::fabs(nominalRate(dev) - rate) < 0.5) return true;
        usleep(10000);
    }
    return std::fabs(nominalRate(dev) - rate) < 0.5;
}

static int setBufferFrames(AudioDeviceID dev, int frames) {
    int mn = 16, mx = 4096;
    AudioIO::bufferRange(dev, mn, mx);
    UInt32 f = static_cast<UInt32>(std::max(mn, std::min(mx, frames)));
    auto a = addr(kAudioDevicePropertyBufferFrameSize);
    AudioObjectSetPropertyData(dev, &a, 0, nullptr, sizeof(f), &f);
    UInt32 got = 0, size = sizeof(got);
    AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, &got);
    return static_cast<int>(got);
}

static UInt32 getU32(AudioObjectID obj, AudioObjectPropertySelector s, AudioObjectPropertyScope sc) {
    auto a = addr(s, sc);
    UInt32 v = 0, size = sizeof(v);
    if (AudioObjectGetPropertyData(obj, &a, 0, nullptr, &size, &v) != noErr) return 0;
    return v;
}

static int deviceLatencyFrames(AudioDeviceID dev, bool input) {
    const AudioObjectPropertyScope sc = input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
    int frames = static_cast<int>(getU32(dev, kAudioDevicePropertyLatency, sc));
    frames += static_cast<int>(getU32(dev, kAudioDevicePropertySafetyOffset, sc));
    auto a = addr(kAudioDevicePropertyStreams, sc);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, nullptr, &size) == noErr && size >= sizeof(AudioStreamID)) {
        std::vector<AudioStreamID> streams(size / sizeof(AudioStreamID));
        if (AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, streams.data()) == noErr)
            frames += static_cast<int>(getU32(streams[0], kAudioStreamPropertyLatency, kAudioObjectPropertyScopeGlobal));
    }
    return frames;
}

static OSStatus overloadListener(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* user) {
    static_cast<AudioIO*>(user)->overloads.fetch_add(1, std::memory_order_relaxed);
    return noErr;
}

// ------------------------------------------------------------ device API
int AudioIO::deviceCount() { return static_cast<int>(allDevices().size()); }

bool AudioIO::deviceInfo(int index, GSXDeviceInfo& out) {
    auto ids = allDevices();
    if (index < 0 || index >= static_cast<int>(ids.size())) return false;
    const AudioDeviceID dev = ids[index];
    std::memset(&out, 0, sizeof(out));
    out.deviceID = dev;
    CFStringRef name = nullptr, uid = nullptr;
    UInt32 size = sizeof(CFStringRef);
    auto an = addr(kAudioObjectPropertyName);
    if (AudioObjectGetPropertyData(dev, &an, 0, nullptr, &size, &name) == noErr && name) { cfToC(name, out.name, sizeof(out.name)); CFRelease(name); }
    size = sizeof(CFStringRef);
    auto au = addr(kAudioDevicePropertyDeviceUID);
    if (AudioObjectGetPropertyData(dev, &au, 0, nullptr, &size, &uid) == noErr && uid) { cfToC(uid, out.uid, sizeof(out.uid)); CFRelease(uid); }
    out.inputChannels = channelCount(dev, true);
    out.outputChannels = channelCount(dev, false);
    out.nominalSampleRate = nominalRate(dev);
    out.isDefaultInput = dev == defaultDevice(true);
    out.isDefaultOutput = dev == defaultDevice(false);
    return true;
}

int AudioIO::sampleRates(uint32_t dev, double* out, int maxCount) {
    auto a = addr(kAudioDevicePropertyAvailableNominalSampleRates);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(dev, &a, 0, nullptr, &size) != noErr || size == 0) return 0;
    std::vector<AudioValueRange> ranges(size / sizeof(AudioValueRange));
    if (AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, ranges.data()) != noErr) return 0;
    static const double kStd[] = {44100, 48000, 88200, 96000, 176400, 192000};
    int n = 0;
    for (double r : kStd) {
        for (auto& rg : ranges) {
            if (r >= rg.mMinimum - 0.5 && r <= rg.mMaximum + 0.5) {
                if (n < maxCount) out[n++] = r;
                break;
            }
        }
    }
    return n;
}

bool AudioIO::bufferRange(uint32_t dev, int& minF, int& maxF) {
    auto a = addr(kAudioDevicePropertyBufferFrameSizeRange);
    AudioValueRange r{}; UInt32 size = sizeof(r);
    if (AudioObjectGetPropertyData(dev, &a, 0, nullptr, &size, &r) != noErr) return false;
    minF = static_cast<int>(r.mMinimum);
    maxF = static_cast<int>(r.mMaximum);
    return true;
}

// ------------------------------------------------------------ callbacks
static OSStatus duplexCallback(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                               UInt32, UInt32 frames, AudioBufferList* ioData) {
    static_cast<AudioIO*>(ref)->renderDuplex(flags, ts, frames, ioData);
    return noErr;
}
static OSStatus outputCallback(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                               UInt32, UInt32 frames, AudioBufferList* ioData) {
    static_cast<AudioIO*>(ref)->renderOutput(flags, ts, frames, ioData);
    return noErr;
}
static OSStatus inputCallback(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                              UInt32, UInt32 frames, AudioBufferList*) {
    static_cast<AudioIO*>(ref)->captureInput(flags, ts, frames);
    return noErr;
}

static void prepareABL(AudioBufferList* abl, int channels, float* store, uint32_t frames) {
    abl->mNumberBuffers = static_cast<UInt32>(channels);
    for (int c = 0; c < channels; ++c) {
        abl->mBuffers[c].mNumberChannels = 1;
        abl->mBuffers[c].mDataByteSize = frames * sizeof(float);
        abl->mBuffers[c].mData = store + static_cast<size_t>(c) * Engine::kMaxBlock;
    }
}

void AudioIO::processAndWrite(const float* const* in, int numIn, uint32_t frames, void* ioDataV) {
    auto* io = static_cast<AudioBufferList*>(ioDataV);
    const int nb = static_cast<int>(io->mNumberBuffers);
    int ch = static_cast<int>(engine_->getParam(P_OUT_CHANNEL)) - 1;
    if (ch < 0 || ch >= nb) ch = 0;
    float* L = (nb > ch && io->mBuffers[ch].mData) ? static_cast<float*>(io->mBuffers[ch].mData) : scratchL_.data();
    float* R = (nb > ch + 1 && io->mBuffers[ch + 1].mData) ? static_cast<float*>(io->mBuffers[ch + 1].mData) : scratchR_.data();
    engine_->process(in, numIn, L, R, static_cast<int>(frames));
    if (nb == 1) {                           // mono output device: sum
        for (uint32_t i = 0; i < frames; ++i) L[i] = 0.5f * (L[i] + R[i]);
    }
    for (int b = 0; b < nb; ++b) {
        if (b == ch || b == ch + 1 || !io->mBuffers[b].mData) continue;
        std::memset(io->mBuffers[b].mData, 0, io->mBuffers[b].mDataByteSize);
    }
}

void AudioIO::renderDuplex(void* flagsV, const void* tsV, uint32_t frames, void* ioData) {
    const uint64_t t0 = mach_absolute_time();
    auto* flags = static_cast<AudioUnitRenderActionFlags*>(flagsV);
    auto* ts = static_cast<const AudioTimeStamp*>(tsV);
    if (frames > static_cast<uint32_t>(Engine::kMaxBlock)) frames = Engine::kMaxBlock;
    auto* abl = static_cast<AudioBufferList*>(inABL_);
    int numIn = 0;
    if (inCh_ > 0) {
        prepareABL(abl, inCh_, inStore_.data(), frames);
        if (AudioUnitRender(static_cast<AudioUnit>(outUnit_), flags, ts, 1, frames, abl) == noErr) numIn = inCh_;
    }
    processAndWrite(numIn ? inPtrs_.data() : nullptr, numIn, frames, ioData);
    const float load = static_cast<float>((mach_absolute_time() - t0) * ticksToNs_ / (frames / fs_ * 1e9));
    cpuPeak_ = std::max(load, cpuPeak_ * 0.997f);
    cpuLoad.store(cpuPeak_, std::memory_order_relaxed);
}

void AudioIO::captureInput(void* flagsV, const void* tsV, uint32_t frames) {
    auto* flags = static_cast<AudioUnitRenderActionFlags*>(flagsV);
    auto* ts = static_cast<const AudioTimeStamp*>(tsV);
    if (frames > static_cast<uint32_t>(Engine::kMaxBlock)) frames = Engine::kMaxBlock;
    auto* abl = static_cast<AudioBufferList*>(inABL_);
    prepareABL(abl, inCh_, inStore_.data(), frames);
    if (AudioUnitRender(static_cast<AudioUnit>(inUnit_), flags, ts, 1, frames, abl) != noErr) return;
    const uint64_t w = ringW_.load(std::memory_order_relaxed);
    const uint64_t r = ringR_.load(std::memory_order_acquire);
    if (w - r + frames > static_cast<uint64_t>(kRingFrames)) {   // overrun: consumer stalled
        overloads.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    for (int c = 0; c < inCh_; ++c) {
        float* dst = ring_.data() + static_cast<size_t>(c) * kRingFrames;
        const float* src = inStore_.data() + static_cast<size_t>(c) * Engine::kMaxBlock;
        for (uint32_t i = 0; i < frames; ++i) dst[(w + i) & (kRingFrames - 1)] = src[i];
    }
    ringW_.store(w + frames, std::memory_order_release);
}

void AudioIO::renderOutput(void*, const void*, uint32_t frames, void* ioData) {
    const uint64_t t0 = mach_absolute_time();
    if (frames > static_cast<uint32_t>(Engine::kMaxBlock)) frames = Engine::kMaxBlock;
    const uint64_t w = ringW_.load(std::memory_order_acquire);
    uint64_t r = ringR_.load(std::memory_order_relaxed);
    uint64_t avail = w - r;
    // Bound latency: if the input clock runs fast, drop the excess.
    const uint64_t maxQueue = static_cast<uint64_t>(frames) * 2 + static_cast<uint64_t>(buffer_) * 2;
    if (avail > maxQueue + frames) { r = w - (maxQueue); avail = w - r; }
    if (!primed_ && avail >= frames) primed_ = true;
    const uint32_t take = static_cast<uint32_t>(std::min<uint64_t>(avail, frames));
    if (primed_ && take < frames) overloads.fetch_add(1, std::memory_order_relaxed);
    for (int c = 0; c < inCh_; ++c) {
        const float* src = ring_.data() + static_cast<size_t>(c) * kRingFrames;
        float* dst = ringOut_.data() + static_cast<size_t>(c) * Engine::kMaxBlock;
        uint32_t i = 0;
        for (; i < take; ++i) dst[i] = src[(r + i) & (kRingFrames - 1)];
        for (; i < frames; ++i) dst[i] = 0.0f;
    }
    ringR_.store(r + take, std::memory_order_release);
    processAndWrite(inCh_ ? ringPtrs_.data() : nullptr, inCh_, frames, ioData);
    const float load = static_cast<float>((mach_absolute_time() - t0) * ticksToNs_ / (frames / fs_ * 1e9));
    cpuPeak_ = std::max(load, cpuPeak_ * 0.997f);
    cpuLoad.store(cpuPeak_, std::memory_order_relaxed);
}

// ------------------------------------------------------------ AUHAL setup
static AudioUnit makeHAL(AudioDeviceID dev, bool enableIn, bool enableOut, std::string& err) {
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) { err = "AUHAL component not found"; return nullptr; }
    AudioUnit unit = nullptr;
    if (AudioComponentInstanceNew(comp, &unit) != noErr || !unit) { err = "Cannot create AUHAL"; return nullptr; }
    UInt32 in = enableIn ? 1 : 0, out = enableOut ? 1 : 0;
    OSStatus s = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &in, sizeof(in));
    s |= AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &out, sizeof(out));
    if (s != noErr) { err = "Cannot configure AUHAL I/O"; AudioComponentInstanceDispose(unit); return nullptr; }
    AudioDeviceID d = dev;
    if (AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &d, sizeof(d)) != noErr) {
        err = "Cannot select audio device"; AudioComponentInstanceDispose(unit); return nullptr;
    }
    UInt32 maxFrames = Engine::kMaxBlock;
    AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof(maxFrames));
    return unit;
}

static AudioStreamBasicDescription floatFormat(double fs, int channels) {
    AudioStreamBasicDescription f{};
    f.mSampleRate = fs;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagsNativeEndian;
    f.mBytesPerPacket = sizeof(float);
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = sizeof(float);
    f.mChannelsPerFrame = static_cast<UInt32>(channels);
    f.mBitsPerChannel = 32;
    return f;
}

bool AudioIO::start(const GSXAudioConfig& cfg, Engine& engine, std::string& err) {
    stop();
    engine_ = &engine;
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    ticksToNs_ = static_cast<double>(tb.numer) / tb.denom;

    inDev_ = cfg.inputDeviceID;   // 0 = no input (output only)
    outDev_ = cfg.outputDeviceID ? cfg.outputDeviceID : defaultDevice(false);
    if (!outDev_) { err = "No output device"; return false; }
    duplex_ = (inDev_ == outDev_);
    inCh_ = inDev_ ? std::min(channelCount(inDev_, true), 64) : 0;
    outCh_ = channelCount(outDev_, false);
    if (outCh_ <= 0) { err = "Selected output device has no output channels"; return false; }

    // sample rate
    if (cfg.sampleRate > 0) {
        if (!setNominalRate(outDev_, cfg.sampleRate)) { err = "Output device does not accept the sample rate"; return false; }
        if (inDev_ && !duplex_ && !setNominalRate(inDev_, cfg.sampleRate)) { err = "Input device does not accept the sample rate"; return false; }
    }
    fs_ = nominalRate(outDev_);
    if (inDev_ && !duplex_ && std::fabs(nominalRate(inDev_) - fs_) > 0.5) {
        err = "Input and output devices run at different sample rates"; return false;
    }

    // buffer size (frames)
    buffer_ = setBufferFrames(outDev_, cfg.bufferFrames);
    if (inDev_ && !duplex_) setBufferFrames(inDev_, cfg.bufferFrames);

    // buffers
    const int inChAlloc = std::max(inCh_, 1);
    inStore_.assign(static_cast<size_t>(inChAlloc) * Engine::kMaxBlock, 0.0f);
    inPtrs_.resize(inChAlloc);
    for (int c = 0; c < inChAlloc; ++c) inPtrs_[c] = inStore_.data() + static_cast<size_t>(c) * Engine::kMaxBlock;
    const size_t ablSize = offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * static_cast<size_t>(inChAlloc);
    inABL_ = std::calloc(1, ablSize);
    scratchL_.assign(Engine::kMaxBlock, 0.0f);
    scratchR_.assign(Engine::kMaxBlock, 0.0f);
    ring_.assign(static_cast<size_t>(inChAlloc) * kRingFrames, 0.0f);
    ringOut_.assign(static_cast<size_t>(inChAlloc) * Engine::kMaxBlock, 0.0f);
    ringPtrs_.resize(inChAlloc);
    for (int c = 0; c < inChAlloc; ++c) ringPtrs_[c] = ringOut_.data() + static_cast<size_t>(c) * Engine::kMaxBlock;
    ringW_ = 0; ringR_ = 0; primed_ = false;
    overloads = 0; cpuPeak_ = 0; cpuLoad = 0;

    engine.prepare(fs_, inCh_);

    const auto outFmt = floatFormat(fs_, outCh_);
    const auto inFmt = floatFormat(fs_, std::max(inCh_, 1));

    if (duplex_) {
        AudioUnit u = makeHAL(outDev_, inCh_ > 0, true, err);
        if (!u) return false;
        outUnit_ = u;
        OSStatus s = AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outFmt, sizeof(outFmt));
        if (inCh_ > 0) s |= AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &inFmt, sizeof(inFmt));
        AURenderCallbackStruct cb{duplexCallback, this};
        s |= AudioUnitSetProperty(u, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));
        if (s != noErr) { err = "Cannot set stream format / callback"; stop(); return false; }
        if (AudioUnitInitialize(u) != noErr) { err = "AUHAL initialise failed"; stop(); return false; }
    } else {
        AudioUnit uo = makeHAL(outDev_, false, true, err);
        if (!uo) return false;
        outUnit_ = uo;
        OSStatus s = AudioUnitSetProperty(uo, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outFmt, sizeof(outFmt));
        AURenderCallbackStruct cb{outputCallback, this};
        s |= AudioUnitSetProperty(uo, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));
        if (s != noErr || AudioUnitInitialize(uo) != noErr) { err = "Cannot initialise output unit"; stop(); return false; }
        if (inDev_ && inCh_ > 0) {
            AudioUnit ui = makeHAL(inDev_, true, false, err);
            if (!ui) { stop(); return false; }
            inUnit_ = ui;
            s = AudioUnitSetProperty(ui, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &inFmt, sizeof(inFmt));
            AURenderCallbackStruct icb{inputCallback, this};
            s |= AudioUnitSetProperty(ui, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &icb, sizeof(icb));
            if (s != noErr || AudioUnitInitialize(ui) != noErr) { err = "Cannot initialise input unit"; stop(); return false; }
        }
    }

    // latency report
    const float msPerFrame = static_cast<float>(1000.0 / fs_);
    inLatMs_ = inDev_ ? (deviceLatencyFrames(inDev_, true) + buffer_) * msPerFrame : 0.0f;
    outLatMs_ = (deviceLatencyFrames(outDev_, false) + buffer_) * msPerFrame;
    if (!duplex_) inLatMs_ += 2.0f * buffer_ * msPerFrame;   // ring-buffer bridging

    auto ov = addr(kAudioDeviceProcessorOverload);
    AudioObjectAddPropertyListener(outDev_, &ov, overloadListener, this);
    if (inUnit_) AudioObjectAddPropertyListener(inDev_, &ov, overloadListener, this);

    if (inUnit_ && AudioOutputUnitStart(static_cast<AudioUnit>(inUnit_)) != noErr) { err = "Cannot start input"; stop(); return false; }
    if (AudioOutputUnitStart(static_cast<AudioUnit>(outUnit_)) != noErr) { err = "Cannot start output"; stop(); return false; }
    running_ = true;
    return true;
}

void AudioIO::stop() {
    auto ov = addr(kAudioDeviceProcessorOverload);
    if (outUnit_) {
        AudioOutputUnitStop(static_cast<AudioUnit>(outUnit_));
        AudioUnitUninitialize(static_cast<AudioUnit>(outUnit_));
        AudioComponentInstanceDispose(static_cast<AudioUnit>(outUnit_));
        outUnit_ = nullptr;
        AudioObjectRemovePropertyListener(outDev_, &ov, overloadListener, this);
    }
    if (inUnit_) {
        AudioOutputUnitStop(static_cast<AudioUnit>(inUnit_));
        AudioUnitUninitialize(static_cast<AudioUnit>(inUnit_));
        AudioComponentInstanceDispose(static_cast<AudioUnit>(inUnit_));
        inUnit_ = nullptr;
        AudioObjectRemovePropertyListener(inDev_, &ov, overloadListener, this);
    }
    if (inABL_) { std::free(inABL_); inABL_ = nullptr; }
    running_ = false;
}

} // namespace gsx

#else  // ---------------------------------------------------------- non-Apple stub

namespace gsx {
bool AudioIO::start(const GSXAudioConfig&, Engine&, std::string& err) { err = "Core Audio is only available on macOS"; return false; }
void AudioIO::stop() { running_ = false; }
int AudioIO::deviceCount() { return 0; }
bool AudioIO::deviceInfo(int, GSXDeviceInfo&) { return false; }
int AudioIO::sampleRates(uint32_t, double*, int) { return 0; }
bool AudioIO::bufferRange(uint32_t, int&, int&) { return false; }
void AudioIO::processAndWrite(const float* const*, int, uint32_t, void*) {}
} // namespace gsx

#endif
