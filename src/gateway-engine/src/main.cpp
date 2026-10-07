#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>

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
    std::cout << "\n[Main] Signal received (" << signum << "), initiating clean shutdown..." << std::endl;
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

    std::cout << "==================================================" << std::endl;
    std::cout << "   Industrial IoT Gateway Engine Core Daemon" << std::endl;
    std::cout << "   Embedded Linux (Scarthgap 5.0 LTS / ARMv7-A)" << std::endl;
    std::cout << "   Active Config: " << config_path << std::endl;
    std::cout << "==================================================" << std::endl;

    ConfigManager config(config_path);
    if (!config.load()) {
        std::cerr << "[Main] Warning: Failed to load config from " << config_path 
                  << ", trying fallback /etc/gateway/gateway_config.json..." << std::endl;
        ConfigManager fallback_config("/etc/gateway/gateway_config.json");
        if (fallback_config.load()) {
            config = std::move(fallback_config);
        } else {
            std::cerr << "[Main] Error: Could not load any valid configuration file!" << std::endl;
        }
    }

    // 1. Storage Manager (SQLite WAL Store-and-Forward offline buffer)
    std::string db_path = "/data/gateway.db";
    size_t max_buffer_mb = 100;
    if (config.get().contains("storage")) {
        db_path = config.get()["storage"].value("db_path", "/data/gateway.db");
        max_buffer_mb = config.get()["storage"].value("max_buffer_size_mb", 100);
    }
    StorageManager storage(db_path, max_buffer_mb);
    if (!storage.init()) {
        std::cerr << "[Main] Warning: SQLite WAL buffer could not be initialized at " << db_path << std::endl;
    }

    // 2. Modbus RTU Collector
    ModbusCollector modbus;
    if (config.get().contains("modbus") && config.get()["modbus"].value("enabled", false)) {
        modbus.init(config.get()["modbus"]);
    }

    // 3. Linux SocketCAN Collector
    CanCollector can;
    if (config.get().contains("can") && config.get()["can"].value("enabled", false)) {
        std::string iface = config.get()["can"].value("interface", "vcan0");
        std::vector<CanFilterConfig> can_filters;
        if (config.get()["can"].contains("filters") && config.get()["can"]["filters"].is_array()) {
            for (const auto& f : config.get()["can"]["filters"]) {
                can_filters.push_back({f.value("can_id", 0u), f.value("mask", 0u)});
            }
        }
        can.init(iface, can_filters);
    }

    // 4. Downlink RPC Handler
    DownlinkHandler downlink;

    // 5. Async MQTT Client (Paho MQTT C++)
    MqttClient mqtt;
    std::string telemetry_topic = "v1/devices/me/telemetry";
    std::string command_topic = "v1/devices/me/rpc/request/+";
    int poll_interval_ms = config.get().value("polling_interval_ms", 1000);

    if (config.get().contains("mqtt")) {
        const auto& mqtt_cfg = config.get()["mqtt"];
        std::string host = mqtt_cfg.value("broker_host", "10.0.2.2");
        int port = mqtt_cfg.value("broker_port", 1883);
        std::string client_id = mqtt_cfg.value("client_id", "iiot-gateway-001");
        std::string username = mqtt_cfg.value("username", "");
        std::string password = mqtt_cfg.value("password", "");

        telemetry_topic = mqtt_cfg.value("telemetry_topic", "v1/devices/me/telemetry");
        command_topic = mqtt_cfg.value("command_topic", "v1/devices/me/rpc/request/+");

        // Wire downlink response to MQTT publish
        downlink.set_response_callback([&mqtt](const std::string& resp_topic, const std::string& resp_payload) {
            std::cout << "[Main] Downlink replying to " << resp_topic << " -> " << resp_payload << std::endl;
            mqtt.publish(resp_topic, resp_payload, 1);
        });

        // Subscribe to downlink commands
        mqtt.subscribe(command_topic, [&downlink](const std::string& topic, const std::string& payload) {
            downlink.handle_command(topic, payload);
        });

        mqtt.connect(host, port, client_id, username, password);
    }

#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1");
    std::cout << "[Main] Systemd service state notified: READY=1" << std::endl;
#endif

    std::cout << "[Main] Engine daemon entering main acquisition loop..." << std::endl;

    while (g_running) {
        // A. FIFO Flush offline records when MQTT is online
        if (mqtt.is_connected()) {
            auto pending = storage.fetch_pending(20);
            for (const auto& [id, record] : pending) {
                const auto& [topic, payload] = record;
                if (mqtt.publish(topic, payload, 1)) {
                    storage.mark_delivered(id);
                } else {
                    break; // Network buffer clogged, pause flushing until next round
                }
            }
        }

        // B. SocketCAN Data Ingest
        if (can.is_open()) {
            auto frames = can.read_frames(20);
            for (const auto& frame : frames) {
                std::string can_payload = frame.dump();
                if (!mqtt.publish(telemetry_topic, can_payload, 1)) {
                    storage.push_payload(telemetry_topic, can_payload);
                }
            }
        }

        // C. Modbus RTU Data Ingest
        nlohmann::json mb_data = modbus.poll_data();
        if (!mb_data.empty()) {
            std::string payload = mb_data.dump();
            if (!mqtt.publish(telemetry_topic, payload, 1)) {
                storage.push_payload(telemetry_topic, payload);
            }
        }

#ifdef HAVE_SYSTEMD
        sd_notify(0, "WATCHDOG=1");
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }

    std::cout << "[Main] Cleaning up resources..." << std::endl;
    modbus.disconnect();
    can.close_socket();
    mqtt.disconnect();
    storage.close();

    std::cout << "[Main] Gateway daemon terminated cleanly." << std::endl;
    return 0;
}
