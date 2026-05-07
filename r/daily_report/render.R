#!/usr/bin/env Rscript
# Quarto 리포트 렌더링 + Telegram 푸시.
#
# Cron 사용: 30 9 * * *  Rscript /opt/lqc/lemuel-quant-core/r/daily_report/render.R
suppressPackageStartupMessages({
  library(quarto)
  library(httr2)
})

doc_dir <- here::here("r", "daily_report")
out_html <- file.path(doc_dir, "report.html")

cat("[daily_report] rendering ...\n")
quarto::quarto_render(file.path(doc_dir, "report.qmd"),
                       output_format = "html",
                       output_file   = "report.html")

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
