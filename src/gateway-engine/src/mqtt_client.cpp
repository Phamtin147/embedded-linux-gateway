#include "mqtt_client.hpp"
#include <iostream>

MqttClient::MqttClient() = default;
MqttClient::~MqttClient() {
    disconnect();
}

bool MqttClient::connect(const std::string& host, int port, const std::string& client_id) {
    host_ = host;
    port_ = port;
    std::cout << "[MqttClient] Connecting to " << host_ << ":" << port_ << " (" << client_id << ")" << std::endl;
    is_connected_ = true;
    return true;
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) {
    if (!is_connected_) return false;
    std::cout << "[MqttClient] Publish -> " << topic << " : " << payload << std::endl;
    return true;
}

bool MqttClient::subscribe(const std::string& topic, MessageCallback callback) {
    if (!is_connected_) return false;
    std::cout << "[MqttClient] Subscribed to -> " << topic << std::endl;
    return true;
}

void MqttClient::disconnect() {
    is_connected_ = false;
}
