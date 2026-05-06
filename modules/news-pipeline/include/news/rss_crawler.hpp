#pragma once

#include <string>
#include <vector>

#include "news/types.hpp"

namespace news {

struct RssSource {
    std::string name;
    std::string feed_url;
};

class RssCrawler {
public:
    explicit RssCrawler(std::vector<RssSource> sources);

    // Fetch each feed once and return parsed Article shells (title/url/source).
    // Body is fetched lazily by `hydrate_body`.
    std::vector<Article> fetch();

    // Fill `article.body` by GETting the article URL and stripping HTML.
    void hydrate_body(Article& article);

private:
    std::vector<RssSource> sources_;
};

// Default Korean financial RSS feeds.
std::vector<RssSource> default_korean_sources();

}  // namespace news
