#pragma once

#include <filesystem>
#include <string>

namespace dwh {

// Cloudflare R2 = S3-compatible object storage. Endpoint:
//   https://<account>.r2.cloudflarestorage.com
class R2Uploader {
public:
    struct Config {
        std::string endpoint;          // https://<acct>.r2.cloudflarestorage.com
        std::string region{"auto"};
        std::string bucket;
        std::string access_key;
        std::string secret_key;
    };

    explicit R2Uploader(Config cfg);

    // Returns the resulting object key, or empty string on failure.
    std::string upload(const std::filesystem::path& local,
                       const std::string& key_hint);

private:
    Config cfg_;
};

}  // namespace dwh
