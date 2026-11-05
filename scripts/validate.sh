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

SIGN_DIR="$(mktemp -d /tmp/ademusic-vocal-rack-validate.XXXXXX)"
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

if [[ -x "${PLUGINVAL_BIN}" ]]; then
    "${PLUGINVAL_BIN}" --strictness-level 7 --validate "${SIGN_DIR}/ademusic Vocal Rack.vst3" | tee docs/verification/pluginval-vst3.log
    "${PLUGINVAL_BIN}" --strictness-level 7 --validate "${SIGN_DIR}/ademusic Vocal Rack.component" | tee docs/verification/pluginval-au.log
else
    echo "pluginval not found at ${PLUGINVAL_BIN}; set PLUGINVAL_BIN." >&2
    exit 2
fi

# auval requires the component in the user Components folder.
OSASCRIPT_DIR="${HOME}/Library/Audio/Plug-Ins/Components"
mkdir -p "${OSASCRIPT_DIR}"
rm -rf "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
ditto --norsrc --noextattr "${SIGN_DIR}/ademusic Vocal Rack.component" "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
xattr -cr "${OSASCRIPT_DIR}/ademusic Vocal Rack.component"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1
auval -v aufx Vrck Adms | tee docs/verification/auval.log

"${TEST_BIN}" --host "${SIGN_DIR}/ademusic Vocal Rack.vst3" vst3 | tee docs/verification/host-vst3.log
"${TEST_BIN}" --host "${OSASCRIPT_DIR}/ademusic Vocal Rack.component" au | tee docs/verification/host-au.log
