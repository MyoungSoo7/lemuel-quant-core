using DataFrames, Parquet2

"""
    load_trades_parquet(path; symbol="btcusdt", channel_prefix="trade.binance")

Load a single rollup-*.parquet (data-warehouse output) and return a DataFrame
filtered to `channel == "trade"` and `tag.symbol == symbol`. Sorted by ts_ns.

For multi-file aggregation, just `vcat` results.
"""
function load_trades_parquet(path::String; symbol::AbstractString="btcusdt",
                              channel_prefix::AbstractString="trade.binance")
    ds = Parquet2.Dataset(path)
    df = DataFrame(ds; copycols=false)
    filter!(:channel => ==("trade"), df)
    if "tag.symbol" in names(df)
        filter!(Symbol("tag.symbol") => ==(symbol), df)
    end
    if "tag.channel" in names(df)
        filter!(Symbol("tag.channel") =>
                 c -> startswith(string(c), channel_prefix), df)
    end
    sort!(df, :ts_ns)
    return df
end
