#include "dwh/r2_uploader.hpp"

#include <iostream>

#ifdef LQC_HAS_AWS_S3
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#endif

namespace dwh {

R2Uploader::R2Uploader(Config cfg) : cfg_(std::move(cfg)) {}

std::string R2Uploader::upload(const std::filesystem::path& local,
                               const std::string& key_hint) {
#ifdef LQC_HAS_AWS_S3
    Aws::SDKOptions opts;
    Aws::InitAPI(opts);
    Aws::Client::ClientConfiguration ccfg;
    ccfg.endpointOverride = cfg_.endpoint;
    ccfg.region           = cfg_.region;
    Aws::Auth::AWSCredentials creds(cfg_.access_key, cfg_.secret_key);
    Aws::S3::S3Client s3(creds, ccfg);

    Aws::S3::Model::PutObjectRequest req;
    req.WithBucket(cfg_.bucket).WithKey(key_hint);
    auto data = Aws::MakeShared<Aws::FStream>(
        "lqc", local.c_str(), std::ios_base::in | std::ios_base::binary);
    req.SetBody(data);

    const auto resp = s3.PutObject(req);
    Aws::ShutdownAPI(opts);
    return resp.IsSuccess() ? key_hint : std::string{};
#else
    (void)local;
    std::cerr << "[r2] AWS SDK not linked; skipping upload of " << key_hint
              << "\n";
    return {};
#endif
}

}  // namespace dwh
