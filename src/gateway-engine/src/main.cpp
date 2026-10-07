#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#include "config_manager.hpp"
#include "modbus_collector.hpp"
#include "can_collector.hpp"
#include "mqtt_client.hpp"
#include "storage_manager.hpp"
#include "downlink_handler.hpp"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    std::cout << "\n[Main] Signal received (" << signum << "), shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string config_path = "/etc/gateway/gateway_config.json";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg.rfind("--", 0) != 0) {
            config_path = arg;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << " Industrial IoT Gateway Engine v1.0.0" << std::endl;
    std::cout << " Target: Embedded Linux (Yocto / QEMU)" << std::endl;
    std::cout << " Using config: " << config_path << std::endl;
    std::cout << "========================================" << std::endl;

    ConfigManager config(config_path);
    if (!config.load()) {
        std::cerr << "[Main] Warning: Failed to load config from " << config_path 
                  << ", trying fallback /etc/gateway/gateway_config.json..." << std::endl;
        ConfigManager fallback_config("/etc/gateway/gateway_config.json");
        if (fallback_config.load()) {
            config = std::move(fallback_config);
        } else {
            std::cerr << "[Main] Error: Could not load any valid configuration!" << std::endl;
        }
    }

    try {
        StorageManager storage(config.get_string("db_path", "/data/gateway.db"));
        storage.init();

        ModbusCollector modbus;
        if (config.get().contains("modbus")) {
            modbus.init(config.get()["modbus"]);
        }

        CanCollector can;
        if (config.get().contains("can") && config.get()["can"].contains("interface")) {
            can.init(config.get()["can"]["interface"]);
        }

        MqttClient mqtt;
        if (config.get().contains("mqtt")) {
            mqtt.connect(
                config.get()["mqtt"].value("broker_host", "127.0.0.1"),
                config.get()["mqtt"].value("broker_port", 1883),
                config.get()["mqtt"].value("client_id", "iiot-gateway-001")
            );
        }

#ifdef HAVE_SYSTEMD
        sd_notify(0, "READY=1");
#endif


    while (g_running) {
        nlohmann::json mb_data = modbus.poll_data();
        if (!mb_data.empty()) {
            std::string payload = mb_data.dump();
            if (!mqtt.publish(config.get()["mqtt"]["telemetry_topic"], payload)) {
                storage.push_payload(config.get()["mqtt"]["telemetry_topic"], payload);
            }
        }

#ifdef HAVE_SYSTEMD
        sd_notify(0, "WATCHDOG=1");
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(config.get().value("polling_interval_ms", 1000)));
    }

    std::cout << "[Main] Gateway daemon terminated cleanly." << std::endl;
    return 0;
}
