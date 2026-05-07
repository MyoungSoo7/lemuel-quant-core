# lemuel-quant-core / r

R 기반 정량 분석 / 통계 모델링 / 리포트 / 대시보드. C++ 코어가 흘려보낸 데이터(R2 Parquet, Redis pub/sub, PostgreSQL DART) 위에 얹는다.

## 디렉토리

| 경로 | 역할 |
|------|------|
| `common/` | R2/Redis/PG 클라이언트 헬퍼 |
| `quant_research/` | PerformanceAnalytics + quantmod 기반 백테스터 |
| `stat_models/` | GARCH, ARIMA, 공적분, Kalman filter — Redis publish |
| `daily_report/` | Quarto 일간 시장 리포트 자동 생성 |
| `shiny_dashboard/` | Shiny 실시간 대시보드 |
| `research_notebook/` | codingtest 챔피언십용 R Markdown 템플릿 |
| `risk_engine/` | 포트폴리오 VaR / CVaR / Monte Carlo |

## 설치

```bash
# Ubuntu
sudo apt install -y r-base r-base-dev libcurl4-openssl-dev libssl-dev \
                    libxml2-dev libfontconfig1-dev libfreetype6-dev \
                    libharfbuzz-dev libfribidi-dev libpng-dev libtiff5-dev \
                    libjpeg-dev pandoc

# macOS
brew install r quarto

# 패키지 일괄 설치 (~10분)
Rscript install.R
```

## 환경변수

```
LQC_REDIS_HOST=127.0.0.1
LQC_REDIS_PORT=6379
R2_ENDPOINT=https://<account>.r2.cloudflarestorage.com
R2_BUCKET=lemuel-backup
R2_ACCESS_KEY=...
R2_SECRET_KEY=...
LQC_PG_DSN=postgres://settlement:settlement1234@127.0.0.1:5433/lqc
TELEGRAM_BOT_TOKEN=...
TELEGRAM_CHAT_ID=...
```

## 운영 위치

- **르무엘** 서버에 R 설치 권장 (32GB RAM, 통계 모델 메모리 여유)
- daily_report 는 cron 으로 매일 09:30 KST 실행
- shiny_dashboard 는 systemd 로 :3838 포트 (사이트와 별도, 내부망 only)
