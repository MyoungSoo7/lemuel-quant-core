#include "news/finbert.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef LQC_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace news {

std::vector<FinBertModel::Probs> FinBertModel::predict_batch(
    const std::vector<std::string>& texts) {
    std::vector<Probs> out;
    out.reserve(texts.size());
    for (const auto& t : texts) out.push_back(predict(t));
    return out;
}

namespace {

// Soft-max over 3 logits.
[[maybe_unused]] FinBertModel::Probs softmax3(double l0, double l1, double l2) {
    const double m = std::max({l0, l1, l2});
    const double e0 = std::exp(l0 - m);
    const double e1 = std::exp(l1 - m);
    const double e2 = std::exp(l2 - m);
    const double s  = e0 + e1 + e2;
    return {e0 / s, e1 / s, e2 / s};
}

#ifdef LQC_HAS_ONNX

// Minimal SentencePiece-style tokenizer wrapper. Real impl pulls in
// google/sentencepiece via CMake; here we expose the interface so the
// rest of the inference path can be exercised once vocab is loaded.
class Tokenizer {
public:
    explicit Tokenizer(const std::string& vocab_path) {
        std::ifstream f(vocab_path);
        if (!f) throw std::runtime_error("vocab not found: " + vocab_path);
        // Production: load 32k SentencePiece pieces and build trie.
        // Skeleton: read line-per-token vocab, assign sequential ids.
        std::string tok;
        int id = 0;
        while (std::getline(f, tok)) ids_[tok] = id++;
        if (auto it = ids_.find("[CLS]"); it != ids_.end()) cls_ = it->second;
        if (auto it = ids_.find("[SEP]"); it != ids_.end()) sep_ = it->second;
        if (auto it = ids_.find("[PAD]"); it != ids_.end()) pad_ = it->second;
        if (auto it = ids_.find("[UNK]"); it != ids_.end()) unk_ = it->second;
    }

    // Greedy whitespace + longest-match piece tokenization. Real model uses
    // SentencePiece BPE; for the scaffold this preserves the integration shape.
    std::vector<std::int64_t> encode(const std::string& text, int max_len) const {
        std::vector<std::int64_t> ids;
        ids.push_back(cls_);
        std::size_t i = 0;
        while (i < text.size() && static_cast<int>(ids.size()) < max_len - 1) {
            // Skip whitespace.
            while (i < text.size() && (text[i] == ' ' || text[i] == '\n' ||
                                        text[i] == '\t')) ++i;
            if (i >= text.size()) break;
            // Longest matching piece up to 16 bytes.
            std::size_t end = std::min(text.size(), i + 16);
            bool matched = false;
            for (; end > i; --end) {
                const auto piece = text.substr(i, end - i);
                if (auto it = ids_.find(piece); it != ids_.end()) {
                    ids.push_back(it->second);
                    i = end;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                ids.push_back(unk_);
                ++i;
            }
        }
        ids.push_back(sep_);
        while (static_cast<int>(ids.size()) < max_len) ids.push_back(pad_);
        return ids;
    }

    std::int64_t pad_id() const { return pad_; }

private:
    std::unordered_map<std::string, int> ids_;
    std::int64_t cls_{0}, sep_{0}, pad_{0}, unk_{0};
};

class FinBertOnnx : public FinBertModel {
public:
    FinBertOnnx(const std::string& model_dir, int max_seq_len)
        : env_(ORT_LOGGING_LEVEL_WARNING, "lqc-finbert"),
          opts_(),
          session_(env_, (model_dir + "/kr-finbert.onnx").c_str(), opts_),
          tokenizer_(model_dir + "/kr-finbert.vocab"),
          max_seq_len_(max_seq_len) {}

    Probs predict(const std::string& text) override {
        // 1) tokenize → input_ids, attention_mask
        const auto ids = tokenizer_.encode(text, max_seq_len_);
        std::vector<std::int64_t> mask(ids.size(), 1);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == tokenizer_.pad_id()) mask[i] = 0;
        }

        // 2) build ORT tensors with shape [1, seq]
        Ort::AllocatorWithDefaultOptions alloc;
        const std::array<std::int64_t, 2> shape{1, max_seq_len_};
        auto mem = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value input_ids = Ort::Value::CreateTensor<std::int64_t>(
            mem, const_cast<std::int64_t*>(ids.data()), ids.size(),
            shape.data(), shape.size());
        Ort::Value attn = Ort::Value::CreateTensor<std::int64_t>(
            mem, mask.data(), mask.size(),
            shape.data(), shape.size());

        const char* in_names[]  = {"input_ids", "attention_mask"};
        const char* out_names[] = {"logits"};
        std::array<Ort::Value, 2> inputs{std::move(input_ids), std::move(attn)};

        auto outs = session_.Run(
            Ort::RunOptions{nullptr}, in_names, inputs.data(), inputs.size(),
            out_names, 1);
        const float* logits = outs[0].GetTensorData<float>();

        return softmax3(logits[0], logits[1], logits[2]);
    }

private:
    Ort::Env             env_;
    Ort::SessionOptions  opts_;
    Ort::Session         session_;
    Tokenizer            tokenizer_;
    int                  max_seq_len_;
};

#endif  // LQC_HAS_ONNX

}  // namespace

std::unique_ptr<FinBertModel> FinBertModel::load(const std::string& model_dir,
                                                 int max_seq_len) {
#ifdef LQC_HAS_ONNX
    try {
        return std::unique_ptr<FinBertModel>(
            new FinBertOnnx(model_dir, max_seq_len));
    } catch (const std::exception& e) {
        std::cerr << "[finbert] load failed: " << e.what() << "\n";
        return nullptr;
    }
#else
    (void)model_dir;
    (void)max_seq_len;
    std::cerr << "[finbert] ONNX Runtime not linked; lexicon fallback in use\n";
    return nullptr;
#endif
}

}  // namespace news
