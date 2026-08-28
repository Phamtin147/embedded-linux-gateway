#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

if [ ! -d "build" ]; then
    echo "Error: build directory not found. Please run ./scripts/env-setup.sh first."
    exit 1
fi

source poky/oe-init-build-env build
echo "=== Starting BitBake build for iiot-gateway-image ==="
bitbake iiot-gateway-image
