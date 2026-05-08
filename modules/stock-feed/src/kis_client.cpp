#include "stock/kis_client.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef LQC_HAS_BEAST
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#endif

#ifdef LQC_HAS_CURL
#include <curl/curl.h>
#endif

#ifdef LQC_HAS_SIMDJSON
#include <simdjson.h>
#endif

namespace stock {

namespace {

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

#ifdef LQC_HAS_CURL
std::size_t curl_write(char* p, std::size_t s, std::size_t n, void* ud) {
    static_cast<std::string*>(ud)->append(p, s * n);
    return s * n;
}

// POST JSON body, return response body. Returns empty string on transport
// error. Caller checks JSON contents for error fields.
std::string http_post(const std::string& url, const std::string& body,
                      const std::vector<std::string>& headers = {}) {
    CURL* c = curl_easy_init();
    if (!c) return {};
    std::string resp;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(body.size()));
    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    for (const auto& h : headers) hdrs = curl_slist_append(hdrs, h.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    curl_easy_perform(c);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return resp;
}
#else
std::string http_post(const std::string&, const std::string&,
                      const std::vector<std::string>& = {}) { return {}; }
#endif

#ifdef LQC_HAS_SIMDJSON
std::string sj_get_string(const std::string& json,
                          std::string_view key) {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(json);
    auto doc = parser.iterate(padded);
    std::string_view v;
    if (doc.find_field(key).get_string().get(v) == simdjson::SUCCESS) {
        return std::string(v);
    }
    return {};
}
#else
std::string sj_get_string(const std::string&, std::string_view) { return {}; }
#endif

struct AuthTokens {
    std::string access_token;
    std::string approval_key;
};

// 1) /oauth2/tokenP   { grant_type, appkey, appsecret } → access_token
// 2) /oauth2/Approval { grant_type, appkey, secretkey } → approval_key
AuthTokens authenticate(const KisClient::Credentials& creds) {
    const std::string base = creds.paper
        ? "https://openapivts.koreainvestment.com:29443"
        : "https://openapi.koreainvestment.com:9443";

    AuthTokens out;
    {
        std::ostringstream body;
        body << "{\"grant_type\":\"client_credentials\","
             << "\"appkey\":\""    << creds.app_key    << "\","
             << "\"appsecret\":\"" << creds.app_secret << "\"}";
        const auto resp = http_post(base + "/oauth2/tokenP", body.str());
        out.access_token = sj_get_string(resp, "access_token");
    }
    {
        std::ostringstream body;
        body << "{\"grant_type\":\"client_credentials\","
             << "\"appkey\":\""   << creds.app_key    << "\","
             << "\"secretkey\":\""<< creds.app_secret << "\"}";
        const auto resp = http_post(base + "/oauth2/Approval", body.str());
        out.approval_key = sj_get_string(resp, "approval_key");
    }
    return out;
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

#ifdef LQC_HAS_BEAST

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

void KisClient::run_loop() {
    while (running_.load()) {
        try {
            std::cerr << "[kis] authenticating (paper="
                      << (opts_.creds.paper ? "yes" : "no") << ", "
                      << "appkey=" << opts_.creds.app_key.substr(0, 8) << "...)"
                      << std::endl;
            const auto tokens = authenticate(opts_.creds);
            std::cerr << "[kis] access_token len="
                      << tokens.access_token.size()
                      << "  approval_key len="
                      << tokens.approval_key.size() << std::endl;
            if (tokens.approval_key.empty()) {
                throw std::runtime_error(
                    "KIS approval_key 발급 실패 — 키/시크릿 또는 paper 모드 확인");
            }

            // KIS WS 호스트: 모의/실전 동일하지만 port 다름. (KIS docs 기준
            // 모의: ops.koreainvestment.com:31000, 실전: ops.koreainvestment.com:21000)
            // 과거 코드에 paper/prod 호스트가 같다는 인라인 표현이 있었지만
            // 그건 KIS spec 그대로이고 분기는 port 로만 결정.
            const std::string ws_host = "ops.koreainvestment.com";
            const std::string ws_port = opts_.creds.paper ? "31000" : "21000";

            net::io_context ioc;
            tcp::resolver resolver{ioc};
            websocket::stream<beast::tcp_stream> ws{ioc};
            ws.read_message_max(256 * 1024);

            std::cerr << "[kis] connecting WS " << ws_host << ":" << ws_port
                      << std::endl;
            const auto results = resolver.resolve(ws_host, ws_port);
            beast::get_lowest_layer(ws).connect(results);
            // KIS WS path 는 단순 "/" — 과거 코드의 /tryitout/* 은 try-it-out
            // 웹 UI 경로였고 실제 endpoint 가 아니었음
            ws.handshake(ws_host, "/");
            std::cerr << "[kis] WS handshake OK, subscribing "
                      << opts_.trade_symbols.size() << " trade + "
                      << opts_.book_symbols.size() << " book symbols"
                      << std::endl;

            // Subscribe to each symbol.
            auto send_sub = [&](const std::string& tr_id,
                                const std::string& sym) {
                std::ostringstream o;
                o << "{\"header\":{\"approval_key\":\""
                  << tokens.approval_key
                  << "\",\"custtype\":\"P\",\"tr_type\":\"1\","
                  << "\"content-type\":\"utf-8\"},"
                  << "\"body\":{\"input\":{\"tr_id\":\"" << tr_id
                  << "\",\"tr_key\":\"" << sym << "\"}}}";
                ws.write(net::buffer(o.str()));
            };
            for (const auto& s : opts_.trade_symbols) send_sub("H0STCNT0", s);
            for (const auto& s : opts_.book_symbols)  send_sub("H0STASP0", s);

            beast::flat_buffer buf;
            while (running_.load()) {
                buf.clear();
                ws.read(buf);
                const auto sv = beast::buffers_to_string(buf.data());
                if (sv.empty()) continue;
                // Real-time data frames are pipe-delimited:
                //   "0|H0STCNT0|001|005930^HHMMSS^...^"
                // JSON frames are PINGPONG / subscribe ack — ignore.
                if (sv.front() == '{') continue;

                std::vector<std::string> parts;
                std::size_t p = 0;
                for (std::size_t i = 0; i <= sv.size(); ++i) {
                    if (i == sv.size() || sv[i] == '|') {
                        parts.emplace_back(sv.substr(p, i - p));
                        p = i + 1;
                    }
                }
                if (parts.size() < 4) continue;
                const auto& tr_id   = parts[1];
                const auto& payload = parts[3];
                // payload: 005930^...^...^...
                const auto sym_end = payload.find('^');
                if (sym_end == std::string::npos) continue;
                const auto symbol = payload.substr(0, sym_end);

                if (tr_id == "H0STCNT0" && on_trade_) {
                    on_trade_(parse_h0stcnt0(payload, symbol));
                } else if (tr_id == "H0STASP0" && on_book_) {
                    on_book_(parse_h0stasp0(payload, symbol));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[kis] " << e.what() << "; reconnecting in "
                      << opts_.reconnect_delay_ms << "ms\n";
            if (!opts_.reconnect) running_ = false;
        }
        if (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(opts_.reconnect_delay_ms));
        }
    }
}

#else

void KisClient::run_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(opts_.reconnect_delay_ms));
    }
}

#endif

// KIS H0STCNT0 payload is a `^` (0x5E) delimited string. Field map per docs:
//   [0] 종목코드, [1] HHMMSS, [2] 현재가, [12] 체결량, [21] 체결구분
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
    auto side  = at(21);
    t.side     = (side == "5") ? lqc::feed::Side::Sell : lqc::feed::Side::Buy;
    return t;
}

