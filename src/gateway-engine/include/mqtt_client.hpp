#pragma once
#include <string>
#include <memory>
#include <functional>

class MqttClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttClient();
    ~MqttClient();

    bool connect(const std::string& host, int port, const std::string& client_id,
                 const std::string& username = "", const std::string& password = "");
    bool publish(const std::string& topic, const std::string& payload, int qos = 1);
    bool subscribe(const std::string& topic, MessageCallback callback, int qos = 1);
    bool is_connected() const;
    void disconnect();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};
