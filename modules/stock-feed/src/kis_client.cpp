#include "stock/kis_client.hpp"

#include <chrono>
#include <thread>

namespace stock {

namespace {
std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
}  // namespace

KisClient::KisClient(Options opts) : opts_(std::move(opts)) {}
KisClient::~KisClient() { stop(); }

void KisClient::subscribe_trades(std::string_view symbol) {
    opts_.trade_symbols.emplace_back(symbol);
}
void KisClient::subscribe_book(std::string_view symbol, int /*depth*/) {
    opts_.book_symbols.emplace_back(symbol);
}

void KisClient::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this] { run_loop(); });
}
void KisClient::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

void KisClient::run_loop() {
    // Skeleton:
    //   1. POST /oauth2/tokenP   { grant_type, appkey, appsecret }
    //   2. POST /uapi/.../approval (Hashkey, body)
    //   3. WS connect to ops.koreainvestment.com:21000 (실전) or :31000 (모의)
    //   4. send subscribe frame per symbol/tr_id (H0STCNT0 / H0STASP0)
    //   5. read frames, dispatch via parse_h0stcnt0 / parse_h0stasp0
    while (running_.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(opts_.reconnect_delay_ms));
    }
}

// KIS H0STCNT0 payload is a `^` (0x5E) delimited string:
//   "유가증권단축종목코드^체결시각^주식현재가^전일대비부호^전일대비^...^체결량^..."
// Field index map per KIS docs (0-based):
//   [0] 종목코드, [1] HHMMSS, [2] 현재가, [12] 체결량, ...
//
// The parser is deliberately tolerant — we only pull what we need.
lqc::feed::Trade KisClient::parse_h0stcnt0(std::string_view msg,
                                           std::string_view symbol) {
    lqc::feed::Trade t;
    t.symbol      = std::string(symbol);
    t.local_ts_ns = now_ns();

    std::vector<std::string_view> f;
    f.reserve(40);
    std::size_t p = 0;
    for (std::size_t i = 0; i <= msg.size(); ++i) {
        if (i == msg.size() || msg[i] == '^') {
            f.push_back(msg.substr(p, i - p));
            p = i + 1;
        }
    }
    auto at = [&](std::size_t i) -> std::string_view {
        return i < f.size() ? f[i] : std::string_view{};
    };
    auto td = [](std::string_view s) {
        return s.empty() ? 0.0 : std::stod(std::string(s));
    };

    t.price    = td(at(2));
    t.quantity = td(at(12));
    // KIS sends 1=매수, 5=매도 in some payloads; here a sentinel field [21]
    // indicates aggressor side. Default to Buy if missing.
    auto side = at(21);
    t.side    = (side == "5") ? lqc::feed::Side::Sell : lqc::feed::Side::Buy;
    return t;
}

lqc::feed::OrderBook KisClient::parse_h0stasp0(std::string_view /*msg*/,
                                               std::string_view symbol) {
    lqc::feed::OrderBook ob;
    ob.symbol      = std::string(symbol);
    ob.local_ts_ns = now_ns();
    // Production: parse 매도호가1~10, 매수호가1~10, 잔량 fields and fill bids/asks.
    return ob;
}

}  // namespace stock
