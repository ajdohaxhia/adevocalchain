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

xattr -cr "${VST}" "${AU}"
codesign --force --deep --sign - "${VST}"
codesign --force --deep --sign - "${AU}"

mkdir -p "${HOME}/Library/Audio/Plug-Ins/VST3" "${HOME}/Library/Audio/Plug-Ins/Components"
rm -rf "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
rm -rf "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
ditto --norsrc "${VST}" "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
ditto --norsrc "${AU}" "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
xattr -cr "${HOME}/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
xattr -cr "${HOME}/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"

killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed user-level:"
echo "  ~/Library/Audio/Plug-Ins/VST3/ademusic Vocal Rack.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
