#!/bin/bash
# Builds, checks, zips and publishes a GuitarSynthX release in one go.
#
#   ./Packaging/make_release.sh            # build + zip + publish on GitHub
#   ./Packaging/make_release.sh --no-publish   # build + zip + notes only
#
# The version comes from CFBundleShortVersionString in Packaging/Info.plist and
# the release notes from the matching section of CHANGELOG.md. Publishing uses
# the GitHub CLI (`gh`) if it is installed and logged in; otherwise the notes
# are copied to the clipboard and the "new release" page opens in your browser.
set -euo pipefail
cd "$(dirname "$0")/.."

REPO="lydianb79/GuitarSynthX"
PUBLISH=1
if [[ "${1:-}" == "--no-publish" ]]; then PUBLISH=0; fi

if [[ "$(uname)" != "Darwin" ]]; then
  echo "ERROR: releases have to be built on a Mac."; exit 1
fi

VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" Packaging/Info.plist)
TAG="v${VERSION}"
echo "> Releasing GuitarSynthX ${TAG}"

# ---- release notes section from CHANGELOG.md
CHANGES=$(awk -v v="${VERSION}" '
  $0 ~ "^## \\[" v "\\]" { found = 1; next }
  found && /^## \[/       { exit }
  found                   { print }
' CHANGELOG.md)
if [[ -z "${CHANGES//[[:space:]]/}" ]]; then
  echo "ERROR: CHANGELOG.md has no section for [${VERSION}]. Add one first."; exit 1
fi

if [[ $PUBLISH -eq 1 ]] && command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
  if gh release view "${TAG}" --repo "${REPO}" >/dev/null 2>&1; then
    echo "ERROR: release ${TAG} already exists on GitHub. Bump the version first."; exit 1
  fi
fi

# ---- build both architectures
./Packaging/build_app.sh

# ---- verify what we are about to ship
check_app() {
  local app="build/GuitarSynthX-$1.app" arch="$2"
  lipo "${app}/Contents/MacOS/GuitarSynthX" -verify_arch "${arch}" \
    || { echo "ERROR: ${app} is not ${arch}"; exit 1; }
  codesign --verify --strict "${app}" \
    || { echo "ERROR: ${app} has an invalid signature"; exit 1; }
  local v
  v=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${app}/Contents/Info.plist")
  [[ "${v}" == "${VERSION}" ]] || { echo "ERROR: ${app} reports version ${v}"; exit 1; }
  echo "OK: ${app} (${arch}, signed, version ${v})"
}
check_app AppleSilicon arm64
check_app Intel x86_64

# ---- zip (ditto keeps the code signature and permissions intact)
mkdir -p dist
ZIPS=()
for n in AppleSilicon Intel; do
  out="dist/GuitarSynthX-${TAG}-macOS-${n}.zip"
  rm -f "${out}"
  ditto -c -k --keepParent "build/GuitarSynthX-${n}.app" "${out}"
  ZIPS+=("${out}")
done

# ---- release notes
NOTES="dist/RELEASE_NOTES-${TAG}.md"
{
  echo "![GuitarSynthX](https://raw.githubusercontent.com/${REPO}/main/docs/screenshot.png)"
  echo
  echo "## What's new in ${VERSION}"
  echo "${CHANGES}"
  echo
  echo "## Downloads"
  echo
  echo "| File | For |"
  echo "|---|---|"
  echo "| \`GuitarSynthX-${TAG}-macOS-AppleSilicon.zip\` | Apple Silicon Macs (M1 and later), arm64 |"
  echo "| \`GuitarSynthX-${TAG}-macOS-Intel.zip\` | Intel Macs, x86_64 |"
  echo
  echo "Not sure which you have? Apple menu > About This Mac: \"Chip: Apple M...\" means Apple Silicon; \"Processor: Intel...\" means Intel."
  echo
  echo "**Requires macOS 13 Ventura or later.**"
  echo
  echo "## Installing"
  echo
  echo "1. Unzip and drag the app to your Applications folder."
  echo "2. **First launch:** the app is ad-hoc signed, not notarized by Apple, so macOS blocks it the first time. Right-click (or Control-click) the app and choose **Open**, then **Open** again. Or click **Open Anyway** under *System Settings > Privacy & Security*."
  echo "3. Allow **microphone access** when asked. Without it the app can't hear your guitar."
  echo "4. Choose your audio interface in Audio Settings (Cmd-,). Using the same device for input and output gives the lowest latency."
  echo
  echo "Updating from an earlier version: replace the old app. Your settings and your own presets are kept."
  echo
  echo "## Checksums (SHA-256)"
  echo
  echo '```'
  (cd dist && shasum -a 256 "GuitarSynthX-${TAG}-macOS-AppleSilicon.zip" "GuitarSynthX-${TAG}-macOS-Intel.zip")
  echo '```'
} > "${NOTES}"
echo "OK: ${NOTES}"
ls -l "${ZIPS[@]}"

if [[ $PUBLISH -eq 0 ]]; then
  echo "Done (not published). Zips and notes are in dist/."; exit 0
fi

# ---- publish
if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
  gh release create "${TAG}" "${ZIPS[@]}" --repo "${REPO}" --target main \
    --title "GuitarSynthX ${TAG}" --notes-file "${NOTES}"
  echo "Published: https://github.com/${REPO}/releases/tag/${TAG}"
else
  pbcopy < "${NOTES}"
  echo
  echo "The GitHub CLI isn't installed or logged in, so publish in the browser:"
  echo "  1. The release page is opening now (tag ${TAG} and title are filled in)."
  echo "  2. Paste the release notes into the description: they are on your clipboard."
  echo "  3. Drag in the two zips from: $(pwd)/dist"
  echo "  4. Click Publish release."
  open "https://github.com/${REPO}/releases/new?tag=${TAG}&target=main&title=GuitarSynthX%20${TAG}"
  open dist
fi
