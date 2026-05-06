#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

cmake -B "$BUILD_DIR" -S "$ROOT" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DLQC_BUILD_JUDGE_ENGINE=ON \
    "$@"

cmake --build "$BUILD_DIR" --parallel
