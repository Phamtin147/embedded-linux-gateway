#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <mutex>

struct sqlite3;

class StorageManager {
public:
    explicit StorageManager(std::string db_path, size_t max_buffer_size_mb = 50);
    ~StorageManager();

    bool init();
    bool push_payload(const std::string& topic, const std::string& payload);
    std::vector<std::pair<int64_t, std::pair<std::string, std::string>>> fetch_pending(int limit = 50);
    bool mark_delivered(int64_t row_id);
    size_t get_pending_count();
    void close();

private:
    std::string db_path_;
    size_t max_buffer_size_mb_{50};
    sqlite3* db_{nullptr};
    std::mutex db_mutex_;

    void prune_if_needed();
};
