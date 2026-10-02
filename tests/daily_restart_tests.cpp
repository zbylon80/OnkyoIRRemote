#include <atomic>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include "VolumeHoldSafety.h"
#include "DailyRestart.h"
#include "FirmwareVersion.h"

// The hardware-facing functions below are extracted from the actual sketch.
// No network, flash, IR or physical restart is performed by these tests.
#define F(value) value
uint64_t fakeUptime = 0;
time_t fakeEpoch = 1774746000;
tm fakeLocal{};
bool clockConversionWorks = true;
std::vector<std::string> events;
int resetCount = 0;
int writeCount = 0;
int64_t esp_timer_get_time() { return static_cast<int64_t>(fakeUptime * 1000); }
time_t fakeTime(time_t *) { return fakeEpoch; }
tm *localtime_r(const time_t *, tm *result) {
  if (!clockConversionWorks) return nullptr;
  *result = fakeLocal;
  return result;
}
struct timeval { time_t tv_sec; long tv_usec; };
struct FakeStorage {
  uint32_t savedDay = 0;
  size_t writeBytes = 4;
  size_t putUInt(const char *key, uint32_t day) {
    assert(std::string(key) == "restartDay");
    ++writeCount;
    events.push_back("write");
    if (writeBytes == 4) savedDay = day;
    return writeBytes;
  }
} restartPreferences;
struct FakeSerial {
  void print(const char *) {}
  void println(const char *) { events.push_back("log"); }
  template<typename... Args> void printf(const char *, Args...) { events.push_back("log"); }
  void flush() { events.push_back("flush"); }
} Serial;
struct FakeESP { void restart() { ++resetCount; events.push_back("restart"); } } ESP;
constexpr int IR_SEND_PIN = 26, LOW = 0;
void digitalWrite(int, int) { events.push_back("low"); }
using String = std::string;
struct FakeServer {
  std::map<std::string, std::string> args;
  int status = 0;
  std::string body;
  bool hasArg(const char *key) { return args.count(key) != 0; }
  String arg(const char *key) { return args.count(key) ? args.at(key) : ""; }
  void send(int code, const char *, const char *value) { status = code; body = value; }
  void send_P(int, const char *, const char *) {}
  void sendHeader(const char *, const char *) {}
} server;
const char *INDEX_PAGE = "", *PWA_MANIFEST = "", *PWA_ICON = "";
using ota_error_t = int;
const char *OTA_HOSTNAME = "test", *OTA_PASSWORD = "test";
constexpr uint16_t OTA_TIMEOUT_SECONDS = 60;
struct FakeOTA {
  std::function<void()> started, ended;
  std::function<void(ota_error_t)> failed;
  void setHostname(const char *) {}
  void setPassword(const char *) {}
  void setTimeout(uint16_t) {}
  void onStart(std::function<void()> fn) { started = fn; }
  void onEnd(std::function<void()> fn) { ended = fn; }
  void onError(std::function<void(ota_error_t)> fn) { failed = fn; }
  void begin() {}
} ArduinoOTA;
void (*syncCallback)(timeval *) = nullptr;
void sntp_set_time_sync_notification_cb(void (*cb)(timeval *)) { syncCallback = cb; }
void configTzTime(const char *zone, const char *, const char *) { assert(std::string(zone) == RESTART_TIME_ZONE); }
DailyRestartPolicy dailyRestart;
VolumeHoldSafety volumeSafety;
const char *preparedVolumeCommand = nullptr;
constexpr unsigned long VOLUME_WATCHDOG_MS = 2000, VOLUME_REPEAT_INTERVAL_MS = 110;
unsigned long lastVolumeSendMs = 0;
bool irTransmissionEnabled = true;
const char *heldVolumeCommand = nullptr, *learningCommand = nullptr;
int irCommands = 0;
bool sendOnkyoCommand(const String &) {
  if (!irTransmissionEnabled || learningCommand != nullptr) return false;
  ++irCommands;
  dailyRestart.recordActivity(fakeUptime);
  return true;
}
std::atomic<bool> timeSynchronized{false};
bool restartStorageReady = true, otaInProgress = false;
uint64_t lastRestartCheckMs = 0;
unsigned long lastVolumeSignalMs = 0;
unsigned long millis() { return static_cast<unsigned long>(fakeUptime); }
#define time fakeTime
#include "daily_restart_under_test.inc"
#undef time

