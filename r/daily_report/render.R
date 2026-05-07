#!/usr/bin/env Rscript
# Quarto 리포트 렌더링 + Telegram 푸시.
#
# Cron 사용: 30 9 * * *  Rscript /opt/lqc/lemuel-quant-core/r/daily_report/render.R
suppressPackageStartupMessages({
  library(httr2)
})

doc_dir <- here::here("r", "daily_report")
out_html <- file.path(doc_dir, "report.html")

cat("[daily_report] rendering ...\n")
# quarto R 패키지는 processx 로 spawn 하다 권한 문제가 자주 생김.
# system2 직접 호출이 가장 단순하고 robust.
quarto_bin <- Sys.getenv("QUARTO_BIN",
                         unset = unname(Sys.which("quarto")))
if (!nzchar(quarto_bin)) stop("quarto CLI not found in PATH")
status <- system2(quarto_bin,
                   args = c("render",
                             shQuote(file.path(doc_dir, "report.qmd")),
                             "--to", "html",
                             "--output", "report.html"),
                   stdout = "", stderr = "")
if (status != 0) stop("quarto render failed (exit ", status, ")")

if (file.exists(out_html)) {
  size <- file.info(out_html)$size
  cat("[daily_report] rendered ", out_html, " (", size, " bytes)\n", sep = "")
}

bot_token <- Sys.getenv("TELEGRAM_BOT_TOKEN")
chat_id   <- Sys.getenv("TELEGRAM_CHAT_ID")
if (nzchar(bot_token) && nzchar(chat_id) && file.exists(out_html)) {
  url <- paste0("https://api.telegram.org/bot", bot_token, "/sendDocument")
  request(url) |>
    req_method("POST") |>
    req_body_multipart(
      chat_id = chat_id,
      caption = paste("daily report —", format(Sys.Date())),
      document = curl::form_file(out_html)
    ) |>
    req_perform()
  cat("[daily_report] sent to Telegram\n")
}
