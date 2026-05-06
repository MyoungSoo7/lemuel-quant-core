# market-feed

암호화폐 거래소 실시간 시세 수집 엔진. 바이낸스/업비트/빗썸 WebSocket 동시 구독 → 호가창 정렬 → Redis pub/sub로 crypto 사이트에 push.

## 상태
스텁. 2~3주차 시작 예정. stock-feed와 공통 추상화 (`shared/feed_core`) 먼저 설계.

## 의존성 (예정)
- Boost.Asio 또는 uWebSockets
- simdjson
- hiredis
