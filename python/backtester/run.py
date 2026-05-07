"""백테스터 CLI.

사용법:
    python -m backtester.run --symbol btcusdt --strategy sma --fast 20 --slow 60

산출물:
    stdout 에 메트릭 (총수익률, 샤프, MDD, 거래수)
    --plot 옵션 시 png 저장
"""
from __future__ import annotations

import argparse
import sys
from datetime import datetime, timezone

import numpy as np
import pandas as pd

from .loader import load_trade_dataframe, to_ohlc
from .strategies import REGISTRY


def backtest(close: pd.Series, long: pd.Series, short: pd.Series,
             fee_bp: float = 5.0) -> dict:
    """단순 long-only 백테스터. fee_bp = 1bp = 0.01%."""
    pos = pd.Series(0, index=close.index, dtype="int8")
    state = 0
    for ts in close.index:
        if state == 0 and bool(long.get(ts, False)):
            state = 1
        elif state == 1 and bool(short.get(ts, False)):
            state = 0
        pos[ts] = state

    ret = close.pct_change().fillna(0.0)
    trade = pos.diff().fillna(0).abs()
    fee   = trade * (fee_bp / 10_000.0)
    pnl   = pos.shift(1).fillna(0) * ret - fee
    equity = (1 + pnl).cumprod()
    if equity.empty:
        return {}

    days = max((close.index[-1] - close.index[0]).total_seconds() / 86400.0, 1.0)
    total_return = float(equity.iloc[-1] - 1)
    cagr = (1 + total_return) ** (365.0 / days) - 1 if total_return > -1 else float("nan")
    daily = pnl.resample("1D").sum()
    sharpe = float(np.sqrt(252) * daily.mean() / daily.std()) if daily.std() else 0.0
    mdd = float((equity / equity.cummax() - 1).min())

    return {
        "total_return": total_return,
        "cagr": cagr,
        "sharpe": sharpe,
        "mdd": mdd,
        "trades": int(trade.sum()),
        "bars": len(close),
        "equity_final": float(equity.iloc[-1]),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--symbol", required=True, help="e.g. btcusdt, 005930")
    ap.add_argument("--channel", default="trade.binance")
    ap.add_argument("--freq", default="1min")
    ap.add_argument("--strategy", default="sma", choices=list(REGISTRY))
    ap.add_argument("--fast", type=int, default=20)
    ap.add_argument("--slow", type=int, default=60)
    ap.add_argument("--period", type=int, default=14)
    ap.add_argument("--fee-bp", type=float, default=5.0)
    ap.add_argument("--since", help="ISO 8601 (e.g. 2026-05-01)")
    ap.add_argument("--until")
    ap.add_argument("--plot", help="output png path")
    args = ap.parse_args()

    since = datetime.fromisoformat(args.since).replace(tzinfo=timezone.utc) if args.since else None
    until = datetime.fromisoformat(args.until).replace(tzinfo=timezone.utc) if args.until else None

    print(f"[backtest] loading {args.symbol} from {args.channel} ...", file=sys.stderr)
    df = load_trade_dataframe(args.symbol, channel=args.channel,
                               since=since, until=until)
    if df.empty:
        print("no data", file=sys.stderr)
        return 1
    ohlc = to_ohlc(df, freq=args.freq)

    if args.strategy == "sma":
        long, short = REGISTRY["sma"](ohlc["close"], args.fast, args.slow)
    elif args.strategy == "rsi":
        long, short = REGISTRY["rsi"](ohlc["close"], period=args.period)
    else:
        long, short = REGISTRY[args.strategy](ohlc["close"])

    metrics = backtest(ohlc["close"], long, short, fee_bp=args.fee_bp)
    for k, v in metrics.items():
        print(f"{k}: {v}")

    if args.plot:
        import matplotlib.pyplot as plt
        plt.figure(figsize=(10, 4))
        ohlc["close"].plot(label="close")
        plt.title(f"{args.symbol} {args.strategy}")
        plt.tight_layout()
        plt.savefig(args.plot)
        print(f"saved plot: {args.plot}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