// H0STASP0 payload: 종목코드^영업시간^매도호가1~10^매도잔량1~10^매수호가1~10^매수잔량1~10^...
lqc::feed::OrderBook KisClient::parse_h0stasp0(std::string_view msg,
                                               std::string_view symbol) {
    lqc::feed::OrderBook ob;
    ob.symbol      = std::string(symbol);
    ob.local_ts_ns = now_ns();

    std::vector<std::string_view> f;
    f.reserve(60);
    std::size_t p = 0;
    for (std::size_t i = 0; i <= msg.size(); ++i) {
        if (i == msg.size() || msg[i] == '^') {
            f.push_back(msg.substr(p, i - p));
            p = i + 1;
        }
    }
    auto td = [](std::string_view s) {
        return s.empty() ? 0.0 : std::stod(std::string(s));
    };
    // Indices per KIS doc (1-based on the spec, here 0-based offsets):
    //   [3..12]  매도호가1~10 (asks)
    //   [13..22] 매수호가1~10 (bids)
    //   [23..32] 매도잔량1~10
    //   [33..42] 매수잔량1~10
    for (int i = 0; i < 10 && static_cast<std::size_t>(3 + i) < f.size(); ++i) {
        lqc::feed::PriceLevel lvl;
        lvl.price    = td(f[3 + i]);
        lvl.quantity = (23 + i < static_cast<int>(f.size())) ? td(f[23 + i]) : 0.0;
        if (lvl.price > 0) ob.asks.push_back(lvl);
    }
    for (int i = 0; i < 10 && static_cast<std::size_t>(13 + i) < f.size(); ++i) {
        lqc::feed::PriceLevel lvl;
        lvl.price    = td(f[13 + i]);
        lvl.quantity = (33 + i < static_cast<int>(f.size())) ? td(f[33 + i]) : 0.0;
        if (lvl.price > 0) ob.bids.push_back(lvl);
    }
    return ob;
}

}  // namespace stock
