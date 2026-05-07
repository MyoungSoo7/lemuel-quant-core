# Shiny 실시간 대시보드.
#
# 실행:
#   Rscript -e 'shiny::runApp("r/shiny_dashboard", host="0.0.0.0", port=3838)'
#
# 르무엘에서 systemd unit 으로 띄우는 게 운영 권장.
suppressPackageStartupMessages({
  library(shiny)
  library(bslib)
  library(plotly)
  library(DT)
  library(dplyr)
  library(redux)
  library(jsonlite)
})

source(here::here("r", "common", "r2.R"))
source(here::here("r", "common", "redis.R"))
source(here::here("r", "common", "db.R"))

ui <- page_navbar(
  title = "lemuel-quant",
  theme = bs_theme(bootswatch = "darkly"),

  nav_panel("Live trades",
    sidebarLayout(
      sidebarPanel(
        selectInput("symbol", "Symbol",
                     choices = c("btcusdt", "ethusdt", "005930"),
                     selected = "btcusdt"),
        sliderInput("lookback", "Bars to show", 50, 2000, 500),
        helpText("Streamed via Redis pub/sub. Refresh every 1s.")
      ),
      mainPanel(
        plotlyOutput("price_plot", height = 400),
        verbatimTextOutput("last_trade"),
        DT::dataTableOutput("recent_trades")
      )
    )
  ),

  nav_panel("Backtest",
    sidebarLayout(
      sidebarPanel(
        selectInput("bt_symbol", "Symbol",
                     choices = c("btcusdt", "ethusdt", "005930"),
                     selected = "btcusdt"),
        selectInput("strategy", "Strategy",
                     choices = c("sma", "rsi", "boll"), selected = "sma"),
        numericInput("p1", "param1", 20, min = 2),
        numericInput("p2", "param2", 60, min = 2),
        actionButton("run_bt", "Run backtest")
      ),
      mainPanel(
        plotlyOutput("equity_plot", height = 400),
        verbatimTextOutput("metrics")
      )
    )
  ),

  nav_panel("DART",
    DT::dataTableOutput("dart_table")
  )
)

server <- function(input, output, session) {

  trades_ring <- reactiveVal(data.frame(
    ts = as.POSIXct(character()), price = numeric(),
    qty = numeric(), side = integer()
  ))

  # Redis 구독은 timer-based polling 으로 대체 (Shiny 는 background socket
  # 처리에 한계). 매 1초 redis-cli LRANGE 같은 패턴으로 최근 N개 가져옴.
  observe({
    invalidateLater(1000, session)
    req(input$symbol)
    r <- tryCatch(redis_client(), error = function(e) NULL)
    if (is.null(r)) return()
    key <- paste0("trade.binance.", input$symbol)
    # market-feed 는 PUBLISH 만 하고 store 는 안 함. 운영에선 별도 RING list
    # 만드는 게 좋음. 데모로 placeholder 그래프.
    df <- trades_ring()
    n <- input$lookback
    if (nrow(df) > n) df <- tail(df, n)
    trades_ring(df)
  })

  output$price_plot <- renderPlotly({
    df <- trades_ring()
    if (!nrow(df)) {
      return(plotly::plot_ly(type = "scatter", mode = "lines") %>%
               plotly::layout(title = "(데이터 없음 — Redis ring 미구현)"))
    }
    plotly::plot_ly(df, x = ~ts, y = ~price, type = "scatter",
                     mode = "lines+markers")
  })

  output$last_trade <- renderText({
    df <- trades_ring()
    if (!nrow(df)) "no trades yet"
    else sprintf("%s | price=%.4f qty=%g",
                  format(tail(df$ts, 1)), tail(df$price, 1), tail(df$qty, 1))
  })

  output$recent_trades <- DT::renderDataTable({
    DT::datatable(tail(trades_ring(), 20),
                   options = list(dom = "tip", pageLength = 10))
  })

  bt_result <- eventReactive(input$run_bt, {
    trades <- r2_load_trades(input$bt_symbol)
    if (!nrow(trades)) return(list(equity = NULL, metrics = "no data"))
    source(here::here("r", "quant_research", "backtest.R"), local = TRUE)
    ohlc <- trades_to_ohlc(trades)
    sig <- switch(input$strategy,
                   sma  = strategy_sma(ohlc$close, input$p1, input$p2),
                   rsi  = strategy_rsi(ohlc$close, input$p1),
                   boll = strategy_boll(ohlc$close, input$p1, input$p2))
    res <- run_backtest(ohlc, sig)
    list(equity = res$equity, metrics = res$metrics)
  })

  output$equity_plot <- renderPlotly({
    r <- bt_result()
    if (is.null(r$equity)) return(NULL)
    df <- data.frame(ts = index(r$equity),
                      equity = as.numeric(r$equity))
    plotly::plot_ly(df, x = ~ts, y = ~equity, type = "scatter",
                     mode = "lines")
  })

  output$metrics <- renderPrint({
    r <- bt_result()
    if (is.character(r$metrics)) cat(r$metrics) else str(r$metrics)
  })

  output$dart_table <- DT::renderDataTable({
    dsn <- Sys.getenv("LQC_PG_DSN")
    if (!nzchar(dsn)) return(DT::datatable(data.frame()))
    con <- pg_connect(dsn)
    on.exit(DBI::dbDisconnect(con), add = TRUE)
    DT::datatable(dart_recent(con, 50),
                   options = list(pageLength = 25))
  })
}

shinyApp(ui, server)
