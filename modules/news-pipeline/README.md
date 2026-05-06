# news-pipeline

뉴스 크롤링 + 한국어 NLP 파이프라인. 종목명 NER + 감성 분석으로 종목별 뉴스 점수 산출.

## 상태
스텁. 4주차 시작.

## 파이프라인
1. RSS/HTML 크롤링 (libcurl + gumbo-parser)
2. 종목명 NER (티커 사전, longest-match 룰)
3. 감성 분석 — 2티어:
   - 기본: 사전 기반 LexiconScorer (KOSAC 시드)
   - 운영: KR-FinBERT (snunlp/KR-FinBert-SC) ONNX 추론
4. NewsScoreBoard — 종목별 반감기 기반 누적 점수
5. Redis push → news 사이트 + stock/dart 사이트 시그널 연동

## 모델 준비

```bash
pip install transformers torch onnx
python scripts/convert_finbert.py --out /opt/lqc/models/finbert
# 산출물:
#   /opt/lqc/models/finbert/kr-finbert.onnx   (~440MB)
#   /opt/lqc/models/finbert/kr-finbert.vocab

export LQC_FINBERT_DIR=/opt/lqc/models/finbert
./build/modules/news-pipeline/news_pipeline
```

`LQC_FINBERT_DIR` 미지정 또는 ONNX Runtime 미링크 시 LexiconScorer로 자동 폴백.
