#!/usr/bin/env python3
"""
dex-feed — DEX 거래량 + 가격 → Redis publish (트레이딩 시그널 소스).

Uniswap V3 메이저 풀의 24h volume + 가격 fetch → Redis trade.uniswap.<symbol> publish.
strategy_bot 가 trade.binance.* / trade.kis.* 와 동일 인터페이스로 처리.

데이터 소스: DefiLlama API (무료, 키 불필요)
  - /protocol/uniswap-v3 (전체 protocol TVL)
  - /pools (top pools by TVL/volume)

환경변수:
  LQC_REDIS_HOST=127.0.0.1
  LQC_REDIS_PORT=6379
  DEX_CYCLE_SEC      기본 60
  DEX_PAIRS          콤마 구분 (예: WETH-USDC,WBTC-USDC,UNI-WETH)
"""
from __future__ import annotations

import os
import sys
import time
import json
import signal
import logging
from datetime import datetime, timedelta, timezone
from urllib.request import Request, urlopen

import redis

KST = timezone(timedelta(hours=9))
HOST = os.getenv("LQC_REDIS_HOST", "127.0.0.1")
PORT = int(os.getenv("LQC_REDIS_PORT", "6379"))
CYCLE = int(os.getenv("DEX_CYCLE_SEC", "60"))
PAIRS = (os.getenv("DEX_PAIRS") or "WETH-USDC,WBTC-USDC,UNI-WETH").split(",")

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")
log = logging.getLogger("dex-feed")

g_run = True
def on_sig(*_):
    global g_run; g_run = False
signal.signal(signal.SIGTERM, on_sig)
signal.signal(signal.SIGINT, on_sig)


# DefiLlama Yields API — pools by chain + protocol
DEFILLAMA_POOLS = "https://yields.llama.fi/pools"
DEFILLAMA_PRICES = "https://coins.llama.fi/prices/current/"


def fetch_top_pools() -> list[dict]:
    """Uniswap V3 ethereum 메이저 풀."""
    req = Request(DEFILLAMA_POOLS, headers={"User-Agent": "lemuel-quant-core/dex-feed"})
    try:
        with urlopen(req, timeout=15) as r:
            data = json.loads(r.read())
    except Exception as e:
        log.warning(f"defillama-pools fetch error: {e}")
        return []
    pools = data.get("data", [])
    # Uniswap V3 ethereum 메이저 풀만
    filtered = [p for p in pools
                if p.get("project") == "uniswap-v3"
                and p.get("chain") == "Ethereum"
                and p.get("symbol") in {"WETH-USDC", "WBTC-USDC", "UNI-WETH",
                                        "DAI-USDC", "WETH-USDT", "USDC-USDT"}]
    filtered.sort(key=lambda p: p.get("tvlUsd", 0), reverse=True)
    return filtered[:6]


def fetch_token_prices(symbols: list[str]) -> dict[str, float]:
    """DefiLlama price feed — coingecko 식별자 사용."""
    coingecko_ids = {
        "WETH": "ethereum",
        "USDC": "usd-coin",
        "USDT": "tether",
        "WBTC": "wrapped-bitcoin",
        "UNI":  "uniswap",
        "DAI":  "dai",
    }
    ids = ",".join(f"coingecko:{coingecko_ids[s]}" for s in symbols if s in coingecko_ids)
    if not ids:
        return {}
    req = Request(DEFILLAMA_PRICES + ids, headers={"User-Agent": "lemuel-quant-core/dex-feed"})
    try:
        with urlopen(req, timeout=15) as r:
            data = json.loads(r.read())
    except Exception as e:
        log.warning(f"defillama-prices fetch error: {e}")
        return {}
    out = {}
    for s, gid in coingecko_ids.items():
        info = data.get("coins", {}).get(f"coingecko:{gid}", {})
        if "price" in info:
            out[s] = float(info["price"])
    return out


def main():
    log.info(f"dex-feed start — redis={HOST}:{PORT} cycle={CYCLE}s")
    r = redis.Redis(host=HOST, port=PORT, decode_responses=True)
    r.ping()
    log.info("redis connected")

    while g_run:
        try:
            pools = fetch_top_pools()
            symbols_seen = set()
            for symbol_pair in PAIRS:
                symbols_seen.update(symbol_pair.split("-"))

            prices = fetch_token_prices(list(symbols_seen))

            published = 0
            for pool in pools:
                sym = pool.get("symbol", "").lower().replace("-", "")
                channel = f"trade.uniswap.{sym}"
                # strategy_bot 의 trade.* schema 와 호환되도록
                # market-feed 는 보통 {ts, symbol, price, volume, side} 같은 schema 발행
                # DEX 는 pool TVL + 24h volume + spot price
                tvl = pool.get("tvlUsd", 0)
                volume = pool.get("volumeUsd24h") or pool.get("volumeUsd1d", 0)
                # 가격은 첫 번째 토큰 기준
                first_token = pool.get("symbol", "").split("-")[0]
                price = prices.get(first_token, 0)

                msg = {
                    "ts": int(time.time() * 1000),
                    "symbol": sym,
                    "pair": pool.get("symbol"),
                    "price": price,
                    "tvl_usd": tvl,
                    "volume_24h_usd": volume,
                    "source": "uniswap-v3",
                    "chain": "ethereum",
                }
                r.publish(channel, json.dumps(msg))
                published += 1

            log.info(f"published {published} pools to trade.uniswap.*")
        except Exception as e:
            log.exception(f"cycle error: {e}")

        for _ in range(CYCLE):
            if not g_run:
                break
            time.sleep(1)

    log.info("dex-feed shutdown")


if __name__ == "__main__":
    main()
