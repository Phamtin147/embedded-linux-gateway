SUMMARY = "Industrial IoT Gateway Core Daemon"
DESCRIPTION = "High-performance C++17 daemon for Modbus/CAN acquisition, SQLite failover and MQTT Cloud sync."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:${THISDIR}/../../../../src/gateway-engine:"

SRC_URI = " \
    file://CMakeLists.txt \
    file://config \
    file://include \
    file://src \
    file://gateway-engine.service \
"


S = "${WORKDIR}"

inherit cmake systemd

DEPENDS = " \
    libmodbus \
    paho-mqtt-cpp \
    sqlite3 \
    nlohmann-json \
    systemd \
"

RDEPENDS:${PN} = " \
    libmodbus \
    paho-mqtt-cpp \
    sqlite3 \
"

SYSTEMD_SERVICE:${PN} = "gateway-engine.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/gateway-engine.service ${D}${systemd_system_unitdir}/

    install -d ${D}${sysconfdir}/gateway
    install -m 0644 ${WORKDIR}/config/default_config.json ${D}${sysconfdir}/gateway/gateway_config.json
}

FILES:${PN} += " \
    ${bindir}/gateway-engine \
    ${systemd_system_unitdir}/gateway-engine.service \
    ${sysconfdir}/gateway/gateway_config.json \
"
