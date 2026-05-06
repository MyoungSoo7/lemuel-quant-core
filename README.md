# lemuel-quant-core

고성능 C++ 시장 데이터 파이프라인 + 채점 엔진. lemuel 인프라(르무엘/루이스/르무엘클라우드 3대 서버)에 배포되는 6개 사이트의 백엔드 코어를 통합한 모노레포.

## 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│                      lemuel-quant-core                       │
└─────────────────────────────────────────────────────────────┘
       │                    │                    │
   ┌───┴───┐           ┌────┴─────┐         ┌────┴─────┐
   │ 루이스 │           │ 르무엘   │         │르무엘클라우드│
   └───┬───┘           └────┬─────┘         └────┬─────┘
       │                    │                    │
  market-feed          news-pipeline        judge-engine
  stock-feed           data-warehouse
  dart-crawler
       │                    │                    │
   crypto/stock         news/data           codingtest
   /dart 사이트         사이트              사이트
```

## 모듈

| 모듈 | 역할 | 연동 사이트 | 배포 서버 |
|------|------|-------------|-----------|
| `judge-engine` | 샌드박스 코드 채점 (seccomp + cgroup) | codingtest | 르무엘클라우드 |
| `market-feed` | 암호화폐 거래소 WebSocket 수집 | crypto | 루이스 |
| `stock-feed` | 한국 주식 시세/호가 수집 (한투/키움) | stock | 루이스 |
| `dart-crawler` | DART 공시 수집/파싱 | dart | 루이스 |
| `news-pipeline` | 뉴스 크롤링 + NER + 감성분석 | news | 르무엘 |
| `data-warehouse` | 통합 시계열 저장 (Parquet + R2 백업) | data | 르무엘 |

## 통신

- 모듈 간: gRPC (서버 간 통신은 Cloudflare Tunnel 내부망)
- 사이트 push: Redis pub/sub
- 영구 저장: PostgreSQL + Apache Arrow Parquet on Cloudflare R2

## 빌드

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 로드맵

- [x] 모노레포 부트스트랩
- [ ] judge-engine MVP (1주차)
- [ ] market-feed + stock-feed 공통 추상화 (2~3주차)
- [ ] news-pipeline KoBERT 추론 (4주차)
- [ ] dart-crawler (5주차)
- [ ] data-warehouse 통합 (6주차)
