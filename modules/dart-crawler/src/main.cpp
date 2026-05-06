#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "dart/dart_client.hpp"
#include "dart/disclosure_store.hpp"

namespace {
std::atomic<bool> g_run{true};
void on_sig(int) { g_run = false; }
}  // namespace

int main() {
    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    const char* key = std::getenv("DART_API_KEY");
    if (!key || !*key) {
        std::cerr << "DART_API_KEY 환경변수 필요. https://opendart.fss.or.kr 에서 발급.\n";
        return 2;
    }

    dart::DartClient client(key);
    std::unique_ptr<dart::DisclosureStore> store;
    if (const char* dsn = std::getenv("LQC_PG_DSN"); dsn && *dsn) {
        store = dart::DisclosureStore::make_postgres(dsn);
    }
    if (!store) store = dart::DisclosureStore::make_in_memory();

    client.poll_loop(
        std::chrono::seconds(60),
        [&](const dart::Disclosure& d) {
            if (store->insert(d)) {
                std::cout << "[NEW] " << d.rcept_no << " " << d.corp_name
                          << " — " << d.report_nm << "\n";
            }
        },
        [] { return g_run.load(); });
    return 0;
}
