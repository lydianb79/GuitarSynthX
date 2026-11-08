// Params.hpp — parameter IDs and metadata table.
#pragma once
#include "include/GSXCore.h"

namespace gsx {

enum ParamID : int {
    // Input / routing
    P_IN_MODE, P_IN_CHANNEL, P_IN_GAIN, P_IN_GATE, P_OUT_CHANNEL,
    // Tracking
    P_TRK_LOWEST, P_TRK_HIGHEST, P_TRK_THRESHOLD, P_TRK_STABILITY, P_TRK_HOP,
    P_TRK_PITCHMODE, P_TRK_GLIDE, P_TRK_TRANSPOSE, P_TRK_RETRIGGER, P_TRK_DYNAMICS,
    // Oscillators
    P_OSCA_WAVE, P_OSCA_OCTAVE, P_OSCA_UNISON, P_OSCA_DETUNE,
    P_OSCB_WAVE, P_OSCB_SEMI, P_OSCB_FINE,
    P_OSC_MIX, P_OSC_PW, P_OSC_FM, P_OSC_SUB, P_OSC_NOISE,
    // Filter
    P_FLT_TYPE, P_FLT_CUTOFF, P_FLT_RES, P_FLT_ENVAMT, P_FLT_KEYTRACK, P_FLT_DYN, P_FLT_DRIVE,
    P_FENV_A, P_FENV_D, P_FENV_S, P_FENV_R,
    // Amp
    P_AENV_A, P_AENV_D, P_AENV_S, P_AENV_R, P_AMP_VEL,
    // LFO
    P_LFO_WAVE, P_LFO_RATE, P_LFO_DELAY, P_LFO_PITCH, P_LFO_FILTER, P_LFO_PW, P_LFO_AMP,
    // Effects
    P_DRV_ON, P_DRV_AMOUNT, P_DRV_TONE, P_DRV_MIX,
    P_CHO_ON, P_CHO_RATE, P_CHO_DEPTH, P_CHO_MIX,
    P_PHA_ON, P_PHA_RATE, P_PHA_DEPTH, P_PHA_FB, P_PHA_MIX,
    P_DLY_ON, P_DLY_TIME, P_DLY_FB, P_DLY_MIX, P_DLY_PINGPONG, P_DLY_DAMP,
    P_DLY_MODE, P_DLY_SYNC, P_DLY_BPM, P_DLY_LOWCUT, P_DLY_MOD, P_DLY_DUCK,
    P_REV_ON, P_REV_SIZE, P_REV_DECAY, P_REV_DAMP, P_REV_PREDELAY, P_REV_MIX,
    P_REV_TYPE, P_REV_LOWCUT, P_REV_MOD, P_REV_SHIMMER, P_REV_WIDTH, P_REV_DUCK,
    // Master
    P_MST_SYNTH, P_MST_DRY, P_MST_VOLUME, P_MST_LIMITER, P_MST_DRYFX,
    P_COUNT
};

struct ParamDef {
    const char* key;
    const char* name;
    const char* group;
    const char* unit;
    const char* choices;
    float minV, maxV, defV;
    int type;
    int scale;
};

const ParamDef& paramDef(int id);

} // namespace gsx
