#pragma once
#include <stdint.h>
#include <stddef.h>

constexpr uint32_t EVENT_LOG_CAPACITY = 48;
constexpr uint64_t LOG_CHECKPOINT_MS = 5ULL * 60 * 1000;
constexpr uint64_t LOG_IMPORTANT_INTERVAL_MS = 60ULL * 1000;
enum class LogEvent : uint8_t {
  Boot, WatchdogReady, WatchdogFailed, WiFiConnected, WiFiDisconnected,
  WiFiRetry, RadioReset, OtaStart, OtaEnd, OtaError, ManualRestart,
  DailyRestart, ClockReady, Health, SlowLoop, StorageError
};
inline const char *logEventName(LogEvent event) {
  switch (event) {
    case LogEvent::Boot: return "boot";
    case LogEvent::WatchdogReady: return "watchdog_ready";
    case LogEvent::WatchdogFailed: return "watchdog_failed";
    case LogEvent::WiFiConnected: return "wifi_connected";
    case LogEvent::WiFiDisconnected: return "wifi_disconnected";
    case LogEvent::WiFiRetry: return "wifi_retry";
    case LogEvent::RadioReset: return "radio_reset";
    case LogEvent::OtaStart: return "ota_start";
    case LogEvent::OtaEnd: return "ota_end";
    case LogEvent::OtaError: return "ota_error";
    case LogEvent::ManualRestart: return "manual_restart";
    case LogEvent::DailyRestart: return "daily_restart";
    case LogEvent::ClockReady: return "clock_ready";
    case LogEvent::Health: return "health";
    case LogEvent::SlowLoop: return "slow_loop";
    case LogEvent::StorageError: return "storage_error";
  }
  return "unknown";
}
// Fixed-size records, no user-supplied strings. Only the Arduino task writes;
// network callbacks leave atomic diagnostics for it to consume.
struct LogEntry {
  uint32_t boot = 0, uptimeSeconds = 0, epochSeconds = 0;
  uint32_t freeBytes = 0, largestBlockBytes = 0, loopMs = 0;
  int32_t detail = 0;
  int16_t rssiDbm = 0; // Zero means no connected Wi-Fi measurement.
  LogEvent event = LogEvent::Boot;
  uint8_t reserved = 0;
};
static_assert(sizeof(LogEntry) == 32, "Stable flash log record layout");
struct LogSnapshot {
  uint32_t magic = 0x4F4E4B4C, schema = 1, boot = 0;
  uint32_t next = 0, count = 0, checksum = 0;
  LogEntry entries[EVENT_LOG_CAPACITY]{};
};
class EventLog {
public:
  bool restore(const LogSnapshot &saved) {
    if (saved.magic != 0x4F4E4B4C || saved.schema != 1 ||
        saved.next >= EVENT_LOG_CAPACITY || saved.count > EVENT_LOG_CAPACITY ||
        saved.checksum != checksum(saved)) return false;
    for (const auto &entry : saved.entries) {
      if (static_cast<uint8_t>(entry.event) > static_cast<uint8_t>(LogEvent::StorageError)) return false;
    }
    data_ = saved;
    return true;
  }
  void beginBoot(uint64_t nowMs) {
    ++data_.boot;
    if (data_.boot == 0) data_.boot = 1;
    lastAttemptMs_ = lastSavedMs_ = nowMs;
    dirty_ = important_ = false;
  }
  void append(LogEntry entry, bool important) {
    entry.boot = data_.boot;
    data_.entries[data_.next] = entry;
    data_.next = (data_.next + 1) % EVENT_LOG_CAPACITY;
    if (data_.count < EVENT_LOG_CAPACITY) ++data_.count;
    dirty_ = true; important_ |= important;
  }
  const LogEntry &at(uint32_t index) const {
    const uint32_t oldest = (data_.next + EVENT_LOG_CAPACITY - data_.count) % EVENT_LOG_CAPACITY;
    return data_.entries[(oldest + index) % EVENT_LOG_CAPACITY];
  }
  LogSnapshot snapshot() const {
    LogSnapshot result = data_; result.checksum = checksum(result); return result;
  }
  bool shouldSave(uint64_t nowMs, bool force, bool busy) const {
    if (busy || !dirty_) return false;
    if (force) return true;
    return nowMs - lastAttemptMs_ >= (important_ ? LOG_IMPORTANT_INTERVAL_MS : LOG_CHECKPOINT_MS);
  }
  void saveResult(uint64_t nowMs, bool success) {
    lastAttemptMs_ = nowMs; // No rapid retries on storage failure.
    if (success) { dirty_ = important_ = false; lastSavedMs_ = nowMs; }
  }
  uint32_t count() const { return data_.count; }
  uint32_t boot() const { return data_.boot; }
  bool pending() const { return dirty_; }
  uint64_t lastSavedMs() const { return lastSavedMs_; }
private:
  static uint32_t checksum(const LogSnapshot &data) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&data);
    uint32_t value = 2166136261U;
    for (size_t i = 0; i < sizeof(data); ++i) {
      if (i >= offsetof(LogSnapshot, checksum) && i < offsetof(LogSnapshot, checksum) + sizeof(data.checksum)) continue;
      value = (value ^ bytes[i]) * 16777619U;
    }
    return value;
  }
  LogSnapshot data_{};
  uint64_t lastAttemptMs_ = 0, lastSavedMs_ = 0;
  bool dirty_ = false, important_ = false;
};
