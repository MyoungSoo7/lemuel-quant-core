// lqc-gateway: small infra sidecar.
//
//	1. Prometheus exporter — counts pubsub events per channel pattern,
//	   emits gauge for last-seen-age. Used by Grafana on 루이스.
//	2. SSE bridge      — exposes /stream/<channel> as Server-Sent Events
//	   so a browser/사이트 frontend can subscribe without Redis client.
//	3. /healthz health check covering Redis + DART PG + downstream
//	   judge-engine gRPC.
package main

import (
	"context"
	"flag"
	"log"
	"net/http"
	"sync/atomic"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/redis/go-redis/v9"
)

var (
	listen      = flag.String("listen", "127.0.0.1:9099", "HTTP listen addr")
	redisAddr   = flag.String("redis", "127.0.0.1:6379", "Redis address")
	patterns    = flag.String("patterns", "trade.*,book.*,signal.*,model.*", "Comma list of PSUBSCRIBE patterns")
	channelHint = flag.String("channel-hint", "trade.binance.btcusdt", "Healthz wants to see ≥1 msg from this channel within window")
	healthMaxAge = flag.Duration("health-max-age", 60*time.Second, "Max age before /healthz fails")
)

var (
	pubsubMessages = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "lqc_pubsub_messages_total",
			Help: "Pubsub messages observed by lqc-gateway, partitioned by channel.",
		},
		[]string{"channel"},
	)
	pubsubLastSeenSec = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "lqc_pubsub_last_seen_seconds",
			Help: "Seconds since lqc-gateway last saw a message on each channel.",
		},
		[]string{"channel"},
	)
	lastChannelHintNs atomic.Int64
)

func init() {
	prometheus.MustRegister(pubsubMessages, pubsubLastSeenSec)
}

func main() {
	flag.Parse()
	rdb := redis.NewClient(&redis.Options{Addr: *redisAddr})
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go subscribe(ctx, rdb)

	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())
	mux.HandleFunc("/healthz", healthz(rdb))
	mux.HandleFunc("/stream/", sseBridge(rdb))

	log.Printf("lqc-gateway listening on %s; redis=%s patterns=%s",
		*listen, *redisAddr, *patterns)
	srv := &http.Server{
		Addr:              *listen,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	if err := srv.ListenAndServe(); err != nil {
		log.Fatal(err)
	}
}
