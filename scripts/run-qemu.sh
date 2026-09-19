#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

source poky/oe-init-build-env build
echo "=== Starting QEMU ARM Simulation ==="
runqemu qemuarm iiot-gateway-image slirp nographic

