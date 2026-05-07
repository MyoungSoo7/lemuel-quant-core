package main

import (
	"context"
	"encoding/json"
	"net/http"
	"time"

	"github.com/redis/go-redis/v9"
)

func healthz(rdb *redis.Client) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		out := map[string]any{}
		ctx, cancel := context.WithTimeout(r.Context(), 2*time.Second)
		defer cancel()

		// Redis ping
		out["redis_ok"] = rdb.Ping(ctx).Err() == nil

		// Most-recent message age on the hint channel
		ts := lastChannelHintNs.Load()
		if ts == 0 {
			out["channel_hint_seen"] = false
			out["channel_hint_age_sec"] = -1
		} else {
			age := time.Since(time.Unix(0, ts))
			out["channel_hint_seen"] = true
			out["channel_hint_age_sec"] = age.Seconds()
			out["channel_hint_fresh"] = age < *healthMaxAge
		}

		ok := true
		if !(out["redis_ok"].(bool)) {
			ok = false
		}
		if seen, _ := out["channel_hint_seen"].(bool); seen {
			if fresh, _ := out["channel_hint_fresh"].(bool); !fresh {
				ok = false
			}
		}

		w.Header().Set("Content-Type", "application/json")
		if !ok {
			w.WriteHeader(http.StatusServiceUnavailable)
		}
		_ = json.NewEncoder(w).Encode(out)
	}
}
