#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-release}"
VST="${BUILD_DIR}/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
AU="${BUILD_DIR}/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component"

if [[ ! -d "${VST}" || ! -d "${AU}" ]]; then
    echo "Build the bundles first with scripts/build.sh." >&2
    exit 2
fi

SIGN_DIR="$(mktemp -d /tmp/ademusic-vocal-rack-install.XXXXXX)"
trap 'rm -rf "${SIGN_DIR}"' EXIT
ditto --norsrc --noextattr "${VST}" "${SIGN_DIR}/ademusic Vocal Rack.vst3"
ditto --norsrc --noextattr "${AU}" "${SIGN_DIR}/ademusic Vocal Rack.component"
find "${SIGN_DIR}" -name 'PkgInfo *' -delete 2>/dev/null || true
find "${SIGN_DIR}" -name '._*' -delete 2>/dev/null || true
xattr -cr "${SIGN_DIR}/ademusic Vocal Rack.vst3" "${SIGN_DIR}/ademusic Vocal Rack.component"
rm -rf "${SIGN_DIR}/ademusic Vocal Rack.vst3/Contents/_CodeSignature"
rm -rf "${SIGN_DIR}/ademusic Vocal Rack.component/Contents/_CodeSignature"
codesign --force --deep --sign - "${SIGN_DIR}/ademusic Vocal Rack.vst3"
codesign --force --deep --sign - "${SIGN_DIR}/ademusic Vocal Rack.component"
codesign --verify --deep --strict "${SIGN_DIR}/ademusic Vocal Rack.vst3"
codesign --verify --deep --strict "${SIGN_DIR}/ademusic Vocal Rack.component"

mkdir -p "${HOME}/Library/Audio/Plug-Ins/VST3" "${HOME}/Library/Audio/Plug-Ins/Components"
rm -rf "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
rm -rf "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
ditto --norsrc --noextattr "${SIGN_DIR}/ademusic Vocal Rack.vst3" "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
ditto --norsrc --noextattr "${SIGN_DIR}/ademusic Vocal Rack.component" "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
xattr -cr "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
xattr -cr "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"

killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed user-level:"
echo "  ~/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
