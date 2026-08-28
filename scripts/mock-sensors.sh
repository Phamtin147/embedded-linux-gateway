#!/usr/bin/env bash
set -e

echo "=== [1/2] Setting up Virtual CAN Interface (vcan0) ==="
sudo modprobe vcan || true
sudo ip link add dev vcan0 type vcan || true
sudo ip link set up vcan0 || true
echo "vcan0 is UP and ready."

echo "=== [2/2] Setting up Virtual Modbus Serial (socat) ==="
echo "Creating pseudo-terminals /dev/ttyVIRT0 <-> /dev/ttyVIRT1 ..."
echo "Run: socat -d -d pty,raw,echo=0,link=/tmp/ttyVIRT0 pty,raw,echo=0,link=/tmp/ttyVIRT1"
