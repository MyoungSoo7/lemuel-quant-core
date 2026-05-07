# PostgreSQL 클라이언트. dart_disclosure 테이블 조회.

suppressPackageStartupMessages({
  library(DBI)
  library(RPostgres)
})

pg_connect <- function(dsn = Sys.getenv("LQC_PG_DSN")) {
  stopifnot(nzchar(dsn))
  # postgres://user:pw@host:port/db
  m <- regmatches(dsn, regexec(
    "^postgres://([^:]+):([^@]+)@([^:/]+):?([0-9]*)/(.+)$", dsn
  ))[[1]]
  stopifnot(length(m) == 6)
  DBI::dbConnect(
    RPostgres::Postgres(),
    user = m[2], password = m[3],
    host = m[4],
    port = if (nchar(m[5])) as.integer(m[5]) else 5432L,
    dbname = m[6]
  )
}

# 최근 N개 공시.
dart_recent <- function(con, limit = 100) {
  DBI::dbGetQuery(
    con,
    paste0(
      "SELECT rcept_no, corp_name, stock_code, report_nm, rcept_dt ",
      "FROM dart_disclosure ORDER BY rcept_no DESC LIMIT $1"
    ),
    params = list(limit)
  )
}

# 종목별 최근 N개 공시.
dart_by_stock <- function(con, stock_code, limit = 50) {
  DBI::dbGetQuery(
    con,
    paste0(
      "SELECT rcept_no, corp_name, report_nm, rcept_dt ",
      "FROM dart_disclosure WHERE stock_code = $1 ",
      "ORDER BY rcept_dt DESC LIMIT $2"
    ),
    params = list(stock_code, limit)
  )
}
