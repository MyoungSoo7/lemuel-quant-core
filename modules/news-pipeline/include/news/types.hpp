#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace news {

struct Article {
    std::string source;          // "yna", "naver", "hankyung", ...
    std::string url;
    std::string title;
    std::string body;
    std::chrono::system_clock::time_point published_at;
};

struct Mention {
    std::string ticker;          // 6자리 종목코드 (matched via dictionary)
    std::string corp_name;
    int         span_start{};    // body offset
    int         span_end{};
};

enum class Sentiment : int {
    Negative = -1,
    Neutral  = 0,
    Positive = 1,
};

struct ScoredArticle {
    Article              article;
    std::vector<Mention> mentions;
    Sentiment            sentiment{Sentiment::Neutral};
    double               sentiment_confidence{0.0};
};

}  // namespace news
