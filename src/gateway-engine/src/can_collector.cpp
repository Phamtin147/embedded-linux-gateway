#include "can_collector.hpp"
#include <iostream>

CanCollector::CanCollector() = default;
CanCollector::~CanCollector() {
    close_socket();
}

bool CanCollector::init(const std::string& interface_name) {
    interface_name_ = interface_name;
    std::cout << "[CanCollector] Initialized interface: " << interface_name_ << std::endl;
    return true;
}

std::vector<nlohmann::json> CanCollector::read_frames(int timeout_ms) {
    std::vector<nlohmann::json> frames;
    return frames;
}

void CanCollector::close_socket() {
    if (socket_fd_ >= 0) {
        socket_fd_ = -1;
    }
}
