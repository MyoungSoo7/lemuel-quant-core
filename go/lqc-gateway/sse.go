package main

import (
	"fmt"
	"net/http"
	"strings"

	"github.com/redis/go-redis/v9"
)

// /stream/<channel> — Server-Sent Events bridge. Browser opens an EventSource
// to this endpoint and receives raw payloads as SSE `data:` lines.
func sseBridge(_ *redis.Client) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		channel := strings.TrimPrefix(r.URL.Path, "/stream/")
		if channel == "" || strings.Contains(channel, "..") {
			http.Error(w, "bad channel", http.StatusBadRequest)
			return
		}
		flusher, ok := w.(http.Flusher)
		if !ok {
			http.Error(w, "streaming unsupported", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "text/event-stream")
		w.Header().Set("Cache-Control", "no-cache")
		w.Header().Set("Connection", "keep-alive")
		w.Header().Set("X-Accel-Buffering", "no")

		c := hub.attach(channel)
		defer hub.detach(channel, c)

		fmt.Fprintf(w, ": connected to %s\n\n", channel)
		flusher.Flush()

		ctx := r.Context()
		for {
			select {
			case <-ctx.Done():
				return
			case payload, alive := <-c:
				if !alive {
					return
				}
				fmt.Fprintf(w, "data: %s\n\n", payload)
				flusher.Flush()
			}
		}
	}
}
