#pragma once

#include <chrono>
#include <filesystem>
#include <string>

#include "dwh/timeseries.hpp"

namespace dwh {

// Rolls up the in-memory store into a Parquet file every interval and
// optionally uploads to Cloudflare R2. After successful upload, prunes
// rows older than `retention`.
struct RollupConfig {
    std::filesystem::path     out_dir{"/opt/lqc/var/dwh"};
    std::chrono::seconds      interval{300};   // 5 min
    std::chrono::seconds      retention{6 * 3600};
    std::string               r2_endpoint;
    std::string               r2_bucket;
    std::string               r2_access_key;
    std::string               r2_secret_key;
    std::string               key_prefix{"snapshots"};
};

// Drains rows newer than the previous rollup, materializes a Parquet,
// uploads to R2 if creds present, returns the local file path.
std::filesystem::path rollup_once(TimeSeriesStore& store,
                                  std::int64_t since_ns,
                                  std::int64_t until_ns,
                                  const RollupConfig& cfg);

}  // namespace dwh
