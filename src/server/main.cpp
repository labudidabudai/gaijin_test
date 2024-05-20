#include "server.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    uint16_t port = std::stoi(argv[1]);

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT);

    Server server(io_context, "config.txt", port);

    signals.async_wait([&](const boost::system::error_code&, int) {
        std::cerr << "signal received, stopping server" << std::endl;
        io_context.stop();
    });

    size_t threads = std::thread::hardware_concurrency();
    std::cerr << "Starting server with " << threads << " threads" << std::endl;
    std::vector<std::thread> thread_pool;
    for (size_t i = 0; i < threads; ++i) {
        thread_pool.emplace_back([&io_context] {
            io_context.run();
        });
    }

    server.run();

    std::cerr << "Server stopped" << std::endl;
    for (auto& t : thread_pool) {
        t.join();
    }
    std::cerr << "Io context threads joined" << std::endl;
    return 0;
}
