#pragma once

#include <stdint.h>
#include <time.h>

// Europe/Warsaw: CET in winter, CEST from the last Sunday in March until
// the last Sunday in October. POSIX offsets have the opposite sign to UTC.
constexpr char RESTART_TIME_ZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";
constexpr uint64_t RESTART_IDLE_MS = 30ULL * 60 * 1000;
constexpr int RESTART_WINDOW_START_HOUR = 3;
constexpr int RESTART_WINDOW_END_HOUR = 5;

// Clock conversion, persistence and the actual reset stay in the sketch.
// All duration checks use the monotonic 64-bit uptime, not NTP wall time.
class DailyRestartPolicy {
public:
  void begin(uint64_t nowMs, uint32_t lastRestartDay) {
    bootMs_ = nowMs;
    lastActivityMs_ = nowMs;
    lastRestartDay_ = lastRestartDay;
  }

  void recordActivity(uint64_t nowMs) { lastActivityMs_ = nowMs; }

  uint32_t dueDay(uint64_t nowMs, const tm *localTime, bool timeSynchronized, bool busy) const {
    if (!timeSynchronized || localTime == nullptr || busy) return 0;
    if (localTime->tm_year < 124 || localTime->tm_mon < 0 || localTime->tm_mon > 11 ||
        localTime->tm_mday < 1 || localTime->tm_mday > 31) return 0;
    if (localTime->tm_hour < RESTART_WINDOW_START_HOUR ||
        localTime->tm_hour >= RESTART_WINDOW_END_HOUR) return 0;
    if (nowMs < bootMs_ || nowMs < lastActivityMs_ ||
        nowMs - bootMs_ < RESTART_IDLE_MS || nowMs - lastActivityMs_ < RESTART_IDLE_MS) return 0;
    const uint32_t day = static_cast<uint32_t>(localTime->tm_year + 1900) * 10000 +
                         static_cast<uint32_t>(localTime->tm_mon + 1) * 100 +
                         static_cast<uint32_t>(localTime->tm_mday);
    // Reject repeated dates and clock corrections into a previously handled day.
    return day > lastRestartDay_ ? day : 0;
  }

  void markRestart(uint32_t day) { lastRestartDay_ = day; }

private:
  uint64_t bootMs_ = 0;
  uint64_t lastActivityMs_ = 0;
  uint32_t lastRestartDay_ = 0;
};
