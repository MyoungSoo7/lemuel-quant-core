#include "dart/dart_client.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>

#ifdef LQC_HAS_CURL
#include <curl/curl.h>
#endif

namespace dart {

namespace {

#ifdef LQC_HAS_CURL
std::size_t curl_write(char* p, std::size_t s, std::size_t n, void* ud) {
    static_cast<std::string*>(ud)->append(p, s * n);
    return s * n;
}
#endif

// Tiny extractor matching market-feed's: enough to pull "rcept_no":"…",
// "report_nm":"…" etc. without an external JSON dep. Replace with simdjson
// when the wider pipeline picks one up.
[[maybe_unused]] std::string extract(const std::string& json,
                                     const std::string& key,
                                     std::size_t from = 0) {
    const std::string k = "\"" + key + "\":";
    auto p = json.find(k, from);
    if (p == std::string::npos) return {};
    p += k.size();
    while (p < json.size() && json[p] == ' ') ++p;
    if (p < json.size() && json[p] == '"') {
        ++p;
        const auto e = json.find('"', p);
        return json.substr(p, e - p);
    }
    const auto e = json.find_first_of(",}", p);
    return json.substr(p, e - p);
}

}  // namespace

struct DartClient::Impl {
    std::string api_key;
    std::string base_url;
};

DartClient::DartClient(std::string api_key, std::string base_url)
    : impl_(std::make_unique<Impl>(Impl{std::move(api_key),
                                         std::move(base_url)})) {}
DartClient::~DartClient() = default;

std::vector<Disclosure> DartClient::list(const ListQuery& q) {
    std::vector<Disclosure> out;

#ifdef LQC_HAS_CURL
    std::ostringstream url;
    url << impl_->base_url << "/api/list.json"
        << "?crtfc_key=" << impl_->api_key
        << "&page_no=" << q.page_no
        << "&page_count=" << q.page_count;
    if (!q.corp_code.empty())  url << "&corp_code=" << q.corp_code;
    if (!q.bgn_de.empty())     url << "&bgn_de="    << q.bgn_de;
    if (!q.end_de.empty())     url << "&end_de="    << q.end_de;
    if (!q.pblntf_ty.empty())  url << "&pblntf_ty=" << q.pblntf_ty;

    CURL* c = curl_easy_init();
    if (!c) return out;
    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    const auto rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) return out;

    // body 안의 list 배열을 객체 단위로 쪼갠 다음 필드 추출.
    std::size_t p = 0;
    while ((p = body.find("\"rcept_no\"", p)) != std::string::npos) {
        const auto end = body.find('}', p);
        if (end == std::string::npos) break;
        const std::string obj = body.substr(p, end - p);
        Disclosure d;
        d.rcept_no   = extract(obj, "rcept_no");
        d.corp_code  = extract(obj, "corp_code");
        d.corp_name  = extract(obj, "corp_name");
        d.stock_code = extract(obj, "stock_code");
        d.corp_cls   = extract(obj, "corp_cls");
        d.report_nm  = extract(obj, "report_nm");
        d.flr_nm     = extract(obj, "flr_nm");
        d.rcept_dt   = extract(obj, "rcept_dt");
        d.rm         = extract(obj, "rm");
        out.push_back(std::move(d));
        p = end + 1;
    }
#else
    (void)q;
    std::cerr << "[dart] libcurl not linked; list() returning empty.\n";
#endif
    return out;
}

void DartClient::poll_loop(std::chrono::seconds interval,
                           std::function<void(const Disclosure&)> on_new,
                           std::function<bool()> keep_running) {
    std::unordered_set<std::string> seen;
    while (keep_running()) {
        ListQuery q;
        q.page_count = 100;
        q.page_no = 1;
        for (auto& d : list(q)) {
            if (seen.insert(d.rcept_no).second) on_new(d);
        }
        std::this_thread::sleep_for(interval);
    }
}

}  // namespace dart
