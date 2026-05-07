# lemuel-quant-core / python

C++ 코어 위에 얹는 Python 프로젝트들. C++ 모듈들이 만들어내는 데이터(Redis 트레이드 스트림, R2 Parquet 스냅샷, PostgreSQL 공시 테이블)를 소비한다.

## 구성

| 디렉토리 | 역할 | 의존 데이터 소스 |
|----------|------|------------------|
| `backtester/` | 전략 백테스트 + 결과 시각화 | R2 Parquet (data-warehouse 산출물) |
| `strategy_bot/` | 실시간 시그널 생성 + Telegram 알림 | Redis pub/sub (market-feed/stock-feed) |
| `common/` | 공통 유틸 (Redis/R2 클라이언트 래퍼) | — |

## 공통 환경변수

```
LQC_REDIS_HOST=127.0.0.1
LQC_REDIS_PORT=6379
R2_ENDPOINT=https://<account>.r2.cloudflarestorage.com
R2_BUCKET=lemuel-quant
R2_ACCESS_KEY=...
R2_SECRET_KEY=...
TELEGRAM_BOT_TOKEN=...
TELEGRAM_CHAT_ID=...
```

## 설치

```bash
cd python
pip install -r requirements.txt
```
