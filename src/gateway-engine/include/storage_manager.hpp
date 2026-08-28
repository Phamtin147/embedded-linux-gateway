#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class StorageManager {
public:
    explicit StorageManager(std::string db_path);
    ~StorageManager();

    bool init();
    bool push_payload(const std::string& topic, const std::string& payload);
    std::vector<std::pair<int64_t, std::pair<std::string, std::string>>> fetch_pending(int limit = 50);
    bool mark_delivered(int64_t row_id);
    size_t get_pending_count();

private:
    std::string db_path_;
    void* db_handle_{nullptr};
};
