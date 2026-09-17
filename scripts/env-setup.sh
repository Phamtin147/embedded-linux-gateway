#!/usr/bin/env bash
set -e

BRANCH="scarthgap"

echo "=== [1/4] Cloning Poky Reference System ($BRANCH) ==="
if [ ! -d "poky" ]; then
    git clone --depth 1 -b $BRANCH https://github.com/yoctoproject/poky.git poky
fi

echo "=== [2/4] Cloning meta-openembedded ($BRANCH) ==="
if [ ! -d "meta-openembedded" ]; then
    git clone --depth 1 -b $BRANCH https://github.com/openembedded/meta-openembedded.git
fi

echo "=== [3/4] Cloning meta-rauc ($BRANCH) ==="
if [ ! -d "meta-rauc" ]; then
    git clone --depth 1 -b $BRANCH https://github.com/rauc/meta-rauc.git
fi


echo "=== [4/4] Setting up Build Environment ==="
source poky/oe-init-build-env build

echo "Adding layers to bblayers.conf..."
bitbake-layers add-layer ../meta-openembedded/meta-oe || true
bitbake-layers add-layer ../meta-openembedded/meta-python || true
bitbake-layers add-layer ../meta-openembedded/meta-networking || true
bitbake-layers add-layer ../meta-rauc || true
bitbake-layers add-layer ../layers/meta-iiot-gateway || true

echo "=== Yocto Environment Ready! Run 'bitbake iiot-gateway-image' to build ==="
