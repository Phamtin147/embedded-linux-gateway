#include "modbus_collector.hpp"
#include <iostream>

ModbusCollector::ModbusCollector() = default;
ModbusCollector::~ModbusCollector() {
    disconnect();
}

bool ModbusCollector::init(const nlohmann::json& config) {
    modbus_config_ = config;
    std::cout << "[ModbusCollector] Initialized with config." << std::endl;
    connected_ = true;
    return true;
}

nlohmann::json ModbusCollector::poll_data() {
    nlohmann::json result = nlohmann::json::object();
    if (!connected_) return result;
    
    result["temperature"] = 26.5;
    result["humidity"] = 60.2;
    result["pressure"] = 1013.2;
    return result;
}

void ModbusCollector::disconnect() {
    connected_ = false;
}
