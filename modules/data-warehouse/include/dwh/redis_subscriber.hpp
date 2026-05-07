#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace dwh {

// Subscribes to channels via Redis PSUBSCRIBE and dispatches each
// (channel, payload) pair to a callback. Designed for the warehouse
// ingest path: market-feed/news-pipeline publish JSON; warehouse
// converts to Row and stores.
class RedisSubscriber {
public:
    using Handler = std::function<void(const std::string& channel,
                                        const std::string& payload)>;

    RedisSubscriber(std::string host, int port,
                    std::vector<std::string> patterns);
    ~RedisSubscriber();

    void on_message(Handler h) { on_message_ = std::move(h); }

    void start();
    void stop();
    bool running() const { return running_.load(); }

private:
    void run_loop();

    std::string host_;
    int port_;
    std::vector<std::string> patterns_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    Handler on_message_;
};

}  // namespace dwh
