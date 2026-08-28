SUMMARY = "Industrial IoT Gateway Custom Production Image"
DESCRIPTION = "Hardened, Read-Only RootFS Image with RAUC OTA, Industrial protocols and C++ Edge Daemon."
LICENSE = "MIT"

inherit core-image

# Enable Read-Only RootFS and systemd features
IMAGE_FEATURES += " \
    read-only-rootfs \
    ssh-server-openssh \
"

# Base & Essential Packages
IMAGE_INSTALL += " \
    packagegroup-core-boot \
    systemd \
    systemd-analyze \
    iproute2 \
    iptables \
    sqlite3 \
"

# Industrial Communication & Protocol Tools
IMAGE_INSTALL += " \
    can-utils \
    libmodbus \
    paho-mqtt-cpp \
    socat \
"

# OTA & System Reliability
IMAGE_INSTALL += " \
    rauc \
    gateway-engine \
"

# Image Configuration
IMAGE_FSTYPES = "tar.bz2 wic wic.bmap ext4"
export IMAGE_BASENAME = "iiot-gateway-image"
