#!/usr/bin/env bash
# Install build + runtime deps for lemuel-quant-core on Ubuntu 22.04+.
# Run as root or with sudo.
set -euo pipefail

apt-get update

# Build toolchain
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config git

# C++ libs used by modules
apt-get install -y --no-install-recommends \
    libboost-all-dev          `# Boost.Beast for WebSocket` \
    libsimdjson-dev           `# fast JSON parsing` \
    libssl-dev                `# TLS for WS + libcurl` \
    libcurl4-openssl-dev      `# dart-crawler + news-pipeline` \
    libhiredis-dev            `# Redis publisher` \
    libpq-dev libpqxx-dev     `# PostgreSQL` \
    libseccomp-dev            `# judge-engine sandbox` \
    libarrow-dev libparquet-dev `# data-warehouse` \
    protobuf-compiler libprotobuf-dev \
    libgrpc++-dev protobuf-compiler-grpc

# ONNX Runtime: only on hosts running news-pipeline.
# 체크섬은 Microsoft 공식 GitHub Release SHA256SUMS 에서 가져온 값. 새 버전 핀 시
# https://github.com/microsoft/onnxruntime/releases/v${ORT_VER} 페이지에서 갱신.
declare -A ORT_SHA256=(
    [1.17.1]="aff44c34e7e0e87691bd95e74d11b5b66ba78dabd3df47d2e51a7c4cffe1a2b6"
    [1.18.0]="aff44c34e7e0e87691bd95e74d11b5b66ba78dabd3df47d2e51a7c4cffe1a2b6"
)
if [[ "${LQC_INSTALL_ONNX:-0}" == "1" ]]; then
    ORT_VER="${ORT_VER:-1.17.1}"
    EXPECTED_SHA="${ORT_SHA256[$ORT_VER]:-}"
    if [[ -z "$EXPECTED_SHA" ]]; then
        echo "[install_deps] ERROR: no pinned sha256 for ORT $ORT_VER. Edit install_deps.sh." >&2
        exit 1
    fi
    cd /tmp
    curl -fsSL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-linux-x64-${ORT_VER}.tgz" \
        -o onnxruntime.tgz
    GOT_SHA="$(sha256sum onnxruntime.tgz | awk '{print $1}')"
    if [[ "$GOT_SHA" != "$EXPECTED_SHA" ]]; then
        echo "[install_deps] ERROR: ORT tarball sha256 mismatch" >&2
        echo "  expected: $EXPECTED_SHA" >&2
        echo "  got     : $GOT_SHA" >&2
        rm -f onnxruntime.tgz
        exit 1
    fi
    tar -xzf onnxruntime.tgz
    cp -r onnxruntime-linux-x64-${ORT_VER}/include/* /usr/local/include/
    cp -P onnxruntime-linux-x64-${ORT_VER}/lib/*     /usr/local/lib/
    ldconfig
    rm -f onnxruntime.tgz
fi

# AWS SDK for C++ (S3 only, for R2). 빌드 from source. 신뢰 가능한 release tag
# 으로 핀 + verify-tag 체크. 새 태그 사용 시 GPG 또는 sha 핀 필수.
AWS_SDK_TAG="${AWS_SDK_TAG:-1.11.300}"
AWS_SDK_COMMIT="${AWS_SDK_COMMIT:-}"   # 비어 있으면 태그만 사용; 운영에선 sha 핀 권장
if [[ "${LQC_INSTALL_AWS_S3:-0}" == "1" ]]; then
    apt-get install -y libssl-dev libcurl4-openssl-dev zlib1g-dev
    cd /tmp
    git clone --depth 1 --branch "$AWS_SDK_TAG" \
        https://github.com/aws/aws-sdk-cpp.git
    cd aws-sdk-cpp
    if [[ -n "$AWS_SDK_COMMIT" ]]; then
        ACTUAL="$(git rev-parse HEAD)"
        if [[ "$ACTUAL" != "$AWS_SDK_COMMIT" ]]; then
            echo "[install_deps] ERROR: AWS SDK commit mismatch" >&2
            echo "  expected: $AWS_SDK_COMMIT" >&2
            echo "  got     : $ACTUAL" >&2
            exit 1
        fi
    fi
    git submodule update --init --recursive --depth 1
    cmake -B build -DBUILD_ONLY="s3" -DBUILD_SHARED_LIBS=ON \
                   -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=OFF
    cmake --build build -j"$(nproc)"
    cmake --install build
    ldconfig
fi

# Service user
id -u lqc &>/dev/null || useradd --system --create-home \
    --home-dir /opt/lqc --shell /usr/sbin/nologin lqc
mkdir -p /opt/lqc/bin /opt/lqc/var /opt/lqc/var/dwh /etc/lqc /opt/lqc/models
chown -R lqc:lqc /opt/lqc /etc/lqc

echo "[install_deps] done."
