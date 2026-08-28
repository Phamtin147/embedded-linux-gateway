#include "config_manager.hpp"
#include <fstream>
#include <iostream>

ConfigManager::ConfigManager(std::string config_path) : config_path_(std::move(config_path)) {}

bool ConfigManager::load() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Failed to open config file: " << config_path_ << std::endl;
        return false;
    }
    try {
        file >> config_data_;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_val) const {
    if (config_data_.contains(key) && config_data_[key].is_string()) {
        return config_data_[key].get<std::string>();
    }
    return default_val;
}
