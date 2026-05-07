"""Redis 클라이언트 래퍼. market-feed/stock-feed/news-pipeline pub/sub 채널 규약 정의."""
from __future__ import annotations

import json
import os
from collections.abc import Iterable, Iterator
from dataclasses import dataclass

import redis


@dataclass
class TradeEvent:
    channel: str
    symbol: str
    price: float
    qty: float
    side: int                # 1=buy, 2=sell
    exchange_ts_ns: int
    local_ts_ns: int

    @classmethod
    def from_payload(cls, channel: str, payload: dict) -> "TradeEvent":
        return cls(
            channel=channel,
            symbol=payload.get("symbol", ""),
            price=float(payload.get("price", 0.0)),
            qty=float(payload.get("qty", 0.0)),
            side=int(payload.get("side", 0)),
            exchange_ts_ns=int(payload.get("ex_ts", 0)),
            local_ts_ns=int(payload.get("local_ts", 0)),
        )


def connect(host: str | None = None, port: int | None = None) -> redis.Redis:
    return redis.Redis(
        host=host or os.environ.get("LQC_REDIS_HOST", "127.0.0.1"),
        port=port or int(os.environ.get("LQC_REDIS_PORT", "6379")),
        decode_responses=True,
    )


def stream_trades(
    patterns: Iterable[str] = ("trade.binance.*", "trade.kis.*"),
    client: redis.Redis | None = None,
) -> Iterator[TradeEvent]:
    """PSUBSCRIBE 후 트레이드 이벤트를 무한 yield."""
    r = client or connect()
    pubsub = r.pubsub()
    pubsub.psubscribe(*patterns)
    for msg in pubsub.listen():
        if msg.get("type") != "pmessage":
            continue
        try:
            data = json.loads(msg["data"])
            yield TradeEvent.from_payload(msg["channel"], data)
        except (json.JSONDecodeError, ValueError):
            continue
