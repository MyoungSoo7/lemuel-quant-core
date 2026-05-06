#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "news/types.hpp"

namespace news {

// Dictionary-based ticker NER. Loads (corp_name, ticker) pairs and finds
// occurrences of the corp_name (and a few aliases) in article body.
//
// For ambiguous matches like "삼성" → 삼성전자/삼성SDI/..., a longest-match
// rule is applied, with optional disambiguation via co-occurrence weights.
class TickerNer {
public:
    // Add a (corp_name, ticker) pair. Aliases can be added separately.
    void add_company(std::string corp_name, std::string ticker);
    void add_alias(const std::string& ticker, std::string alias);

    // Returns mentions sorted by span_start.
    std::vector<Mention> tag(const std::string& body) const;

    // Bulk-load from a CSV at `path`: ticker,corp_name,alias1,alias2,...
    void load_csv(const std::string& path);

    std::size_t size() const { return by_alias_.size(); }

private:
    // alias → (canonical corp_name, ticker)
    std::unordered_map<std::string, std::pair<std::string, std::string>>
        by_alias_;
};

}  // namespace news
