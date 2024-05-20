#pragma once

#include "storage.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>

#include <rapidjson/document.h>

#include <memory>
#include <variant>

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(boost::asio::ip::tcp::socket socket, std::weak_ptr<Storage> storage);

    void run();
private:
    enum class ParseFailed {
        NOT_FULL,
        ERROR,
    };

    void schedule_read();
    void schedule_write();

    std::variant<rapidjson::Document, ParseFailed> parse_command();

    void handle_get(Storage& storage, const std::string& key);
    void handle_set(Storage& storage, const std::string& key, const std::string& value);

    boost::asio::ip::tcp::socket socket_;
    std::weak_ptr<Storage> storage_;
    std::vector<char> total_input_;
    std::array<char, 4096> next_input_;
    std::string output_;
};
