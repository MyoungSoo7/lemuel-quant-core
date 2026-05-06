#pragma once

#include <memory>
#include <string>

#include "news/types.hpp"

namespace news {

// Sentiment scorer interface. Two implementations:
//   1. Lexicon-based fallback (always available, KOSAC seed)
//   2. KR-FinBERT ONNX inference (when ONNX Runtime is linked + model dir set)
//      - 모델: snunlp/KR-FinBert-SC (한국 금융 뉴스 감성 분류)
class SentimentScorer {
public:
    virtual ~SentimentScorer() = default;

    struct Result {
        Sentiment label;
        double    confidence;        // [0, 1]
    };

    virtual Result score(const std::string& text) const = 0;

    // Defaults to FinBERT backend if available, else lexicon.
    static std::unique_ptr<SentimentScorer> make_default(
        const std::string& model_dir = "");
    static std::unique_ptr<SentimentScorer> make_lexicon();
    // model_dir 안에 kr-finbert.onnx + kr-finbert.vocab 가 있어야 함.
    static std::unique_ptr<SentimentScorer> make_finbert(
        const std::string& model_dir);
};

}  // namespace news
