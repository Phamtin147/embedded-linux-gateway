#include "mqtt_client.hpp"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <mqtt/async_client.h>

class MqttClient::Impl : public virtual mqtt::callback {
public:
    Impl() = default;
    ~Impl() override {
        disconnect();
    }

    bool connect(const std::string& host, int port, const std::string& client_id,
                 const std::string& username, const std::string& password) {
        std::string server_uri = "tcp://" + host + ":" + std::to_string(port);
        std::cout << "[MqttClient] Connecting to " << server_uri << " (" << client_id << ")" << std::endl;

        try {
            client_ = std::make_unique<mqtt::async_client>(server_uri, client_id);
            client_->set_callback(*this);

            mqtt::connect_options_builder conn_builder;
            conn_builder.clean_session(true)
                        .keep_alive_interval(std::chrono::seconds(20))
                        .automatic_reconnect(true);

            if (!username.empty()) {
                conn_builder.user_name(username);
            }
            if (!password.empty()) {
                conn_builder.password(password);
            }

            conn_opts_ = conn_builder.finalize();

            mqtt::token_ptr conntok = client_->connect(conn_opts_);
            conntok->wait_for(std::chrono::seconds(4));
            is_connected_ = client_->is_connected();

            if (is_connected_) {
                std::cout << "[MqttClient] Successfully connected to " << server_uri << std::endl;
                
                // Resubscribe any registered topics
                std::lock_guard<std::mutex> lock(sub_mutex_);
                for (const auto& [topic, cb] : callbacks_) {
                    client_->subscribe(topic, 1);
                }
            } else {
                std::cerr << "[MqttClient] Initial connection timeout to " << server_uri << " (Will auto-retry)" << std::endl;
            }
            return is_connected_;
        } catch (const mqtt::exception& exc) {
            std::cerr << "[MqttClient] Exception during MQTT connect: " << exc.what() << std::endl;
            is_connected_ = false;
            return false;
        }
    }

    bool publish(const std::string& topic, const std::string& payload, int qos) {
        if (!client_ || !client_->is_connected()) {
            return false;
        }

        try {
            mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
            pubmsg->set_qos(qos);
            client_->publish(pubmsg)->wait_for(std::chrono::seconds(2));
            return true;
        } catch (const mqtt::exception& exc) {
            std::cerr << "[MqttClient] Publish failed: " << exc.what() << std::endl;
            return false;
        }
    }

    bool subscribe(const std::string& topic, MessageCallback callback, int qos) {
        {
            std::lock_guard<std::mutex> lock(sub_mutex_);
            callbacks_[topic] = callback;
        }

        if (client_ && client_->is_connected()) {
            try {
                client_->subscribe(topic, qos)->wait_for(std::chrono::seconds(2));
                std::cout << "[MqttClient] Subscribed to topic: " << topic << std::endl;
                return true;
            } catch (const mqtt::exception& exc) {
                std::cerr << "[MqttClient] Subscribe error: " << exc.what() << std::endl;
                return false;
            }
        }
        return true;
    }

    bool is_connected() const {
        return client_ && client_->is_connected();
    }

    void disconnect() {
        if (client_) {
            try {
                if (client_->is_connected()) {
                    client_->disconnect()->wait_for(std::chrono::seconds(2));
                }
            } catch (...) {}
            client_.reset();
        }
        is_connected_ = false;
    }

    // Callbacks from mqtt::callback
    void connection_lost(const std::string& cause) override {
        is_connected_ = false;
        std::cerr << "[MqttClient] Connection lost: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        std::cout << "[MqttClient] Downlink command received on " << topic << ": " << payload << std::endl;

        std::lock_guard<std::mutex> lock(sub_mutex_);
        auto it = callbacks_.find(topic);
        if (it != callbacks_.end() && it->second) {
            it->second(topic, payload);
        }
    }

    void delivery_complete(mqtt::delivery_token_ptr /*tok*/) override {}

private:
    std::unique_ptr<mqtt::async_client> client_;
    mqtt::connect_options conn_opts_;
    std::atomic<bool> is_connected_{false};
    std::mutex sub_mutex_;
    std::unordered_map<std::string, MessageCallback> callbacks_;
};

MqttClient::MqttClient() : pimpl_(std::make_unique<Impl>()) {}
MqttClient::~MqttClient() = default;

bool MqttClient::connect(const std::string& host, int port, const std::string& client_id,
                         const std::string& username, const std::string& password) {
    return pimpl_->connect(host, port, client_id, username, password);
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) {
    return pimpl_->publish(topic, payload, qos);
}

bool MqttClient::subscribe(const std::string& topic, MessageCallback callback, int qos) {
    return pimpl_->subscribe(topic, std::move(callback), qos);
}

bool MqttClient::is_connected() const {
    return pimpl_->is_connected();
}

void MqttClient::disconnect() {
    pimpl_->disconnect();
}
