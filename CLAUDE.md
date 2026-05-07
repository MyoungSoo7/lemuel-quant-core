# CLAUDE.md — lemuel-quant-core

이 레포에서 Claude / 다른 AI 도구가 알아야 할 컨벤션과 컨텍스트.

## 프로젝트 한 줄 요약

C++ 코어(6개 모듈) + Python 클라이언트(2개)로 lemuel 인프라(르무엘/루이스/르무엘클라우드 3대 서버) 위에 올라가는 6개 사이트(crypto, stock, dart, news, data, codingtest)의 백엔드 데이터 파이프라인 + 채점 엔진.

## 디렉토리

```
modules/
  judge-engine/     codingtest 사이트 — seccomp+cgroup 샌드박스 채점기 (gRPC)
  market-feed/      crypto 사이트   — Binance WSS → Redis pub/sub
  stock-feed/       stock 사이트    — KIS OpenAPI WS → Redis
  dart-crawler/     dart 사이트     — DART OpenAPI 폴러 → PostgreSQL
  news-pipeline/    news 사이트     — RSS + NER + KR-FinBERT
  data-warehouse/   data 사이트     — Parquet + R2 백업
shared/include/     모듈 공통 인터페이스 (lqc::feed::FeedClient 등)
python/
  backtester/       R2 Parquet 읽어 vectorbt 전략 백테스트
  strategy_bot/     Redis 구독 → 시그널 → Telegram 푸시
  common/           R2/Redis/Telegram 클라이언트 래퍼
deploy/
  systemd/          서비스 6+1 종 unit 파일
  install_deps.sh   Ubuntu 22.04+ apt 일괄 설치
  build_and_install.sh  LQC_MODULES 로 호스트별 빌드
```

## 서버 배치 (배포 결정 권한 사항)

| 서버 | 모듈 | 이유 |
|------|------|------|
| 르무엘클라우드 (43.201.110.54) | judge-engine | codingtest 사이트와 동일 호스트, 외부 코드 격리는 클라우드가 안전 |
| 루이스 (192.168.219.106) | market-feed, stock-feed, dart-crawler | crypto/stock/dart 사이트가 루이스에 있음 — localhost Redis push 가능 |
| 르무엘 (192.168.219.101) | news-pipeline, data-warehouse | news/data 사이트가 르무엘. KR-FinBERT 메모리 + 디스크 I/O 위주라 홈서버가 적합 |

## 빌드 전제

- C++20, CMake 3.20+, Ninja 권장
- 외부 의존: Boost.Beast, simdjson, OpenSSL, libcurl, hiredis, libpqxx, libseccomp, Apache Arrow/Parquet, gRPC/Protobuf, ONNX Runtime, AWS SDK for C++ (S3)
- 모든 의존은 호스트 OS 패키지로 설치 (deploy/install_deps.sh)
- macOS dev: brew install boost simdjson hiredis libpq libpqxx apache-arrow onnxruntime grpc cmake ninja

## 호스트별 빌드 명령

```bash
# 전체 빌드 (CI / dev)
cmake -B build -DLQC_BUILD_JUDGE_ENGINE=ON -DLQC_BUILD_MARKET_FEED=ON \
                -DLQC_BUILD_STOCK_FEED=ON -DLQC_BUILD_DART_CRAWLER=ON \
                -DLQC_BUILD_NEWS_PIPELINE=ON -DLQC_BUILD_DATA_WAREHOUSE=ON \
                -DLQC_JUDGE_GRPC=ON
cmake --build build --parallel

# 호스트별 (deploy 스크립트 사용)
LQC_MODULES='judge_engine' bash deploy/build_and_install.sh        # 르무엘클라우드
LQC_MODULES='market_feed stock_feed dart_crawler' bash deploy/build_and_install.sh  # 루이스
LQC_MODULES='news_pipeline data_warehouse' bash deploy/build_and_install.sh         # 르무엘
```

## 환경변수 규칙

모든 환경변수는 `LQC_` 프리픽스(우리 자체 토글) 또는 외부 SDK 표준 이름(`R2_*`, `KIS_*`, `DART_API_KEY` 등) 사용. systemd 의 `EnvironmentFile=/etc/lqc/<service>.env` 로 주입.

