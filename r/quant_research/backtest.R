# quant_research/backtest.R — R2 Parquet 로딩 → OHLC 변환 → 전략 백테스트
#
# 사용:
#   Rscript r/quant_research/backtest.R btcusdt sma 20 60
#   Rscript r/quant_research/backtest.R 005930 garch 0.95
#   Rscript r/quant_research/backtest.R btcusdt boll 20 2
#
# 산출:
#   stdout 메트릭 (총수익률, CAGR, Sharpe, Sortino, MDD)
#   /tmp/backtest_<symbol>_<strategy>.png — equity curve
suppressPackageStartupMessages({
  library(quantmod)
  library(PerformanceAnalytics)
  library(TTR)
  library(xts)
})

source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "r2.R"))

`%||%` <- function(a, b) if (!is.null(a)) a else b

trades_to_ohlc <- function(df, freq = "1 min") {
  if (!nrow(df)) return(NULL)
  df <- df[order(df$ts), ]
  prices <- xts::xts(
    df[["val.price"]], order.by = df$ts
  )
  ep <- xts::endpoints(prices, on = "minutes", k = 1)
  ohlc <- xts::period.apply(prices, ep, function(x) {
    c(open = as.numeric(x[1]),
      high = max(as.numeric(x), na.rm = TRUE),
      low  = min(as.numeric(x), na.rm = TRUE),
      close = as.numeric(tail(x, 1)))
  })
  ohlc <- xts::xts(do.call(rbind, lapply(seq_len(nrow(ohlc)), function(i) {
    as.numeric(ohlc[i])
  })), order.by = index(ohlc))
  colnames(ohlc) <- c("open", "high", "low", "close")
  ohlc
}

strategy_sma <- function(close, fast = 20, slow = 60) {
  f <- TTR::SMA(close, n = fast)
  s <- TTR::SMA(close, n = slow)
  signal <- xts::xts(rep(0L, length(close)), order.by = index(close))
  state <- 0L
  fv <- as.numeric(f); sv <- as.numeric(s)
  for (i in seq_along(fv)) {
    if (is.na(fv[i]) || is.na(sv[i])) { signal[i] <- state; next }
    if (state == 0L && fv[i] > sv[i]) state <- 1L
    else if (state == 1L && fv[i] < sv[i]) state <- 0L
    signal[i] <- state
  }
  signal
}

strategy_rsi <- function(close, period = 14, oversold = 30, overbought = 70) {
  rsi <- TTR::RSI(close, n = period)
  signal <- xts::xts(rep(0L, length(close)), order.by = index(close))
  state <- 0L
  rv <- as.numeric(rsi)
  for (i in seq_along(rv)) {
    if (is.na(rv[i])) { signal[i] <- state; next }
    if (state == 0L && rv[i] < oversold) state <- 1L
    else if (state == 1L && rv[i] > overbought) state <- 0L
    signal[i] <- state
  }
  signal
}

strategy_boll <- function(close, period = 20, sd_ = 2) {
  bb <- TTR::BBands(close, n = period, sd = sd_)
  signal <- xts::xts(rep(0L, length(close)), order.by = index(close))
  state <- 0L
  cl <- as.numeric(close); up <- bb[, "up"]; lo <- bb[, "dn"]
  for (i in seq_along(cl)) {
    if (is.na(up[i])) { signal[i] <- state; next }
    if (state == 0L && cl[i] > up[i]) state <- 1L
    else if (state == 1L && cl[i] < lo[i]) state <- 0L
    signal[i] <- state
  }
  signal
}

run_backtest <- function(ohlc, signal, fee_bp = 5) {
  ret <- TTR::ROC(ohlc$close, type = "discrete")
  trade <- abs(diff(signal))
  trade[is.na(trade)] <- 0
  fee <- trade * (fee_bp / 1e4)
  pnl <- lag(signal, 1) * ret - fee
  pnl[is.na(pnl)] <- 0
  equity <- cumprod(1 + pnl)
  list(
    pnl    = pnl,
    equity = equity,
    metrics = list(
      total_return = as.numeric(tail(equity, 1)) - 1,
      sharpe       = as.numeric(SharpeRatio.annualized(pnl, scale = 252 * 390)),
      sortino      = as.numeric(SortinoRatio(pnl)),
      max_drawdown = as.numeric(maxDrawdown(pnl)),
      trades       = sum(as.numeric(trade), na.rm = TRUE),
      bars         = length(pnl)
    )
  )
}

main <- function(args) {
  if (length(args) < 2) {
    stop("usage: backtest.R <symbol> <strategy: sma|rsi|boll> [params...]")
  }
  symbol <- args[1]; strat <- args[2]

  cat("[load] R2 trades for", symbol, "...\n")
  trades <- r2_load_trades(symbol)
  if (!nrow(trades)) {
    cat("no data; abort\n"); return(invisible(NULL))
  }
  ohlc <- trades_to_ohlc(trades)
  cat("[ohlc] rows=", nrow(ohlc), " range=", format(start(ohlc)),
      " ~ ", format(end(ohlc)), "\n", sep = "")

  signal <- switch(
    strat,
    sma  = strategy_sma(ohlc$close,
                         fast = as.integer(args[3] %||% "20"),
                         slow = as.integer(args[4] %||% "60")),
    rsi  = strategy_rsi(ohlc$close, period = as.integer(args[3] %||% "14")),
    boll = strategy_boll(ohlc$close,
                          period = as.integer(args[3] %||% "20"),
                          sd_   = as.numeric(args[4] %||% "2")),
    stop("unknown strategy: ", strat)
  )
  res <- run_backtest(ohlc, signal)
  for (k in names(res$metrics)) cat(k, ": ", res$metrics[[k]], "\n", sep = "")

  out <- file.path(tempdir(), paste0("backtest_", symbol, "_", strat, ".png"))
  png(out, width = 900, height = 400)
  on.exit(dev.off(), add = TRUE)
  plot.xts(res$equity, main = paste(symbol, strat))
  cat("[plot] saved to ", out, "\n", sep = "")
}

if (!interactive()) main(commandArgs(trailingOnly = TRUE))
