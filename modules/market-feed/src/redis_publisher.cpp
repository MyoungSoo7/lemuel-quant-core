#include "market/redis_publisher.hpp"

#include <chrono>
#include <sstream>
#include <string>

#ifdef LQC_HAS_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace market {

namespace {

std::string trade_to_json(const lqc::feed::Trade& t) {
    std::ostringstream os;
    os << "{\"symbol\":\"" << t.symbol << "\","
       << "\"price\":" << t.price << ","
       << "\"qty\":" << t.quantity << ","
       << "\"side\":" << (t.side == lqc::feed::Side::Buy ? 1 : 2) << ","
       << "\"ex_ts\":" << t.exchange_ts_ns << ","
       << "\"local_ts\":" << t.local_ts_ns << ","
       << "\"trade_id\":\"" << t.trade_id << "\"}";
    return os.str();
}

std::string book_to_json(const lqc::feed::OrderBook& b) {
    std::ostringstream os;
    os << "{\"symbol\":\"" << b.symbol << "\",\"bids\":[";
    for (std::size_t i = 0; i < b.bids.size(); ++i) {
        if (i) os << ",";
        os << "[" << b.bids[i].price << "," << b.bids[i].quantity << "]";
    }
    os << "],\"asks\":[";
    for (std::size_t i = 0; i < b.asks.size(); ++i) {
        if (i) os << ",";
        os << "[" << b.asks[i].price << "," << b.asks[i].quantity << "]";
    }
    os << "],\"ex_ts\":" << b.exchange_ts_ns
       << ",\"local_ts\":" << b.local_ts_ns << "}";
    return os.str();
}

}  // namespace

struct RedisPublisher::Impl {
    std::string host;
    int port;
    std::string prefix;
#ifdef LQC_HAS_HIREDIS
    redisContext* ctx{nullptr};
#endif
    Impl(std::string h, int p, std::string pfx)
        : host(std::move(h)), port(p), prefix(std::move(pfx)) {
#ifdef LQC_HAS_HIREDIS
        timeval tv{1, 0};
        ctx = redisConnectWithTimeout(host.c_str(), port, tv);
#endif
    }
    ~Impl() {
#ifdef LQC_HAS_HIREDIS
        if (ctx) redisFree(ctx);
#endif
    }
    void publish(const std::string& channel, const std::string& payload) {
#ifdef LQC_HAS_HIREDIS
        if (!ctx || ctx->err) return;
        auto* r = static_cast<redisReply*>(redisCommand(
            ctx, "PUBLISH %s %b", channel.c_str(), payload.data(),
            payload.size()));
        if (r) freeReplyObject(r);
#else
        (void)channel;
        (void)payload;
#endif
    }
};

RedisPublisher::RedisPublisher(std::string host, int port, std::string prefix)
    : impl_(std::make_unique<Impl>(std::move(host), port, std::move(prefix))) {}
RedisPublisher::~RedisPublisher() = default;

void RedisPublisher::publish_trade(const lqc::feed::Trade& t) {
    impl_->publish(impl_->prefix + "trade.binance." + t.symbol,
                   trade_to_json(t));
}
void RedisPublisher::publish_book(const lqc::feed::OrderBook& b) {
    impl_->publish(impl_->prefix + "book.binance." + b.symbol,
                   book_to_json(b));
}

}  // namespace market
