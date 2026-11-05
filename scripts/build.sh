#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-release}"
GENERATOR="${GENERATOR:-Unix Makefiles}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
CMAKE_ARGS=(
    -S .
    -B "${BUILD_DIR}"
    -G "${GENERATOR}"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    -DVR_BUILD_TESTS=ON
)

if [[ -n "${VR_JUCE_PATH:-}" ]]; then
    CMAKE_ARGS+=("-DVR_JUCE_PATH=${VR_JUCE_PATH}")
fi

cmake "${CMAKE_ARGS[@]}"

cmake --build "${BUILD_DIR}" --target VocalRack_VST3 VocalRack_AU VocalRackTests -j"${JOBS}"

echo
echo "Built:"
echo "  ${BUILD_DIR}/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
echo "  ${BUILD_DIR}/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component"
echo "  ${BUILD_DIR}/VocalRackTests_artefacts/Release/VocalRackTests"
