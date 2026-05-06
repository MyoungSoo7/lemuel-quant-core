# data-warehouse

통합 시계열 저장소. market-feed/stock-feed/dart-crawler/news-pipeline에서 들어오는 raw 데이터를 Parquet으로 압축 저장하고 일별로 Cloudflare R2에 백업.

## 상태
스텁. 6주차 시작. data 사이트 차트 시각화 백엔드.

## 의존성 (예정)
- Apache Arrow / Parquet C++
- AWS SDK for C++ (R2는 S3 호환)
- gRPC 시계열 쿼리 API
