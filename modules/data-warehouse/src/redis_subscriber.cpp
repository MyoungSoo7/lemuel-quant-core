#include "dwh/redis_subscriber.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#ifdef LQC_HAS_HIREDIS
#include <hiredis/hiredis.h>
#endif

namespace dwh {

RedisSubscriber::RedisSubscriber(std::string host, int port,
                                 std::vector<std::string> patterns)
    : host_(std::move(host)), port_(port), patterns_(std::move(patterns)) {}

RedisSubscriber::~RedisSubscriber() { stop(); }

void RedisSubscriber::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this] { run_loop(); });
}

void RedisSubscriber::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

#ifdef LQC_HAS_HIREDIS

void RedisSubscriber::run_loop() {
    while (running_.load()) {
        timeval tv{2, 0};
        redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, tv);
        if (!ctx || ctx->err) {
            if (ctx) redisFree(ctx);
            std::cerr << "[dwh-sub] redis connect failed; retry in 2s\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        for (const auto& p : patterns_) {
            auto* r = static_cast<redisReply*>(
                redisCommand(ctx, "PSUBSCRIBE %s", p.c_str()));
            if (r) freeReplyObject(r);
        }

        redisReply* reply = nullptr;
        while (running_.load() &&
               redisGetReply(ctx, reinterpret_cast<void**>(&reply)) == REDIS_OK) {
            // pmessage reply: [pmessage, pattern, channel, payload]
            if (reply && reply->type == REDIS_REPLY_ARRAY &&
                reply->elements >= 4) {
                const std::string kind(reply->element[0]->str,
                                        reply->element[0]->len);
                if (kind == "pmessage" && on_message_) {
                    on_message_(
                        std::string(reply->element[2]->str,
                                     reply->element[2]->len),
                        std::string(reply->element[3]->str,
                                     reply->element[3]->len));
                }
            }
            if (reply) { freeReplyObject(reply); reply = nullptr; }
        }
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        if (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

#else

void RedisSubscriber::run_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

#endif

}  // namespace dwh
