#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dwh {

// Generic time-series row. Channels:
//   "trade"   — symbol, price, qty, side
//   "book"    — symbol, bid_px, bid_qty, ask_px, ask_qty (top-of-book)
//   "news"    — ticker, sentiment_score
//   "dart"    — corp_code, rcept_no, report_nm
//
// Schema is flexible: columns are stored in `tags` (string) and `values`
// (double). The Parquet writer materializes them into typed columns.
struct Row {
    std::int64_t           ts_ns;
    std::string            channel;
    std::vector<std::pair<std::string, std::string>> tags;
    std::vector<std::pair<std::string, double>>      values;
};

class TimeSeriesStore {
public:
    virtual ~TimeSeriesStore() = default;

    virtual void append(const Row& r) = 0;
    virtual void append_batch(const std::vector<Row>& rows) = 0;

    // Query a [from, to] range filtered by channel.
    virtual std::vector<Row> query(std::string_view channel,
                                   std::int64_t from_ns,
                                   std::int64_t to_ns) = 0;

    // Flush pending rows to durable storage (Parquet → R2).
    virtual void flush() = 0;

    static std::unique_ptr<TimeSeriesStore> make_in_memory();
};

}  // namespace dwh
