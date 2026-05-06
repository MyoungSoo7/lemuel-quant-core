#!/usr/bin/env bash
# Build lemuel-quant-core from source and install binaries to /opt/lqc/bin.
# Run on the target host (each server builds its own subset).
#
# Env:
#   LQC_MODULES   space-separated list of CMake options to enable (default: all)
#                 e.g. "judge_engine market_feed"
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
INSTALL_BIN="${LQC_INSTALL_BIN:-/opt/lqc/bin}"

declare -A MODULE_ON=(
    [judge_engine]="-DLQC_BUILD_JUDGE_ENGINE=ON -DLQC_JUDGE_GRPC=ON"
    [market_feed]="-DLQC_BUILD_MARKET_FEED=ON"
    [stock_feed]="-DLQC_BUILD_STOCK_FEED=ON"
    [dart_crawler]="-DLQC_BUILD_DART_CRAWLER=ON"
    [news_pipeline]="-DLQC_BUILD_NEWS_PIPELINE=ON"
    [data_warehouse]="-DLQC_BUILD_DATA_WAREHOUSE=ON"
)
declare -A MODULE_OFF=(
    [judge_engine]="-DLQC_BUILD_JUDGE_ENGINE=OFF"
    [market_feed]="-DLQC_BUILD_MARKET_FEED=OFF"
    [stock_feed]="-DLQC_BUILD_STOCK_FEED=OFF"
    [dart_crawler]="-DLQC_BUILD_DART_CRAWLER=OFF"
    [news_pipeline]="-DLQC_BUILD_NEWS_PIPELINE=OFF"
    [data_warehouse]="-DLQC_BUILD_DATA_WAREHOUSE=OFF"
)

if [[ -z "${LQC_MODULES:-}" ]]; then
    LQC_MODULES="${!MODULE_ON[@]}"
fi

# Build a set of selected modules for set-difference.
declare -A SELECTED
for m in $LQC_MODULES; do SELECTED[$m]=1; done

CMAKE_FLAGS=(-DCMAKE_BUILD_TYPE=Release)
for m in $LQC_MODULES; do
    [[ -n "${MODULE_ON[$m]:-}" ]] || { echo "unknown module: $m"; exit 1; }
    # shellcheck disable=SC2206
    CMAKE_FLAGS+=(${MODULE_ON[$m]})
done
# Explicitly OFF for unselected modules so the CMakeLists defaults don't
# pull them into the build.
for m in "${!MODULE_ON[@]}"; do
    if [[ -z "${SELECTED[$m]:-}" ]]; then
        # shellcheck disable=SC2206
        CMAKE_FLAGS+=(${MODULE_OFF[$m]})
    fi
done

echo "[build] modules: $LQC_MODULES"
echo "[build] flags : ${CMAKE_FLAGS[*]}"

cmake -B "$BUILD" -S "$ROOT" -G Ninja "${CMAKE_FLAGS[@]}"
cmake --build "$BUILD" --parallel

mkdir -p "$INSTALL_BIN"
for m in $LQC_MODULES; do
    case "$m" in
        judge_engine)    cp "$BUILD/modules/judge-engine/judge_engine"        "$INSTALL_BIN/"
                         [[ -f "$BUILD/modules/judge-engine/grpc/judge_engine_grpc" ]] \
                            && cp "$BUILD/modules/judge-engine/grpc/judge_engine_grpc" "$INSTALL_BIN/" ;;
        market_feed)     cp "$BUILD/modules/market-feed/market_feed"          "$INSTALL_BIN/" ;;
        stock_feed)      cp "$BUILD/modules/stock-feed/stock_feed"            "$INSTALL_BIN/" ;;
        dart_crawler)    cp "$BUILD/modules/dart-crawler/dart_crawler"        "$INSTALL_BIN/" ;;
        news_pipeline)   cp "$BUILD/modules/news-pipeline/news_pipeline"      "$INSTALL_BIN/" ;;
        data_warehouse)  cp "$BUILD/modules/data-warehouse/data_warehouse"    "$INSTALL_BIN/" ;;
    esac
done

echo "[build] installed binaries to $INSTALL_BIN"
ls -la "$INSTALL_BIN"
