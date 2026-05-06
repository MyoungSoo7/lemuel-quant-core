#pragma once

// Common abstractions for any market feed (crypto / equities / disclosures).
// market-feed (crypto) and stock-feed (equities) both implement FeedClient.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lqc::feed {

enum class Side : std::uint8_t { Buy = 1, Sell = 2 };

struct Trade {
    std::string  symbol;
    double       price{};
    double       quantity{};
    Side         side{};
    std::int64_t exchange_ts_ns{};   // exchange-provided
    std::int64_t local_ts_ns{};      // local arrival
    std::string  trade_id;
};

struct PriceLevel {
    double price{};
    double quantity{};
};

struct OrderBook {
    std::string             symbol;
    std::vector<PriceLevel> bids;   // sorted desc
    std::vector<PriceLevel> asks;   // sorted asc
    std::int64_t            exchange_ts_ns{};
    std::int64_t            local_ts_ns{};
};

using TradeHandler = std::function<void(const Trade&)>;
using BookHandler  = std::function<void(const OrderBook&)>;

class FeedClient {
public:
    virtual ~FeedClient() = default;

    virtual void subscribe_trades(std::string_view symbol) = 0;
    virtual void subscribe_book(std::string_view symbol, int depth = 20) = 0;

    virtual void on_trade(TradeHandler h) = 0;
    virtual void on_book(BookHandler h) = 0;

    virtual void start() = 0;
    virtual void stop()  = 0;
    virtual bool running() const = 0;
};

class Publisher {
public:
    virtual ~Publisher() = default;
    virtual void publish_trade(const Trade&) = 0;
    virtual void publish_book(const OrderBook&) = 0;
};

}  // namespace lqc::feed
