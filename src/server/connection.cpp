#include "connection.h"

#include <iostream>

#include <regex>
#include <boost/asio/write.hpp>

#include "../util/defer.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


Connection::Connection(boost::asio::ip::tcp::socket socket, std::weak_ptr<Storage> storage)
    : socket_(std::move(socket))
    , storage_(storage) {
}

void Connection::run() {
    schedule_read();
}

void Connection::schedule_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(next_input_), [this, self](const boost::system::error_code& error, size_t length) {
        if (error) {
            std::cerr << "Error reading data: " << error.message() << std::endl;
            return;
        }

        total_input_.insert(total_input_.end(), next_input_.begin(), next_input_.begin() + length);

        auto cmd = parse_command();

        if (auto* err = std::get_if<ParseFailed>(&cmd)) {
            if (*err == ParseFailed::NOT_FULL) {
                schedule_read();
                return;
            }
            output_ = "ERROR";
            schedule_write();
            return;
        }

        auto storage = storage_.lock();
        if (!storage) {
            std::cerr << "Storage is gone" << std::endl;
            return;
        }

        const auto& d = std::get<rapidjson::Document>(cmd);
        std::string command = d["command"].GetString();
        if (command == "get") {
            std::string key = d["key"].GetString();
            handle_get(*storage, key);
        } else if (command == "set") {
            std::string key = d["key"].GetString();
            std::string value = d["value"].GetString();
            handle_set(*storage, key, value);
        } else {
            output_ = "ERROR";
            schedule_write();
        }
    });
}

void Connection::schedule_write() {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(output_),
    [this, self] (const boost::system::error_code& error, size_t length) {
        if (error) {
            std::cerr << "Error writing data: " << error.message() << std::endl;
            return;
        }

        output_.clear();
        schedule_read();
    });
}

std::variant<rapidjson::Document, Connection::ParseFailed> Connection::parse_command() {
    if (total_input_.size() < 4) {
        return ParseFailed::NOT_FULL;
    }
    int32_t message_size = reinterpret_cast<const int32_t*>(total_input_.data())[0];
    message_size = ntohl(message_size);
    if (total_input_.size() < message_size + 4) {
        return ParseFailed::NOT_FULL;
    }

    Defer defer([&] {
        total_input_.erase(total_input_.begin(), total_input_.begin() + message_size + 4);
    });

    std::string_view message(total_input_.data() + 4, message_size);

    rapidjson::Document d;
    d.Parse(message.data(), message.size());
    if (d.HasParseError()) {
        return ParseFailed::ERROR;
    }

    return d;
}

void Connection::handle_get(Storage& storage, const std::string& key) {
    auto [value, stat] = storage.get(key);
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("stat", rapidjson::Value().SetObject(), d.GetAllocator());
    d["stat"].AddMember("get_count", stat.get_count, d.GetAllocator());
    d["stat"].AddMember("set_count", stat.set_count, d.GetAllocator());
    d.AddMember("ok", true, d.GetAllocator());
    d.AddMember("key", rapidjson::Value(key.c_str(), key.size(), d.GetAllocator()), d.GetAllocator());
    if (value.has_value()) {
        d.AddMember("found", true, d.GetAllocator());
        d.AddMember("value", rapidjson::Value(value->c_str(), value->size(), d.GetAllocator()), d.GetAllocator());
    } else {
        d.AddMember("found", false, d.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);
    output_ = buffer.GetString();

    schedule_write();
}

void Connection::handle_set(Storage& storage, const std::string& key, const std::string& value) {
    auto stat = storage.set(key, value);
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("stat", rapidjson::Value().SetObject(), d.GetAllocator());
    d["stat"].AddMember("get_count", stat.get_count, d.GetAllocator());
    d["stat"].AddMember("set_count", stat.set_count, d.GetAllocator());
    d.AddMember("ok", true, d.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);
    output_ = buffer.GetString();

    schedule_write();
}
