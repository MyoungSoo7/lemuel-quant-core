"""실시간 시그널 생성기. trade event 스트림 → 시그널 emission."""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from typing import Iterable

from common.redis_io import TradeEvent


@dataclass
class Signal:
    symbol: str
    kind: str            # "ma_cross_up", "ma_cross_down", "rsi_oversold", ...
    price: float
    detail: str = ""


class MovingAverageDetector:
    """심볼별 단/장 이평선 교차 감지. 각 트레이드마다 호출."""

    def __init__(self, fast: int = 20, slow: int = 60):
        self.fast = fast
        self.slow = slow
        self._win: dict[str, deque[float]] = {}

    def feed(self, ev: TradeEvent) -> Iterable[Signal]:
        win = self._win.setdefault(ev.symbol, deque(maxlen=self.slow + 2))
        prev_fast, prev_slow = self._snapshot(win)
        win.append(ev.price)
        cur_fast, cur_slow = self._snapshot(win)
        if prev_fast is None or cur_fast is None:
            return ()
        if prev_fast <= prev_slow and cur_fast > cur_slow:
            return (Signal(ev.symbol, "ma_cross_up", ev.price,
                            f"f={cur_fast:.2f} s={cur_slow:.2f}"),)
        if prev_fast >= prev_slow and cur_fast < cur_slow:
            return (Signal(ev.symbol, "ma_cross_down", ev.price,
                            f"f={cur_fast:.2f} s={cur_slow:.2f}"),)
        return ()

    def _snapshot(self, win: deque[float]) -> tuple[float | None, float | None]:
        if len(win) < self.slow:
            return None, None
        as_list = list(win)
        f = sum(as_list[-self.fast:]) / self.fast
        s = sum(as_list[-self.slow:]) / self.slow
        return f, s


@dataclass
class PriceJumpDetector:
    """직전 N틱 대비 X% 이상 변동시 시그널."""
    threshold_pct: float = 0.5
    window: int = 100
    _state: dict[str, deque[float]] = field(default_factory=dict)

    def feed(self, ev: TradeEvent) -> Iterable[Signal]:
        win = self._state.setdefault(ev.symbol, deque(maxlen=self.window))
        win.append(ev.price)
        if len(win) < self.window:
            return ()
        ref = win[0]
        change = (ev.price - ref) / ref * 100.0
        if abs(change) >= self.threshold_pct:
            kind = "price_jump_up" if change > 0 else "price_jump_down"
            win.clear()
            return (Signal(ev.symbol, kind, ev.price,
                            f"{change:+.2f}% over last {self.window} trades"),)
        return ()
