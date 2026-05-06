#pragma once

#include <memory>
#include <string>

#include "lqc/feed_core.hpp"

namespace market {

// Redis pub/sub publisher. Channel layout:
//   trade.binance.<symbol>     → JSON Trade
//   book.binance.<symbol>      → JSON OrderBook (depth=20 snapshot)
class RedisPublisher : public lqc::feed::Publisher {
public:
    RedisPublisher(std::string host, int port,
                   std::string channel_prefix = "");
    ~RedisPublisher() override;

    void publish_trade(const lqc::feed::Trade& t) override;
    void publish_book(const lqc::feed::OrderBook& b) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace market
