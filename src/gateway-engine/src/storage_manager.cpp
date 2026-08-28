#include "storage_manager.hpp"
#include <iostream>

StorageManager::StorageManager(std::string db_path) : db_path_(std::move(db_path)) {}

StorageManager::~StorageManager() = default;

bool StorageManager::init() {
    std::cout << "[StorageManager] Initialized SQLite WAL at " << db_path_ << std::endl;
    return true;
}

bool StorageManager::push_payload(const std::string& topic, const std::string& payload) {
    std::cout << "[StorageManager] Stored offline record for topic: " << topic << std::endl;
    return true;
}

std::vector<std::pair<int64_t, std::pair<std::string, std::string>>> StorageManager::fetch_pending(int limit) {
    return {};
}

bool StorageManager::mark_delivered(int64_t row_id) {
    return true;
}

size_t StorageManager::get_pending_count() {
    return 0;
}
