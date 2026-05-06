#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lqc/feed_core.hpp"

namespace market {

// Binance combined-stream WebSocket client.
// Streams: <symbol>@trade, <symbol>@depth20@100ms
//
// Implementation notes:
//   - The skeleton ships an interface + connect loop scaffold.
//   - Real WS handshake will use Boost.Beast or uWebSockets; both expose
//     the same callback shape so the public API does not change.
class BinanceClient : public lqc::feed::FeedClient {
public:
    struct Options {
        std::string host{"stream.binance.com"};
        std::string port{"9443"};
        // raw symbols, lowercased: "btcusdt", "ethusdt", ...
        std::vector<std::string> trade_symbols;
        std::vector<std::string> book_symbols;
        int book_depth{20};
        bool reconnect{true};
        int  reconnect_delay_ms{1000};
    };

    explicit BinanceClient(Options opts);
    ~BinanceClient() override;

    void subscribe_trades(std::string_view symbol) override;
    void subscribe_book(std::string_view symbol, int depth = 20) override;

    void on_trade(lqc::feed::TradeHandler h) override { on_trade_ = std::move(h); }
    void on_book(lqc::feed::BookHandler h)  override { on_book_  = std::move(h); }

    void start() override;
    void stop()  override;
    bool running() const override { return running_.load(); }

    // Parses the @trade payload (separated for unit tests).
    static lqc::feed::Trade parse_trade(std::string_view json,
                                        std::string_view symbol);
    // Parses the @depth20 payload.
    static lqc::feed::OrderBook parse_book(std::string_view json,
                                           std::string_view symbol);

private:
    void run_loop();

    Options options_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    lqc::feed::TradeHandler on_trade_;
    lqc::feed::BookHandler  on_book_;
};

}  // namespace market
