#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "news/types.hpp"

namespace news {

// Per-ticker rolling sentiment score with exponential decay.
//
// Each new ScoredArticle contributes:
//   contribution = sentiment_value * confidence
// to every mentioned ticker, decayed by the article's age.
class NewsScoreBoard {
public:
    explicit NewsScoreBoard(std::chrono::seconds half_life =
                                std::chrono::hours(2));

    void ingest(const ScoredArticle& a);

    // Decayed score for `ticker` evaluated at `now`. Range roughly [-N, +N]
    // where N = number of recent contributions.
    double score(const std::string& ticker,
                 std::chrono::system_clock::time_point now) const;

    std::unordered_map<std::string, double> snapshot(
        std::chrono::system_clock::time_point now) const;

private:
    struct Entry {
        double                                 contribution;
        std::chrono::system_clock::time_point  ts;
    };
    std::chrono::seconds half_life_;
    mutable std::unordered_map<std::string, std::vector<Entry>> entries_;
};

}  // namespace news
