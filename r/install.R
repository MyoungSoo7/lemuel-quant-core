# 일괄 설치 스크립트. 한 번만 실행.
options(repos = c(CRAN = "https://cloud.r-project.org"))

required <- c(
  # 공통
  "arrow", "paws.storage", "redux", "RPostgres", "DBI",
  "jsonlite", "httr2", "fs", "tibble", "dplyr", "lubridate", "purrr",

  # 시각화 / 리포트
  "ggplot2", "plotly", "DT", "knitr", "rmarkdown", "scales",

  # 정량 / 시계열
  "quantmod", "PerformanceAnalytics", "TTR", "xts", "zoo",
  "forecast", "rugarch", "urca", "tseries", "FKF",

  # 리스크
  "PortfolioAnalytics", "ROI", "ROI.plugin.glpk",

  # 대시보드
  "shiny", "shinydashboard", "bslib",

  # 알림
  "telegram.bot"
)

new_pkgs <- setdiff(required, rownames(installed.packages()))
if (length(new_pkgs) > 0) {
  cat("[install] installing", length(new_pkgs), "packages...\n")
  install.packages(new_pkgs, dependencies = TRUE)
}

# Quarto CLI 별도 설치 (R 패키지 아님): https://quarto.org/docs/get-started/
if (Sys.which("quarto") == "") {
  message("Note: Quarto CLI 미설치. daily_report 사용하려면 https://quarto.org 에서 설치.")
}

cat("[install] done.\n")
