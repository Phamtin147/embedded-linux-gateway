#pragma once
#include <string>
#include <nlohmann/json.hpp>

class DownlinkHandler {
public:
    DownlinkHandler();
    void handle_command(const std::string& command_payload);

private:
    void execute_reboot();
    void execute_relay_toggle(int pin, bool state);
};
