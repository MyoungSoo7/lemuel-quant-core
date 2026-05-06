# news-pipeline

뉴스 크롤링 + 한국어 NLP 파이프라인. 종목명 NER + 감성 분석으로 종목별 뉴스 점수 산출.

## 상태
스텁. 4주차 시작.

## 파이프라인
1. RSS/HTML 크롤링 (libcurl + gumbo-parser)
2. 형태소 분석 (mecab-ko)
3. 종목명 NER (티커 사전 + 룰 기반)
4. 감성 분석 (KoBERT ONNX 추론, ONNX Runtime C++)
5. Redis push → news 사이트 + stock/dart 사이트 시그널 연동
