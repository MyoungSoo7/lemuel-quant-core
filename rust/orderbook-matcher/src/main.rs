use clap::Parser;
use std::sync::Arc;
use tracing::{error, info};

mod book;
mod feed;
mod publisher;

use book::OrderBook;
use feed::BinanceFeed;
use publisher::RedisPublisher;

#[derive(Parser, Debug)]
#[command(version, about)]
struct Args {
    /// Comma-separated lower-case symbols (e.g. "btcusdt,ethusdt")
    #[arg(long, default_value = "btcusdt,ethusdt")]
    symbols: String,

    /// Depth level for the @depth20@100ms stream
    #[arg(long, default_value_t = 20)]
    depth: u32,

    /// Redis url. Empty disables publishing.
    #[arg(long, default_value = "redis://127.0.0.1/")]
    redis_url: String,

    /// Top-of-book throttle (publish only every N book updates)
    #[arg(long, default_value_t = 1)]
    publish_every: u32,
}

#[tokio::main(flavor = "multi_thread")]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "info".into()),
        )
        .init();

    let args = Args::parse();
    let symbols: Vec<String> = args
        .symbols
        .split(',')
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .collect();
    info!(?symbols, depth = args.depth, "starting matcher");

    let books: Arc<parking_lot::RwLock<std::collections::HashMap<String, OrderBook>>> =
        Arc::new(parking_lot::RwLock::new(
            symbols
                .iter()
                .map(|s| (s.clone(), OrderBook::default()))
                .collect(),
        ));

    let publisher = if args.redis_url.is_empty() {
        None
    } else {
        match RedisPublisher::connect(&args.redis_url).await {
            Ok(p) => Some(Arc::new(p)),
            Err(e) => {
                error!("redis connect failed: {e}; running without publish");
                None
            }
        }
    };

    let mut feed = BinanceFeed::new(symbols.clone(), args.depth);
    feed.run({
        let books = books.clone();
        let publisher = publisher.clone();
        let mut tick = 0u32;
        let every = args.publish_every.max(1);
        move |evt| {
            let mut guard = books.write();
            if let Some(b) = guard.get_mut(&evt.symbol) {
                b.apply(&evt);
                tick = tick.wrapping_add(1);
                if tick % every == 0 {
                    if let (Some(top), Some(p)) = (b.top(), publisher.as_ref()) {
                        let p = p.clone();
                        let symbol = evt.symbol.clone();
                        tokio::spawn(async move {
                            if let Err(e) = p.publish_top(&symbol, &top).await {
                                error!("redis publish err: {e}");
                            }
                        });
                    }
                }
            }
        }
    })
    .await?;
    Ok(())
}
