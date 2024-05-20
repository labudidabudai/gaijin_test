#include "client.h"

#include <fstream>
#include <iostream>
#include <random>
#include <thread>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


struct Params {
    std::string host;
    int port;
    int n_requests;
    int requests_period_us;
    std::vector<std::string> keys;
    std::string statistics_output;
};

void help() {
    std::cerr << "Usage: load_test_client <host> <port> <n_requests> <requests_period_us> <keys_list_file> <statistics_output>" << std::endl;
    std::cerr << "host - server host" << std::endl;
    std::cerr << "port - server port" << std::endl;
    std::cerr << "n_requests - number of requests to send, must be positive" << std::endl;
    std::cerr << "requests_period_us - period between requests in microseconds, must be positive" << std::endl;
    std::cerr << "keys_list_file - file with keys list" << std::endl;
    std::cerr << "statistics_output - file to write statistics (optional)" << std::endl;
    exit(1);
}

Params parse_params(int argc, char** argv) {
    if (argc != 6 && argc != 7) {
        help();
    }

    Params params;
    params.host = argv[1];
    params.port = std::stoi(argv[2]);
    params.n_requests = std::stoi(argv[3]);
    params.requests_period_us = std::stoi(argv[4]);

    std::ifstream keys_file(argv[5]);
    if (!keys_file.is_open()) {
        std::cerr << "Failed to open keys list file" << std::endl;
        help();
    }
    std::string key;
    while (keys_file >> key) {
        params.keys.push_back(key);
    }
    keys_file.close();
    if (params.keys.empty()) {
        std::cerr << "Keys list is empty" << std::endl;
        help();
    }
    if (argc == 7) {
        params.statistics_output = argv[6];
    }

    if (params.n_requests <= 0) {
        help();
    }
    if (params.requests_period_us < 0) {
        help();
    }

    return params;
}

std::string random_alphanumerical_string(size_t length_min, size_t length_max, std::mt19937& gen) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<size_t> length_dist(length_min, length_max);
    std::uniform_int_distribution<int> letters_dist(0, sizeof(alphanum) - 1);
    std::string s;
    size_t length = length_dist(gen);
    for (size_t i = 0; i < length; ++i) {
        s += alphanum[letters_dist(gen)];
    }
    return s;
}

class Stat {
public:
    void report_value(double val) {
        sum_ += val;
        ++number_of_samples_;
    }

    size_t get_number_of_samples() {
        return number_of_samples_;
    }

    double get_mean() {
        return sum_ / number_of_samples_;
    }

private:
    size_t number_of_samples_ = 0;
    double sum_ = 0.0;
};


int main(int argc, char** argv) {
    Params params = parse_params(argc, argv);

    Client client(params.host, params.port);

    // We add pid so we can initialize several clients automatically and be sure
    //  that they will have different random generators
    std::mt19937 gen(time(nullptr) + getpid());
    std::uniform_int_distribution<int> key_dist(0, params.keys.size() - 1);
    std::uniform_int_distribution<int> command_dist(0, 99);

    Stat read_stat;
    Stat write_stat;

    int requests_sent = 0;
    bool should_reconnect = false;
    while (requests_sent < params.n_requests) {
        if (should_reconnect) {
            std::cerr << "Reconnecting..." << std::endl;
            client.connect();
            std::cerr << "Reconnected" << std::endl;
            should_reconnect = false;
        }

        bool request_good;
        if (command_dist(gen) == 0) {
            const auto& key = params.keys[key_dist(gen)];
            auto value = random_alphanumerical_string(1, 100, gen);

            auto start = std::chrono::steady_clock::now();
            auto [_, ok] = client.set(key, value);
            auto end = std::chrono::steady_clock::now();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            write_stat.report_value(duration_us);
            request_good = ok;
        } else {
            const auto& key = params.keys[key_dist(gen)];

            auto start = std::chrono::steady_clock::now();
            auto [_, ok] = client.get(key);
            auto end = std::chrono::steady_clock::now();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            read_stat.report_value(duration_us);
            request_good = ok;
        }

        if (!request_good) {
            std::cerr << "Failed to send request" << std::endl;
            should_reconnect = true;
        } else {
            ++requests_sent;
            std::this_thread::sleep_for(std::chrono::microseconds(params.requests_period_us));
        }
    }

    if (!params.statistics_output.empty()) {
        rapidjson::Document d;
        d.SetObject();
        if (read_stat.get_number_of_samples() > 0) {
            d.AddMember("read", rapidjson::Value().SetObject(), d.GetAllocator());
            d["read"].AddMember("mean", read_stat.get_mean(), d.GetAllocator());
            d["read"].AddMember("n_samples", read_stat.get_number_of_samples(), d.GetAllocator());
        }
        if (write_stat.get_number_of_samples() > 0) {
            d.AddMember("write", rapidjson::Value().SetObject(), d.GetAllocator());
            d["write"].AddMember("mean", write_stat.get_mean(), d.GetAllocator());
            d["write"].AddMember("n_samples", write_stat.get_number_of_samples(), d.GetAllocator());
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        d.Accept(writer);
        std::ofstream statistics_output(params.statistics_output);
        statistics_output << buffer.GetString();
    }

    return 0;
}