#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#include "storage.h"

#include <iostream>

#include <unordered_set>


class Server {
public:
    Server(boost::asio::io_context& io_context, const std::string& storage_path, uint16_t port);
    ~Server();

    void run();

private:
    void dump_storage_job();
    void statistics_print_job();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;

    boost::asio::steady_timer dump_timer_;
    boost::asio::steady_timer stat_timer_;

    std::shared_ptr<Storage> storage_;
    bool stopped_ = false;
};
