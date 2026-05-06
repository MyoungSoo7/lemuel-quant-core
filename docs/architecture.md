# 아키텍처 노트

## 서버 배치 결정 근거

| 모듈 | 배치 서버 | 이유 |
|------|-----------|------|
| `market-feed` | 루이스 | crypto 사이트가 루이스에 있음 → localhost Redis로 push |
| `stock-feed` | 루이스 | stock 사이트가 루이스에 있음 |
| `dart-crawler` | 루이스 | dart 사이트가 루이스에 있음 |
| `news-pipeline` | 르무엘 | news 사이트가 르무엘. KoBERT 모델 메모리 사용량 큼 → 홈서버가 더 여유 |
| `data-warehouse` | 르무엘 | data 사이트가 르무엘. 디스크 I/O 위주 → 홈서버 SSD가 유리 |
| `judge-engine` | 르무엘클라우드 | codingtest 사이트와 같은 서버. 외부 코드 실행 격리는 클라우드가 더 안전 |

## 통신 토폴로지

```
[루이스]                  [르무엘]                [르무엘클라우드]
market-feed ─┐           news-pipeline ─┐        judge-engine
stock-feed   ├─ Redis ◄─ data-warehouse │           │
dart-crawler ┘    ▲          ▲          │           ▼
                  │          │          │     codingtest
              gRPC│      gRPC│          │      (REST)
                  │          │          │
              [Cloudflare Tunnel 내부망]
```

- 모듈 간 RPC: gRPC (HTTP/2, 내부망 only)
- 사이트 push: Redis pub/sub (각 호스트의 로컬 Redis → 사이트가 구독)
- 영구 저장: PostgreSQL (호스트별) + Parquet on Cloudflare R2 (백업)

## 보안 경계

- judge-engine: seccomp-bpf 화이트리스트 + cgroup v2 + non-root user + readonly rootfs
- API 키: 모든 모듈은 환경변수로 키 주입, repo에는 절대 커밋하지 않음
- 외부망 노출: judge-engine과 data-warehouse만 gRPC 게이트웨이 통해 노출, 나머지는 내부망 전용
