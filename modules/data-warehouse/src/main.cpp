#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "dwh/redis_subscriber.hpp"
#include "dwh/rollup.hpp"
#include "dwh/timeseries.hpp"

namespace {

std::atomic<bool> g_run{true};
void on_sig(int) { g_run = false; }

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string env_or(const char* k, const std::string& d = "") {
    const char* v = std::getenv(k);
    return v ? std::string(v) : d;
}

double getd(const std::string& json, const std::string& key) {
    const std::string k = "\"" + key + "\":";
    auto p = json.find(k);
    if (p == std::string::npos) return 0.0;
    p += k.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '"')) ++p;
    auto e = p;
    while (e < json.size() && json[e] != ',' && json[e] != '}' &&
           json[e] != '"') ++e;
    return std::stod(json.substr(p, e - p));
}

std::string gets(const std::string& json, const std::string& key) {
    const std::string k = "\"" + key + "\":\"";
    auto p = json.find(k);
    if (p == std::string::npos) return {};
    p += k.size();
    auto e = json.find('"', p);
    return json.substr(p, e - p);
}

dwh::Row trade_to_row(const std::string& channel, const std::string& payload) {
    dwh::Row r;
    r.ts_ns   = now_ns();
    r.channel = "trade";
    r.tags    = {{"channel", channel},
                  {"symbol", gets(payload, "symbol")}};
    r.values  = {{"price", getd(payload, "price")},
                  {"qty",   getd(payload, "qty")},
                  {"side",  getd(payload, "side")}};
    return r;
}

}  // namespace

int main() {
    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    auto store = dwh::TimeSeriesStore::make_in_memory();

    dwh::RollupConfig cfg;
    cfg.out_dir       = env_or("LQC_DWH_OUT", "/opt/lqc/var/dwh");
    cfg.interval      = std::chrono::seconds(
        std::stoi(env_or("LQC_DWH_INTERVAL_SEC", "300")));
    cfg.retention     = std::chrono::seconds(
        std::stoi(env_or("LQC_DWH_RETENTION_SEC", "21600")));
    cfg.r2_endpoint   = env_or("R2_ENDPOINT");
    cfg.r2_bucket     = env_or("R2_BUCKET", "lemuel-quant");
    cfg.r2_access_key = env_or("R2_ACCESS_KEY");
    cfg.r2_secret_key = env_or("R2_SECRET_KEY");

    dwh::RedisSubscriber sub(
        env_or("LQC_REDIS_HOST", "127.0.0.1"),
        std::stoi(env_or("LQC_REDIS_PORT", "6379")),
        {"trade.binance.*", "trade.kis.*", "news.score.*"});
    sub.on_message([&](const std::string& ch, const std::string& payload) {
        if (ch.rfind("trade.", 0) == 0) {
            store->append(trade_to_row(ch, payload));
        }
        // 추가 채널은 여기서 dispatch
    });
    sub.start();

    std::cout << "[dwh] running; interval="
              << cfg.interval.count() << "s, out=" << cfg.out_dir.string()
              << ", r2=" << (cfg.r2_access_key.empty() ? "off" : "on") << "\n";

    auto last_until = now_ns();
    auto next_tick  = std::chrono::steady_clock::now() + cfg.interval;
    while (g_run) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::chrono::steady_clock::now() < next_tick) continue;
        const auto until = now_ns();
        try {
            dwh::rollup_once(*store, last_until, until, cfg);
        } catch (const std::exception& e) {
            std::cerr << "[dwh] rollup failed: " << e.what() << "\n";
        }
        last_until = until;
        next_tick  = std::chrono::steady_clock::now() + cfg.interval;
    }
    sub.stop();
    return 0;
}
