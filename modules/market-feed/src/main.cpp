#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "market/binance_client.hpp"
#include "market/redis_publisher.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void on_sig(int) { g_stop = 1; }
}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    market::BinanceClient::Options opts;
    opts.trade_symbols = {"btcusdt", "ethusdt"};
    opts.book_symbols  = {"btcusdt", "ethusdt"};

    const std::string redis_host = (argc > 1) ? argv[1] : "127.0.0.1";
    const int         redis_port = (argc > 2) ? std::atoi(argv[2]) : 6379;

    market::RedisPublisher pub(redis_host, redis_port);
    market::BinanceClient client(opts);

    client.on_trade([&](const lqc::feed::Trade& t) { pub.publish_trade(t); });
    client.on_book ([&](const lqc::feed::OrderBook& b) { pub.publish_book(b); });

    client.start();
    std::cout << "market-feed running. Ctrl-C to stop.\n";
    while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.stop();
    return 0;
}
