#include "market/binance_client.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef LQC_HAS_BEAST
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#endif

#ifdef LQC_HAS_SIMDJSON
#include <simdjson.h>
#endif

namespace market {

namespace {

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

#ifdef LQC_HAS_SIMDJSON
double sj_double(simdjson::ondemand::value v) {
    if (v.type() == simdjson::ondemand::json_type::string) {
        std::string_view s = v.get_string();
        return s.empty() ? 0.0 : std::stod(std::string(s));
    }
    double d;
    auto err = v.get_double().get(d);
    return err ? 0.0 : d;
}
std::int64_t sj_int(simdjson::ondemand::value v) {
    if (v.type() == simdjson::ondemand::json_type::string) {
        std::string_view s = v.get_string();
        return s.empty() ? 0 : std::stoll(std::string(s));
    }
    std::int64_t i;
    auto err = v.get_int64().get(i);
    return err ? 0 : i;
}
#endif

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

#ifdef LQC_HAS_BEAST

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = net::ssl;
using tcp           = net::ip::tcp;

void BinanceClient::run_loop() {
    while (running_.load()) {
        try {
            net::io_context ioc;
            ssl::context ctx{ssl::context::tlsv12_client};
            ctx.set_default_verify_paths();
            ctx.set_verify_mode(ssl::verify_peer);

            tcp::resolver resolver{ioc};
            websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws{
                ioc, ctx};

            const auto results =
                resolver.resolve(options_.host, options_.port);
            beast::get_lowest_layer(ws).connect(results);

            if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(),
                                           options_.host.c_str())) {
                throw beast::system_error(beast::error_code(
                    static_cast<int>(::ERR_get_error()),
                    net::error::get_ssl_category()));
            }
            ws.next_layer().handshake(ssl::stream_base::client);

            // Build combined-stream path:
            //   /stream?streams=btcusdt@trade/btcusdt@depth20@100ms/...
            std::ostringstream path;
            path << "/stream?streams=";
            bool first = true;
            for (const auto& s : options_.trade_symbols) {
                if (!first) path << "/";
                path << s << "@trade";
                first = false;
            }
            for (const auto& s : options_.book_symbols) {
                if (!first) path << "/";
                path << s << "@depth" << options_.book_depth << "@100ms";
                first = false;
            }
            ws.handshake(options_.host, path.str());

            beast::flat_buffer buf;
            while (running_.load()) {
                buf.clear();
                ws.read(buf);
                const auto sv = beast::buffers_to_string(buf.data());
                // {"stream":"btcusdt@trade","data":{...}}
                const auto stream_pos = sv.find("\"stream\":\"");
                const auto data_pos   = sv.find("\"data\":{");
                if (stream_pos == std::string::npos ||
                    data_pos == std::string::npos) continue;
                const auto stream_start = stream_pos + 10;
                const auto stream_end   = sv.find('"', stream_start);
                const auto stream_name  = sv.substr(
                    stream_start, stream_end - stream_start);
                const auto data_start = data_pos + 7;
                // Find matching close brace.
                int depth = 0;
                std::size_t i = data_start;
                for (; i < sv.size(); ++i) {
                    if (sv[i] == '{') ++depth;
                    else if (sv[i] == '}') { if (--depth == 0) break; }
                }
                const auto data_payload = sv.substr(data_start, i - data_start + 1);

                const auto at = stream_name.find('@');
                std::string symbol = stream_name.substr(0, at);
                std::string kind   = stream_name.substr(at + 1);

                if (kind == "trade" && on_trade_) {
                    on_trade_(parse_trade(data_payload, symbol));
                } else if (kind.rfind("depth", 0) == 0 && on_book_) {
                    on_book_(parse_book(data_payload, symbol));
                }
            }
            beast::error_code ec;
            ws.close(websocket::close_code::normal, ec);
        } catch (const std::exception& e) {
            std::cerr << "[binance] " << e.what() << "; reconnecting in "
                      << options_.reconnect_delay_ms << "ms\n";
            if (!options_.reconnect) running_ = false;
        }
        if (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(options_.reconnect_delay_ms));
        }
    }
}

#else

void BinanceClient::run_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options_.reconnect_delay_ms));
    }
}

#endif

#ifdef LQC_HAS_SIMDJSON

lqc::feed::Trade BinanceClient::parse_trade(std::string_view json,
                                            std::string_view symbol) {
    lqc::feed::Trade t;
    t.symbol      = std::string(symbol);
    t.local_ts_ns = now_ns();

    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(json);
    auto doc = parser.iterate(padded);
    auto obj = doc.get_object();
    for (auto field : obj) {
        std::string_view key = field.unescaped_key();
        if (key == "p")      t.price          = sj_double(field.value());
        else if (key == "q") t.quantity       = sj_double(field.value());
        else if (key == "t") t.trade_id       = std::to_string(sj_int(field.value()));
        else if (key == "T") t.exchange_ts_ns = sj_int(field.value()) * 1'000'000;
        else if (key == "m") {
            bool is_maker;
            if (!field.value().get_bool().get(is_maker)) {
                t.side = is_maker ? lqc::feed::Side::Sell : lqc::feed::Side::Buy;
            }
        }
    }
    return t;
}

lqc::feed::OrderBook BinanceClient::parse_book(std::string_view json,
                                               std::string_view symbol) {
    lqc::feed::OrderBook ob;
    ob.symbol      = std::string(symbol);
    ob.local_ts_ns = now_ns();

    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(json);
    auto doc = parser.iterate(padded);
    auto obj = doc.get_object();
    for (auto field : obj) {
        std::string_view key = field.unescaped_key();
        if (key == "lastUpdateId") {
            ob.exchange_ts_ns = sj_int(field.value());
        } else if (key == "bids" || key == "asks") {
            auto& dst = (key == "bids") ? ob.bids : ob.asks;
            for (auto level : field.value().get_array()) {
                auto arr = level.get_array();
                auto it = arr.begin();
                lqc::feed::PriceLevel lvl;
                if (it != arr.end()) lvl.price    = sj_double(*it);
                ++it;
                if (it != arr.end()) lvl.quantity = sj_double(*it);
                dst.push_back(lvl);
            }
        }
    }
    return ob;
}

#else

// Fallback parser (kept for builds without simdjson).
lqc::feed::Trade BinanceClient::parse_trade(std::string_view,
                                            std::string_view symbol) {
    return {std::string(symbol), 0.0, 0.0, lqc::feed::Side::Buy, 0, now_ns(), {}};
}
lqc::feed::OrderBook BinanceClient::parse_book(std::string_view,
                                               std::string_view symbol) {
    lqc::feed::OrderBook ob;
    ob.symbol = std::string(symbol);
    ob.local_ts_ns = now_ns();
    return ob;
}

#endif

}  // namespace market
