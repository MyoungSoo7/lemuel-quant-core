# QuantTools.jl

Julia 정량 분석 패키지. R/Python 백테스터보다 빠르고, 옵션/포트폴리오 최적화에 강함.

## 모듈

| 파일 | 내용 |
|------|------|
| `src/blackscholes.jl` | European option 가격 + 5 Greek + Newton-Raphson IV |
| `src/portfolio.jl` | JuMP + HiGHS Markowitz mean-variance + efficient frontier |
| `src/backtest.jl` | SMA crossover + 가중 멀티에셋 returns 백테스터 |
| `src/loader.jl` | data-warehouse rollup-*.parquet 로더 |

## 설치

```bash
cd julia/quant-tools
julia --project=. -e 'using Pkg; Pkg.instantiate()'
```

## 예시

```julia
using QuantTools

# 1) Black-Scholes call price
price = black_scholes(100.0, 100.0, 0.05, 0.2, 1.0)
g = greeks(100.0, 100.0, 0.05, 0.2, 1.0)
σ_imp = implied_vol(10.4506, 100.0, 100.0, 0.05, 1.0)

# 2) Markowitz portfolio (returns is an N×K matrix)
opt = optimize_portfolio(returns; target_return=0.0008,
                          allow_short=false, max_weight=0.4)
println("weights: ", opt.weights, "  vol: ", opt.volatility)

# 3) SMA backtest on data-warehouse parquet
df = load_trades_parquet("/opt/lqc/var/dwh/rollup-20260507-100000.parquet";
                          symbol="btcusdt")
res = backtest_sma(df[!, "val.price"]; fast=20, slow=60)
println("Sharpe: ", res.sharpe, "  MDD: ", res.max_drawdown)
```

## R/Python 과 비교

- backtest_sma: R 동일 로직 대비 50~100배 빠름 (`@inbounds` + native loops)
- optimize_portfolio: R `quadprog` 보다 큰 문제도 잘 풀림 (HiGHS 솔버)
- 단점: 첫 실행 컴파일 지연 (TTFX) 5~10초. 운영시 SystemImage 만들거나 데몬 띄움.
