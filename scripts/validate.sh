#!/usr/bin/env bash
set -euo pipefail
set -o pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-release}"
VST="${BUILD_DIR}/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
AU="${BUILD_DIR}/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component"
PLUGINVAL_BIN="${PLUGINVAL_BIN:-tools/pluginval/pluginval.app/Contents/MacOS/pluginval}"
TEST_BIN="${BUILD_DIR}/VocalRackTests_artefacts/Release/VocalRackTests"

mkdir -p docs/verification

xattr -cr "${VST}" "${AU}"
codesign --force --deep --sign - "${VST}"
codesign --force --deep --sign - "${AU}"
codesign --verify --deep --strict "${VST}"
codesign --verify --deep --strict "${AU}"

if [[ -x "${PLUGINVAL_BIN}" ]]; then
    "${PLUGINVAL_BIN}" --strictness-level 7 --validate "${VST}" | tee docs/verification/pluginval-vst3.log
    "${PLUGINVAL_BIN}" --strictness-level 7 --validate "${AU}" | tee docs/verification/pluginval-au.log
else
    echo "pluginval not found at ${PLUGINVAL_BIN}; set PLUGINVAL_BIN." >&2
    exit 2
fi

# auval requires the component in the user Components folder.
OSASCRIPT_DIR="${HOME}/Library/Audio/Plug-Ins/Components"
mkdir -p "${OSASCRIPT_DIR}"
rm -rf "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
ditto --norsrc "${AU}" "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
xattr -cr "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1
auval -v aufx Vrck Adms | tee docs/verification/auval.log

"${TEST_BIN}" --host "${VST}" vst3 | tee docs/verification/host-vst3.log
"${TEST_BIN}" --host "${OSASCRIPT_DIR}/ademusic Vocal Rack.component" au | tee docs/verification/host-au.log
