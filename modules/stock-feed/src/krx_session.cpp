#include "stock/krx_session.hpp"

#include <chrono>
#include <ctime>

namespace stock {

namespace {

// KST = UTC+9. Convert a UTC time_point to local KST {wday, h, m, s, date}.
struct KstClock {
    int year, mon, day, wday, h, m, s;
};

KstClock to_kst(std::chrono::system_clock::time_point t_utc) {
    const auto t = std::chrono::system_clock::to_time_t(t_utc) + 9 * 3600;
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    return {tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_wday,         tm.tm_hour,    tm.tm_min, tm.tm_sec};
}

int minutes_of_day(const KstClock& k) { return k.h * 60 + k.m; }

}  // namespace

bool KrxSession::is_market_day(std::chrono::system_clock::time_point t) const {
    const auto k = to_kst(t);
    if (k.wday == 0 || k.wday == 6) return false;   // Sun / Sat
    // TODO: KRX holiday calendar plug-in. For now weekends-only.
    return true;
}

KrxSession::State KrxSession::state_at(
    std::chrono::system_clock::time_point t) const {
    if (!is_market_day(t)) return State::Closed;
    const auto m = minutes_of_day(to_kst(t));
    if (m >= 8 * 60 + 30 && m < 9 * 60)            return State::PreOpen;
    if (m >= 9 * 60      && m < 15 * 60 + 20)      return State::Regular;
    if (m >= 15 * 60 + 20 && m < 15 * 60 + 30)     return State::Closing;
    if (m >= 15 * 60 + 40 && m < 18 * 60)          return State::AfterHours;
    return State::Closed;
}

std::chrono::milliseconds KrxSession::time_until_next(
    std::chrono::system_clock::time_point t) const {
    static const int boundaries[] = {
        8 * 60 + 30, 9 * 60, 15 * 60 + 20, 15 * 60 + 30, 15 * 60 + 40,
        18 * 60,
    };
    const auto k = to_kst(t);
    const int now_min = minutes_of_day(k);
    for (int b : boundaries) {
        if (b > now_min) {
            return std::chrono::minutes(b - now_min) -
                   std::chrono::seconds(k.s);
        }
    }
    // Wrap to next day's pre-open at 08:30.
    const int remaining = (24 * 60 - now_min) + (8 * 60 + 30);
    return std::chrono::minutes(remaining) - std::chrono::seconds(k.s);
}

const char* to_string(KrxSession::State s) {
    switch (s) {
        case KrxSession::State::Closed:     return "CLOSED";
        case KrxSession::State::PreOpen:    return "PREOPEN";
        case KrxSession::State::Regular:    return "REGULAR";
        case KrxSession::State::Closing:    return "CLOSING";
        case KrxSession::State::AfterHours: return "AFTERHOURS";
    }
    return "?";
}

}  // namespace stock
