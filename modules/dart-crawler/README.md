# dart-crawler

DART(전자공시) 실시간 공시 수집/파싱. 비동기 폴링 → XML 파싱 → 종목별 이벤트 추출 → PostgreSQL 저장.

## 상태
스텁. 5주차 시작. market-feed/stock-feed의 시세 데이터와 join하여 "공시 직후 가격 변동" 분석.

## 의존성 (예정)
- libcurl (multi handle)
- pugixml
- libpqxx (PostgreSQL)
