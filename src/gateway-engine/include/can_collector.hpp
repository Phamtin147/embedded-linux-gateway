#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class CanCollector {
public:
    CanCollector();
    ~CanCollector();

    bool init(const std::string& interface_name);
    std::vector<nlohmann::json> read_frames(int timeout_ms = 100);
    void close_socket();

private:
    int socket_fd_{-1};
    std::string interface_name_;
};
