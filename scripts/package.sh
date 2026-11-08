#!/usr/bin/env bash
set -euo pipefail
set -o pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-release}"
VERSION="2.3.0"
NAME="ademusic-vocal-rack-${VERSION}-macOS-arm64"
STAGE="dist/${NAME}"
VST="${BUILD_DIR}/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
AU="${BUILD_DIR}/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component"

if [[ ! -d "${VST}" || ! -d "${AU}" ]]; then
    echo "Build the bundles first with scripts/build.sh." >&2
    exit 2
fi

# This workspace lives under a FileProvider path that can reattach Finder
# metadata to bundles while they are being signed.  Sign a copy in /tmp, verify
# it there, and only then stage it.  The ZIP itself carries no AppleDouble data.
SIGN_DIR="$(mktemp -d /tmp/ademusic-vocal-rack-sign.XXXXXX)"
EXTRACT_DIR="$(mktemp -d /tmp/ademusic-vocal-rack-extract.XXXXXX)"
trap 'rm -rf "${SIGN_DIR}" "${EXTRACT_DIR}"' EXIT

mkdir -p "${SIGN_DIR}"
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
echo "Temporary copies signed and verified: PASS"

mkdir -p dist
rm -rf "${STAGE}"
mkdir -p "${STAGE}/VST3" "${STAGE}/Components" "${STAGE}/presets" "${STAGE}/LICENSES" "${STAGE}/docs"
ditto --norsrc --noextattr "${SIGN_DIR}/ademusic Vocal Rack.vst3" "${STAGE}/VST3/ademusic Vocal Rack.vst3"
ditto --norsrc --noextattr "${SIGN_DIR}/ademusic Vocal Rack.component" "${STAGE}/Components/ademusic Vocal Rack.component"
cp -R presets/. "${STAGE}/presets/"
cp -R LICENSES/. "${STAGE}/LICENSES/"
cp LICENSE THIRD_PARTY_NOTICES.md README.md INSTALL.txt "${STAGE}/"
cp docs/PRESET_FORMAT.md docs/MANUAL_CHECKLIST.md docs/VERIFICATION.md docs/ARCHITECTURE.md docs/PRESET_RESEARCH.md docs/RELEASE_NOTES_v2.3.0.md docs/UI_REDESIGN.md docs/AUDIT_2.0.md docs/PLAN_2.0.md docs/AUDIT_2.1.md docs/PHASE_A_2.1.md docs/AUDIT_2.2.md docs/AUDIT_2.3.md "${STAGE}/docs/"
cp -R docs/verification "${STAGE}/docs/verification"
cp docs/vocal-rack-editor.png docs/vocal-rack-editor-960.png docs/vocal-rack-editor-detail.png docs/vocal-rack-preset-browser.png docs/vocal-rack-spectral-warning.png docs/vocal-rack-long-name.png docs/vocal-rack-detail-cleanup.png docs/vocal-rack-detail-pitch.png docs/vocal-rack-detail-space.png docs/vocal-rack-detail-comp.png "${STAGE}/docs/"

find "${STAGE}" -name 'PkgInfo *' -delete 2>/dev/null || true
find "${STAGE}" -name '._*' -delete 2>/dev/null || true
find "${STAGE}" -name '.DS_Store' -delete 2>/dev/null || true
xattr -cr "${STAGE}"

rm -f "dist/${NAME}.zip"
(cd dist && zip -qry "${NAME}.zip" "${NAME}" -x "*.DS_Store" -x "__MACOSX/*")

# Verify the archive on a non-FileProvider path, including signatures.
ditto -x -k "dist/${NAME}.zip" "${EXTRACT_DIR}"
xattr -cr "${EXTRACT_DIR}/${NAME}"
codesign --verify --deep --strict "${EXTRACT_DIR}/${NAME}/VST3/ademusic Vocal Rack.vst3"
codesign --verify --deep --strict "${EXTRACT_DIR}/${NAME}/Components/ademusic Vocal Rack.component"
echo "Extracted bundle signatures: PASS"

if diff -r "${STAGE}" "${EXTRACT_DIR}/${NAME}" >/dev/null; then
    echo "ZIP extraction comparison: PASS"
else
    echo "ZIP extraction comparison: FAIL" >&2
    diff -r "${STAGE}" "${EXTRACT_DIR}/${NAME}" || true
    exit 3
fi

shasum -a 256 "dist/${NAME}.zip" | tee "dist/${NAME}.zip.sha256"
echo
echo "Staged ${STAGE} and dist/${NAME}.zip"
