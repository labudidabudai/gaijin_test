#include "client.h"

#include <iostream>
#include <regex>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }
    Client client(argv[1], std::stoi(argv[2]));

    std::regex get_regex(R"(^\$get\s+([^\s=]+)\s*$)");
    std::regex set_regex(R"(^\$set\s+([^\s=]+)\s*=\s*([^\s]+)\s*$)");

    bool should_reconnect = false;
    while (true) {
        if (std::cin.eof()) {
            break;
        }
        std::cout << "> ";
        if (should_reconnect) {
            std::cout << "Reconnecting..." << std::endl;
            client.connect();
            std::cout << "Reconnected" << std::endl;
            should_reconnect = false;
        }

        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd.empty()) {
            continue;
        }
        std::smatch match;
        if (std::regex_match(cmd, match, get_regex)) {
            auto [value, ok] = client.get(match[1]);
            if (!ok) {
                std::cout << "Failed to get value" << std::endl;
                should_reconnect = true;
                continue;
            }
            std::cout << value << std::endl;
        } else if (std::regex_match(cmd, match, set_regex)) {
            auto [value, ok] = client.set(match[1], match[2]);
            if (!ok) {
                std::cout << "Failed to set value" << std::endl;
                should_reconnect = true;
                continue;
            }
            std::cout << value << std::endl;
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }

    std::cout << "Client stopped" << std::endl;

    return 0;
}
