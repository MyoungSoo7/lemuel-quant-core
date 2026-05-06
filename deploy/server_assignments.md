# 서버별 배포 계획

| 서버 | 모듈 | LQC_MODULES | 추가 의존성 |
|------|------|-------------|-------------|
| 르무엘클라우드 (43.201.110.54) | judge-engine | `judge_engine` | libseccomp, gRPC, protobuf |
| 루이스 | market-feed, stock-feed, dart-crawler | `market_feed stock_feed dart_crawler` | hiredis, libpq, libpqxx, libcurl |
| 르무엘 | news-pipeline, data-warehouse | `news_pipeline data_warehouse` | libcurl, ONNX Runtime, Arrow/Parquet, AWS SDK |

## 배포 절차 (서버별 공통)

```bash
# 1. 코드 가져오기
sudo mkdir -p /opt/lqc && sudo chown $USER:$USER /opt/lqc
cd /opt/lqc
git clone https://github.com/MyoungSoo7/lemuel-quant-core.git

# 2. 의존성 설치 (호스트에 따라 ONNX/AWS 옵션 토글)
sudo LQC_INSTALL_ONNX=1 LQC_INSTALL_AWS_S3=1 \
    bash lemuel-quant-core/deploy/install_deps.sh

# 3. 빌드 (호스트별 LQC_MODULES 다름)
LQC_MODULES="judge_engine" \
    bash lemuel-quant-core/deploy/build_and_install.sh

# 4. 환경변수 파일 작성
sudo install -m 600 -o lqc -g lqc /dev/null /etc/lqc/judge-engine.env
sudo $EDITOR /etc/lqc/judge-engine.env   # 키들 채우기

# 5. systemd unit 등록
sudo cp lemuel-quant-core/deploy/systemd/judge-engine.service \
        /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now judge-engine.service
sudo systemctl status judge-engine
```

## 르무엘 (news-pipeline) 추가 단계

```bash
# KR-FinBERT 모델 변환 (한 번만)
pip install transformers torch onnx
python lemuel-quant-core/scripts/convert_finbert.py \
    --out /opt/lqc/models/finbert
sudo chown -R lqc:lqc /opt/lqc/models
```

## 루이스 (dart-crawler) PostgreSQL 준비

```bash
# 루이스에 이미 PG 있으면 DB만 만들기
sudo -u postgres psql <<SQL
CREATE DATABASE lqc;
CREATE USER lqc WITH PASSWORD '<random>';
GRANT ALL PRIVILEGES ON DATABASE lqc TO lqc;
SQL

# disclosure 테이블은 dart-crawler 첫 기동 시 마이그레이션 (TODO)
```

## 환경변수 템플릿

### /etc/lqc/judge-engine.env (르무엘클라우드)
```
LQC_JUDGE_PORT=50051
```

### /etc/lqc/market-feed.env (루이스)
```
# 키 불필요 (Binance public WS)
```

### /etc/lqc/stock-feed.env (루이스)
```
KIS_APP_KEY=...
KIS_APP_SECRET=...
# KIS_PAPER=1     # 모의투자 모드
```

### /etc/lqc/dart-crawler.env (루이스)
```
DART_API_KEY=...
LQC_PG_DSN=postgres://lqc:<pw>@127.0.0.1:5432/lqc
```

### /etc/lqc/news-pipeline.env (르무엘)
```
LQC_FINBERT_DIR=/opt/lqc/models/finbert
LQC_NEWS_TICKERS=/opt/lqc/data/tickers.csv
```

### /etc/lqc/data-warehouse.env (르무엘)
```
R2_ENDPOINT=https://<account>.r2.cloudflarestorage.com
R2_BUCKET=lemuel-quant
R2_ACCESS_KEY=...
R2_SECRET_KEY=...
```
