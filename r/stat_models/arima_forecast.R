# stat_models/arima_forecast.R — auto.arima 다음봉 가격 예측 + Redis publish.
#
# 사용:
#   Rscript r/stat_models/arima_forecast.R btcusdt 60
suppressPackageStartupMessages({
  library(forecast)
  library(xts)
})
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "r2.R"))
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "redis.R"))

`%||%` <- function(a, b) if (!is.null(a)) a else b

main <- function(args) {
  symbol <- args[1] %||% "btcusdt"
  horizon <- as.integer(args[2] %||% "60")

  trades <- r2_load_trades(symbol)
  if (nrow(trades) < 500) stop("insufficient data")

  prices <- xts::xts(trades[["val.price"]], order.by = trades$ts)
  ep <- xts::endpoints(prices, on = "minutes", k = 1)
  m1 <- xts::period.apply(prices, ep, function(x) as.numeric(tail(x, 1)))
  ts_data <- ts(as.numeric(m1))

  fit <- auto.arima(ts_data, max.p = 5, max.q = 5, max.d = 2,
                     stepwise = TRUE, approximation = FALSE)
  fc <- forecast(fit, h = horizon, level = c(80, 95))

  out <- list(
    model           = paste(fit$arma[c(1, 6, 2, 3, 7, 4, 5)], collapse = ","),
    aic             = fit$aic,
    bic             = fit$bic,
    horizon         = horizon,
    point_forecast  = as.numeric(fc$mean),
    lower_80        = as.numeric(fc$lower[, 1]),
    upper_80        = as.numeric(fc$upper[, 1]),
    lower_95        = as.numeric(fc$lower[, 2]),
    upper_95        = as.numeric(fc$upper[, 2])
  )
  cat("[arima] model=", out$model, " AIC=", out$aic, "\n", sep = "")
  cat("[arima] next 5 bars:", head(out$point_forecast, 5), "\n")

  if (Sys.getenv("LQC_PUBLISH", "1") == "1") {
    r <- redis_client()
    redis_publish_model(r, "arima", symbol, out)
  }
}

if (!interactive()) main(commandArgs(trailingOnly = TRUE))
