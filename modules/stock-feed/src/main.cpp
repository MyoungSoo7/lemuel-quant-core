#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

#include "stock/kis_client.hpp"
#include "stock/krx_session.hpp"

#ifdef LQC_HAS_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace {
volatile std::sig_atomic_t g_stop = 0;
void on_sig(int) { g_stop = 1; }

// Inline 미니 Redis publisher (market-feed 의 RedisPublisher 와 동등 동작).
// stock-feed 가 별도 빌드 단위라 가볍게 인라인. 추후 shared/ 로 통합 가능.
class MiniRedisPublisher {
public:
    MiniRedisPublisher(const std::string& host, int port) {
#ifdef LQC_HAS_HIREDIS
        timeval tv{1, 0};
        ctx_ = redisConnectWithTimeout(host.c_str(), port, tv);
        if (!ctx_ || ctx_->err) {
            std::cerr << "[stock-feed] redis connect failed: "
                      << (ctx_ ? ctx_->errstr : "no ctx") << std::endl;
        }
#endif
    }
    ~MiniRedisPublisher() {
#ifdef LQC_HAS_HIREDIS
        if (ctx_) redisFree(ctx_);
#endif
    }
    void publish(const std::string& channel, const std::string& payload) {
#ifdef LQC_HAS_HIREDIS
        if (!ctx_ || ctx_->err) return;
        auto* r = static_cast<redisReply*>(redisCommand(
            ctx_, "PUBLISH %s %b", channel.c_str(),
            payload.data(), payload.size()));
        if (r) freeReplyObject(r);
#endif
    }
private:
#ifdef LQC_HAS_HIREDIS
    redisContext* ctx_ = nullptr;
#endif
};

std::string trade_to_json(const lqc::feed::Trade& t) {
    std::ostringstream os;
    os << "{\"symbol\":\"" << t.symbol << "\","
       << "\"price\":" << t.price << ","
       << "\"qty\":" << t.quantity << ","
       << "\"side\":" << (t.side == lqc::feed::Side::Buy ? 1 : 2) << ","
       << "\"ex_ts\":" << t.exchange_ts_ns << ","
       << "\"local_ts\":" << t.local_ts_ns << "}";
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
    os << "],\"local_ts\":" << b.local_ts_ns << "}";
    return os.str();
}

}  // namespace

int main() {
    // systemd pipe 캡처시 fully-buffered → unbuffered 강제
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    stock::KisClient::Options opts;
    if (const char* k = std::getenv("KIS_APP_KEY"))    opts.creds.app_key = k;
    if (const char* s = std::getenv("KIS_APP_SECRET")) opts.creds.app_secret = s;
    opts.creds.paper = (std::getenv("KIS_PAPER") != nullptr);
    opts.trade_symbols = {"005930", "000660"};   // 삼성전자, SK하이닉스
    opts.book_symbols  = {"005930", "000660"};

    const std::string redis_host =
        std::getenv("LQC_REDIS_HOST") ? std::getenv("LQC_REDIS_HOST")
                                      : "127.0.0.1";
    const int redis_port =
        std::getenv("LQC_REDIS_PORT") ?
            std::atoi(std::getenv("LQC_REDIS_PORT")) : 6379;

    MiniRedisPublisher pub(redis_host, redis_port);

    stock::KrxSession sess;
    stock::KisClient client(opts);

    client.on_trade([&](const lqc::feed::Trade& t) {
        pub.publish("trade.kis." + t.symbol, trade_to_json(t));
    });
    client.on_book([&](const lqc::feed::OrderBook& b) {
        pub.publish("book.kis." + b.symbol, book_to_json(b));
    });

    while (!g_stop) {
        const auto now = std::chrono::system_clock::now();
        const auto state = sess.state_at(now);
        const bool tradeable =
            state == stock::KrxSession::State::Regular ||
            state == stock::KrxSession::State::Closing ||
            state == stock::KrxSession::State::PreOpen ||
            state == stock::KrxSession::State::AfterHours;

        if (tradeable && !client.running()) {
            std::cout << "[stock-feed] " << stock::to_string(state)
                      << ": starting feed\n";
            client.start();
        } else if (!tradeable && client.running()) {
            std::cout << "[stock-feed] " << stock::to_string(state)
                      << ": stopping feed\n";
            client.stop();
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    client.stop();
    return 0;
}
