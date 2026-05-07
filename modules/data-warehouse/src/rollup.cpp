#include "dwh/rollup.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "dwh/parquet_writer.hpp"
#include "dwh/r2_uploader.hpp"

namespace dwh {

namespace {

std::string ymd_hms(std::int64_t ts_ns) {
    using namespace std::chrono;
    const auto secs = ts_ns / 1'000'000'000LL;
    const std::time_t t = static_cast<std::time_t>(secs);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return os.str();
}

}  // namespace

std::filesystem::path rollup_once(TimeSeriesStore& store,
                                  std::int64_t since_ns,
                                  std::int64_t until_ns,
                                  const RollupConfig& cfg) {
    std::filesystem::create_directories(cfg.out_dir);

    static const char* channels[] = {"trade", "book", "news", "dart"};
    std::vector<Row> all;
    for (const char* ch : channels) {
        auto rows = store.query(ch, since_ns, until_ns);
        all.insert(all.end(),
                   std::make_move_iterator(rows.begin()),
                   std::make_move_iterator(rows.end()));
    }
    if (all.empty()) return {};

    const auto fname = "rollup-" + ymd_hms(until_ns) + ".parquet";
    const auto path  = cfg.out_dir / fname;
    ParquetWriter writer(path);
    writer.write(all);

    if (!cfg.r2_access_key.empty() && !cfg.r2_endpoint.empty()) {
        R2Uploader::Config uc;
        uc.endpoint   = cfg.r2_endpoint;
        uc.bucket     = cfg.r2_bucket;
        uc.access_key = cfg.r2_access_key;
        uc.secret_key = cfg.r2_secret_key;
        R2Uploader up(uc);
        const std::string key = cfg.key_prefix + "/" + fname;
        const auto rk = up.upload(path, key);
        if (!rk.empty()) {
            std::cout << "[dwh] r2 upload " << rk << " ("
                      << all.size() << " rows)\n";
        } else {
            std::cerr << "[dwh] r2 upload failed for " << key << "\n";
        }
    } else {
        std::cout << "[dwh] local-only rollup " << path << " ("
                  << all.size() << " rows)\n";
    }
    return path;
}

}  // namespace dwh
