#pragma once

#include <chrono>
#include <string>

namespace stock {

// Korea Exchange (KRX) trading session windows. All times KST.
//
//   Pre-open auction: 08:30 - 09:00
//   Regular session : 09:00 - 15:30
//   Closing auction : 15:20 - 15:30 (overlaps regular)
//   After-hours     : 15:40 - 18:00 (single-price + 시간외 단일가)
//
// Holidays come from the KRX holiday calendar; the skeleton ships a
// hard-coded weekend check + an injectable holiday set for tests.
struct KrxSession {
    enum class State {
        Closed,
        PreOpen,
        Regular,
        Closing,
        AfterHours,
    };

    State state_at(std::chrono::system_clock::time_point t_utc) const;

    bool is_market_day(std::chrono::system_clock::time_point t_utc) const;

    // For schedulers: returns ms until the next state change.
    std::chrono::milliseconds time_until_next(
        std::chrono::system_clock::time_point t_utc) const;
};

const char* to_string(KrxSession::State s);

}  // namespace stock
