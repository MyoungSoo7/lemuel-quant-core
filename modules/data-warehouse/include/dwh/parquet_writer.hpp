#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "dwh/timeseries.hpp"

namespace dwh {

class ParquetWriter {
public:
    explicit ParquetWriter(std::filesystem::path output_path);

    // Writes rows as a single row-group. When Arrow/Parquet is not linked
    // at build time this falls back to CSV with the same path (.csv).
    void write(const std::vector<Row>& rows);

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace dwh
