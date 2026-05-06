#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace news {

// KR-FinBERT (snunlp/KR-FinBert-SC) ONNX inference wrapper.
//
// 모델 산출물 가정:
//   - kr-finbert.onnx           : 모델 그래프 (HF → ONNX 변환, scripts/convert_finbert.py)
//   - kr-finbert.vocab          : SentencePiece 32k 어휘 (또는 vocab.txt)
//   - 입력: input_ids[seq], attention_mask[seq]   (seq=128 권장)
//   - 출력: logits[3]                              (negative, neutral, positive)
class FinBertModel {
public:
    struct Probs {
        double negative{0.0};
        double neutral{0.0};
        double positive{0.0};

        // argmax → -1, 0, +1
        int argmax_label() const {
            if (positive >= negative && positive >= neutral) return  1;
            if (negative >= neutral)                         return -1;
            return 0;
        }
        // confidence = max prob
        double confidence() const {
            return std::max({negative, neutral, positive});
        }
    };

    // model_dir 안에 kr-finbert.onnx + vocab 파일이 있어야 함.
    // ONNX Runtime 미링크 환경에서는 throw 대신 nullptr 반환 (factory에서 처리).
    static std::unique_ptr<FinBertModel> load(const std::string& model_dir,
                                              int max_seq_len = 128);

    virtual ~FinBertModel() = default;

    // text → 확률 벡터. 토큰 길이가 max_seq_len 보다 길면 앞뒤를 잘라 헤드+테일
    // 결합 (금융뉴스에서 lead/lede 가 가장 정보량이 많음).
    virtual Probs predict(const std::string& text) = 0;

    // 배치 추론 (RPS 향상). 디폴트 구현은 단일 호출 반복.
    virtual std::vector<Probs> predict_batch(
        const std::vector<std::string>& texts);
};

}  // namespace news
