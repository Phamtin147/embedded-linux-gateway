#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttClient();
    ~MqttClient();

    bool connect(const std::string& host, int port, const std::string& client_id);
    bool publish(const std::string& topic, const std::string& payload, int qos = 1);
    bool subscribe(const std::string& topic, MessageCallback callback);
    bool is_connected() const { return is_connected_; }
    void disconnect();

private:
    bool is_connected_{false};
    std::string host_;
    int port_{1883};
};
