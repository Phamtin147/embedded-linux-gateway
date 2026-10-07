#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

struct _modbus;
typedef struct _modbus modbus_t;

struct ModbusRegisterConfig {
    std::string name;
    int address{0};
    std::string type{"int16"};
    double scale{1.0};
};

class ModbusCollector {
public:
    ModbusCollector();
    ~ModbusCollector();

    bool init(const nlohmann::json& config);
    nlohmann::json poll_data();
    void disconnect();
    bool is_connected() const { return connected_; }

private:
    modbus_t* mb_ctx_{nullptr};
    bool connected_{false};
    nlohmann::json modbus_config_;
    std::vector<ModbusRegisterConfig> registers_;
    int failed_poll_count_{0};

    bool connect_rtu();
};
