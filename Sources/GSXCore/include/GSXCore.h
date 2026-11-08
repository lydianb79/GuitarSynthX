// GSXCore.h — C interface to the GuitarSynthX real-time engine.
//
// Everything that runs on the audio thread lives in C++ behind this API, so the
// Core Audio render callback never touches Swift, ARC, locks or the allocator.
// The Swift UI talks to the engine only through these functions:
//   * parameters are lock-free atomics (gsx_set_param / gsx_get_param)
//   * meters / tracker state are atomics written by the audio thread
//   * visual data (scope + spectrum) is computed on the *calling* thread
#ifndef GSXCORE_H
#define GSXCORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- devices
typedef struct {
    uint32_t deviceID;
    char     name[128];
    char     uid[128];
    int32_t  inputChannels;
    int32_t  outputChannels;
    double   nominalSampleRate;
    int32_t  isDefaultInput;
    int32_t  isDefaultOutput;
} GSXDeviceInfo;

/// Re-scans the hardware and returns the number of audio devices.
int32_t gsx_device_count(void);
/// Fills `out` for device at `index` (0..count-1). Returns 1 on success.
int32_t gsx_device_info(int32_t index, GSXDeviceInfo* out);
/// Writes up to `maxRates` supported nominal sample rates. Returns count.
int32_t gsx_device_sample_rates(uint32_t deviceID, double* outRates, int32_t maxRates);
/// Supported I/O buffer size range in frames. Returns 1 on success.
int32_t gsx_device_buffer_range(uint32_t deviceID, int32_t* minFrames, int32_t* maxFrames);

// ---------------------------------------------------------------- engine
typedef struct {
    uint32_t inputDeviceID;   // 0 = no input
    uint32_t outputDeviceID;  // 0 = system default output
    double   sampleRate;     // requested; device is switched to it if supported
    int32_t  bufferFrames;   // requested I/O buffer (e.g. 32, 64, 128 ...)
} GSXAudioConfig;

/// Starts audio. Returns 1 on success; on failure writes a message to errBuf.
int32_t gsx_start(const GSXAudioConfig* config, char* errBuf, int32_t errLen);
void    gsx_stop(void);
int32_t gsx_is_running(void);
/// Silences all voices and clears effect tails.
void    gsx_panic(void);

// ---------------------------------------------------------------- parameters
enum {
    GSX_PARAM_CONTINUOUS = 0,
    GSX_PARAM_INTEGER    = 1,
    GSX_PARAM_CHOICE     = 2,
    GSX_PARAM_TOGGLE     = 3
};
enum {
    GSX_SCALE_LINEAR = 0,
    GSX_SCALE_LOG    = 1,
    GSX_SCALE_SQUARE = 2
};

typedef struct {
    char    key[32];       // stable identifier used by presets, e.g. "flt.cutoff"
    char    name[32];      // display name
    char    group[24];     // UI group, e.g. "Filter"
    char    unit[12];      // "Hz", "ms", "dB", "ct", "st", ...
    char    choices[160];  // '|' separated labels for GSX_PARAM_CHOICE
    float   minValue;
    float   maxValue;
    float   defaultValue;
    int32_t type;
    int32_t scale;
} GSXParamInfo;

int32_t gsx_param_count(void);
int32_t gsx_param_info(int32_t paramID, GSXParamInfo* out);
void    gsx_set_param(int32_t paramID, float value);
float   gsx_get_param(int32_t paramID);

// ---------------------------------------------------------------- status
typedef struct {
    int32_t  running;
    double   sampleRate;
    int32_t  bufferFrames;
    int32_t  inputChannels;
    int32_t  outputChannels;
    int32_t  duplex;           // 1 = single device full-duplex path
    float    inputPeak;        // linear, decaying
    float    outputPeak;
    float    cpuLoad;          // 0..1 of the real-time budget (peak, decaying)
    uint32_t overloads;        // device-reported overloads + ring under/overruns
    float    inputLatencyMs;   // hardware + buffer
    float    outputLatencyMs;
    int32_t  activeVoices;
    int32_t  trackerCount;     // 1 in mono mode, 6 in hex mode
} GSXStatus;

typedef struct {
    float   frequency;      // last detected fundamental (Hz), 0 if none
    float   midi;           // fractional MIDI note of frequency
    float   aperiodicity;   // YIN confidence measure (lower = cleaner)
    float   levelDb;        // input envelope (dBFS)
    int32_t gate;           // input gate open
    int32_t noteActive;     // tracker currently holding a note
} GSXTrackerStatus;

void    gsx_get_status(GSXStatus* out);
int32_t gsx_get_tracker(int32_t index, GSXTrackerStatus* out);

/// Copies a trigger-stabilised oscilloscope trace of the output (`scopeCount`
/// samples, -1..1) and a log-frequency spectrum (`bins` values, 0..1).
/// Safe to call from the UI thread. Returns 1 if data was produced.
int32_t gsx_read_visual(float* scope, int32_t scopeCount, float* spectrum, int32_t bins);

#ifdef __cplusplus
}
#endif

#endif
