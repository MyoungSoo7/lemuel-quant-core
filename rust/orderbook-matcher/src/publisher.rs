use crate::book::TopOfBook;
use anyhow::Result;
use redis::AsyncCommands;
use tokio::sync::Mutex;

pub struct RedisPublisher {
    conn: Mutex<redis::aio::MultiplexedConnection>,
}

impl RedisPublisher {
    pub async fn connect(url: &str) -> Result<Self> {
        let client = redis::Client::open(url)?;
        let conn = client.get_multiplexed_async_connection().await?;
        Ok(Self {
            conn: Mutex::new(conn),
        })
    }

    pub async fn publish_top(&self, symbol: &str, top: &TopOfBook) -> Result<()> {
        let payload = serde_json::json!({
            "symbol": symbol,
            "bid_px": top.bid_px,
            "bid_qty": top.bid_qty,
            "ask_px": top.ask_px,
            "ask_qty": top.ask_qty,
            "spread_bps": top.spread_bps,
            "ts_ns": std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_nanos() as u64)
                .unwrap_or(0),
        })
        .to_string();
        let channel = format!("book.binance.{symbol}.top");
        let mut conn = self.conn.lock().await;
        let _: () = conn.publish(channel, payload).await?;
        Ok(())
    }
}
