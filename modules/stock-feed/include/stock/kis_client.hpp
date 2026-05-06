#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "lqc/feed_core.hpp"

namespace stock {

// 한국투자증권 (KIS) OpenAPI WebSocket client.
// stock-feed의 유일한 거래소 어댑터 — KRX 시세는 KIS 채널로만 받음.
//
// REST: https://openapi.koreainvestment.com:9443
// WS  : ws://ops.koreainvestment.com:21000  (실전)
//       ws://ops.koreainvestment.com:31000  (모의)
//
// 인증 흐름:
//   1) /oauth2/tokenP            → access_token (24h)
//   2) /uapi/.../approval        → ws approval_key
//   3) WS connect, subscribe with approval_key + tr_id (H0STCNT0 체결, H0STASP0 호가)
class KisClient : public lqc::feed::FeedClient {
public:
    struct Credentials {
        std::string app_key;
        std::string app_secret;
        bool        paper{false};   // 모의투자 모드
    };

    struct Options {
        Credentials creds;
        std::vector<std::string> trade_symbols;   // 종목코드 6자리: "005930"
        std::vector<std::string> book_symbols;
        bool reconnect{true};
        int  reconnect_delay_ms{2000};
    };

    explicit KisClient(Options opts);
    ~KisClient() override;

    void subscribe_trades(std::string_view symbol) override;
    void subscribe_book(std::string_view symbol, int depth = 10) override;

    void on_trade(lqc::feed::TradeHandler h) override { on_trade_ = std::move(h); }
    void on_book(lqc::feed::BookHandler h)  override { on_book_  = std::move(h); }

    void start() override;
    void stop()  override;
    bool running() const override { return running_.load(); }

    // KIS H0STCNT0 체결 메시지 파서 (실시간 체결가)
    static lqc::feed::Trade parse_h0stcnt0(std::string_view msg,
                                           std::string_view symbol);
    // KIS H0STASP0 호가 메시지 파서
    static lqc::feed::OrderBook parse_h0stasp0(std::string_view msg,
                                               std::string_view symbol);

private:
    void run_loop();

    Options opts_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    lqc::feed::TradeHandler on_trade_;
    lqc::feed::BookHandler  on_book_;
};

}  // namespace stock
