# stat_models/garch_volatility.R — GARCH(1,1) 변동성 추정 + Redis publish.
#
# 사용:
#   Rscript r/stat_models/garch_volatility.R btcusdt
#
# Publish: model.garch.<symbol> = {sigma_next, persistence, half_life_days}
suppressPackageStartupMessages({
  library(rugarch)
  library(xts)
})
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "r2.R"))
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "redis.R"))

`%||%` <- function(a, b) if (!is.null(a)) a else b

fit_garch <- function(returns) {
  spec <- ugarchspec(
    variance.model = list(model = "sGARCH", garchOrder = c(1, 1)),
    mean.model     = list(armaOrder = c(0, 0), include.mean = FALSE),
    distribution.model = "std"   # student-t
  )
  fit <- ugarchfit(spec = spec, data = returns, solver = "hybrid")
  coefs <- coef(fit)
  alpha <- coefs["alpha1"]; beta <- coefs["beta1"]
  persistence <- alpha + beta
  fc <- ugarchforecast(fit, n.ahead = 1)
  list(
    sigma_next     = sigma(fc)[1, 1],
    alpha          = alpha,
    beta           = beta,
    persistence    = persistence,
    half_life_min  = log(0.5) / log(persistence),
    log_likelihood = likelihood(fit)
  )
}

main <- function(args) {
  symbol <- args[1] %||% "btcusdt"
  cat("[garch] symbol=", symbol, "\n", sep = "")

  trades <- r2_load_trades(symbol)
  if (nrow(trades) < 200) stop("insufficient data: need 200+ trades")

  prices <- xts::xts(trades[["val.price"]], order.by = trades$ts)
  rets   <- diff(log(prices))[-1]
  cat("[garch] returns=", length(rets), "\n", sep = "")

  out <- fit_garch(rets)
  for (k in names(out)) cat(k, ": ", out[[k]], "\n", sep = "")

  if (Sys.getenv("LQC_PUBLISH", "1") == "1") {
    r <- redis_client()
    redis_publish_model(r, "garch", symbol, out)
    cat("[garch] published model.garch.", symbol, "\n", sep = "")
  }
}

if (!interactive()) main(commandArgs(trailingOnly = TRUE))
