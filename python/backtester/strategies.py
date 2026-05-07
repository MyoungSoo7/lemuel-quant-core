"""기본 전략 모음. 모두 (df) → buy/sell 신호 (bool Series) 형태."""
from __future__ import annotations

import pandas as pd


def sma_crossover(close: pd.Series, fast: int = 20, slow: int = 60):
    """이평선 교차. fast > slow 일 때 매수 시그널."""
    f = close.rolling(fast, min_periods=fast).mean()
    s = close.rolling(slow, min_periods=slow).mean()
    long  = (f > s) & (f.shift(1) <= s.shift(1))
    short = (f < s) & (f.shift(1) >= s.shift(1))
    return long, short


def rsi_reversion(close: pd.Series, period: int = 14,
                  oversold: float = 30, overbought: float = 70):
    """RSI 과매도/과매수 반등."""
    delta = close.diff()
    gain  = delta.clip(lower=0).rolling(period).mean()
    loss  = (-delta.clip(upper=0)).rolling(period).mean()
    rs    = gain / loss.replace(0, 1e-9)
    rsi   = 100 - 100 / (1 + rs)
    long  = (rsi < oversold) & (rsi.shift(1) >= oversold)
    short = (rsi > overbought) & (rsi.shift(1) <= overbought)
    return long, short


def bollinger_breakout(close: pd.Series, period: int = 20, stdev: float = 2.0):
    """볼린저 밴드 상단 돌파 매수."""
    m = close.rolling(period).mean()
    s = close.rolling(period).std()
    upper = m + stdev * s
    lower = m - stdev * s
    long  = (close > upper) & (close.shift(1) <= upper.shift(1))
    short = (close < lower) & (close.shift(1) >= lower.shift(1))
    return long, short


REGISTRY = {
    "sma": sma_crossover,
    "rsi": rsi_reversion,
    "boll": bollinger_breakout,
}
