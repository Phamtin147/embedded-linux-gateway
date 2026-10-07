#include "downlink_handler.hpp"
#include <iostream>
#include <cstdlib>
#include <unistd.h>

DownlinkHandler::DownlinkHandler() = default;

void DownlinkHandler::handle_command(const std::string& topic, const std::string& command_payload) {
    std::cout << "[DownlinkHandler] Processing command from " << topic << ": " << command_payload << std::endl;

    try {
        nlohmann::json cmd = nlohmann::json::parse(command_payload);
        std::string method = cmd.value("method", "");
        nlohmann::json response = nlohmann::json::object();

        if (method == "reboot") {
            response["status"] = "success";
            response["message"] = "Reboot triggered by Cloud RPC";
            execute_reboot();
        } else if (method == "setRelay" || method == "setValue") {
            bool state = cmd.value("params", false);
            int pin = cmd.value("pin", 18);
            execute_relay_toggle(pin, state);
            response["status"] = "success";
            response["relay_state"] = state;
            response["pin"] = pin;
        } else if (method == "ping") {
            response["status"] = "pong";
            response["daemon"] = "gateway-engine";
            response["version"] = "1.0.0";
        } else {
            response["status"] = "unknown_method";
            response["method"] = method;
        }

        // If topic is ThingsBoard RPC request format: v1/devices/me/rpc/request/{id}
        // Then respond to: v1/devices/me/rpc/response/{id}
        if (response_cb_) {
            std::string resp_topic = topic;
            size_t req_pos = topic.find("/request/");
            if (req_pos != std::string::npos) {
                resp_topic = topic.substr(0, req_pos) + "/response/" + topic.substr(req_pos + 9);
            } else {
                resp_topic = topic + "/response";
            }
            response_cb_(resp_topic, response.dump());
        }
    } catch (const std::exception& e) {
        std::cerr << "[DownlinkHandler] Error parsing JSON command: " << e.what() << std::endl;
    }
}

void DownlinkHandler::execute_reboot() {
    std::cout << "[DownlinkHandler] Syncing disks and scheduling reboot..." << std::endl;
    sync();
    // In production Linux, reboot can be invoked via systemd:
    // system("systemctl reboot");
}

void DownlinkHandler::execute_relay_toggle(int pin, bool state) {
    std::cout << "[DownlinkHandler] GPIO Actuator Control: Pin " << pin 
              << " set to " << (state ? "HIGH (ON)" : "LOW (OFF)") << std::endl;
}
