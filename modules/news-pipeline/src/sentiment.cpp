#include "news/sentiment.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

#ifdef LQC_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace news {

namespace {

class LexiconScorer : public SentimentScorer {
public:
    Result score(const std::string& text) const override {
        // Tiny seed lexicon. Production: use 한국어 감성사전(KOSAC) + 도메인 보강.
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
        const double score = static_cast<double>(p - n) / (p + n);
        Result r;
        r.label = score > 0.1 ? Sentiment::Positive
                  : score < -0.1 ? Sentiment::Negative
                                 : Sentiment::Neutral;
        r.confidence = std::min(1.0, std::abs(score) + 0.1 * (p + n));
        return r;
    }
};

#ifdef LQC_HAS_ONNX
class KoBertScorer : public SentimentScorer {
public:
    explicit KoBertScorer(const std::string& model_path)
        : env_(ORT_LOGGING_LEVEL_WARNING, "lqc-news"),
          session_opts_(),
          session_(env_, model_path.c_str(), session_opts_) {}

    Result score(const std::string& text) const override {
        // TODO: tokenize via SentencePiece + run session_.Run(input_ids,
        // attention_mask) → softmax over [neg, neutral, pos].
        // The scaffold returns neutral so the dependency is exercised.
        (void)text;
        return {Sentiment::Neutral, 0.0};
    }

private:
    Ort::Env             env_;
    Ort::SessionOptions  session_opts_;
    Ort::Session         session_;
};
#endif

}  // namespace

std::unique_ptr<SentimentScorer> SentimentScorer::make_lexicon() {
    return std::make_unique<LexiconScorer>();
}

std::unique_ptr<SentimentScorer> SentimentScorer::make_kobert(
    const std::string& model_path) {
#ifdef LQC_HAS_ONNX
    return std::make_unique<KoBertScorer>(model_path);
#else
    (void)model_path;
    return make_lexicon();
#endif
}

std::unique_ptr<SentimentScorer> SentimentScorer::make_default(
    const std::string& model_path) {
#ifdef LQC_HAS_ONNX
    if (!model_path.empty()) return make_kobert(model_path);
#else
    (void)model_path;
#endif
    return make_lexicon();
}

}  // namespace news
