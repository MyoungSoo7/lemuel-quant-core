#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "stock/kis_client.hpp"
#include "stock/krx_session.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void on_sig(int) { g_stop = 1; }
}  // namespace

int main() {
    // systemd 가 stdout/stderr 를 pipe 로 캡처하면 fully-buffered 가 되어
    // std::cout 한 줄이 즉시 journal 에 안 보임 → unbuffered 강제.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    stock::KisClient::Options opts;
    if (const char* k = std::getenv("KIS_APP_KEY"))    opts.creds.app_key = k;
    if (const char* s = std::getenv("KIS_APP_SECRET")) opts.creds.app_secret = s;
    opts.creds.paper = (std::getenv("KIS_PAPER") != nullptr);
    opts.trade_symbols = {"005930", "000660"};   // 삼성전자, SK하이닉스
    opts.book_symbols  = {"005930", "000660"};   // 호가도 같이 구독

    stock::KrxSession sess;
    stock::KisClient client(opts);

    while (!g_stop) {
        const auto now = std::chrono::system_clock::now();
        const auto state = sess.state_at(now);
        const bool tradeable =
            state == stock::KrxSession::State::Regular ||
            state == stock::KrxSession::State::Closing ||
            state == stock::KrxSession::State::PreOpen ||
            state == stock::KrxSession::State::AfterHours;

        if (tradeable && !client.running()) {
            std::cout << "[stock-feed] " << stock::to_string(state)
                      << ": starting feed\n";
            client.start();
        } else if (!tradeable && client.running()) {
            std::cout << "[stock-feed] " << stock::to_string(state)
                      << ": stopping feed\n";
            client.stop();
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    client.stop();
    return 0;
}
