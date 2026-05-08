#!/usr/bin/env python3
"""
gas-tracker — Ethereum + Base 가스 GWEI 모니터.

데이비드 systemd. 1분 cycle (가스는 빠르게 바뀜).
- < 5 gwei : "🟢 송금 타이밍" 알림 (cooldown 1h)
- > 30 gwei: "🟡 혼잡" 알림 (cooldown 1h)
- 09:00 일간 요약

환경변수:
  TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID
  GAS_LOW_THRESHOLD   기본 5
  GAS_HIGH_THRESHOLD  기본 30
  GAS_CYCLE_SEC       기본 60
"""
from __future__ import annotations

import os
import sys
import time
import json
import signal
from datetime import datetime, timedelta, timezone
from urllib.request import Request, urlopen

KST = timezone(timedelta(hours=9))
LOW = float(os.getenv("GAS_LOW_THRESHOLD", "5"))
HIGH = float(os.getenv("GAS_HIGH_THRESHOLD", "30"))
CYCLE = int(os.getenv("GAS_CYCLE_SEC", "60"))
BOT = os.getenv("TELEGRAM_BOT_TOKEN", "")
CHAT = os.getenv("TELEGRAM_CHAT_ID", "")

if not BOT or not CHAT:
    print("[error] TELEGRAM_BOT_TOKEN / CHAT_ID 없음", flush=True)
    sys.exit(2)

# 무료 RPC endpoints (rate-limited but enough for 1/min)
RPC = {
    "ethereum": "https://eth.llamarpc.com",      # public
    "base":     "https://mainnet.base.org",       # base 공식 public
}

g_run = True
def on_sig(*_):
    global g_run; g_run = False
signal.signal(signal.SIGTERM, on_sig)
signal.signal(signal.SIGINT, on_sig)


def gwei(chain_url: str) -> float | None:
    """eth_gasPrice 호출 → wei → gwei 변환."""
    payload = json.dumps({
        "jsonrpc": "2.0", "method": "eth_gasPrice", "params": [], "id": 1
    }).encode()
    req = Request(chain_url, data=payload,
                  headers={"Content-Type": "application/json",
                           "User-Agent": "lemuel-quant-core/gas-tracker"})
    try:
        with urlopen(req, timeout=10) as r:
            data = json.loads(r.read())
        wei_hex = data.get("result", "0x0")
        return int(wei_hex, 16) / 1e9
    except Exception as e:
        print(f"[rpc-error] {chain_url}: {e}", flush=True)
        return None


def telegram(msg: str) -> None:
    url = f"https://api.telegram.org/bot{BOT}/sendMessage"
    data = f"chat_id={CHAT}&text={msg}".encode()
    try:
        with urlopen(Request(url, data=data), timeout=10) as r:
            r.read()
    except Exception as e:
        print(f"[telegram-error] {e}", flush=True)


def main():
    last_alert: dict[str, datetime] = {c: datetime.fromtimestamp(0, tz=KST) for c in RPC}
    last_summary: datetime | None = None

    print(f"[gas-tracker] start — chains={list(RPC.keys())} low={LOW} high={HIGH} cycle={CYCLE}s", flush=True)

    while g_run:
        now = datetime.now(KST)
        readings = {}

        for chain, url in RPC.items():
            g = gwei(url)
            if g is None:
                continue
            readings[chain] = g

            cooldown_ok = (now - last_alert[chain]).total_seconds() > 3600

            if g < LOW and cooldown_ok:
                telegram(f"🟢 {chain.upper()} 가스 낮음 — {g:.2f} gwei\n송금 타이밍 (< {LOW})")
                last_alert[chain] = now
                print(f"[alert] {chain} LOW {g:.2f} gwei — telegram sent", flush=True)
            elif g > HIGH and cooldown_ok:
                telegram(f"🟡 {chain.upper()} 가스 높음 — {g:.2f} gwei\n혼잡 (> {HIGH})")
                last_alert[chain] = now
                print(f"[alert] {chain} HIGH {g:.2f} gwei — telegram sent", flush=True)

        # 09:00 KST 일간 요약 (가격 트래커와 동시 발송)
        if now.hour == 9 and now.minute < 5:
            if last_summary is None or last_summary.date() != now.date():
                lines = ["⛽ 일간 가스 요약"]
                for c, g in readings.items():
                    icon = "🟢" if g < LOW else ("🟡" if g > HIGH else "⚪")
                    lines.append(f"  {icon} {c.upper():9s} {g:>6.2f} gwei")
                telegram("\n".join(lines))
                last_summary = now
                print(f"[daily-summary] sent at {now.isoformat()}", flush=True)

        # 메트릭 로그 (5분에 1회만 출력 — 1분마다 출력하면 너무 많음)
        if now.minute % 5 == 0 and now.second < CYCLE:
            line = " | ".join(f"{c}={g:.2f}gwei" for c, g in readings.items())
            print(f"[gas] {line}", flush=True)

        time.sleep(CYCLE)

    print("[gas-tracker] shutdown", flush=True)


if __name__ == "__main__":
    main()
