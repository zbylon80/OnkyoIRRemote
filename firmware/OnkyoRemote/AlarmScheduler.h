#pragma once
#include <stdint.h>
#include <time.h>

// Persistent, versioned record. Both actions are POWER toggles; slot 1 also
// selects a source. Consume the one-shot action before transmission to prevent replay after reset.
constexpr uint32_t ALARM_STORAGE_VERSION = 2;
constexpr const char *ALARM_SOURCES[] = {"TUNER", "CD", "PHONO", "TAPE-1", "TAPE-2", "VIDEO-1", "VIDEO-2"};
constexpr uint8_t ALARM_SOURCE_COUNT = sizeof(ALARM_SOURCES) / sizeof(ALARM_SOURCES[0]);
struct AlarmSlot {
  uint8_t enabled = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t source = 0;
  uint32_t dueDay = 0;
};
struct AlarmSettings {
  uint32_t version = ALARM_STORAGE_VERSION;
  AlarmSlot slots[2] = {{0, 2, 0, 0, 0}, {0, 7, 0, 0, 0}};
};
inline bool validAlarms(const AlarmSettings &value) {
  if (value.version != ALARM_STORAGE_VERSION) return false;
  for (const auto &slot : value.slots)
    if (slot.enabled > 1 || slot.hour > 23 || slot.minute > 59 || slot.source >= ALARM_SOURCE_COUNT) return false;
  return !(value.slots[0].enabled && value.slots[1].enabled &&
           value.slots[0].hour == value.slots[1].hour && value.slots[0].minute == value.slots[1].minute &&
           value.slots[0].dueDay == value.slots[1].dueDay);
}
inline uint32_t alarmDay(const tm *local) {
  if (!local || local->tm_year < 124 || local->tm_mon < 0 || local->tm_mon > 11 ||
      local->tm_mday < 1 || local->tm_mday > 31 || local->tm_hour < 0 ||
      local->tm_hour > 23 || local->tm_min < 0 || local->tm_min > 59) return 0;
  return (local->tm_year + 1900) * 10000 + (local->tm_mon + 1) * 100 + local->tm_mday;
}

inline uint32_t nextAlarmDay(const tm *local, uint8_t hour, uint8_t minute) {
  if (!alarmDay(local) || hour > 23 || minute > 59) return 0;
  tm next = *local;
  if (hour * 60 + minute <= local->tm_hour * 60 + local->tm_min) ++next.tm_mday;
  // Normalize date at noon, away from DST gaps and repeated local hours.
  next.tm_hour = 12; next.tm_min = 0; next.tm_sec = 0; next.tm_isdst = -1;
  if (mktime(&next) == static_cast<time_t>(-1)) return 0;
  return alarmDay(&next);
}

inline bool upgradeAlarmSettings(AlarmSettings &saved) {
  if (saved.version == 1) {
    // v1 stored last execution dates here, not target dates. Keep inputs and
    // enabled state; plan active legacy actions once after NTP becomes ready.
    saved.version = ALARM_STORAGE_VERSION;
    for (auto &slot : saved.slots) slot.dueDay = 0;
  }
  return validAlarms(saved);
}

class AlarmScheduler {
public:
  AlarmSettings settings;
  bool storageReady = false;
  const char *result = "Brak wykonanej akcji od uruchomienia";

  bool pending() const { return sourcePending_; }
  void cancelSource() {
    if (sourcePending_) result = "Wybór źródła anulowany przez ręczne sterowanie lub OTA";
    sourcePending_ = false;
  }
  bool blocksRestart(const tm *local) const {
    if (sourcePending_) return true;
    const auto day = alarmDay(local);
    if (!day) return false;
    const int now = local->tm_hour * 60 + local->tm_min;
    for (const auto &slot : settings.slots) {
      const int until = slot.hour * 60 + slot.minute - now;
      if (slot.enabled && day == slot.dueDay && until >= 0 && until <= 2) return true;
    }
    return false;
  }
  template<class Persist, class Send>
  void tick(uint64_t nowMs, const tm *local, bool clockReady, bool blocked, Persist persist, Send send) {
    if (sourcePending_) {
      if (nowMs >= sourceAtMs_) {
        // Never replay a delayed source after OTA, learning or a long stall.
        if (!blocked && nowMs - sourceAtMs_ <= 1000)
          result = send(ALARM_SOURCES[source_]) ? "Pobudka: wysłano POWER i źródło" : "Pobudka: nie wysłano źródła";
        else result = "Pobudka: pominięto opóźniony wybór źródła";
        sourcePending_ = false;
      }
      return;
    }
    const uint32_t day = clockReady ? alarmDay(local) : 0;
    if (!day || !storageReady) return;
    for (uint8_t i = 0; i < 2; ++i) {
      const auto &slot = settings.slots[i];
      if (!slot.enabled || !slot.dueDay || day < slot.dueDay) continue;
      const int now = local->tm_hour * 60 + local->tm_min;
      const int target = slot.hour * 60 + slot.minute;
      if (day == slot.dueDay && now < target) continue;
      const bool missed = day > slot.dueDay || now > target;
      auto committed = settings;
      committed.slots[i].enabled = 0;
      if (!persist(committed)) {
        storageReady = false;
        result = "Harmonogram zatrzymany: błąd zapisu pamięci";
        return;
      }
      settings = committed;
      if (missed) { result = "Pominięto przegapioną akcję"; return; }
      // A busy alarm is skipped, not queued into a surprising later POWER.
      if (blocked) { result = "Pominięto akcję: trwa OTA, nauka lub przytrzymanie"; return; }
      if (!send("POWER")) { result = "Nie udało się wysłać POWER"; return; }
      if (i == 0) result = "Wyłączenie: wysłano POWER";
      else {
        source_ = slot.source;
        sourceAtMs_ = nowMs + 2000;
        sourcePending_ = true;
        result = "Pobudka: wysłano POWER, oczekiwanie na źródło";
      }
      return;
    }
  }
private:
  bool sourcePending_ = false;
  uint8_t source_ = 0;
  uint64_t sourceAtMs_ = 0;
};
