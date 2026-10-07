#include "storage_manager.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <sqlite3.h>

StorageManager::StorageManager(std::string db_path, size_t max_buffer_size_mb)
    : db_path_(std::move(db_path)), max_buffer_size_mb_(max_buffer_size_mb) {}

StorageManager::~StorageManager() {
    close();
}

bool StorageManager::init() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    // Ensure parent directory exists
    try {
        std::filesystem::path p(db_path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Warning: Filesystem directory check failed: " << e.what() << std::endl;
    }

    int rc = sqlite3_open_v2(db_path_.c_str(), &db_, 
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[StorageManager] Failed to open SQLite DB: " 
                  << (db_ ? sqlite3_errmsg(db_) : "Unknown error") << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Configure WAL mode & durability tuning for Industrial Flash endurance
    char* err_msg = nullptr;
    const char* init_sql = 
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA busy_timeout = 5000;"
        "CREATE TABLE IF NOT EXISTS telemetry_buffer ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  topic TEXT NOT NULL,"
        "  payload TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_telemetry_id ON telemetry_buffer(id ASC);";

    rc = sqlite3_exec(db_, init_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "[StorageManager] SQL init error: " << (err_msg ? err_msg : "Unknown") << std::endl;
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }

    std::cout << "[StorageManager] Initialized SQLite WAL store-and-forward at: " << db_path_ << std::endl;
    return true;
}

bool StorageManager::push_payload(const std::string& topic, const std::string& payload) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (!db_) return false;

    prune_if_needed();

    const char* insert_sql = "INSERT INTO telemetry_buffer (topic, payload, created_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[StorageManager] Prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, topic.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now_ms);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[StorageManager] Stored offline telemetry -> " << topic << " (" << payload.size() << " bytes)" << std::endl;
    } else {
        std::cerr << "[StorageManager] Failed to insert telemetry: " << sqlite3_errmsg(db_) << std::endl;
    }
    return success;
}

std::vector<std::pair<int64_t, std::pair<std::string, std::string>>> StorageManager::fetch_pending(int limit) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<std::pair<int64_t, std::pair<std::string, std::string>>> records;
    if (!db_) return records;

    const char* select_sql = "SELECT id, topic, payload FROM telemetry_buffer ORDER BY id ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return records;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char* topic_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* payload_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        records.emplace_back(id, std::make_pair(
            topic_str ? std::string(topic_str) : "",
            payload_str ? std::string(payload_str) : ""
        ));
    }

    sqlite3_finalize(stmt);
    return records;
}

bool StorageManager::mark_delivered(int64_t row_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (!db_) return false;

    const char* delete_sql = "DELETE FROM telemetry_buffer WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, row_id);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

size_t StorageManager::get_pending_count() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (!db_) return 0;

    const char* count_sql = "SELECT COUNT(*) FROM telemetry_buffer;";
    sqlite3_stmt* stmt = nullptr;
    size_t count = 0;

    if (sqlite3_prepare_v2(db_, count_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

void StorageManager::prune_if_needed() {
    try {
        if (std::filesystem::exists(db_path_)) {
            auto file_size = std::filesystem::file_size(db_path_);
            size_t max_bytes = max_buffer_size_mb_ * 1024 * 1024;
            if (file_size > max_bytes) {
                // Remove oldest 500 records to free storage safely
                const char* prune_sql = "DELETE FROM telemetry_buffer WHERE id IN "
                                        "(SELECT id FROM telemetry_buffer ORDER BY id ASC LIMIT 500);";
                sqlite3_exec(db_, prune_sql, nullptr, nullptr, nullptr);
                std::cout << "[StorageManager] Pruned 500 oldest records due to buffer size quota ("
                          << (file_size / (1024 * 1024)) << "MB / " << max_buffer_size_mb_ << "MB)" << std::endl;
            }
        }
    } catch (...) {}
}

void StorageManager::close() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}
