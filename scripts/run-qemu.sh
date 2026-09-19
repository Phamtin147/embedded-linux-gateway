#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

source poky/oe-init-build-env build

CONF_FILE="tmp/deploy/images/qemuarm/iiot-gateway-image-qemuarm.rootfs.qemuboot.conf"
if [ ! -f "$CONF_FILE" ]; then
    echo "Error: $CONF_FILE not found. Please run ./scripts/build-image.sh first."
    exit 1
fi

echo "=== Starting QEMU ARM Simulation ==="
echo "Tips: To exit QEMU, press 'Ctrl + A' then 'X' (or type 'poweroff')"
runqemu "$CONF_FILE" slirp nographic
