#pragma once
#include <string>
#include <nlohmann/json.hpp>

class ConfigManager {
public:
    explicit ConfigManager(std::string config_path);
    bool load();
    const nlohmann::json& get() const { return config_data_; }
    std::string get_string(const std::string& key, const std::string& default_val = "") const;

private:
    std::string config_path_;
    nlohmann::json config_data_;
};
