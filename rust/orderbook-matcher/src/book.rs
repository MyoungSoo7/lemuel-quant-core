//! L2 orderbook keyed by price (BTreeMap → naturally sorted).
//!
//! Operations are O(log N) per level update which is dominated by network
//! latency anyway. For a faster matcher consider a fenwick/skiplist.

use std::collections::BTreeMap;

use crate::feed::{BookUpdate, Side};

/// Total-ordering wrapper that stores prices as fixed-point i64 (×1e8).
/// Avoids f64 hashing/eq pitfalls and makes BTreeMap iteration deterministic.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Px(pub i64);
impl Px {
    pub fn from_f64(v: f64) -> Self {
        Self((v * 1e8).round() as i64)
    }
    pub fn to_f64(self) -> f64 {
        self.0 as f64 / 1e8
    }
}

#[derive(Default)]
pub struct OrderBook {
    pub bids: BTreeMap<Px, f64>, // descending: iter().rev() for best
    pub asks: BTreeMap<Px, f64>, // ascending: iter() for best
}

#[derive(Debug, Clone)]
pub struct TopOfBook {
    pub bid_px: f64,
    pub bid_qty: f64,
    pub ask_px: f64,
    pub ask_qty: f64,
    pub spread_bps: f64,
}

impl OrderBook {
    pub fn apply(&mut self, ev: &BookUpdate) {
        // Snapshot semantics — depth20 stream sends a full top-N snapshot
        // every frame, so replace each side wholesale.
        self.bids.clear();
        self.asks.clear();
        for lvl in &ev.bids {
            let q = lvl.qty;
            if q > 0.0 {
                self.bids.insert(Px::from_f64(lvl.price), q);
            }
        }
        for lvl in &ev.asks {
            let q = lvl.qty;
            if q > 0.0 {
                self.asks.insert(Px::from_f64(lvl.price), q);
            }
        }
        let _ = Side::Buy; // silence unused if Side gets trimmed later
    }

    pub fn top(&self) -> Option<TopOfBook> {
        let (&bp, &bq) = self.bids.iter().next_back()?;
        let (&ap, &aq) = self.asks.iter().next()?;
        let bid = bp.to_f64();
        let ask = ap.to_f64();
        let mid = 0.5 * (bid + ask);
        let spread_bps = if mid > 0.0 {
            (ask - bid) / mid * 10_000.0
        } else {
            0.0
        };
        Some(TopOfBook {
            bid_px: bid,
            bid_qty: bq,
            ask_px: ask,
            ask_qty: aq,
            spread_bps,
        })
    }
}
