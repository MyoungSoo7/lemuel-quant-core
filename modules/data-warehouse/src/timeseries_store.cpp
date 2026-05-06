#include "dwh/timeseries.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

namespace dwh {

namespace {

class InMemoryStore : public TimeSeriesStore {
public:
    void append(const Row& r) override {
        std::lock_guard lk(m_);
        by_channel_[r.channel].push_back(r);
    }
    void append_batch(const std::vector<Row>& rows) override {
        std::lock_guard lk(m_);
        for (const auto& r : rows) by_channel_[r.channel].push_back(r);
    }
    std::vector<Row> query(std::string_view channel,
                           std::int64_t from_ns,
                           std::int64_t to_ns) override {
        std::lock_guard lk(m_);
        std::vector<Row> out;
        const auto it = by_channel_.find(std::string(channel));
        if (it == by_channel_.end()) return out;
        for (const auto& r : it->second) {
            if (r.ts_ns >= from_ns && r.ts_ns <= to_ns) out.push_back(r);
        }
        std::sort(out.begin(), out.end(),
                  [](const Row& a, const Row& b) { return a.ts_ns < b.ts_ns; });
        return out;
    }
    void flush() override {
        // No-op in memory; production wiring routes channels into ParquetWriter
        // and rotates per-day files for R2 upload.
    }

private:
    std::mutex m_;
    std::map<std::string, std::vector<Row>> by_channel_;
};

}  // namespace

std::unique_ptr<TimeSeriesStore> TimeSeriesStore::make_in_memory() {
    return std::make_unique<InMemoryStore>();
}

}  // namespace dwh
