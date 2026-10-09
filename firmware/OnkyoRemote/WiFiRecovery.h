#pragma once

#include <stdint.h>

constexpr uint64_t WIFI_RETRY_MS = 15000;
constexpr uint64_t WIFI_RADIO_RESET_MS = 60000;
constexpr uint32_t LOOP_WATCHDOG_MS = 15000;

// Monotonic, nonblocking recovery. An unavailable router must not reboot the
// remote repeatedly or discard its synchronized clock and pending alarms.
class WiFiRecovery {
public:
  enum class Action { None, Connected, Retry, ResetRadio };

  void begin(uint64_t nowMs) {
    connected_ = false;
    lastAttemptMs_ = lastRadioResetMs_ = nowMs;
    retries = radioResets = 0;
  }

  void interrupted(uint64_t nowMs) {
    if (connected_) lastAttemptMs_ = lastRadioResetMs_ = nowMs;
    connected_ = false;
  }

  Action update(uint64_t nowMs, bool connected, bool otaBusy) {
    if (connected) {
      if (connected_) return Action::None;
      connected_ = true;
      return Action::Connected;
    }
    interrupted(nowMs);
    if (otaBusy) {
      lastAttemptMs_ = lastRadioResetMs_ = nowMs;
      return Action::None;
    }
    if (nowMs - lastRadioResetMs_ >= WIFI_RADIO_RESET_MS) {
      lastAttemptMs_ = lastRadioResetMs_ = nowMs;
      ++radioResets;
      return Action::ResetRadio;
    }
    if (nowMs - lastAttemptMs_ >= WIFI_RETRY_MS) {
      lastAttemptMs_ = nowMs;
      ++retries;
      return Action::Retry;
    }
    return Action::None;
  }

  uint32_t retries = 0;
  uint32_t radioResets = 0;

private:
  bool connected_ = false;
  uint64_t lastAttemptMs_ = 0;
  uint64_t lastRadioResetMs_ = 0;
};
