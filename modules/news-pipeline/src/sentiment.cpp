#include "news/sentiment.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

#include "news/finbert.hpp"

namespace news {

namespace {

class LexiconScorer : public SentimentScorer {
public:
    Result score(const std::string& text) const override {
        // 시드 사전. 운영 시 KOSAC + 도메인 보강.
        static const std::unordered_set<std::string> pos = {
            "상승", "급등", "호실적", "성장", "흑자", "수주", "확대",
            "신고가", "최고", "회복", "개선", "달성",
        };
        static const std::unordered_set<std::string> neg = {
            "하락", "급락", "적자", "악재", "감산", "감원", "리콜",
            "신저가", "부진", "악화", "충격", "우려", "조사", "처분",
        };
        int p = 0, n = 0;
        for (const auto& w : pos) if (text.find(w) != std::string::npos) ++p;
        for (const auto& w : neg) if (text.find(w) != std::string::npos) ++n;
        if (p == 0 && n == 0) return {Sentiment::Neutral, 0.0};
        const double s = static_cast<double>(p - n) / (p + n);
        Result r;
        r.label = s > 0.1 ? Sentiment::Positive
                  : s < -0.1 ? Sentiment::Negative
                             : Sentiment::Neutral;
        r.confidence = std::min(1.0, std::abs(s) + 0.1 * (p + n));
        return r;
    }
};

class FinBertScorer : public SentimentScorer {
public:
    explicit FinBertScorer(std::unique_ptr<FinBertModel> model)
        : model_(std::move(model)) {}

    Result score(const std::string& text) const override {
        if (!model_) return {Sentiment::Neutral, 0.0};
        const auto p = model_->predict(text);
        Result r;
        const int label = p.argmax_label();
        r.label = label > 0 ? Sentiment::Positive
                  : label < 0 ? Sentiment::Negative
                              : Sentiment::Neutral;
        r.confidence = p.confidence();
        return r;
    }

private:
    std::unique_ptr<FinBertModel> model_;
};

}  // namespace

std::unique_ptr<SentimentScorer> SentimentScorer::make_lexicon() {
    return std::make_unique<LexiconScorer>();
}

std::unique_ptr<SentimentScorer> SentimentScorer::make_finbert(
    const std::string& model_dir) {
    auto m = FinBertModel::load(model_dir);
    if (!m) return make_lexicon();
    return std::make_unique<FinBertScorer>(std::move(m));
}

std::unique_ptr<SentimentScorer> SentimentScorer::make_default(
    const std::string& model_dir) {
    if (!model_dir.empty()) return make_finbert(model_dir);
    return make_lexicon();
}

}  // namespace news
