#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class DownlinkHandler {
public:
    using ResponseCallback = std::function<void(const std::string& response_topic, const std::string& response_payload)>;

    DownlinkHandler();
    void set_response_callback(ResponseCallback cb) { response_cb_ = std::move(cb); }
    void handle_command(const std::string& topic, const std::string& command_payload);

private:
    ResponseCallback response_cb_;
    void execute_reboot();
    void execute_relay_toggle(int pin, bool state);
};
