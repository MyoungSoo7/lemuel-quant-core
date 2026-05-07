# Redis 클라이언트. market-feed/stock-feed/news-pipeline 채널 publish/subscribe.

suppressPackageStartupMessages({
  library(redux)
  library(jsonlite)
})

redis_client <- function(host = NULL, port = NULL) {
  redux::hiredis(
    host = if (is.null(host)) Sys.getenv("LQC_REDIS_HOST", "127.0.0.1") else host,
    port = as.integer(
      if (is.null(port)) Sys.getenv("LQC_REDIS_PORT", "6379") else port
    )
  )
}

# 종목 시그널 publish. 채널 규약: signal.<symbol>
redis_publish_signal <- function(client, symbol, payload) {
  channel <- paste0("signal.", symbol)
  client$PUBLISH(channel, jsonlite::toJSON(payload, auto_unbox = TRUE))
}

# 모델 산출물 publish. 채널 규약: model.<name>.<symbol>
redis_publish_model <- function(client, name, symbol, payload) {
  channel <- paste0("model.", name, ".", symbol)
  client$PUBLISH(channel, jsonlite::toJSON(payload, auto_unbox = TRUE))
}
