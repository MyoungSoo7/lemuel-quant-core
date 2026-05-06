#include "news/news_score.hpp"

#include <cmath>

namespace news {

NewsScoreBoard::NewsScoreBoard(std::chrono::seconds half_life)
    : half_life_(half_life) {}

void NewsScoreBoard::ingest(const ScoredArticle& a) {
    const double s = static_cast<double>(static_cast<int>(a.sentiment)) *
                     a.sentiment_confidence;
    if (s == 0.0 || a.mentions.empty()) return;
    for (const auto& m : a.mentions) {
        entries_[m.ticker].push_back({s, a.article.published_at});
    }
}

double NewsScoreBoard::score(const std::string& ticker,
                             std::chrono::system_clock::time_point now) const {
    auto it = entries_.find(ticker);
    if (it == entries_.end()) return 0.0;
    const double half = static_cast<double>(half_life_.count());
    double sum = 0.0;
    auto& vec = it->second;
    auto write = vec.begin();
    for (auto& e : vec) {
        const double age =
            std::chrono::duration_cast<std::chrono::seconds>(now - e.ts)
                .count();
        if (age > half * 8) continue;   // 8 half-lives ≈ negligible
        sum += e.contribution * std::pow(0.5, age / half);
        *write++ = e;
    }
    vec.erase(write, vec.end());
    return sum;
}

std::unordered_map<std::string, double> NewsScoreBoard::snapshot(
    std::chrono::system_clock::time_point now) const {
    std::unordered_map<std::string, double> out;
    out.reserve(entries_.size());
    for (auto& [ticker, _] : entries_) out[ticker] = score(ticker, now);
    return out;
}

}  // namespace news
