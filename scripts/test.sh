#!/usr/bin/env bash
set -euo pipefail
set -o pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-release}"
TEST_BIN="${BUILD_DIR}/VocalRackTests_artefacts/Release/VocalRackTests"

mkdir -p docs/verification

"${TEST_BIN}" | tee docs/verification/tests.log
"${TEST_BIN}" --latency | tee docs/verification/latency.txt

if [[ "${VR_RUN_BENCH:-1}" == "1" ]]; then
    "${TEST_BIN}" --bench | tee docs/verification/benchmark.txt
fi
