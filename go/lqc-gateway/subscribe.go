package main

import (
	"context"
	"strings"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
)

// fan-out hub for the SSE bridge. Each /stream/<channel> request adds a
// goroutine + channel; the subscribe loop pushes inbound messages here.
type fanout struct {
	mu   sync.RWMutex
	subs map[string]map[chan string]struct{} // channel name → set of subscribers
}

var hub = &fanout{subs: map[string]map[chan string]struct{}{}}

func (f *fanout) attach(ch string) chan string {
	f.mu.Lock()
	defer f.mu.Unlock()
	c := make(chan string, 64)
	if _, ok := f.subs[ch]; !ok {
		f.subs[ch] = map[chan string]struct{}{}
	}
	f.subs[ch][c] = struct{}{}
	return c
}

func (f *fanout) detach(ch string, c chan string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if set, ok := f.subs[ch]; ok {
		delete(set, c)
		if len(set) == 0 {
			delete(f.subs, ch)
		}
	}
	close(c)
}

func (f *fanout) deliver(ch, payload string) {
	f.mu.RLock()
	defer f.mu.RUnlock()
	for c := range f.subs[ch] {
		select {
		case c <- payload:
		default: // slow consumer — drop
		}
	}
}

func subscribe(ctx context.Context, rdb *redis.Client) {
	pats := strings.Split(*patterns, ",")
	for i := range pats {
		pats[i] = strings.TrimSpace(pats[i])
	}
	pubsub := rdb.PSubscribe(ctx, pats...)
	defer pubsub.Close()

	go markStaleness(ctx)

	for msg := range pubsub.Channel() {
		pubsubMessages.WithLabelValues(msg.Channel).Inc()
		lastSeen.Store(msg.Channel, time.Now().UnixNano())
		if msg.Channel == *channelHint {
			lastChannelHintNs.Store(time.Now().UnixNano())
		}
		hub.deliver(msg.Channel, msg.Payload)
	}
}

// per-channel last seen ts; sync.Map for lock-free reads on the gauge update.
var lastSeen sync.Map

func markStaleness(ctx context.Context) {
	t := time.NewTicker(5 * time.Second)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case now := <-t.C:
			lastSeen.Range(func(k, v any) bool {
				ch := k.(string)
				ts := v.(int64)
				age := float64(now.UnixNano()-ts) / 1e9
				pubsubLastSeenSec.WithLabelValues(ch).Set(age)
				return true
			})
		}
	}
}
