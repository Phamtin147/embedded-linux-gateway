#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

source poky/oe-init-build-env build

echo "=== [1/3] Compiling gateway-engine C++ daemon (~5s) ==="
bitbake -c compile -f gateway-engine

echo "=== [2/3] Deploying binary to running QEMU (Port 2222) ==="
scp -P 2222 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    tmp/work/cortexa15t2hf-neon-poky-linux-gnueabi/gateway-engine/1.0/build/gateway-engine \
    root@127.0.0.1:/usr/bin/

echo "=== [3/3] Restarting gateway-engine service in QEMU ==="
ssh -p 2222 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    root@127.0.0.1 "systemctl restart gateway-engine.service && sleep 1 && systemctl status gateway-engine.service -l --no-pager"

echo "=== Deployment Succeeded! ==="
