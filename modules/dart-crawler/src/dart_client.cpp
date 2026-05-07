#include "dart/dart_client.hpp"

#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>

#ifdef LQC_HAS_CURL
#include <curl/curl.h>
#endif

#ifdef LQC_HAS_SIMDJSON
#include <simdjson.h>
#endif

namespace dart {

namespace {

#ifdef LQC_HAS_CURL
std::size_t curl_write(char* p, std::size_t s, std::size_t n, void* ud) {
    static_cast<std::string*>(ud)->append(p, s * n);
    return s * n;
}
#endif

#ifdef LQC_HAS_SIMDJSON
std::string sj_str(simdjson::ondemand::value v) {
    std::string_view s;
    if (v.get_string().get(s) == simdjson::SUCCESS) return std::string(s);
    return {};
}
#endif

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
    CURL* c = curl_easy_init();
    if (!c) return out;
    auto esc = [c](const std::string& s) {
        char* e = curl_easy_escape(c, s.c_str(),
                                    static_cast<int>(s.size()));
        std::string out_s = e ? e : s;
        if (e) curl_free(e);
        return out_s;
    };
    std::ostringstream url;
    url << impl_->base_url << "/api/list.json"
        << "?crtfc_key=" << esc(impl_->api_key)
        << "&page_no="   << q.page_no
        << "&page_count="<< q.page_count;
    if (!q.corp_code.empty())  url << "&corp_code=" << esc(q.corp_code);
    if (!q.bgn_de.empty())     url << "&bgn_de="    << esc(q.bgn_de);
    if (!q.end_de.empty())     url << "&end_de="    << esc(q.end_de);
    if (!q.pblntf_ty.empty())  url << "&pblntf_ty=" << esc(q.pblntf_ty);

    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "lemuel-quant-core/0.1");
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    const auto rc = curl_easy_perform(c);
    long http_code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        std::cerr << "[dart] curl error " << rc << ": "
                  << curl_easy_strerror(rc) << " (url=" << url.str() << ")\n";
        return out;
    }
    if (http_code != 200) {
        std::cerr << "[dart] HTTP " << http_code << " for " << url.str()
                  << " body=" << body.substr(0, 200) << "\n";
        return out;
    }

#ifdef LQC_HAS_SIMDJSON
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(body);
    auto doc = parser.iterate(padded);
    bool saw_list = false;
    for (auto top : doc.get_object()) {
        std::string_view top_key;
        if (top.unescaped_key().get(top_key) != simdjson::SUCCESS) continue;
        if (top_key != "list") continue;
        saw_list = true;
        for (auto item : top.value().get_array()) {
            Disclosure d;
            for (auto field : item.get_object()) {
                std::string_view key = field.unescaped_key();
                if      (key == "rcept_no")   d.rcept_no   = sj_str(field.value());
                else if (key == "corp_code")  d.corp_code  = sj_str(field.value());
                else if (key == "corp_name")  d.corp_name  = sj_str(field.value());
                else if (key == "stock_code") d.stock_code = sj_str(field.value());
                else if (key == "corp_cls")   d.corp_cls   = sj_str(field.value());
                else if (key == "report_nm")  d.report_nm  = sj_str(field.value());
                else if (key == "flr_nm")     d.flr_nm     = sj_str(field.value());
                else if (key == "rcept_dt")   d.rcept_dt   = sj_str(field.value());
                else if (key == "rm")         d.rm         = sj_str(field.value());
            }
            if (!d.rcept_no.empty()) out.push_back(std::move(d));
        }
        break;
    }
    if (!saw_list) {
        std::cerr << "[dart] no 'list' field in response (preview: "
                  << body.substr(0, 200) << ")\n";
    }
#else
    (void)body;
#endif

#else
    (void)q;
    std::cerr << "[dart] libcurl not linked; list() returning empty.\n";
#endif
    return out;
}

void DartClient::poll_loop(std::chrono::seconds interval,
                           std::function<void(const Disclosure&)> on_new,
                           std::function<bool()> keep_running) {
    // Bounded LRU-style: keep only the most recent N rcept_no's. DART公시
    // 번호는 시간순 단조 증가하므로 가장 작은 값부터 evict.
    constexpr std::size_t kMaxSeen = 10'000;
    std::unordered_set<std::string> seen;
    std::deque<std::string> seen_order;
    while (keep_running()) {
        ListQuery q;
        q.page_count = 100;
        q.page_no = 1;
        for (auto& d : list(q)) {
            if (seen.insert(d.rcept_no).second) {
                seen_order.push_back(d.rcept_no);
                if (seen_order.size() > kMaxSeen) {
                    seen.erase(seen_order.front());
                    seen_order.pop_front();
                }
                on_new(d);
            }
        }
        std::this_thread::sleep_for(interval);
    }
}

}  // namespace dart
