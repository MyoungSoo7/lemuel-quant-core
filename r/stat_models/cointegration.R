# stat_models/cointegration.R — 페어트레이딩 후보 발굴
#
# Engle-Granger + Johansen 두 검정으로 공적분 페어 찾고 헤지비 계산.
#
# 사용:
#   Rscript r/stat_models/cointegration.R btcusdt ethusdt
suppressPackageStartupMessages({
  library(urca)
  library(tseries)
  library(xts)
})
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "r2.R"))
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "redis.R"))

`%||%` <- function(a, b) if (!is.null(a)) a else b

align_pair <- function(a, b) {
  m <- merge(a, b, join = "inner")
  colnames(m) <- c("a", "b")
  m <- na.omit(m)
  m
}

test_cointegration <- function(pair) {
  # Engle-Granger: lm(log(a) ~ log(b)), residual ADF.
  la <- log(pair$a); lb <- log(pair$b)
  fit <- lm(la ~ lb)
  resid <- residuals(fit)
  adf <- urca::ur.df(resid, type = "drift", lags = 1)
  adf_stat <- adf@teststat[1, 1]
  adf_crit <- adf@cval[1, "5pct"]

  # Johansen trace test
  jo <- urca::ca.jo(cbind(la, lb), type = "trace",
                     ecdet = "const", K = 2)
  jo_stat <- jo@teststat[1]
  jo_crit <- jo@cval[1, "5pct"]

  list(
    hedge_ratio       = unname(coef(fit)[2]),
    spread_mean       = mean(resid),
    spread_sd         = sd(resid),
    adf_statistic     = adf_stat,
    adf_critical_5pct = adf_crit,
    adf_pass          = adf_stat < adf_crit,
    johansen_trace    = jo_stat,
    johansen_critical = jo_crit,
    johansen_pass     = jo_stat > jo_crit,
    cointegrated      = (adf_stat < adf_crit) && (jo_stat > jo_crit)
  )
}

main <- function(args) {
  if (length(args) < 2) stop("usage: cointegration.R <sym_a> <sym_b>")
  sa <- args[1]; sb <- args[2]
  cat("[coint]", sa, "vs", sb, "\n")

  ta <- r2_load_trades(sa); tb <- r2_load_trades(sb)
  if (!nrow(ta) || !nrow(tb)) stop("missing data")

  pa <- xts::xts(ta[["val.price"]], order.by = ta$ts)
  pb <- xts::xts(tb[["val.price"]], order.by = tb$ts)

  # 1분봉 종가로 다운샘플
  to_minute <- function(p) {
    ep <- xts::endpoints(p, on = "minutes", k = 1)
    xts::period.apply(p, ep, function(x) as.numeric(tail(x, 1)))
  }
  pa1 <- to_minute(pa); pb1 <- to_minute(pb)

  pair <- align_pair(pa1, pb1)
  if (nrow(pair) < 100) stop("aligned series too short")
  cat("[coint] aligned bars:", nrow(pair), "\n")

  out <- test_cointegration(pair)
  for (k in names(out)) cat(k, ":", out[[k]], "\n")

  if (Sys.getenv("LQC_PUBLISH", "1") == "1") {
    r <- redis_client()
    redis_publish_model(r, "cointegration",
                         paste0(sa, "_", sb), out)
  }
}

if (!interactive()) main(commandArgs(trailingOnly = TRUE))
