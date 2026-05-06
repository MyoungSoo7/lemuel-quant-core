#include "dwh/parquet_writer.hpp"

#include <fstream>
#include <iostream>
#include <set>

#ifdef LQC_HAS_PARQUET
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>
#endif

namespace dwh {

ParquetWriter::ParquetWriter(std::filesystem::path output_path)
    : path_(std::move(output_path)) {}

namespace {

void write_csv_fallback(const std::filesystem::path& path,
                        const std::vector<Row>& rows) {
    std::set<std::string> tag_keys, value_keys;
    for (const auto& r : rows) {
        for (const auto& [k, _] : r.tags)   tag_keys.insert(k);
        for (const auto& [k, _] : r.values) value_keys.insert(k);
    }
    auto out_path = path;
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

#ifdef LQC_HAS_PARQUET

// Build an Arrow Table by discovering the union of tag/value keys, then
// project each row's columns.
arrow::Result<std::shared_ptr<arrow::Table>> rows_to_table(
    const std::vector<Row>& rows) {
    std::set<std::string> tag_keys, value_keys;
    for (const auto& r : rows) {
        for (const auto& [k, _] : r.tags)   tag_keys.insert(k);
        for (const auto& [k, _] : r.values) value_keys.insert(k);
    }

    arrow::Int64Builder ts_b;
    arrow::StringBuilder channel_b;
    std::vector<std::pair<std::string, arrow::StringBuilder>> tag_builders;
    std::vector<std::pair<std::string, arrow::DoubleBuilder>> value_builders;
    tag_builders.reserve(tag_keys.size());
    for (const auto& k : tag_keys)
        tag_builders.emplace_back(k, arrow::StringBuilder{});
    value_builders.reserve(value_keys.size());
    for (const auto& k : value_keys)
        value_builders.emplace_back(k, arrow::DoubleBuilder{});

    for (const auto& r : rows) {
        ARROW_RETURN_NOT_OK(ts_b.Append(r.ts_ns));
        ARROW_RETURN_NOT_OK(channel_b.Append(r.channel));
        for (auto& [k, b] : tag_builders) {
            std::string val;
            for (const auto& [tk, tv] : r.tags) {
                if (tk == k) { val = tv; break; }
            }
            if (val.empty()) ARROW_RETURN_NOT_OK(b.AppendNull());
            else             ARROW_RETURN_NOT_OK(b.Append(val));
        }
        for (auto& [k, b] : value_builders) {
            bool found = false;
            for (const auto& [vk, vv] : r.values) {
                if (vk == k) {
                    ARROW_RETURN_NOT_OK(b.Append(vv));
                    found = true;
                    break;
                }
            }
            if (!found) ARROW_RETURN_NOT_OK(b.AppendNull());
        }
    }

    std::vector<std::shared_ptr<arrow::Field>>  fields;
    std::vector<std::shared_ptr<arrow::Array>>  arrays;

    fields.push_back(arrow::field("ts_ns",   arrow::int64()));
    fields.push_back(arrow::field("channel", arrow::utf8()));
    {
        std::shared_ptr<arrow::Array> a;
        ARROW_RETURN_NOT_OK(ts_b.Finish(&a));      arrays.push_back(a);
        ARROW_RETURN_NOT_OK(channel_b.Finish(&a)); arrays.push_back(a);
    }
    for (auto& [k, b] : tag_builders) {
        fields.push_back(arrow::field("tag." + k, arrow::utf8()));
        std::shared_ptr<arrow::Array> a;
        ARROW_RETURN_NOT_OK(b.Finish(&a));
        arrays.push_back(a);
    }
    for (auto& [k, b] : value_builders) {
        fields.push_back(arrow::field("val." + k, arrow::float64()));
        std::shared_ptr<arrow::Array> a;
        ARROW_RETURN_NOT_OK(b.Finish(&a));
        arrays.push_back(a);
    }

    auto schema = std::make_shared<arrow::Schema>(fields);
    return arrow::Table::Make(schema, arrays);
}

#endif

}  // namespace

void ParquetWriter::write(const std::vector<Row>& rows) {
    if (rows.empty()) return;

#ifdef LQC_HAS_PARQUET
    auto table_res = rows_to_table(rows);
    if (!table_res.ok()) {
        std::cerr << "[parquet] table build failed: "
                  << table_res.status().ToString() << "\n";
        write_csv_fallback(path_, rows);
        return;
    }
    auto table = *table_res;

    std::shared_ptr<arrow::io::FileOutputStream> out;
    auto open_res = arrow::io::FileOutputStream::Open(path_.string());
    if (!open_res.ok()) {
        std::cerr << "[parquet] open failed: "
                  << open_res.status().ToString() << "\n";
        write_csv_fallback(path_, rows);
        return;
    }
    out = *open_res;

    auto status = parquet::arrow::WriteTable(
        *table, arrow::default_memory_pool(), out, /*chunk_size=*/64 * 1024);
    if (!status.ok()) {
        std::cerr << "[parquet] write failed: " << status.ToString() << "\n";
        write_csv_fallback(path_, rows);
    }
#else
    write_csv_fallback(path_, rows);
#endif
}

}  // namespace dwh
