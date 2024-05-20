#include "client.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/write.hpp>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>
#include <thread>

#include <arpa/inet.h>


Client::Client(const std::string& host, uint16_t port)
    : socket_(io_context_)
    , host_(host)
    , port_(port) {
    std::cerr << "Client created" << std::endl;
    connect();
}

void Client::connect(std::chrono::seconds timeout) {
    while (!socket_.is_open()) {
        try {
            std::cerr << "Connecting to " << host_ << ":" << port_ << std::endl;
            socket_.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host_), port_));
        } catch (const boost::system::system_error& e) {
            std::cerr << "Failed to connect to " << host_ << ":" << port_ << ": " << e.what() << std::endl;
            socket_.close();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    std::cerr << "Connected to " << host_ << ":" << port_ << std::endl;
}

std::pair<std::string, bool> Client::get(const std::string& key) {
    rapidjson::Document d;
    d.SetObject();
    {
        rapidjson::Value v;
        v.SetString("get");
        d.AddMember("command", v, d.GetAllocator());
        v.SetString(rapidjson::StringRef(key.data(), key.size()));
        d.AddMember("key", v, d.GetAllocator());
    }

    std::cerr << "Sending get request: " << key << std::endl;

    std::array<char, 4096> data;
    size_t length = 0;
    try {
        length = send_request_and_get_response(d, data);
    } catch (const boost::system::system_error& e) {
        std::cerr << "Failed to send get request: " << e.what() << std::endl;
        socket_.close();
        return {"", false};
    }

    std::cerr << "Response to get: " << std::string(data.data(), length) << std::endl;

    return {std::string(data.data(), length), true};
}

std::pair<std::string, bool> Client::set(const std::string& key, const std::string& value) {
    rapidjson::Document d;
    d.SetObject();
    {
        rapidjson::Value v;
        v.SetString("set");
        d.AddMember("command", v, d.GetAllocator());
        v.SetString(rapidjson::StringRef(key.data(), key.size()));
        d.AddMember("key", v, d.GetAllocator());
        v.SetString(rapidjson::StringRef(value.data(), value.size()));
        d.AddMember("value", v, d.GetAllocator());
    }

    std::cerr << "Sending set request: " << key << " -> " << value << std::endl;

    std::array<char, 4096> data;
    size_t length = 0;
    try {
        length = send_request_and_get_response(d, data);
    } catch (const boost::system::system_error& e) {
        std::cerr << "Failed to send set request: " << e.what() << std::endl;
        socket_.close();
        return {"", false};
    }

    std::cerr << "Response to set: " << std::string(data.data(), length) << std::endl;

    return {std::string(data.data(), length), true};
}

size_t Client::send_request_and_get_response(const rapidjson::Document& d, std::array<char, 4096>& resp) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    int32_t len = buffer.GetLength();
    len = htonl(len);
    std::string len_str = std::string(reinterpret_cast<char*>(&len), sizeof(len));
    std::string message = len_str + buffer.GetString();

    boost::asio::write(socket_, boost::asio::buffer(message));
    return socket_.read_some(boost::asio::buffer(resp));
}