tm local(int hour, int minute = 0, int second = 0, int day = 2, int month = 10) {
  tm value{};
  value.tm_year = 126; value.tm_mon = month - 1; value.tm_mday = day;
  value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second;
  return value;
}
void resetFixture() {
  fakeUptime = 0; fakeLocal = local(3); clockConversionWorks = true;
  events.clear(); resetCount = 0; writeCount = 0;
  restartPreferences = FakeStorage{};
  restartStorageReady = true; otaInProgress = false; timeSynchronized.store(true);
  lastRestartCheckMs = 0; heldVolumeCommand = nullptr; learningCommand = nullptr;
  dailyRestart.begin(0, 0);
  volumeSafety.begin(0x1234);
  preparedVolumeCommand = nullptr; irCommands = 0; irTransmissionEnabled = true;
  lastVolumeSignalMs = lastVolumeSendMs = 0;
  server.args.clear(); server.status = 0; server.body.clear();
}

uint64_t press(const char *direction) {
  server.args = {{"direction", direction}};
  handleVolumePress();
  assert(server.status == 200);
  const std::string marker = "\"session\":\"";
  const size_t from = server.body.find(marker) + marker.size();
  return std::stoull(server.body.substr(from, server.body.find('"', from) - from));
}
void start(uint64_t session) {
  server.args = {{"session", std::to_string(session)}};
  handleVolumeStart();
}

