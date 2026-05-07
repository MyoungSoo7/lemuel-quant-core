using Statistics

"""
    backtest_sma(prices; fast=20, slow=60, fee_bp=5)

Simple SMA crossover. Returns a NamedTuple with metrics + equity curve.
1-min bars assumed for annualization (252 * 390).
"""
function backtest_sma(prices::AbstractVector; fast::Int=20, slow::Int=60,
                       fee_bp::Real=5)
    n = length(prices)
    n >= slow || error("need ≥ slow ($slow) bars; got $n")

    sma(arr, w) = [i < w ? NaN : mean(arr[i-w+1:i]) for i in 1:length(arr)]
    f = sma(prices, fast)
    s = sma(prices, slow)

    pos = zeros(Int, n)
    state = 0
    @inbounds for i in 2:n
        if state == 0 && !isnan(f[i]) && f[i] > s[i]
            state = 1
        elseif state == 1 && f[i] < s[i]
            state = 0
        end
        pos[i] = state
    end

    rets = diff(prices) ./ prices[1:end-1]
    pos_lag = pos[1:end-1]
    trades = abs.(diff(pos))
    fees = trades .* (fee_bp / 10_000)
    pnl = pos_lag .* rets .- fees
    equity = cumprod(1 .+ pnl)

    annualization = 252 * 390   # 1-min bars in regular session
    sharpe = mean(pnl) / std(pnl) * sqrt(annualization)
    rolling_max = accumulate(max, equity)
    drawdowns = equity ./ rolling_max .- 1
    max_dd = minimum(drawdowns)

    return (
        equity = equity,
        total_return = equity[end] - 1,
        sharpe = sharpe,
        max_drawdown = max_dd,
        trades = sum(trades),
        bars = length(pnl),
    )
end

"""
    backtest_returns(weights, returns; fee_bp=5)

Apply a (n × k) returns matrix with constant target weights — rebalanced each
bar. Returns the same NamedTuple shape as `backtest_sma`.
"""
function backtest_returns(weights::AbstractVector, returns::AbstractMatrix;
                          fee_bp::Real=5)
    n, k = size(returns)
    length(weights) == k || error("weight/asset dim mismatch")
    pnl = returns * weights
    equity = cumprod(1 .+ pnl)
    annualization = 252 * 390
    sharpe = mean(pnl) / std(pnl) * sqrt(annualization)
    rolling_max = accumulate(max, equity)
    max_dd = minimum(equity ./ rolling_max .- 1)
    (equity=equity, total_return=equity[end] - 1, sharpe=sharpe,
     max_drawdown=max_dd, trades=NaN, bars=n)
end
