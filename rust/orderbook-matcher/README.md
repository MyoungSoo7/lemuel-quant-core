# lqc-orderbook-matcher (Rust)

C++ market-feed 가 trade.* 채널만 push 하는 데 비해, Rust 모듈은 Binance `@depth20@100ms` 풀 호가창을 파싱해서 top-of-book + spread 을 `book.binance.<symbol>.top` 채널로 publish.

## 빌드

```bash
cargo build --release
./target/release/lqc-orderbook-matcher --symbols btcusdt,ethusdt --depth 20
```

## 환경

- 의존: tokio, tokio-tungstenite, redis-rs, parking_lot, sonic-rs, clap
- Cross-compile: `cargo build --release --target x86_64-unknown-linux-musl` (르무엘/루이스 정적 배포)

## 운영

deploy/systemd/orderbook-matcher.service 로 systemd unit 추가 가능. 같은 redis 를 market-feed 와 공유.

## 왜 Rust?

- BTreeMap 기반 호가창은 `unsafe` 없이 무경합 update
- tokio multiplexed redis 연결 → publish 지연 < 100µs
- C++ Boost.Beast 보다 짧은 코드, 안전한 동시성
