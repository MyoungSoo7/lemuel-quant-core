#include "market/binance_client.hpp"

#include <chrono>
#include <iostream>
#include <string>

namespace market {

namespace {

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Tiny extractor: pull a numeric/string field from a flat JSON object.
// Real impl will swap to simdjson; keeping it dep-free for the skeleton
// so the module compiles standalone.
std::string_view extract(std::string_view json, std::string_view key) {
    const auto k = std::string("\"") + std::string(key) + "\":";
    auto p = json.find(k);
    if (p == std::string_view::npos) return {};
    p += k.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '"')) ++p;
    auto e = p;
    while (e < json.size() && json[e] != ',' && json[e] != '}' &&
           json[e] != '"') ++e;
    return json.substr(p, e - p);
}

double to_double(std::string_view sv) {
    if (sv.empty()) return 0.0;
    return std::stod(std::string(sv));
}
std::int64_t to_int64(std::string_view sv) {
    if (sv.empty()) return 0;
    return std::stoll(std::string(sv));
}

}  // namespace

BinanceClient::BinanceClient(Options opts) : options_(std::move(opts)) {}
BinanceClient::~BinanceClient() { stop(); }

void BinanceClient::subscribe_trades(std::string_view symbol) {
    options_.trade_symbols.emplace_back(symbol);
}
void BinanceClient::subscribe_book(std::string_view symbol, int depth) {
    options_.book_symbols.emplace_back(symbol);
    options_.book_depth = depth;
}

void BinanceClient::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this] { run_loop(); });
}

void BinanceClient::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void BinanceClient::run_loop() {
    // Skeleton: real WS connect/handshake/read loop goes here.
    // Replace with Boost.Beast once OpenSSL+Boost are wired into CMake.
    // The structure below documents the intended shape.
    while (running_.load()) {
        // 1. Build combined stream URL:
        //    /stream?streams=<sym>@trade/<sym>@depth20@100ms/...
        // 2. Connect WSS, read frames, dispatch by stream name.
        // 3. On disconnect, sleep options_.reconnect_delay_ms and retry.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options_.reconnect_delay_ms));
    }
}

lqc::feed::Trade BinanceClient::parse_trade(std::string_view json,
                                            std::string_view symbol) {
    lqc::feed::Trade t;
    t.symbol         = std::string(symbol);
    t.price          = to_double(extract(json, "p"));
    t.quantity       = to_double(extract(json, "q"));
    t.trade_id       = std::string(extract(json, "t"));
    t.exchange_ts_ns = to_int64(extract(json, "T")) * 1'000'000;
    t.local_ts_ns    = now_ns();
    // "m": true → buyer is market maker → trade was a SELL aggressor
    auto m = extract(json, "m");
    t.side = (m == "true") ? lqc::feed::Side::Sell : lqc::feed::Side::Buy;
    return t;
}

lqc::feed::OrderBook BinanceClient::parse_book(std::string_view /*json*/,
                                               std::string_view symbol) {
    // Production impl: parse "bids":[[p,q],...] and "asks":[[p,q],...]
    // arrays via simdjson and populate bids/asks.
    lqc::feed::OrderBook ob;
    ob.symbol      = std::string(symbol);
    ob.local_ts_ns = now_ns();
    return ob;
}

}  // namespace market
