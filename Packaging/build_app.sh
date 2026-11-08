#!/bin/bash
# Builds an optimised, ad-hoc signed GuitarSynthX.app into ./build.
#
#   ./Packaging/build_app.sh             # universal: arm64 + x86_64 (default)
#   ./Packaging/build_app.sh native      # only this Mac's architecture (faster)
#   ./Packaging/build_app.sh arm64       # Apple Silicon only
#   ./Packaging/build_app.sh x86_64      # Intel only
#
# Each architecture is compiled separately and merged with `lipo`, so this
# works with just the Xcode Command Line Tools (no full Xcode needed).
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-universal}"
MIN_OS="13.0"
case "$MODE" in
  universal) ARCHS=(arm64 x86_64) ;;
  arm64|x86_64) ARCHS=("$MODE") ;;
  native) ARCHS=("$(uname -m)") ;;
  *) echo "Unknown mode '${MODE}' (use universal | native | arm64 | x86_64)"; exit 1 ;;
esac

# Newer SwiftPM ("swiftbuild" build system) writes every architecture to the
# same output path, so each slice is copied aside right after it is built.
SLICES=build/slices
rm -rf "$SLICES"
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

APP=build/GuitarSynthX.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

if [[ ${#BINARIES[@]} -gt 1 ]]; then
  echo "> Merging into a universal binary..."
  lipo -create "${BINARIES[@]}" -output "$APP/Contents/MacOS/GuitarSynthX"
else
  cp "${BINARIES[0]}" "$APP/Contents/MacOS/GuitarSynthX"
fi

cp Packaging/Info.plist "$APP/Contents/Info.plist"
cp Packaging/Icon/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
printf 'APPL????' > "$APP/Contents/PkgInfo"

echo "> Signing (ad-hoc, hardened runtime + audio-input entitlement)..."
codesign --force --options runtime --entitlements Packaging/GuitarSynthX.entitlements --sign - "$APP"

# Make Finder/Dock pick up the new icon immediately
touch "$APP"

echo "OK: Built ${APP}"
echo "  Architectures: $(lipo -archs "$APP/Contents/MacOS/GuitarSynthX")"
echo "  Run:  open ${APP}"
