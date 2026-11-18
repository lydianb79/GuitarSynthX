#!/bin/sh
# Replays a recording from Audio > Record Input through the engine and prints
# the notes it detects. Usage: ./Tests/replay.sh file.wav [key=value ...]
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX=${CXX:-clang++}
BIN=/tmp/gsx_replay
$CXX -std=c++17 -O2 -w -o "$BIN" "$ROOT/Tests/replay.cpp" \
  "$ROOT"/Sources/GSXCore/Engine.cpp "$ROOT"/Sources/GSXCore/PitchTracker.cpp \
  "$ROOT"/Sources/GSXCore/Synth.cpp "$ROOT"/Sources/GSXCore/Effects.cpp "$ROOT"/Sources/GSXCore/Params.cpp \
  $( [ "$(uname)" = "Darwin" ] && echo "-framework Accelerate" )
exec "$BIN" "$@"
