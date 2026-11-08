#!/bin/bash
# Builds optimised, ad-hoc signed GuitarSynthX apps into ./build, named by
# the architecture they run on:
#
#   build/GuitarSynthX-AppleSilicon.app   arm64
#   build/GuitarSynthX-Intel.app          x86_64
#   build/GuitarSynthX-Universal.app      arm64 + x86_64
#
#   ./Packaging/build_app.sh             # all three (default)
#   ./Packaging/build_app.sh native      # only the app for this Mac (faster)
#   ./Packaging/build_app.sh arm64       # Apple Silicon app only
#   ./Packaging/build_app.sh x86_64      # Intel app only
#
# Each architecture is compiled separately (and merged with `lipo` for the
# universal app), so this works with just the Xcode Command Line Tools.
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-all}"
MIN_OS="13.0"
case "$MODE" in
  all|universal) ARCHS=(arm64 x86_64) ;;
  arm64|x86_64) ARCHS=("$MODE") ;;
  native) ARCHS=("$(uname -m)") ;;
  *) echo "Unknown mode '${MODE}' (use all | native | arm64 | x86_64)"; exit 1 ;;
esac

app_name() {
  case "$1" in
    arm64) echo "GuitarSynthX-AppleSilicon" ;;
    x86_64) echo "GuitarSynthX-Intel" ;;
    universal) echo "GuitarSynthX-Universal" ;;
  esac
}

# Creates build/<name>.app from one or more architecture slices.
make_app() {
  local name="$1"; shift
  local app="build/${name}.app"
  rm -rf "$app"
  mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
  if [[ $# -gt 1 ]]; then
    lipo -create "$@" -output "$app/Contents/MacOS/GuitarSynthX"
  else
    cp "$1" "$app/Contents/MacOS/GuitarSynthX"
  fi
  cp Packaging/Info.plist "$app/Contents/Info.plist"
  cp Packaging/Icon/AppIcon.icns "$app/Contents/Resources/AppIcon.icns"
  printf 'APPL????' > "$app/Contents/PkgInfo"
  codesign --force --options runtime --entitlements Packaging/GuitarSynthX.entitlements --sign - "$app"
  touch "$app"   # make Finder/Dock pick up the icon
  echo "OK: ${app}  [$(lipo -archs "$app/Contents/MacOS/GuitarSynthX")]"
}

# Newer SwiftPM ("swiftbuild" build system) writes every architecture to the
# same output path, so each slice is copied aside right after it is built.
SLICES=build/slices
rm -rf "$SLICES"
rm -rf build/GuitarSynthX.app   # bundle name used by older versions of this script
BINARIES=()
for ARCH in "${ARCHS[@]}"; do
  TRIPLE="${ARCH}-apple-macosx${MIN_OS}"
  echo "> Building release for ${ARCH}..."
  swift build -c release --triple "$TRIPLE"
  BIN_DIR=$(swift build -c release --triple "$TRIPLE" --show-bin-path)
  mkdir -p "${SLICES}/${ARCH}"
  cp "${BIN_DIR}/GuitarSynthX" "${SLICES}/${ARCH}/GuitarSynthX"
  if ! lipo "${SLICES}/${ARCH}/GuitarSynthX" -verify_arch "${ARCH}"; then
    echo "ERROR: the ${ARCH} build produced: $(lipo -archs "${SLICES}/${ARCH}/GuitarSynthX")"
    exit 1
  fi
  BINARIES+=("${SLICES}/${ARCH}/GuitarSynthX")
done

echo "> Packaging and signing (ad-hoc, hardened runtime + audio-input entitlement)..."
for ARCH in "${ARCHS[@]}"; do
  make_app "$(app_name "$ARCH")" "${SLICES}/${ARCH}/GuitarSynthX"
done
if [[ ${#BINARIES[@]} -gt 1 ]]; then
  make_app "$(app_name universal)" "${BINARIES[@]}"
fi

echo "Done. Run e.g.:  open build/$(app_name "$(uname -m)").app"
