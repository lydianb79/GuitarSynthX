#!/bin/sh
# Builds and runs the DSP engine tests/benchmarks (no audio hardware needed).
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-clang++}
$CXX -std=c++17 -O3 -Wall -Wextra -Wno-unused-parameter -o /tmp/gsx_engine_test \
  Tests/engine_test.cpp Sources/GSXCore/Engine.cpp Sources/GSXCore/PitchTracker.cpp \
  Sources/GSXCore/Synth.cpp Sources/GSXCore/Effects.cpp Sources/GSXCore/Params.cpp \
  Sources/GSXCore/GSXCore.cpp Sources/GSXCore/AudioIO.cpp \
  $( [ "$(uname)" = "Darwin" ] && echo "-framework CoreAudio -framework AudioToolbox -framework Accelerate -framework CoreFoundation" )
/tmp/gsx_engine_test
