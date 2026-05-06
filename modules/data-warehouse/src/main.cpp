#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "dwh/parquet_writer.hpp"
#include "dwh/r2_uploader.hpp"
#include "dwh/timeseries.hpp"

namespace {

std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string env_or(const char* k, const std::string& d) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : d;
}

}  // namespace

int main() {
    auto store = dwh::TimeSeriesStore::make_in_memory();

    // Smoke ingest a few rows so the rotation path is exercised.
    dwh::Row r;
    r.ts_ns   = now_ns();
    r.channel = "trade";
    r.tags    = {{"symbol", "btcusdt"}, {"side", "1"}};
    r.values  = {{"price", 65000.0}, {"qty", 0.01}};
    store->append(r);

    const auto rows = store->query("trade", 0, std::numeric_limits<std::int64_t>::max());
    const auto out  = std::filesystem::temp_directory_path() / "lqc-dwh-smoke.parquet";
    dwh::ParquetWriter writer(out);
    writer.write(rows);
    std::cout << "[dwh] wrote " << rows.size() << " rows to " << writer.path()
              << " (csv fallback path: " << out.string() << " -> .csv)\n";

    if (const char* ak = std::getenv("R2_ACCESS_KEY")) {
        dwh::R2Uploader::Config cfg;
        cfg.endpoint   = env_or("R2_ENDPOINT", "");
        cfg.bucket     = env_or("R2_BUCKET",   "lemuel-quant");
        cfg.access_key = ak;
        cfg.secret_key = env_or("R2_SECRET_KEY", "");
        dwh::R2Uploader up(cfg);
        const auto key = "snapshots/" + std::to_string(now_ns()) + ".parquet";
        const auto rk  = up.upload(out, key);
        std::cout << "[dwh] r2 upload key=" << rk << "\n";
    }
    return 0;
}
