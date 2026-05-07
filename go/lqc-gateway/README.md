# lqc-gateway (Go)

작은 인프라 사이드카. 한 바이너리에 3가지 일:

1. **Prometheus exporter** — `/metrics` — 채널별 pubsub 메시지 카운트 + last-seen age (Grafana 가 루이스에서 스크랩)
2. **SSE bridge** — `/stream/<channel>` — 브라우저가 EventSource 로 직접 trade/book 채널 구독 (Redis 클라이언트 없이도 사이트에서 실시간 차트)
3. **Healthz** — `/healthz` — Redis ping + 핵심 채널 fresh 여부

## 빌드

```bash
cd go/lqc-gateway
go build -ldflags='-s -w' -o /opt/lqc/bin/lqc-gateway
```

## 실행

```bash
/opt/lqc/bin/lqc-gateway \
    --listen 127.0.0.1:9099 \
    --redis 127.0.0.1:6379 \
    --patterns 'trade.*,book.*,signal.*,model.*' \
    --channel-hint trade.binance.btcusdt
```

- `127.0.0.1` 바인딩 → 외부 노출 없음. nginx/Cloudflare Tunnel 뒤에 둠.
- Prometheus job:

```yaml
- job_name: lqc-gateway
  static_configs: [{ targets: ['127.0.0.1:9099'] }]
```

## 사이트 통합 예시

```html
<script>
const es = new EventSource("https://crypto.lemuel.co.kr/stream/trade.binance.btcusdt");
es.onmessage = (e) => {
  const t = JSON.parse(e.data);
  // update chart with t.price
};
</script>
```
