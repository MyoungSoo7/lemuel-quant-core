#include "news/rss_crawler.hpp"

#include <algorithm>
#include <chrono>

#ifdef LQC_HAS_CURL
#include <curl/curl.h>
#endif

namespace news {

namespace {

#ifdef LQC_HAS_CURL
std::size_t curl_write(char* p, std::size_t s, std::size_t n, void* ud) {
    static_cast<std::string*>(ud)->append(p, s * n);
    return s * n;
}

std::string http_get(const std::string& url) {
    CURL* c = curl_easy_init();
    if (!c) return {};
    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
    curl_easy_perform(c);
    curl_easy_cleanup(c);
    return body;
}
#else
std::string http_get(const std::string&) { return {}; }
#endif

// Strip HTML tags. Crude but enough for skeleton; production swaps to gumbo.
std::string strip_tags(std::string_view html) {
    std::string out;
    out.reserve(html.size());
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') in_tag = true;
        else if (c == '>') in_tag = false;
        else if (!in_tag) out.push_back(c);
    }
    return out;
}

// Pull every <item>…</item> block, extract <title> and <link>.
std::vector<std::pair<std::string, std::string>> parse_rss_items(
    const std::string& xml) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t p = 0;
    while ((p = xml.find("<item", p)) != std::string::npos) {
        const auto end = xml.find("</item>", p);
        if (end == std::string::npos) break;
        const auto block = xml.substr(p, end - p);

        auto find_tag = [&](const std::string& tag) -> std::string {
            const auto a = block.find("<" + tag);
            if (a == std::string::npos) return {};
            const auto open_end = block.find('>', a);
            if (open_end == std::string::npos) return {};
            const auto b = block.find("</" + tag + ">", open_end);
            if (b == std::string::npos) return {};
            std::string raw = block.substr(open_end + 1, b - open_end - 1);
            const auto cdata = raw.find("<![CDATA[");
            if (cdata != std::string::npos) {
                const auto end_cdata = raw.find("]]>", cdata);
                if (end_cdata != std::string::npos) {
                    raw = raw.substr(cdata + 9, end_cdata - cdata - 9);
                }
            }
            return raw;
        };

        out.emplace_back(find_tag("title"), find_tag("link"));
        p = end + 7;
    }
    return out;
}

}  // namespace

RssCrawler::RssCrawler(std::vector<RssSource> sources)
    : sources_(std::move(sources)) {}

std::vector<Article> RssCrawler::fetch() {
    std::vector<Article> out;
    for (const auto& src : sources_) {
        const auto xml = http_get(src.feed_url);
        if (xml.empty()) continue;
        for (auto& [title, link] : parse_rss_items(xml)) {
            Article a;
            a.source = src.name;
            a.title  = std::move(title);
            a.url    = std::move(link);
            a.published_at = std::chrono::system_clock::now();
            out.push_back(std::move(a));
        }
    }
    return out;
}

void RssCrawler::hydrate_body(Article& article) {
    if (article.url.empty()) return;
    const auto html = http_get(article.url);
    article.body = strip_tags(html);
}

std::vector<RssSource> default_korean_sources() {
    return {
        {"yna",       "https://www.yna.co.kr/rss/economy.xml"},
        {"hankyung",  "https://www.hankyung.com/feed/finance"},
        {"mt",        "https://rss.mt.co.kr/mt_rss.xml"},
    };
}

}  // namespace news