int main() {
  DailyRestartPolicy policy;
  policy.begin(0, 0);
  tm at3 = local(3), before3 = local(2, 59, 59), before5 = local(4, 59, 59), at5 = local(5);
  assert(policy.dueDay(RESTART_IDLE_MS - 1, &at3, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &at3, true, false) == 20261002);
  assert(policy.dueDay(RESTART_IDLE_MS, &before3, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &before5, true, false) == 20261002);
  assert(policy.dueDay(RESTART_IDLE_MS, &at5, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &at3, false, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, nullptr, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &at3, true, true) == 0);

  // 02:50 use postpones a 03:00 restart until 03:20, never into daytime.
  policy.begin(0, 0);
  policy.recordActivity(50ULL * 60 * 1000);
  assert(policy.dueDay(60ULL * 60 * 1000, &at3, true, false) == 0);
  tm at320 = local(3, 20);
  assert(policy.dueDay(80ULL * 60 * 1000 - 1, &at320, true, false) == 0);
  assert(policy.dueDay(80ULL * 60 * 1000, &at320, true, false) == 20261002);
  policy.recordActivity(175ULL * 60 * 1000); // 04:55
  assert(policy.dueDay(210ULL * 60 * 1000, &at5, true, false) == 0);

  // Persistent calendar guard, including clock moving backwards.
  policy.markRestart(20261002);
  policy.begin(0, 20261002);
  assert(policy.dueDay(RESTART_IDLE_MS, &at3, true, false) == 0);
  tm yesterday = local(3, 0, 0, 1), tomorrow = local(3, 0, 0, 3);
  assert(policy.dueDay(RESTART_IDLE_MS, &yesterday, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &tomorrow, true, false) == 20261003);

  // Monotonic durations still work beyond the 32-bit millis() rollover.
  const uint64_t longUptime = (1ULL << 32) + 123;
  policy.begin(longUptime, 0);
  policy.recordActivity(longUptime + 10);
  assert(policy.dueDay(longUptime + 10 + RESTART_IDLE_MS, &at3, true, false) == 20261002);
  assert(policy.dueDay(longUptime - 1, &at3, true, false) == 0);
  tm spring = local(3, 0, 0, 29, 3), autumn = local(3, 0, 0, 25, 10);
  policy.begin(0, 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &spring, true, false) == 20260329);
  policy.markRestart(20260329);
  assert(policy.dueDay(RESTART_IDLE_MS, &spring, true, false) == 0);
  assert(policy.dueDay(RESTART_IDLE_MS, &autumn, true, false) == 20261025);

  resetFixture(); fakeUptime = RESTART_IDLE_MS;
  handleDailyRestart();
  assert(resetCount == 1 && writeCount == 1 && restartPreferences.savedDay == 20261002);
  assert(events.front() == "write" && events.back() == "restart");
  fakeUptime += 1000; handleDailyRestart();
  assert(resetCount == 1 && writeCount == 1);
  dailyRestart.begin(fakeUptime, restartPreferences.savedDay);
  fakeUptime += RESTART_IDLE_MS; handleDailyRestart();
  assert(resetCount == 1); // Simulated reboot reads the saved day.
  fakeLocal = local(3, 0, 0, 3); fakeUptime += 1000; handleDailyRestart();
  assert(resetCount == 2);

  resetFixture(); restartPreferences.writeBytes = 0; fakeUptime = RESTART_IDLE_MS;
  handleDailyRestart();
  assert(resetCount == 0 && writeCount == 1 && !restartStorageReady);
  fakeUptime += 1000; handleDailyRestart(); assert(writeCount == 1);

  for (int reason = 0; reason < 5; ++reason) {
    resetFixture(); fakeUptime = RESTART_IDLE_MS;
    if (reason == 0) otaInProgress = true;
    if (reason == 1) learningCommand = "POWER";
    if (reason == 2) heldVolumeCommand = "VOL+";
    if (reason == 3) timeSynchronized.store(false);
    if (reason == 4) clockConversionWorks = false;
    handleDailyRestart(); assert(resetCount == 0 && writeCount == 0);
  }

  resetFixture(); startTimeSynchronization(); assert(syncCallback != nullptr);
  timeval invalid{0, 0}, valid{fakeEpoch, 0};
  syncCallback(&invalid); assert(!timeSynchronized.load());
  syncCallback(&valid); assert(timeSynchronized.load());
  syncCallback(nullptr); assert(!timeSynchronized.load());

  resetFixture(); startOta(); fakeUptime = RESTART_IDLE_MS;
  ArduinoOTA.started(); assert(otaInProgress);
  handleDailyRestart(); assert(resetCount == 0);
  fakeUptime += 1000; ArduinoOTA.failed(1); assert(!otaInProgress);
  handleDailyRestart(); assert(resetCount == 0);
  fakeUptime += RESTART_IDLE_MS; handleDailyRestart(); assert(resetCount == 1);
  resetFixture(); startOta(); ArduinoOTA.started(); ArduinoOTA.ended(); assert(!otaInProgress);

  // Page/status reads and stray renewals do not count as control activity.
  resetFixture(); fakeUptime = RESTART_IDLE_MS - 1000;
  handleRoot(); handleVersion(); handleManifest(); handleIcon(); handleVolumeKeepalive();
  fakeUptime = RESTART_IDLE_MS; handleDailyRestart(); assert(resetCount == 1);
  resetFixture(); fakeUptime = RESTART_IDLE_MS - 1000;
  const uint64_t active = volumeSafety.prepare(false, static_cast<uint32_t>(fakeUptime));
  volumeSafety.start(active, static_cast<uint32_t>(fakeUptime)); heldVolumeCommand = "VOL-";
  lastVolumeSignalMs = static_cast<unsigned long>(fakeUptime);
  server.args = {{"session", std::to_string(active)}};
  handleVolumeKeepalive(); handleVolumeStop();
  fakeUptime = RESTART_IDLE_MS; handleDailyRestart(); assert(resetCount == 0);
  fakeUptime += RESTART_IDLE_MS; handleDailyRestart(); assert(resetCount == 1);
  // A short press sends exactly one command and never arms repetition.
  resetFixture();
  uint64_t first = press("up");
  assert(irCommands == 1 && heldVolumeCommand == nullptr);
  fakeUptime = 1000; repeatHeldVolume(); assert(irCommands == 1);
  // Stop arriving before start permanently closes that session.
  server.args = {{"session", std::to_string(first)}};
  handleVolumeStop(); start(first);
  assert(server.status == 409 && heldVolumeCommand == nullptr && irCommands == 1);

  // Frequent renewals and duplicate starts cannot extend the two-second cap.
  resetFixture(); first = press("up"); fakeUptime = 350; start(first);
  assert(server.status == 200 && irCommands == 1);
  for (fakeUptime = 500; fakeUptime < 2000; fakeUptime += 150) {
    handleVolumeKeepalive(); repeatHeldVolume(); assert(heldVolumeCommand != nullptr);
  }
  fakeUptime = 1999; start(first); assert(server.status == 200);
  const int beforeLimit = irCommands;
  fakeUptime = 2000; handleVolumeKeepalive(); repeatHeldVolume();
  assert(server.status == 409 && heldVolumeCommand == nullptr && irCommands == beforeLimit);
  fakeUptime = 2100; start(first); assert(server.status == 409);
  // A late first start also counts from the initial press, not from its arrival.
  resetFixture(); first = press("up"); fakeUptime = 2000; start(first);
  assert(server.status == 409 && heldVolumeCommand == nullptr && irCommands == 1);

  // An old stop/renew/start cannot stop, sustain or replace a newer hold.
  resetFixture(); first = press("up"); start(first);
  uint64_t second = press("down"); start(second);
  const unsigned long signal = lastVolumeSignalMs;
  fakeUptime = 100;
  server.args = {{"session", std::to_string(first)}};
  handleVolumeStop(); assert(std::string(heldVolumeCommand) == "VOL-");
  handleVolumeKeepalive(); assert(server.status == 409 && lastVolumeSignalMs == signal);
  start(first); assert(server.status == 409 && std::string(heldVolumeCommand) == "VOL-");
  start(second); assert(server.status == 200);

  // Down/tuning remain continuous with renewals, with the disconnect watchdog.
  for (const char *direction : {"down", "tuning-up", "tuning-down"}) {
    resetFixture(); first = press(direction); start(first);
    for (fakeUptime = 150; fakeUptime <= 4500; fakeUptime += 150) {
      handleVolumeKeepalive(); repeatHeldVolume(); assert(heldVolumeCommand != nullptr);
    }
    fakeUptime = 6501; // No renewal for more than two seconds.
    handleVolumeKeepalive();
    assert(server.status == 409 && heldVolumeCommand == nullptr);
    const int atDisconnect = irCommands;
    repeatHeldVolume(); assert(irCommands == atDisconnect);
    start(first); assert(server.status == 409);
  }

  // Closed/pending sessions and tokens from an earlier boot cannot be reused.
  resetFixture(); first = press("up"); stopVolume(); start(first);
  assert(server.status == 409);
  volumeSafety.begin(0x4321); second = press("down"); start(second);
  start(first); assert(server.status == 409 && std::string(heldVolumeCommand) == "VOL-");
  for (const char *invalid : {"", "-1", "+1", "123x", "18446744073709551616", "0"}) {
    server.args = {{"session", invalid}};
    assert(readVolumeSession() == 0);
    handleVolumeStart(); assert(server.status == 409);
  }

  // Duration arithmetic handles the 32-bit millis() rollover.
  VolumeHoldSafety safety;
  safety.begin(42);
  const uint64_t wrapped = safety.prepare(true, UINT32_MAX - 500);
  assert(safety.start(wrapped, UINT32_MAX - 100) == VolumeHoldSafety::StartResult::Started);
  assert(!safety.reachedLimit(1498)); assert(safety.reachedLimit(1499));
  safety.closeAll(); assert(safety.start(wrapped, 1600) == VolumeHoldSafety::StartResult::Rejected);

  resetFixture(); irTransmissionEnabled = false;
  server.args = {{"direction", "up"}}; handleVolumePress();
  assert(server.status == 400 && irCommands == 0 && heldVolumeCommand == nullptr);
  std::cout << "Daily restart and volume safety integration tests passed (no hardware actions).\n";
}
