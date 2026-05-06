#include "news/ner.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace news {

namespace {

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

}  // namespace

void TickerNer::add_company(std::string corp_name, std::string ticker) {
    by_alias_[corp_name] = {corp_name, ticker};
}
void TickerNer::add_alias(const std::string& ticker, std::string alias) {
    // Find canonical name (the alias whose value's ticker matches).
    for (const auto& [_, v] : by_alias_) {
        if (v.second == ticker) {
            by_alias_[alias] = {v.first, ticker};
            return;
        }
    }
}

void TickerNer::load_csv(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto cols = split_csv(line);
        if (cols.size() < 2) continue;
        const auto& ticker = cols[0];
        const auto& corp   = cols[1];
        add_company(corp, ticker);
        for (std::size_t i = 2; i < cols.size(); ++i) {
            add_alias(ticker, cols[i]);
        }
    }
}

std::vector<Mention> TickerNer::tag(const std::string& body) const {
    // Longest-match scan. Build a sorted alias list once for stable output.
    std::vector<const std::string*> aliases;
    aliases.reserve(by_alias_.size());
    for (const auto& [k, _] : by_alias_) aliases.push_back(&k);
    std::sort(aliases.begin(), aliases.end(),
              [](const std::string* a, const std::string* b) {
                  return a->size() > b->size();
              });

    std::vector<Mention> hits;
    std::vector<bool> consumed(body.size(), false);
    for (const auto* a : aliases) {
        if (a->empty()) continue;
        std::size_t p = 0;
        while ((p = body.find(*a, p)) != std::string::npos) {
            const auto end = p + a->size();
            bool overlaps = false;
            for (std::size_t i = p; i < end; ++i) {
                if (consumed[i]) { overlaps = true; break; }
            }
            if (!overlaps) {
                const auto& [corp, ticker] = by_alias_.at(*a);
                hits.push_back({ticker, corp,
                                static_cast<int>(p),
                                static_cast<int>(end)});
                std::fill(consumed.begin() + p, consumed.begin() + end, true);
            }
            p = end;
        }
    }
    std::sort(hits.begin(), hits.end(),
              [](const Mention& a, const Mention& b) {
                  return a.span_start < b.span_start;
              });
    return hits;
}

}  // namespace news
