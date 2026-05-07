# risk_engine/portfolio_risk.R — 포트폴리오 VaR / CVaR / Monte Carlo.
#
# 사용:
#   Rscript r/risk_engine/portfolio_risk.R btcusdt:0.5 ethusdt:0.3 005930:0.2
suppressPackageStartupMessages({
  library(PerformanceAnalytics)
  library(xts)
})
source(file.path(dirname(sys.frame(1)$ofile %||% "."),
                  "..", "common", "r2.R"))

`%||%` <- function(a, b) if (!is.null(a)) a else b

parse_position <- function(arg) {
  parts <- strsplit(arg, ":", fixed = TRUE)[[1]]
  list(symbol = parts[1], weight = as.numeric(parts[2]))
}

build_returns <- function(symbols) {
  cols <- list()
  for (s in symbols) {
    trades <- r2_load_trades(s)
    if (!nrow(trades)) {
      warning("no data for ", s); next
    }
    prices <- xts::xts(trades[["val.price"]], order.by = trades$ts)
    ep <- xts::endpoints(prices, on = "minutes", k = 1)
    m1 <- xts::period.apply(prices, ep, function(x) as.numeric(tail(x, 1)))
    rets <- na.omit(diff(log(m1)))
    cols[[s]] <- rets
  }
  if (!length(cols)) stop("no return series built")
  m <- Reduce(function(a, b) merge(a, b, join = "inner"), cols)
  colnames(m) <- names(cols)
  m
}

portfolio_metrics <- function(returns, weights, alpha = 0.95,
                                n_sim = 10000) {
  port_rets <- as.numeric(returns %*% weights)

  hist_var  <- as.numeric(quantile(port_rets, 1 - alpha))
  hist_cvar <- mean(port_rets[port_rets <= hist_var])

  # parametric Gaussian VaR
  mu <- mean(port_rets); sig <- sd(port_rets)
  z <- qnorm(1 - alpha)
  param_var  <- mu + sig * z
  param_cvar <- mu - sig * dnorm(z) / (1 - alpha)

  # Monte Carlo (multivariate normal)
  cov_mat <- cov(returns)
  L <- chol(cov_mat)
  set.seed(42)
  Z <- matrix(rnorm(n_sim * ncol(returns)), nrow = n_sim)
  sim_rets <- Z %*% L
  sim_port <- as.numeric(sim_rets %*% weights) + mu
  mc_var  <- as.numeric(quantile(sim_port, 1 - alpha))
  mc_cvar <- mean(sim_port[sim_port <= mc_var])

  list(
    alpha               = alpha,
    n_obs               = length(port_rets),
    expected_return_pa  = (1 + mu)^(252 * 390) - 1,  # 1-min bars
    volatility_pa       = sig * sqrt(252 * 390),
    sharpe              = mu / sig * sqrt(252 * 390),
    historical_var_1min = hist_var,
    historical_cvar_1min = hist_cvar,
    parametric_var_1min = param_var,
    parametric_cvar_1min = param_cvar,
    monte_carlo_var_1min = mc_var,
    monte_carlo_cvar_1min = mc_cvar,
    n_simulations       = n_sim
  )
}

main <- function(args) {
  if (!length(args)) stop("usage: portfolio_risk.R sym:weight ...")
  positions <- lapply(args, parse_position)
  symbols <- vapply(positions, `[[`, character(1), "symbol")
  weights <- vapply(positions, `[[`, numeric(1), "weight")
  if (abs(sum(weights) - 1) > 1e-6) {
    warning("weights sum != 1; normalizing")
    weights <- weights / sum(weights)
  }
  cat("[risk] positions:\n")
  for (i in seq_along(symbols)) {
    cat(sprintf("  %s = %.2f%%\n", symbols[i], 100 * weights[i]))
  }

  rets <- build_returns(symbols)
  cat("[risk] aligned bars:", nrow(rets), "\n")

  m <- portfolio_metrics(rets, weights)
  for (k in names(m)) cat(sprintf("%-22s : %s\n", k, format(m[[k]])))
}

if (!interactive()) main(commandArgs(trailingOnly = TRUE))
