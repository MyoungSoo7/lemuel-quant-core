//! Binance combined-stream WebSocket feed. Reconnects on error.

use anyhow::{Context, Result};
use futures_util::StreamExt;
use std::time::Duration;
use tokio_tungstenite::{connect_async, tungstenite::Message};
use tracing::{info, warn};

#[derive(Debug, Clone, Copy)]
pub enum Side {
    Buy,
    Sell,
}

#[derive(Debug, Clone)]
pub struct Level {
    pub price: f64,
    pub qty: f64,
}

#[derive(Debug, Clone)]
pub struct BookUpdate {
    pub symbol: String,
    pub bids: Vec<Level>,
    pub asks: Vec<Level>,
    pub last_update_id: u64,
}

pub struct BinanceFeed {
    symbols: Vec<String>,
    depth: u32,
}

impl BinanceFeed {
    pub fn new(symbols: Vec<String>, depth: u32) -> Self {
        Self { symbols, depth }
    }

    pub async fn run<F>(&mut self, mut on_book: F) -> Result<()>
    where
        F: FnMut(BookUpdate) + Send + 'static,
    {
        loop {
            let url = self.url();
            info!(%url, "connecting");
            let res = self.run_once(&url, &mut on_book).await;
            warn!(?res, "feed disconnected; reconnecting in 1s");
            tokio::time::sleep(Duration::from_secs(1)).await;
        }
    }

    fn url(&self) -> String {
        let streams = self
            .symbols
            .iter()
            .map(|s| format!("{}@depth{}@100ms", s, self.depth))
            .collect::<Vec<_>>()
            .join("/");
        format!("wss://stream.binance.com:9443/stream?streams={streams}")
    }

    async fn run_once<F>(&self, url: &str, on_book: &mut F) -> Result<()>
    where
        F: FnMut(BookUpdate) + Send,
    {
        let (ws, _) = connect_async(url).await.context("connect_async")?;
        let (_, mut read) = ws.split();

        while let Some(msg) = read.next().await {
            let msg = msg?;
            if let Message::Text(text) = msg {
                if let Some(b) = parse_combined_book(&text) {
                    on_book(b);
                }
            }
        }
        Ok(())
    }
}

fn parse_combined_book(text: &str) -> Option<BookUpdate> {
    // Combined stream: {"stream":"<sym>@depth20@100ms","data":{...}}
    let v: serde_json::Value = serde_json::from_str(text).ok()?;
    let stream = v.get("stream")?.as_str()?;
    let symbol = stream.split('@').next()?.to_string();
    let data = v.get("data")?;
    let last_update_id = data.get("lastUpdateId")?.as_u64().unwrap_or(0);

    let extract = |key: &str| -> Vec<Level> {
        data.get(key)
            .and_then(|x| x.as_array())
            .map(|arr| {
                arr.iter()
                    .filter_map(|lvl| {
                        let pair = lvl.as_array()?;
                        let p = pair.first()?.as_str()?.parse::<f64>().ok()?;
                        let q = pair.get(1)?.as_str()?.parse::<f64>().ok()?;
                        Some(Level { price: p, qty: q })
                    })
                    .collect()
            })
            .unwrap_or_default()
    };
    Some(BookUpdate {
        symbol,
        bids: extract("bids"),
        asks: extract("asks"),
        last_update_id,
    })
}
