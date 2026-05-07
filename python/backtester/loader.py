"""data-warehouse R2 Parquet → pandas DataFrame 시리즈 변환."""
from __future__ import annotations

from datetime import datetime, timezone

import pandas as pd

from common.r2 import R2Client


def load_trade_dataframe(
    symbol: str,
    *,
    channel: str = "trade.binance",
    since: datetime | None = None,
    until: datetime | None = None,
    r2: R2Client | None = None,
) -> pd.DataFrame:
    """data-warehouse 스냅샷에서 단일 심볼 트레이드 시계열을 모아 반환.

    반환 컬럼: ts_ns, ts (DatetimeIndex), price, qty, side
    """
    r2 = r2 or R2Client()
    keys = r2.list_keys("snapshots/")
    frames: list[pd.DataFrame] = []
    for k in keys:
        df = r2.read_parquet(k)
        if df.empty or "channel" not in df.columns:
            continue
        df = df[df["channel"] == "trade"]
        if df.empty:
            continue
        # tags 와 values 가 컬럼으로 펼쳐져 있음 (tag.symbol, val.price, ...)
        sym_col   = "tag.symbol"
        ch_col    = "tag.channel"
        if sym_col not in df.columns:
            continue
        mask = df[sym_col] == symbol
        if ch_col in df.columns:
            mask &= df[ch_col].str.startswith(channel, na=False)
        sub = df[mask]
        if sub.empty:
            continue
        frames.append(sub)
    if not frames:
        return pd.DataFrame(columns=["ts_ns", "price", "qty", "side"])

    out = pd.concat(frames, ignore_index=True)
    out["ts_ns"] = out["ts_ns"].astype("int64")
    out["ts"] = pd.to_datetime(out["ts_ns"], unit="ns", utc=True)
    keep = ["ts_ns", "ts"]
    for col in ("val.price", "val.qty", "val.side"):
        if col in out.columns:
            out[col.replace("val.", "")] = out[col]
            keep.append(col.replace("val.", ""))
    out = out[keep].sort_values("ts").set_index("ts")

    if since is not None:
        out = out[out.index >= since]
    if until is not None:
        out = out[out.index <= until]
    return out


def to_ohlc(df: pd.DataFrame, freq: str = "1min") -> pd.DataFrame:
    """트레이드 → OHLCV 리샘플링."""
    if df.empty:
        return df
    g = df["price"].resample(freq)
    o = g.first().rename("open")
    h = g.max().rename("high")
    l = g.min().rename("low")
    c = g.last().rename("close")
    v = df["qty"].resample(freq).sum().rename("volume")
    return pd.concat([o, h, l, c, v], axis=1).dropna()
