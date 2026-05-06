#pragma once

#include <memory>
#include <string>

#include "news/types.hpp"

namespace news {

// Sentiment scorer interface. Two implementations:
//   1. Lexicon-based fallback (always available)
//   2. KoBERT ONNX inference (when ONNX Runtime is linked)
class SentimentScorer {
public:
    virtual ~SentimentScorer() = default;

    struct Result {
        Sentiment label;
        double    confidence;        // [0, 1]
    };

    virtual Result score(const std::string& text) const = 0;

    // Defaults to ONNX backend if available, else lexicon.
    static std::unique_ptr<SentimentScorer> make_default(
        const std::string& model_path = "");
    static std::unique_ptr<SentimentScorer> make_lexicon();
    static std::unique_ptr<SentimentScorer> make_kobert(
        const std::string& model_path);
};

}  // namespace news
