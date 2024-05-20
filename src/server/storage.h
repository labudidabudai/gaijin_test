#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <rapidjson/document.h>

class Storage {
public:
    struct Stat {
        size_t get_count = 0;
        size_t set_count = 0;
    };

public:
    Storage(const std::string& path);

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    Storage(Storage&&) = delete;
    Storage& operator=(Storage&&) = delete;

    ~Storage();

    Stat set(const std::string& key, const std::string& value);
    std::pair<std::optional<std::string>, Stat> get(const std::string& key) const;

    void dump_to_file() const;

    std::pair<Stat, Stat> get_and_reset_stats() const;

private:
    rapidjson::Document dictionary_;
    mutable std::shared_mutex dictionary_mutex_;

    mutable std::atomic_bool need_dump_ = false;

    struct AtomicStat {
        std::atomic<size_t> get_count = 0;
        std::atomic<size_t> set_count = 0;

        void inc_get() {
            get_count.fetch_add(1);
        }
        void inc_set() {
            set_count.fetch_add(1);
        }

        Stat take() const {
            return {get_count.load(), set_count.load()};
        }

        Stat take_and_reset() {
            return {get_count.exchange(0), set_count.exchange(0)};
        }
    };

    AtomicStat& get_key_stats(const std::string& key) const;

    mutable std::unordered_map<std::string, AtomicStat> stats_per_key_;
    mutable AtomicStat total_stats_;
    mutable AtomicStat last_period_total_stats_;
    mutable std::shared_mutex stats_mutex_;

    const std::string path_;
    const std::string tmp_path_;
};
