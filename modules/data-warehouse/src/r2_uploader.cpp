#include "dwh/r2_uploader.hpp"

#include <iostream>

#ifdef LQC_HAS_AWS_S3
#include <fstream>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
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
    ccfg.endpointOverride = cfg_.endpoint.c_str();
    ccfg.region           = cfg_.region.c_str();
    Aws::Auth::AWSCredentials creds(cfg_.access_key.c_str(),
                                     cfg_.secret_key.c_str());
    auto creds_provider =
        Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("lqc", creds);
    Aws::S3::S3Client s3(
        creds_provider, /*endpointProvider*/ nullptr, ccfg);

    Aws::S3::Model::PutObjectRequest req;
    req.WithBucket(cfg_.bucket.c_str()).WithKey(key_hint.c_str());
    auto body = Aws::MakeShared<Aws::StringStream>("lqc");
    {
        std::ifstream src(local, std::ios_base::in | std::ios_base::binary);
        (*body) << src.rdbuf();
    }
    req.SetBody(body);

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
