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

# ONNX Runtime: only on hosts running news-pipeline
if [[ "${LQC_INSTALL_ONNX:-0}" == "1" ]]; then
    ORT_VER="${ORT_VER:-1.17.1}"
    cd /tmp
    curl -fsSL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-linux-x64-${ORT_VER}.tgz" \
        -o onnxruntime.tgz
    tar -xzf onnxruntime.tgz
    cp -r onnxruntime-linux-x64-${ORT_VER}/include/* /usr/local/include/
    cp -r onnxruntime-linux-x64-${ORT_VER}/lib/*    /usr/local/lib/
    ldconfig
fi

# AWS SDK for C++ (S3 only, for R2). Pre-built deb is rare; build from source
# *only* if data-warehouse runs on this host.
if [[ "${LQC_INSTALL_AWS_S3:-0}" == "1" ]]; then
    apt-get install -y libssl-dev libcurl4-openssl-dev zlib1g-dev
    cd /tmp
    git clone --depth 1 --branch 1.11.300 \
        https://github.com/aws/aws-sdk-cpp.git
    cd aws-sdk-cpp
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
