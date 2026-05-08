#!/usr/bin/env python3
"""
crypto-price-tracker — 주요 토큰 가격 모니터링 + 텔레그램 알림.

데이비드에서 systemd 로 5분 주기 실행 (또는 데몬 모드).
무료 CoinGecko API (호출 50/min 제한, 토큰 6개 × 12 cycle/h = 72 호출 — 안전).

알림:
  - 1시간 ±3% 변동 시 즉시 알림 (per-token cooldown 1h)
  - 09:00 KST 일간 요약 (24h 변동률)

환경변수:
  TELEGRAM_BOT_TOKEN  (lemuel-secrets / strategy-bot.env 와 동일 가능)
  TELEGRAM_CHAT_ID
  CRYPTO_TOKENS       콤마 구분 (기본: bitcoin,ethereum,usd-coin,tether,uniswap,aave)
  CRYPTO_THRESHOLD    백분율 (기본 3.0)
  CRYPTO_CYCLE_SEC    체크 주기 (기본 300)
"""
from __future__ import annotations

import os
import sys
import time
import json
import signal
from collections import deque
from datetime import datetime, timedelta, timezone
from urllib.request import Request, urlopen
from urllib.error import URLError

KST = timezone(timedelta(hours=9))

TOKENS = (os.getenv("CRYPTO_TOKENS")
          or "bitcoin,ethereum,usd-coin,tether,uniswap,aave").split(",")
LABEL = {
    "bitcoin": "BTC",
    "ethereum": "ETH",
    "usd-coin": "USDC",
    "tether": "USDT",
    "uniswap": "UNI",
    "aave": "AAVE",
}
THRESHOLD = float(os.getenv("CRYPTO_THRESHOLD", "3.0"))
CYCLE_SEC = int(os.getenv("CRYPTO_CYCLE_SEC", "300"))
BOT = os.getenv("TELEGRAM_BOT_TOKEN", "")
CHAT = os.getenv("TELEGRAM_CHAT_ID", "")

if not BOT or not CHAT:
    print("[error] TELEGRAM_BOT_TOKEN / TELEGRAM_CHAT_ID 없음", flush=True)
    sys.exit(2)

g_run = True
def on_sig(signum, frame):
    global g_run
    g_run = False
signal.signal(signal.SIGTERM, on_sig)
signal.signal(signal.SIGINT, on_sig)


def fetch_prices() -> dict[str, dict]:
    """CoinGecko simple/price — 1 호출로 모든 토큰."""
    ids = ",".join(TOKENS)
    url = (f"https://api.coingecko.com/api/v3/simple/price"
           f"?ids={ids}&vs_currencies=usd&include_24hr_change=true")
    req = Request(url, headers={"User-Agent": "lemuel-quant-core/crypto-price-tracker"})
    try:
        with urlopen(req, timeout=10) as r:
            return json.loads(r.read())
    except URLError as e:
        print(f"[fetch-error] {e}", flush=True)
        return {}


def telegram(msg: str) -> None:
    url = f"https://api.telegram.org/bot{BOT}/sendMessage"
    data = f"chat_id={CHAT}&text={msg}".encode()
    try:
        with urlopen(Request(url, data=data), timeout=10) as r:
            r.read()
    except Exception as e:
        print(f"[telegram-error] {e}", flush=True)


def main():
    # per-token: 가격 history (max 12 = 1시간) + 마지막 알림 시각 (cooldown 1h)
    history: dict[str, deque] = {t: deque(maxlen=12) for t in TOKENS}
    last_alert: dict[str, datetime] = {t: datetime.fromtimestamp(0, tz=KST) for t in TOKENS}
    last_daily_summary: datetime | None = None

    print(f"[crypto-price-tracker] start — tokens={TOKENS} threshold={THRESHOLD}% cycle={CYCLE_SEC}s", flush=True)

    while g_run:
        prices = fetch_prices()
        now = datetime.now(KST)

        if not prices:
            time.sleep(CYCLE_SEC)
            continue

        for token in TOKENS:
            data = prices.get(token)
            if not data:
                continue
            current = float(data.get("usd", 0))
            change_24h = float(data.get("usd_24h_change", 0))
            history[token].append((now, current))

            # 1시간 변동 (history 가 12개 차야 의미 — 5분×12=60분)
            if len(history[token]) == 12:
                old_price = history[token][0][1]
                pct = ((current - old_price) / old_price * 100) if old_price else 0
                if abs(pct) >= THRESHOLD and (now - last_alert[token]).total_seconds() > 3600:
                    arrow = "🚀" if pct > 0 else "🩸"
                    msg = (f"{arrow} {LABEL.get(token, token)} {pct:+.2f}% (1h)\n"
                           f"price: ${current:,.4f}\n"
                           f"24h: {change_24h:+.2f}%")
                    telegram(msg)
                    last_alert[token] = now
                    print(f"[alert] {token} {pct:+.2f}% — telegram sent", flush=True)

        # 09:00 KST 일간 요약
        if now.hour == 9 and now.minute < 5:
            if last_daily_summary is None or last_daily_summary.date() != now.date():
                lines = ["📊 일간 가격 요약"]
                for token in TOKENS:
                    data = prices.get(token, {})
                    p = data.get("usd", 0)
                    c = data.get("usd_24h_change", 0)
                    lines.append(f"  {LABEL.get(token, token):5s} ${p:>12,.4f}  ({c:+.2f}%)")
                telegram("\n".join(lines))
                last_daily_summary = now
                print(f"[daily-summary] sent at {now.isoformat()}", flush=True)

        time.sleep(CYCLE_SEC)

    print("[crypto-price-tracker] shutdown", flush=True)


if __name__ == "__main__":
    main()
