#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class ModbusCollector {
public:
    ModbusCollector();
    ~ModbusCollector();

    bool init(const nlohmann::json& config);
    nlohmann::json poll_data();
    void disconnect();

private:
    void* mb_ctx_{nullptr};
    bool connected_{false};
    nlohmann::json modbus_config_;
};
