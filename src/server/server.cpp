#include "server.h"

#include "connection.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/write.hpp>

#include <iostream>
#include <regex>

Server::Server(boost::asio::io_context& io_context, const std::string& storage_path, uint16_t port)
    : io_context_(io_context)
    , acceptor_(io_context_, {boost::asio::ip::tcp::v4(), port})
    , storage_(std::make_shared<Storage>(storage_path))
    , dump_timer_(io_context_)
    , stat_timer_(io_context_) {
    dump_storage_job();
    statistics_print_job();
}

Server::~Server() {
    io_context_.stop();
}

void Server::run() {
    boost::asio::ip::tcp::socket socket(io_context_);
    std::cerr << "Accepting connection" << std::endl;
    acceptor_.async_accept([this] (const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
        if (error) {
            std::cerr << "Error accepting connection: " << error.message() << std::endl;
            return;
        }

        auto connection = std::make_shared<Connection>(std::move(socket), storage_);
        connection->run();

        run();
    });
}

void Server::dump_storage_job() {
    storage_->dump_to_file();

    dump_timer_.expires_after(std::chrono::seconds(5));
    dump_timer_.async_wait([this](const boost::system::error_code& e) {
        dump_storage_job();
    });
}

void Server::statistics_print_job() {
    auto [total_stats, last_stats] = storage_->get_and_reset_stats();
    std::cout << "Total stats: " << total_stats.get_count << " get, " << total_stats.set_count << " set" << std::endl;
    std::cout << "Last stats: " << last_stats.get_count << " get, " << last_stats.set_count << " set" << std::endl;

    stat_timer_.expires_after(std::chrono::seconds(5));
    stat_timer_.async_wait([this](const boost::system::error_code& e) {
        statistics_print_job();
    });
}
