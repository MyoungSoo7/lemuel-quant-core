# Cloudflare R2 (S3 호환) 클라이언트. data-warehouse 산출 Parquet 읽기 전용.
#
# 사용:
#   source("r/common/r2.R")
#   df <- r2_read_parquet("snapshots/rollup-20260507-100000.parquet")
#   keys <- r2_list("snapshots/")

suppressPackageStartupMessages({
  library(paws.storage)
  library(arrow)
  library(tibble)
})

r2_client <- function(cfg = NULL) {
  if (is.null(cfg)) {
    cfg <- list(
      endpoint   = Sys.getenv("R2_ENDPOINT"),
      bucket     = Sys.getenv("R2_BUCKET", "lemuel-backup"),
      access_key = Sys.getenv("R2_ACCESS_KEY"),
      secret_key = Sys.getenv("R2_SECRET_KEY"),
      region     = "auto"
    )
  }
  stopifnot(nzchar(cfg$endpoint), nzchar(cfg$access_key), nzchar(cfg$secret_key))
  s3 <- paws.storage::s3(
    config = list(
      endpoint    = cfg$endpoint,
      region      = cfg$region,
      credentials = list(creds = list(
        access_key_id     = cfg$access_key,
        secret_access_key = cfg$secret_key
      ))
    )
  )
  structure(list(s3 = s3, bucket = cfg$bucket), class = "r2_client")
}

r2_list <- function(prefix = "snapshots/", client = r2_client()) {
  keys <- character()
  token <- NULL
  repeat {
    args <- list(Bucket = client$bucket, Prefix = prefix)
    if (!is.null(token)) args$ContinuationToken <- token
    resp <- do.call(client$s3$list_objects_v2, args)
    keys <- c(keys, vapply(resp$Contents, `[[`, character(1), "Key"))
    if (!isTRUE(resp$IsTruncated)) break
    token <- resp$NextContinuationToken
  }
  keys
}

r2_read_parquet <- function(key, client = r2_client()) {
  resp <- client$s3$get_object(Bucket = client$bucket, Key = key)
  arrow::read_parquet(arrow::BufferReader$new(resp$Body))
}

# trade 채널 데이터를 종목별로 모아 단일 tibble 로 반환.
r2_load_trades <- function(symbol,
                            channel_prefix = "trade.binance",
                            client = r2_client()) {
  keys <- r2_list("snapshots/", client = client)
  frames <- list()
  for (k in keys) {
    df <- tryCatch(r2_read_parquet(k, client = client), error = function(e) NULL)
    if (is.null(df) || !nrow(df)) next
    if (!"channel" %in% names(df)) next
    sub <- df[df$channel == "trade", , drop = FALSE]
    if (!nrow(sub) || !"tag.symbol" %in% names(sub)) next
    sub <- sub[sub$`tag.symbol` == symbol, , drop = FALSE]
    if ("tag.channel" %in% names(sub)) {
      sub <- sub[startsWith(sub$`tag.channel`, channel_prefix), , drop = FALSE]
    }
    if (!nrow(sub)) next
    frames[[length(frames) + 1L]] <- tibble::as_tibble(sub)
  }
  if (!length(frames)) return(tibble::tibble())
  out <- do.call(rbind, frames)
  out$ts <- as.POSIXct(out$ts_ns / 1e9, origin = "1970-01-01", tz = "UTC")
  out
}