| 변수 | 모듈 | 의미 |
|------|------|------|
| `LQC_DISABLE_SECCOMP` | judge-engine | 1 이면 seccomp 미설치 (디버그용) |
| `LQC_FINBERT_DIR` | news-pipeline | KR-FinBERT ONNX 모델 디렉토리 |
| `LQC_NEWS_TICKERS` | news-pipeline | NER 사전 CSV 경로 |
| `LQC_PG_DSN` | dart-crawler | PostgreSQL DSN. 없으면 in-memory |
| `LQC_REDIS_HOST/PORT` | data-warehouse, python/* | Redis 접속 |
| `LQC_DWH_INTERVAL_SEC` | data-warehouse | rollup 주기 (기본 300) |
| `KIS_APP_KEY/SECRET` | stock-feed | 한투 OpenAPI |
| `KIS_PAPER` | stock-feed | 모의투자 모드 |
| `DART_API_KEY` | dart-crawler | DART OpenAPI |
| `R2_*` | data-warehouse, python | Cloudflare R2 인증 |
| `TELEGRAM_BOT_TOKEN/CHAT_ID` | strategy-bot | 알림 |

## 코드 컨벤션

- C++ 헤더는 `#pragma once`, 네임스페이스는 모듈명 그대로 (`market::`, `stock::`, `dart::`, `news::`, `dwh::`, `judge::`)
- 공통 인터페이스는 `lqc::feed`, `lqc::` 네임스페이스 — `shared/include/lqc/`
- gRPC proto 패키지는 `lqc.<module>.v1`
- Korean comments 환영. 외부 API 응답 필드명은 원어(`approval_key`, `rcept_no`) 그대로 보존
- 외부 의존성은 `find_package` / `pkg_check_modules` 후 `LQC_HAS_<X>` define 으로 코드에서 분기 — 누락된 환경에서도 폴백으로 빌드되어야 함 (예: simdjson 없으면 자체 추출자, ONNX 없으면 lexicon)
- 빌드 옵션은 `LQC_BUILD_<MODULE>=ON/OFF`. 디폴트는 `ON` 이지만 `deploy/build_and_install.sh` 가 명시적으로 OFF 도 세팅

## 자주 빠지는 함정

- **macOS clang vs Linux gcc**: `<algorithm>` 등 transitive 통과되는 헤더가 GCC 에서 명시 include 필요. 새 std::max/min 등 사용하면 헤더 한 번 더 체크.
- **Boost 1.90 (Homebrew)**: `find_package(Boost COMPONENTS system)` 안 됨 — Beast/Asio 는 header-only 라 `find_package(Boost CONFIG)` + `Boost::headers` 사용.
- **seccomp 화이트리스트**: glibc 2.35+ (Ubuntu 22.04+) 는 startup 에서 `clone3`, `rseq`, `prctl`, `execve` 사용. judge-engine 의 `sandbox_linux.cpp` 화이트리스트가 부족하면 hello world 도 SIGSYS 로 죽음. 새 syscall 추가시 빌드 후 르무엘클라우드에서 smoke 검증.
- **AWS SDK for C++ 2.0**: `S3Client(creds, ccfg)` 생성자 deprecated. `SimpleAWSCredentialsProvider` + `(provider, endpointProvider, ccfg)` 형태로.
- **Apache Arrow apt**: Ubuntu 24.04 기본 저장소에 `libarrow-dev` 없음. `apache-arrow-apt-source` 추가 또는 폴백(CSV writer) 사용.

## 배포 운영

서비스는 systemd 로 관리. unit 파일은 `deploy/systemd/`. 환경변수는 `/etc/lqc/<service>.env`.

```bash
sudo systemctl status <service>          # 상태
sudo journalctl -u <service> -f          # 로그 추적
sudo systemctl restart <service>         # 재시작
```

배포 시퀀스: `git pull && cmake --build build && sudo install -m 755 -o lqc -g lqc <bin> /opt/lqc/bin/ && sudo systemctl restart <service>`.

## 문제 발생시 디버깅 순서

1. `journalctl -u <service> -n 50` 로그 확인
2. `LQC_DISABLE_SECCOMP=1` 또는 `LQC_PG_DSN=` 비우기 등으로 외부 의존성 격리
3. 바이너리 직접 실행 (`sudo -u lqc /opt/lqc/bin/<name>`) 으로 systemd 영향 배제
4. `strace -c <bin>` 으로 syscall 패턴 확인 (seccomp 의심시)
5. Redis 채널 구독 (`docker exec product-redis redis-cli psubscribe 'trade.*'`) 으로 데이터 흐름 확인

## 외부 의존 사이트 (이 레포 외부)

- `crypto.lemuel.co.kr` (루이스) — Redis 의 `trade.binance.*` 채널 구독
- `stock.lemuel.co.kr` (루이스) — `trade.kis.*` 구독
- `dart.lemuel.co.kr` (루이스) — PostgreSQL `dart_disclosure` 테이블 조회
- `news.lemuel.co.kr` (르무엘) — news-pipeline 출력 score
- `data.lemuel.co.kr` (르무엘) — R2 Parquet 시각화
- `codingtest.lemuel.co.kr` (르무엘클라우드) — judge-engine gRPC :50051 호출

## 메모리 / 자원 가이드

- judge-engine 1 채점 = 256MB AS, 1 CPU sec, 64KB output (기본)
- market-feed/stock-feed = 5MB 이하
- news-pipeline = lexicon 모드 5MB / FinBERT 모드 1.5GB (모델 로딩)
- data-warehouse = 5분 주기 rollup, in-memory store 6시간 retention
