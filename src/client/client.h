#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>

#include <rapidjson/document.h>

class Client {
public:
    Client(const std::string& host, uint16_t port);

    void connect(std::chrono::seconds timeout = std::chrono::seconds(5));

    std::pair<std::string, bool> get(const std::string& key);
    std::pair<std::string, bool> set(const std::string& key, const std::string& value);

private:
    size_t send_request_and_get_response(const rapidjson::Document& d, std::array<char, 4096>& resp);

    std::string host_;
    uint16_t port_;

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
};
