#include "dwh/parquet_writer.hpp"

#include <fstream>
#include <set>

#ifdef LQC_HAS_PARQUET
// Real Parquet writer goes here (arrow/io/file.h, parquet/arrow/writer.h).
// Skeleton keeps the CSV fallback compiled-in either way for visibility.
#endif

namespace dwh {

ParquetWriter::ParquetWriter(std::filesystem::path output_path)
    : path_(std::move(output_path)) {}

void ParquetWriter::write(const std::vector<Row>& rows) {
    if (rows.empty()) return;

    // Discover the union of tag keys + value keys across rows so the CSV
    // fallback has a stable header. Production Parquet writer builds an
    // Arrow Schema instead, but the column discovery step is identical.
    std::set<std::string> tag_keys, value_keys;
    for (const auto& r : rows) {
        for (const auto& [k, _] : r.tags)   tag_keys.insert(k);
        for (const auto& [k, _] : r.values) value_keys.insert(k);
    }

    auto out_path = path_;
    out_path.replace_extension(".csv");
    std::ofstream out(out_path);

    out << "ts_ns,channel";
    for (const auto& k : tag_keys)   out << ",tag." << k;
    for (const auto& k : value_keys) out << ",val." << k;
    out << "\n";

    for (const auto& r : rows) {
        out << r.ts_ns << "," << r.channel;
        for (const auto& k : tag_keys) {
            out << ",";
            for (const auto& [tk, tv] : r.tags) {
                if (tk == k) { out << tv; break; }
            }
        }
        for (const auto& k : value_keys) {
            out << ",";
            for (const auto& [vk, vv] : r.values) {
                if (vk == k) { out << vv; break; }
            }
        }
        out << "\n";
    }
}

}  // namespace dwh
