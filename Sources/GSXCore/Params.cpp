// Params.cpp — parameter metadata. The Swift UI is generated from this table.
#include "Params.hpp"

namespace gsx {

#define C  GSX_PARAM_CONTINUOUS
#define I  GSX_PARAM_INTEGER
#define CH GSX_PARAM_CHOICE
#define T  GSX_PARAM_TOGGLE
#define LIN GSX_SCALE_LINEAR
#define LOG GSX_SCALE_LOG
#define SQR GSX_SCALE_SQUARE

static const char* kWaves = "Sine|Triangle|Saw|Square|Pulse";

static const ParamDef kDefs[P_COUNT] = {
    // key               name              group       unit   choices                         min     max     def    type scale
    {"in.mode",        "Input Mode",     "Input",    "",    "Mono|Hex (6 ch)",              0,      1,      0,     CH, LIN},
    {"in.channel",     "Input Ch",       "Input",    "",    "",                             1,      32,     1,     I,  LIN},
    {"in.gain",        "Input Gain",     "Input",    "dB",  "",                             -12,    36,     0,     C,  LIN},
    {"in.gate",        "Gate",           "Input",    "dB",  "",                             -80,    -20,    -54,   C,  LIN},
    {"out.channel",    "Output Ch",      "Input",    "",    "",                             1,      31,     1,     I,  LIN},

    {"trk.lowest",     "Lowest Note",    "Tracking", "Hz",  "",                             28,     200,    75,    C,  LOG},
    {"trk.highest",    "Highest Note",   "Tracking", "Hz",  "",                             400,    2000,   1400,  C,  LOG},
    {"trk.threshold",  "Sensitivity",    "Tracking", "",    "",                             0.05f,  0.40f,  0.15f, C,  LIN},
    {"trk.stability",  "Stability",      "Tracking", "",    "",                             1,      6,      2,     I,  LIN},
    {"trk.hop",        "Analysis Rate",  "Tracking", "",    "0.5 ms|1 ms|2 ms",             0,      2,      1,     CH, LIN},
    {"trk.pitchmode",  "Pitch Mode",     "Tracking", "",    "Track|Quantize|Quantize+Glide",0,      2,      1,     CH, LIN},
    {"trk.glide",      "Glide",          "Tracking", "ms",  "",                             0,      1000,   60,    C,  SQR},
    {"trk.transpose",  "Transpose",      "Tracking", "st",  "",                             -24,    24,     0,     I,  LIN},
    {"trk.retrigger",  "Pick Retrigger", "Tracking", "",    "",                             0,      1,      1,     T,  LIN},
    {"trk.dynamics",   "Dynamics",       "Tracking", "",    "",                             0,      1,      0.5f,  C,  LIN},

    {"oscA.wave",      "Osc A Wave",     "Osc",      "",    kWaves,                         0,      4,      2,     CH, LIN},
    {"oscA.octave",    "Osc A Octave",   "Osc",      "oct", "",                             -2,     2,      0,     I,  LIN},
    {"oscA.unison",    "Unison",         "Osc",      "",    "",                             1,      7,      1,     I,  LIN},
    {"oscA.detune",    "Unison Detune",  "Osc",      "ct",  "",                             0,      50,     12,    C,  LIN},
    {"oscB.wave",      "Osc B Wave",     "Osc",      "",    kWaves,                         0,      4,      3,     CH, LIN},
    {"oscB.semi",      "Osc B Semi",     "Osc",      "st",  "",                             -24,    24,     0,     I,  LIN},
    {"oscB.fine",      "Osc B Fine",     "Osc",      "ct",  "",                             -50,    50,     7,     C,  LIN},
    {"osc.mix",        "A / B Mix",      "Osc",      "",    "",                             0,      1,      0.5f,  C,  LIN},
    {"osc.pw",         "Pulse Width",    "Osc",      "",    "",                             0.05f,  0.95f,  0.5f,  C,  LIN},
    {"osc.fm",         "FM B>A",         "Osc",      "",    "",                             0,      1,      0,     C,  SQR},
    {"osc.sub",        "Sub Osc",        "Osc",      "",    "",                             0,      1,      0,     C,  LIN},
    {"osc.noise",      "Noise",          "Osc",      "",    "",                             0,      1,      0,     C,  SQR},

    {"flt.type",       "Filter Type",    "Filter",   "",    "LP 24|LP 12|BP 12|HP 12",      0,      3,      0,     CH, LIN},
    {"flt.cutoff",     "Cutoff",         "Filter",   "Hz",  "",                             20,     20000,  2500,  C,  LOG},
    {"flt.res",        "Resonance",      "Filter",   "",    "",                             0,      1,      0.2f,  C,  LIN},
    {"flt.envamt",     "Env Amount",     "Filter",   "oct", "",                             -5,     8,      2,     C,  LIN},
    {"flt.keytrack",   "Key Track",      "Filter",   "",    "",                             0,      1,      0.5f,  C,  LIN},
    {"flt.dyn",        "Pick Dynamics",  "Filter",   "oct", "",                             0,      6,      1,     C,  LIN},
    {"flt.drive",      "Filter Drive",   "Filter",   "",    "",                             0,      1,      0.1f,  C,  LIN},
    {"fenv.a",         "F Attack",       "Filter",   "ms",  "",                             0.5f,   5000,   2,     C,  LOG},
    {"fenv.d",         "F Decay",        "Filter",   "ms",  "",                             1,      5000,   300,   C,  LOG},
    {"fenv.s",         "F Sustain",      "Filter",   "",    "",                             0,      1,      0.3f,  C,  LIN},
    {"fenv.r",         "F Release",      "Filter",   "ms",  "",                             1,      8000,   300,   C,  LOG},

    {"aenv.a",         "Attack",         "Amp",      "ms",  "",                             0.5f,   5000,   1,     C,  LOG},
    {"aenv.d",         "Decay",          "Amp",      "ms",  "",                             1,      5000,   200,   C,  LOG},
    {"aenv.s",         "Sustain",        "Amp",      "",    "",                             0,      1,      0.8f,  C,  LIN},
    {"aenv.r",         "Release",        "Amp",      "ms",  "",                             1,      8000,   250,   C,  LOG},
    {"amp.vel",        "Velocity",       "Amp",      "",    "",                             0,      1,      0.6f,  C,  LIN},

    {"lfo.wave",       "LFO Wave",       "LFO",      "",    "Sine|Triangle|Saw|Square|S&H", 0,      4,      0,     CH, LIN},
    {"lfo.rate",       "LFO Rate",       "LFO",      "Hz",  "",                             0.05f,  20,     5,     C,  LOG},
    {"lfo.delay",      "LFO Fade-in",    "LFO",      "ms",  "",                             0,      2000,   300,   C,  SQR},
    {"lfo.pitch",      "Vibrato",        "LFO",      "ct",  "",                             0,      100,    0,     C,  SQR},
    {"lfo.filter",     "LFO > Cutoff",   "LFO",      "oct", "",                             0,      4,      0,     C,  LIN},
    {"lfo.pw",         "LFO > PW",       "LFO",      "",    "",                             0,      0.45f,  0,     C,  LIN},
    {"lfo.amp",        "Tremolo",        "LFO",      "",    "",                             0,      1,      0,     C,  LIN},

    {"drv.on",         "Drive",          "Drive",    "",    "",                             0,      1,      0,     T,  LIN},
    {"drv.amount",     "Amount",         "Drive",    "",    "",                             0,      1,      0.4f,  C,  LIN},
    {"drv.tone",       "Tone",           "Drive",    "Hz",  "",                             500,    12000,  5000,  C,  LOG},
    {"drv.mix",        "Mix",            "Drive",    "",    "",                             0,      1,      1,     C,  LIN},

    {"cho.on",         "Chorus",         "Chorus",   "",    "",                             0,      1,      0,     T,  LIN},
    {"cho.rate",       "Rate",           "Chorus",   "Hz",  "",                             0.05f,  5,      0.6f,  C,  LOG},
    {"cho.depth",      "Depth",          "Chorus",   "",    "",                             0,      1,      0.5f,  C,  LIN},
    {"cho.mix",        "Mix",            "Chorus",   "",    "",                             0,      1,      0.4f,  C,  LIN},

    {"pha.on",         "Phaser",         "Phaser",   "",    "",                             0,      1,      0,     T,  LIN},
    {"pha.rate",       "Rate",           "Phaser",   "Hz",  "",                             0.02f,  5,      0.3f,  C,  LOG},
    {"pha.depth",      "Depth",          "Phaser",   "",    "",                             0,      1,      0.7f,  C,  LIN},
    {"pha.fb",         "Feedback",       "Phaser",   "",    "",                             0,      0.9f,   0.5f,  C,  LIN},
    {"pha.mix",        "Mix",            "Phaser",   "",    "",                             0,      1,      0.5f,  C,  LIN},

    {"dly.on",         "Delay",          "Delay",    "",    "",                             0,      1,      0,     T,  LIN},
    {"dly.time",       "Time",           "Delay",    "ms",  "",                             10,     2000,   350,   C,  LOG},
    {"dly.fb",         "Feedback",       "Delay",    "",    "",                             0,      0.95f,  0.35f, C,  LIN},
    {"dly.mix",        "Mix",            "Delay",    "",    "",                             0,      1,      0.25f, C,  LIN},
    {"dly.pingpong",   "Ping-Pong",      "Delay",    "",    "",                             0,      1,      1,     T,  LIN},
    {"dly.damp",       "High Cut",       "Delay",    "Hz",  "",                             500,    16000,  5000,  C,  LOG},
    {"dly.mode",       "Character",      "Delay",    "",    "Digital|Tape|Analog",          0,      2,      0,     CH, LIN},
    {"dly.sync",       "Sync",           "Delay",    "",    "Free|1/2|1/4|1/4 T|1/8 D|1/8|1/8 T|1/16", 0, 7,    0,     CH, LIN},
    {"dly.bpm",        "Tempo",          "Delay",    "BPM", "",                             40,     240,    120,   C,  LIN},
    {"dly.lowcut",     "Low Cut",        "Delay",    "Hz",  "",                             20,     1000,   80,    C,  LOG},
    {"dly.mod",        "Modulation",     "Delay",    "",    "",                             0,      1,      0,     C,  LIN},
    {"dly.duck",       "Ducking",        "Delay",    "",    "",                             0,      1,      0,     C,  LIN},

    {"rev.on",         "Reverb",         "Reverb",   "",    "",                             0,      1,      0,     T,  LIN},
    {"rev.size",       "Size",           "Reverb",   "",    "",                             0,      1,      0.6f,  C,  LIN},
    {"rev.decay",      "Decay",          "Reverb",   "s",   "",                             0.2f,   20,     2.5f,  C,  LOG},
    {"rev.damp",       "High Cut",        "Reverb",   "Hz",  "",                             1000,   18000,  7000,  C,  LOG},
    {"rev.predelay",   "Pre-delay",      "Reverb",   "ms",  "",                             0,      200,    20,    C,  LIN},
    {"rev.mix",        "Mix",            "Reverb",   "",    "",                             0,      1,      0.25f, C,  LIN},
    {"rev.type",       "Type",           "Reverb",   "",    "Room|Hall|Plate",              0,      2,      1,     CH, LIN},
    {"rev.lowcut",     "Low Cut",        "Reverb",   "Hz",  "",                             20,     1000,   120,   C,  LOG},
    {"rev.mod",        "Modulation",     "Reverb",   "",    "",                             0,      1,      0.3f,  C,  LIN},
    {"rev.shimmer",    "Shimmer",        "Reverb",   "",    "",                             0,      1,      0,     C,  LIN},
    {"rev.width",      "Width",          "Reverb",   "",    "",                             0,      1,      1,     C,  LIN},
    {"rev.duck",       "Ducking",        "Reverb",   "",    "",                             0,      1,      0,     C,  LIN},

    {"mst.synth",      "Synth Level",    "Master",   "dB",  "",                             -60,    6,      -6,    C,  LIN},
    {"mst.dry",        "Dry Guitar",     "Master",   "dB",  "",                             -60,    6,      -60,   C,  LIN},
    {"mst.volume",     "Master",         "Master",   "dB",  "",                             -60,    6,      0,     C,  LIN},
    {"mst.limiter",    "Limiter",        "Master",   "",    "",                             0,      1,      1,     T,  LIN},
    {"mst.dryfx",      "Dry > FX",       "Master",   "",    "",                             0,      1,      1,     T,  LIN},
};

const ParamDef& paramDef(int id) { return kDefs[id]; }

} // namespace gsx
