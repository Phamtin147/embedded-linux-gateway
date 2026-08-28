#include "downlink_handler.hpp"
#include <iostream>

DownlinkHandler::DownlinkHandler() = default;

void DownlinkHandler::handle_command(const std::string& command_payload) {
    std::cout << "[DownlinkHandler] Processing command payload: " << command_payload << std::endl;
}

void DownlinkHandler::execute_reboot() {
    std::cout << "[DownlinkHandler] Triggering system reboot..." << std::endl;
}

void DownlinkHandler::execute_relay_toggle(int pin, bool state) {
    std::cout << "[DownlinkHandler] Toggling GPIO pin " << pin << " to state " << state << std::endl;
}
