# stock-feed

한국 주식 실시간 시세/호가 수집 엔진. 한국투자증권 OpenAPI / 키움 OpenAPI+ WebSocket 연동 → Redis pub/sub로 stock 사이트에 push.

## 상태
스텁. 2~3주차 시작 예정. 장 시간대 스케줄러 + 휴장일 처리 포함.

## 의존성 (예정)
- 한투 OpenAPI REST + WebSocket
- KIS Developers SDK (참고용)
- simdjson, hiredis
