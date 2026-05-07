module QuantTools

include("blackscholes.jl")
include("portfolio.jl")
include("backtest.jl")
include("loader.jl")

export black_scholes, implied_vol, greeks
export optimize_portfolio, efficient_frontier
export backtest_sma, backtest_returns
export load_trades_parquet

end # module
