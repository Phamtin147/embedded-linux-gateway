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
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "========================================" << std::endl;
    std::cout << " Industrial IoT Gateway Engine v1.0.0" << std::endl;
    std::cout << " Target: Embedded Linux (Yocto / QEMU)" << std::endl;
    std::cout << "========================================" << std::endl;

    ConfigManager config(config_path);
    config.load();

    StorageManager storage(config.get_string("db_path", "/data/gateway.db"));
    storage.init();

    ModbusCollector modbus;
    modbus.init(config.get()["modbus"]);

    CanCollector can;
    can.init(config.get()["can"]["interface"]);

    MqttClient mqtt;
    mqtt.connect(
        config.get()["mqtt"]["broker_host"],
        config.get()["mqtt"]["broker_port"],
        config.get()["mqtt"]["client_id"]
    );

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
