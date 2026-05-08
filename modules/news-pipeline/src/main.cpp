#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "news/news_score.hpp"
#include "news/ner.hpp"
#include "news/rss_crawler.hpp"
#include "news/sentiment.hpp"

namespace {
std::atomic<bool> g_run{true};
void on_sig(int) { g_run = false; }
}  // namespace

int main() {
    // systemd 가 stdout 을 파이프로 가져갈 때 fully-buffered → 로그 늦게 / 안 나옴.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    news::RssCrawler crawler(news::default_korean_sources());
    news::TickerNer  ner;
    if (const char* path = std::getenv("LQC_NEWS_TICKERS")) {
        ner.load_csv(path);
    } else {
        // Seed: a few KOSPI heavyweights so the binary works without a CSV.
        ner.add_company("삼성전자",   "005930");
        ner.add_company("SK하이닉스", "000660");
        ner.add_company("LG에너지솔루션", "373220");
        ner.add_company("현대차", "005380");
    }

    auto scorer = news::SentimentScorer::make_default(
        std::getenv("LQC_FINBERT_DIR") ? std::getenv("LQC_FINBERT_DIR") : "");
    news::NewsScoreBoard board;

    while (g_run) {
        for (auto art : crawler.fetch()) {
            crawler.hydrate_body(art);
            news::ScoredArticle sa;
            sa.article  = std::move(art);
            sa.mentions = ner.tag(sa.article.body);
            const auto s = scorer->score(sa.article.title + "\n" +
                                          sa.article.body);
            sa.sentiment            = s.label;
            sa.sentiment_confidence = s.confidence;
            board.ingest(sa);
        }
        const auto snap = board.snapshot(std::chrono::system_clock::now());
        std::cout << "[cycle] articles=" << /* not tracked */ 0
                  << " snapshot_size=" << snap.size() << std::endl;
        for (const auto& [ticker, sc] : snap) {
            std::cout << "[news-score] " << ticker << " = " << sc << std::endl;
        }
        std::cout << "[sleep] 5min" << std::endl;
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }
    return 0;
}
