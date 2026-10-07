#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

struct CanFilterConfig {
    uint32_t can_id{0};
    uint32_t mask{0};
};

class CanCollector {
public:
    CanCollector();
    ~CanCollector();

    bool init(const std::string& interface_name, const std::vector<CanFilterConfig>& filters = {});
    std::vector<nlohmann::json> read_frames(int timeout_ms = 50);
    bool send_frame(uint32_t can_id, const std::vector<uint8_t>& data);
    void close_socket();
    bool is_open() const { return socket_fd_ >= 0; }

private:
    int socket_fd_{-1};
    std::string interface_name_;
};
