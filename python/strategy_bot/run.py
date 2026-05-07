"""strategy-bot 메인. Redis 트레이드 구독 → 시그널 → Telegram 푸시.

사용법:
    python -m strategy_bot.run

환경변수:
    LQC_REDIS_HOST, LQC_REDIS_PORT
    TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID
    LQC_PATTERNS=trade.binance.*,trade.kis.*  (콤마 구분)
"""
from __future__ import annotations

import os
import signal
import sys
import time
from collections import defaultdict, deque

from common.redis_io import stream_trades
from common.telegram import TelegramNotifier

from .signals import MovingAverageDetector, PriceJumpDetector, Signal


def emoji_for(kind: str) -> str:
    return {
        "ma_cross_up":     "📈",
        "ma_cross_down":   "📉",
        "price_jump_up":   "🚀",
        "price_jump_down": "🩸",
    }.get(kind, "🔔")


def format_signal(sig: Signal) -> str:
    return (f"{emoji_for(sig.kind)} {sig.kind}\n"
            f"symbol: {sig.symbol}\n"
            f"price: {sig.price}\n"
            f"detail: {sig.detail}")


def main() -> int:
    patterns = os.environ.get(
        "LQC_PATTERNS", "trade.binance.*,trade.kis.*"
    ).split(",")
    notifier = TelegramNotifier()

    ma   = MovingAverageDetector(fast=20, slow=60)
    jump = PriceJumpDetector(threshold_pct=0.5, window=100)
    detectors = [ma, jump]

    # 같은 시그널 중복 방지: (symbol, kind) 별 마지막 발신 timestamp
    last_sent: dict[tuple[str, str], float] = defaultdict(float)
    cooldown_sec = 60.0

    stop = False
    def handle_sig(*_):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGINT, handle_sig)
    signal.signal(signal.SIGTERM, handle_sig)

    print(f"[strategy-bot] subscribing {patterns}", file=sys.stderr)
    for ev in stream_trades(patterns=patterns):
        if stop:
            break
        for det in detectors:
            for sig in det.feed(ev):
                key = (sig.symbol, sig.kind)
                now = time.time()
                if now - last_sent[key] < cooldown_sec:
                    continue
                last_sent[key] = now
                try:
                    notifier.send(format_signal(sig))
                    print(f"[sig] {sig}", file=sys.stderr)
                except Exception as e:
                    print(f"[strategy-bot] telegram failed: {e}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
