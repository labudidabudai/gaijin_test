#include "storage.h"

#include <filesystem>
#include <fstream>
#include <mutex>

#include <iostream>

#include <yaml-cpp/yaml.h>

#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


Storage::Storage(const std::string& path)
    : path_(path)
    , tmp_path_(path + ".tmp") {
    if (std::filesystem::exists(tmp_path_)) {
        throw std::runtime_error(
            ".tmp file exists at start, there must be a problem with the previous run. Fix it manually and restart the server."
        );
    }
    if (!std::filesystem::exists(path_)) {
        throw std::runtime_error(
            "Dictionary file does not exist."
        );
    }

    std::ifstream file(path_);
    rapidjson::IStreamWrapper isw(file);
    dictionary_.ParseStream(isw);
    if (dictionary_.HasParseError()) {
        throw std::runtime_error(
            "Error parsing dictionary file."
        );
    }
    for (const auto& [key, value] : dictionary_.GetObject()) {
        if (!value.IsString()) {
            throw std::runtime_error(
                "Dictionary file contains non-string values."
            );
        }
    }
}

Storage::~Storage() {
    dump_to_file();
}

Storage::Stat Storage::set(const std::string& key, const std::string& value) {
    auto& key_stat = get_key_stats(key);
    key_stat.inc_set();
    auto res = stats_per_key_[key].take();

    total_stats_.inc_set();
    last_period_total_stats_.inc_set();

    rapidjson::Value key_json = rapidjson::Value(key.c_str(), dictionary_.GetAllocator());
    rapidjson::Value value_json = rapidjson::Value(value.c_str(), dictionary_.GetAllocator());

    std::unique_lock lock(dictionary_mutex_);
    if (dictionary_.HasMember(key_json)) {
        dictionary_[key_json].SetString(value_json.GetString(), dictionary_.GetAllocator());
    } else {
        dictionary_.AddMember(key_json, value_json, dictionary_.GetAllocator());
    }
    need_dump_.store(true);

    return res;
}

std::pair<std::optional<std::string>, Storage::Stat> Storage::get(const std::string& key) const {
    auto& key_stat = get_key_stats(key);
    key_stat.inc_get();
    auto res = stats_per_key_[key].take();

    total_stats_.inc_get();
    last_period_total_stats_.inc_get();

    auto key_json = rapidjson::StringRef(key.data(), key.size());
    std::shared_lock lock(dictionary_mutex_);
    if (dictionary_.HasMember(key_json)) {
        return {dictionary_.FindMember(key_json)->value.GetString(), res};
    }

    return {std::nullopt, res};
}

void Storage::dump_to_file() const {
    if (!need_dump_.exchange(false)) {
        return;
    }
    rapidjson::Document dictionary_copy;
    {
        std::shared_lock lock(dictionary_mutex_);
        dictionary_copy.CopyFrom(dictionary_, dictionary_copy.GetAllocator());
    }

    std::ofstream tmp_file(tmp_path_);
    rapidjson::OStreamWrapper osw(tmp_file);
    rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
    dictionary_copy.Accept(writer);
    tmp_file.close();

    std::filesystem::rename(tmp_path_, path_);
}

std::pair<Storage::Stat, Storage::Stat> Storage::get_and_reset_stats() const {
    auto total_stats = total_stats_.take();
    auto last_period_total_stats = last_period_total_stats_.take_and_reset();
    return {total_stats, last_period_total_stats};
}

Storage::AtomicStat& Storage::get_key_stats(const std::string& key) const {
    std::shared_lock stats_lock(stats_mutex_);
    if (!stats_per_key_.contains(key)) [[unlikely]] {
        stats_lock.unlock();
        {
            std::unique_lock insert_stats_lock(stats_mutex_);
            stats_per_key_[key];
        }
        stats_lock.lock();
    }

    return stats_per_key_[key];
}
