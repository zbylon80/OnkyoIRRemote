#include <ArduinoOTA.h>
#include <IRremote.hpp>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <errno.h>

#include "WiFiConfig.h"
#include "FirmwareVersion.h"
#include "DailyRestart.h"
#include "VolumeHoldSafety.h"
#include "VolumeControls.h"
#include "DeviceDiagnostics.h"
#include "WiFiRecovery.h"
#include "DiagnosticsPage.h"
#include "AlarmScheduler.h"
#include "AlarmPage.h"

constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_RECEIVE_PIN = 27;
constexpr uint16_t ONKYO_ADDRESS = 0x6DD2;

struct RemoteFunction {
  const char *id;
  const char *label;
  const char *group;
  const char *preferenceKey;
  decode_type_t hardcodedProtocol;
  uint16_t hardcodedAddress;
  uint16_t hardcodedCommand;
  uint8_t hardcodedBits;
  uint8_t learnedValue;
  decode_type_t learnedProtocol = UNKNOWN;
  uint16_t learnedAddress = 0;
  uint16_t learnedCommand = 0;
  uint8_t learnedBits = 0;
};

// Complete RC-209S configuration. This is the single source of truth for the
// current remote; every command works immediately after flashing a new ESP32.
RemoteFunction REMOTE_FUNCTIONS[] = {
    {"POWER", "POWER", "AMPLITUNER", "irPower", NEC, 0x6DD2, 0x04, 32, 0},
    {"SLEEP", "SLEEP", "AMPLITUNER", "irSleep", NEC, 0x6DD2, 0x5D, 32, 0},
    {"SPK-MAIN", "SPEAKERS MAIN", "AMPLITUNER", "irSpkMain", NEC, 0x6DD2, 0x59, 32, 0},
    {"SPK-REMOTE", "SPEAKERS REMOTE", "AMPLITUNER", "irSpkRemote", NEC, 0x6DD2, 0x5A, 32, 0},
    {"SIMUL", "SIMUL SOURCE", "AMPLITUNER", "irSimul", NEC, 0x6DD2, 0xCC, 32, 0},
    {"SIMUL-UP", "SIMUL SOURCE UP", "AMPLITUNER", "irSimulUp", NEC, 0x6DD2, 0xC2, 32, 0},
    {"SIMUL-DOWN", "SIMUL SOURCE DOWN", "AMPLITUNER", "irSimulDown", NEC, 0x6DD2, 0xC3, 32, 0},
    {"TUNER", "TUNER", "WEJŚCIA", "irTuner", NEC, 0x6DD2, 0x0B, 32, 0},
    {"PHONO", "PHONO", "WEJŚCIA", "irPhono", NEC, 0x6DD2, 0x0A, 32, 0},
    {"CD", "CD", "WEJŚCIA", "irCd", NEC, 0x6DD2, 0x09, 32, 0},
    {"DIRECT", "DIRECT", "WEJŚCIA", "irDirect", NEC, 0x6DD2, 0x44, 32, 0},
    {"VIDEO-1", "VIDEO-1", "WEJŚCIA", "irVideo1", NEC, 0x6DD2, 0x0F, 32, 0},
    {"VIDEO-2", "VIDEO-2", "WEJŚCIA", "irVideo2", NEC, 0x6DD2, 0x0E, 32, 0},
    {"TAPE-1", "TAPE-1", "WEJŚCIA", "irTape1", NEC, 0x6DD2, 0x08, 32, 0},
    {"TAPE-2", "TAPE-2", "WEJŚCIA", "irTape2", NEC, 0x6DD2, 0x07, 32, 0},
    {"CLASS", "CLASS", "TUNER", "irClass", NEC, 0x6DD2, 0x4A, 32, 0},
    {"PRESET-", "PRESET ◀", "TUNER", "irPresetM", NEC, 0x6DD2, 0x01, 32, 0},
    {"PRESET+", "PRESET ▶", "TUNER", "irPresetP", NEC, 0x6DD2, 0x00, 32, 0},
    {"DECK-A-REV", "◀", "DECK A", "irDeckARev", NEC, 0x6DD2, 0x4F, 32, 0},
    {"DECK-A-PLAY", "▶", "DECK A", "irDeckAPlay", PULSE_DISTANCE, 0x0000, 0x0000, 7, 0},
    {"DECK-A-REC", "REC / PAUSE", "DECK A", "irDeckARec", NEC, 0x6DD2, 0x50, 32, 0},
    {"DECK-A-STOP", "STOP", "DECK A", "irDeckAStop", NEC, 0x6DD2, 0x4D, 32, 0},
    {"DECK-A-REW", "REW", "DECK A", "irDeckARew", NEC, 0x6DD2, 0x52, 32, 0},
    {"DECK-A-FF", "FF", "DECK A", "irDeckAFf", NEC, 0x6DD2, 0x51, 32, 0},
    {"DECK-B-REV", "◀", "DECK B", "irDeckBRev", NEC, 0x6DD2, 0x16, 32, 0},
    {"DECK-B-PLAY", "▶", "DECK B", "irDeckBPlay", NEC, 0x6DD2, 0x15, 32, 0},
    {"DECK-B-REC", "REC / PAUSE", "DECK B", "irDeckBRec", NEC, 0x6DD2, 0x18, 32, 0},
    {"DECK-B-STOP", "STOP", "DECK B", "irDeckBStop", NEC, 0x6DD2, 0x13, 32, 0},
    {"DECK-B-REW", "REW", "DECK B", "irDeckBRew", NEC, 0x6DD2, 0x1A, 32, 0},
    {"DECK-B-FF", "FF", "DECK B", "irDeckBFf", NEC, 0x6DD2, 0x19, 32, 0},
    {"CD-PAUSE", "PAUSE", "CD", "irCdPause", NEC, 0x6DD2, 0x1F, 32, 0},
    {"CD-PLAY", "PLAY", "CD", "irCdPlay", NEC, 0x6DD2, 0x1B, 32, 0},
    {"CD-STOP", "STOP", "CD", "irCdStop", NEC, 0x6DD2, 0x1C, 32, 0},
    {"CD-PREV", "◀◀", "CD", "irCdPrev", NEC, 0x6DD2, 0x1E, 32, 0},
    {"CD-NEXT", "▶▶", "CD", "irCdNext", NEC, 0x6DD2, 0x1D, 32, 0},
    {"CENTER-ON", "CENTER OFF/ON", "DŹWIĘK", "irCenterOn", NEC, 0x6DD2, 0x98, 32, 0},
    {"CENTER-UP", "CENTER UP", "DŹWIĘK", "irCenterUp", NEC, 0x6DD2, 0x80, 32, 0},
    {"CENTER-DOWN", "CENTER DOWN", "DŹWIĘK", "irCenterDown", NEC, 0x6DD2, 0x81, 32, 0},
    {"REAR-UP", "REAR LEVEL UP", "DŹWIĘK", "irRearUp", NEC, 0x6DD2, 0x42, 32, 0},
    {"REAR-DOWN", "REAR LEVEL DOWN", "DŹWIĘK", "irRearDown", NEC, 0x6DD2, 0x43, 32, 0},
    {"MUTE", "MUTING", "DŹWIĘK", "irMute", NEC, 0x6DD2, 0x05, 32, 0},
    {"VOL+", "VOLUME UP", "DŹWIĘK", "irVolP", NEC, 0x6DD2, 0x02, 32, 0},
    {"VOL-", "VOLUME DOWN", "DŹWIĘK", "irVolM", NEC, 0x6DD2, 0x03, 32, 0},
    {"SURROUND", "SURROUND MODE", "DŹWIĘK", "irSurround", NEC, 0x6DD2, 0x4C, 32, 0},
    {"DELAY", "DELAY TIME", "DŹWIĘK", "irDelay", NEC, 0x6DD2, 0x53, 32, 0},
    {"TEST", "TEST", "DŹWIĘK", "irTest", NEC, 0x6DD2, 0x9A, 32, 0},
};

WebServer server(80);
Preferences preferences;
Preferences restartPreferences;
Preferences alarmPreferences;
AlarmScheduler alarms;
DailyRestartPolicy dailyRestart;
std::atomic<bool> timeSynchronized{false};
bool restartStorageReady = false;
bool otaInProgress = false;
bool manualRestartRequested = false;
uint64_t manualRestartAtMs = 0;
uint64_t lastRestartCheckMs = 0;
VolumeHoldSafety volumeSafety;
DeviceDiagnostics deviceDiagnostics;
WiFiRecovery wifiRecovery;
bool loopWatchdogReady = false;
bool networkServicesStarted = false;
const char *preparedVolumeCommand = nullptr;

constexpr unsigned long VOLUME_REPEAT_INTERVAL_MS = 110;
// Mobile browsers and Wi-Fi can delay a keepalive briefly.  Keep a finite
// watchdog so a lost connection cannot leave a volume command repeating.
constexpr unsigned long VOLUME_WATCHDOG_MS = 2000;
constexpr unsigned long LEARN_RECEIVER_SETTLE_MS = 750;
// ArduinoOTA expects milliseconds, not seconds. Allow brief Wi-Fi stalls.
constexpr uint32_t OTA_RECEIVE_TIMEOUT_MS = 5000;
const char *heldVolumeCommand = nullptr;
const char *learningCommand = nullptr;
bool irTransmissionEnabled = true;
bool irReceiverEnabled = false;
unsigned long learningStartedMs = 0;
unsigned long lastVolumeSignalMs = 0;
unsigned long lastVolumeSendMs = 0;

uint64_t restartUptimeMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

void recordUserActivity() {
  dailyRestart.recordActivity(restartUptimeMs());
}

void onWiFiDiagnostics(WiFiEvent_t event, WiFiEventInfo_t info) {
  // The network event task only updates atomic counters; no HTTP or flash work.
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    deviceDiagnostics.connected();
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    deviceDiagnostics.disconnected(info.wifi_sta_disconnected.reason,
                                   static_cast<uint32_t>(restartUptimeMs() / 1000));
  }
}

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt_watchdog";
    case ESP_RST_TASK_WDT: return "task_watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

void handleDiagnostics() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t disconnects = deviceDiagnostics.disconnects.load();
  char rssi[16] = "null", reason[16] = "null", disconnectedAt[16] = "null";
  if (connected) snprintf(rssi, sizeof(rssi), "%d", static_cast<int>(WiFi.RSSI()));
  if (disconnects != 0) {
    snprintf(reason, sizeof(reason), "%lu", static_cast<unsigned long>(deviceDiagnostics.lastDisconnectReason.load()));
    snprintf(disconnectedAt, sizeof(disconnectedAt), "%lu",
             static_cast<unsigned long>(deviceDiagnostics.lastDisconnectUptimeSeconds.load()));
  }
  const esp_reset_reason_t reset = esp_reset_reason();
  char body[1024];
  const int size = snprintf(body, sizeof(body),
      "{\"version\":\"%s\",\"uptimeSeconds\":%llu,"
      "\"wifi\":{\"connected\":%s,\"rssiDbm\":%s,\"sleepEnabled\":%s,\"connections\":%lu,"
      "\"disconnects\":%lu,\"lastDisconnectReason\":%s,\"lastDisconnectUptimeSeconds\":%s},"
      "\"memory\":{\"freeBytes\":%lu,\"minimumFreeBytes\":%lu,\"largestFreeBlockBytes\":%lu},"
      "\"reset\":{\"code\":%d,\"reason\":\"%s\"},\"loopMaxMs\":%lu,\"timeSynchronized\":%s,"
      "\"volumeUpLimitMs\":%lu,\"volumeWatchdogMs\":%lu,"
      "\"recovery\":{\"wifiRetries\":%lu,\"radioResets\":%lu,\"loopWatchdogEnabled\":%s,\"loopWatchdogMs\":%lu}}",
      ONKYO_FIRMWARE_VERSION, static_cast<unsigned long long>(restartUptimeMs() / 1000),
      connected ? "true" : "false", rssi, WiFi.getSleep() != WIFI_PS_NONE ? "true" : "false",
      static_cast<unsigned long>(deviceDiagnostics.connections.load()), static_cast<unsigned long>(disconnects),
      reason, disconnectedAt, static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()), static_cast<unsigned long>(ESP.getMaxAllocHeap()),
      static_cast<int>(reset), resetReasonName(reset), static_cast<unsigned long>(deviceDiagnostics.maxLoopMs),
      timeSynchronized.load() ? "true" : "false", static_cast<unsigned long>(VOLUME_UP_MAX_HOLD_MS),
      static_cast<unsigned long>(VOLUME_WATCHDOG_MS),
      static_cast<unsigned long>(wifiRecovery.retries), static_cast<unsigned long>(wifiRecovery.radioResets),
      loopWatchdogReady ? "true" : "false", static_cast<unsigned long>(LOOP_WATCHDOG_MS));
  server.sendHeader("Cache-Control", "no-store");
  if (size < 0 || static_cast<size_t>(size) >= sizeof(body)) {
    server.send(500, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", body);
}

void handleDiagnosticsPage() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", DIAGNOSTICS_PAGE);
}

void onTimeSynchronized(struct timeval *timeValue) {
  // NTP runs in another task. No flash writes or control changes in this callback.
  timeSynchronized.store(timeValue != nullptr && timeValue->tv_sec >= 1704067200);
}

void startTimeSynchronization() {
  sntp_set_time_sync_notification_cb(onTimeSynchronized);
  configTzTime(RESTART_TIME_ZONE, "pool.ntp.org", "time.nist.gov");
}

void handleDailyRestart() {
  const uint64_t uptime = restartUptimeMs();
  if (uptime - lastRestartCheckMs < 1000) return;
  lastRestartCheckMs = uptime;
  if (!restartStorageReady || !timeSynchronized.load()) return;

  const time_t now = time(nullptr);
  tm localTime{};
  // Nonblocking: never wait for NTP from the control loop.
  if (localtime_r(&now, &localTime) == nullptr) return;
  const bool busy = otaInProgress || learningCommand != nullptr || heldVolumeCommand != nullptr || alarms.blocksRestart(&localTime);
  const uint32_t day = dailyRestart.dueDay(uptime, &localTime, true, busy);
  if (day == 0) return;

  // Commit before resetting. If storage fails, do not risk a restart loop.
  if (restartPreferences.putUInt("restartDay", day) != sizeof(day)) {
    restartStorageReady = false;
    Serial.println(F("Daily restart skipped: cannot save restart date."));
    return;
  }
  dailyRestart.markRestart(day);
  digitalWrite(IR_SEND_PIN, LOW);
  Serial.printf("Daily idle restart: %04d-%02d-%02d %02d:%02d Europe/Warsaw\n",
                localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                localTime.tm_hour, localTime.tm_min);
  Serial.flush();
  ESP.restart();
}

const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#101010">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <link rel="manifest" href="/manifest.webmanifest">
  <link rel="icon" href="/icon.svg" type="image/svg+xml">
  <link rel="apple-touch-icon" href="/icon.svg">
  <title>Pilot Onkyo</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: #a9a7a2; color: #eeeae2; -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; }
    main { width: min(100%, 430px); min-height: 100vh; padding: 24px 18px 32px; background: linear-gradient(110deg, #121212, #202020 52%, #141414); border: 1px solid #515151; box-shadow: inset 0 0 0 2px #0b0b0b, 0 16px 38px #0008; }
    header { display: flex; justify-content: space-between; align-items: center; gap:12px; margin-bottom: 28px; padding-bottom: 13px; border-bottom: 1px solid #555; }
    header > div { min-width:0; }
    h1 { margin: 0; font-size: 1.1rem; letter-spacing: .08em; text-shadow: 0 1px #000; }
    .onkyo-logo { font-family: Georgia, serif; font-size: 1.55rem; font-weight: 900; letter-spacing: -.06em; }
    header p { margin: 4px 0 0; color: #bbb5aa; font-size: .8rem; }
    .schedule-icon { display:inline-flex;flex:0 0 44px;width:44px;height:44px;align-items:center;justify-content:center;color:#eeeae2;border:1px solid #666;border-radius:5px;background:linear-gradient(135deg,#3b3b3b,#1d1d1d); }
    .schedule-icon:hover { border-color:#d1c8ba; }
    .schedule-icon:focus-visible { outline:2px solid #d1c8ba;outline-offset:2px; }
    button { min-height: 56px; border: 1px solid #666; border-radius: 5px; background: linear-gradient(135deg, #3b3b3b, #1d1d1d); box-shadow: inset 0 1px #696969, 0 2px 2px #000; color: inherit; font: inherit; font-size: 1rem; font-weight: 650; cursor: pointer; touch-action: manipulation; -webkit-tap-highlight-color: transparent; }
    button:focus { outline: none; }
    button:focus-visible { outline: 2px solid #d1c8ba; outline-offset: 2px; }
    button:active:not([data-hold-command]), button[data-hold-command].hold-pressed { transform: translateY(1px); background: #111; box-shadow: inset 0 2px 3px #000; }
    .power { width: 100%; background: linear-gradient(135deg, #9f3b35, #64231f); border-color: #c27067; font-size: 1.2rem; letter-spacing: .1em; }
    .power:active { background: #55201d; }
    .label { margin: 25px 0 9px; padding-top: 7px; border-top: 1px solid #555; color: #c8c1b6; font-size: .72rem; letter-spacing: .12em; }
    .volume { display: grid; grid-template-columns: 1fr 1fr; gap: 7px; }
    .sources { display: grid; grid-template-columns: repeat(4, 1fr); gap: 7px; }
    .sources .source { grid-column: span 2; }
    .sources .preset-button { grid-column: span 1; }
    .volume button { min-height: 72px; font-size: 2rem; }
    .mute { width: 100%; margin-top: 10px; }
    .advanced-link { display: block; margin-top: 26px; color: #d1c8ba; font-size: .82rem; text-align: center; }
    .firmware-version { margin: 14px 0 0; color: #bbb5aa; font-size: .72rem; text-align: center; }
    .diagnostics-link { display: block; margin-top: 12px; color: #bbb5aa; font-size: .72rem; text-align: center; }
    @media (min-width: 431px) { main { min-height: auto; border-radius: 9px; } }
  </style>
</head>
<body>
  <main>
    <header>
      <div><h1 class="onkyo-logo">ONKYO</h1><p>REMOTE CONTROL TRANSMITTER · RC-209S</p></div>
      <a class="schedule-icon" href="/schedule" aria-label="Budzik i wyłączenie" title="Budzik i wyłączenie"><svg aria-hidden="true" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg></a>
    </header>
    <button class="power" type="button" data-command="POWER">POWER</button>
    <p class="label">VOLUME</p>
    <div class="volume" aria-label="Volume controls">
      <button type="button" data-hold-command="VOL-" aria-label="Volume down">−</button>
      <button type="button" data-hold-command="VOL+" aria-label="Volume up">+</button>
    </div>
    <button class="mute" type="button" data-command="MUTE">MUTE</button>
    <p class="label">SOURCE</p>
    <div class="sources" aria-label="Source selection">
      <button class="source" type="button" data-source="TAPE-1">TAPE-1</button>
      <button class="source" type="button" data-source="CD">CD</button>
      <button class="source" type="button" data-source="PHONO">PHONO</button>
      <button class="source" type="button" data-source="TUNER">TUNER</button>
      <button class="source" type="button" data-source="VIDEO-1">VIDEO-1</button>
      <button class="preset-button" type="button" data-command="PRESET-">◀</button>
      <button class="preset-button" type="button" data-command="PRESET+">▶</button>
    </div>
    <a class="advanced-link" href="/advanced">Otwórz pilot Advanced</a>
    <p class="firmware-version">Firmware v)HTML" ONKYO_FIRMWARE_VERSION R"HTML(</p>
    <a class="diagnostics-link" href="/status">Diagnostyka</a>
  </main>
  <script>
    let requestInFlight = false;
    function sendCommand(command) {
      if (requestInFlight) return;
      requestInFlight = true;
      fetch('/command', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'name=' + encodeURIComponent(command) })
        .then((response) => { if (!response.ok) throw new Error('Command failed'); })
        .catch(() => {})
        .finally(() => { requestInFlight = false; });
    }
    document.querySelectorAll('[data-command]').forEach((button) => button.addEventListener('click', () => sendCommand(button.dataset.command)));
    document.querySelectorAll('[data-source]').forEach((button) => button.addEventListener('click', () => {
      sendCommand(button.dataset.source);
    }));
  </script>
  <script src="/volume.js"></script>
</body>
</html>
)HTML";

const char PWA_MANIFEST[] PROGMEM = R"JSON(
{
  "name": "Pilot Onkyo",
  "short_name": "Onkyo",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#101010",
  "theme_color": "#101010",
  "icons": [{
    "src": "/icon.svg",
    "sizes": "any",
    "type": "image/svg+xml",
    "purpose": "any maskable"
  }]
}
)JSON";

const char PWA_ICON[] PROGMEM = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 192">
  <rect width="192" height="192" rx="38" fill="#101010"/>
  <rect x="48" y="27" width="96" height="138" rx="14" fill="#242424" stroke="#e0d8ca" stroke-width="5"/>
  <circle cx="96" cy="55" r="10" fill="#d8342b"/>
  <rect x="67" y="80" width="58" height="12" rx="6" fill="#e0d8ca"/>
  <rect x="67" y="105" width="58" height="12" rx="6" fill="#e0d8ca"/>
  <rect x="67" y="130" width="58" height="12" rx="6" fill="#e0d8ca"/>
</svg>
)SVG";

RemoteFunction *findRemoteFunction(const String &id) {
  for (RemoteFunction &function : REMOTE_FUNCTIONS) {
    if (id == function.id) return &function;
  }
  return nullptr;
}

bool hasLearnedCode(const RemoteFunction &function) {
  return function.learnedProtocol != UNKNOWN || function.learnedValue != 0;
}

String remotePageHead(const char *title) {
  String page = F("<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta name=\"theme-color\" content=\"#101010\"><title>");
  page += title;
  page += F("</title><style>:root{color-scheme:dark;font-family:system-ui,sans-serif}*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#101010;color:#f5f2ed}main{max-width:760px;margin:auto;padding:24px 18px 36px;background:#181818}h1{margin:0;font-size:1.15rem;letter-spacing:.07em}h2{margin:28px 0 9px;color:#aaa6a0;font-size:.75rem;letter-spacing:.12em}.hint,#status{line-height:1.45;color:#c2bdb5}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}button{min-height:54px;border:0;border-radius:14px;background:#292929;color:inherit;font:inherit;font-weight:650;cursor:pointer}button:active:not([data-hold-command]){transform:scale(.98);background:#393939}.missing{opacity:.52}.learning{background:#806326}.saved:after{content:' ✓';color:#91d27a}a{display:block;margin-top:28px;color:#aaa6a0;text-align:center}@media(min-width:600px){.grid{grid-template-columns:repeat(3,minmax(0,1fr))}main{margin-top:20px;border-radius:28px;box-shadow:0 18px 50px #0008}}</style></head><body><main>");
  return page;
}

String renderRemoteButtons(bool learning) {
  String page;
  const char *currentGroup = nullptr;
  for (const RemoteFunction &function : REMOTE_FUNCTIONS) {
    if (currentGroup == nullptr || strcmp(currentGroup, function.group) != 0) {
      if (currentGroup != nullptr) page += F("</div>");
      currentGroup = function.group;
      page += F("<h2>");
      page += currentGroup;
      page += F("</h2><div class=\"grid\">");
    }
    page += F("<button type=\"button\" data-id=\"");
    page += function.id;
    page += F("\" class=\"");
    if (!learning && function.hardcodedProtocol == UNKNOWN && !hasLearnedCode(function)) page += F("missing");
    if (learning && (function.hardcodedProtocol != UNKNOWN || hasLearnedCode(function))) page += F("saved");
    page += F("\">");
    page += function.label;
    page += F("</button>");
  }
  if (currentGroup != nullptr) page += F("</div>");
  return page;
}

String advancedButton(const char *id, bool learning = false) {
  RemoteFunction *function = findRemoteFunction(id);
  if (function == nullptr) return String();
  String button = F("<button type=\"button\" class=\"key ");
  if (learning && (function->hardcodedProtocol != UNKNOWN || hasLearnedCode(*function))) button += F("saved ");
  if (!learning && function->hardcodedProtocol == UNKNOWN && !hasLearnedCode(*function)) button += F("unlearned ");
  if (strcmp(id, "POWER") == 0) button += F("power-key ");
  button += F("\" data-id=\"");
  button += id;
  button += F("\"");
  if (!learning && (strcmp(id, "VOL+") == 0 || strcmp(id, "VOL-") == 0)) {
    button += F(" data-hold-command=\"");
    button += id;
    button += F("\"");
  }
  button += F(">");
  button += function->label;
  button += F("</button>");
  return button;
}

String remoteShellStyle() {
  return F("<style>main{max-width:590px;background:linear-gradient(110deg,#111,#242424 52%,#121212);border:1px solid #585858;box-shadow:inset 0 0 0 2px #090909,0 18px 42px #0009}.remote{max-width:550px;margin:auto;border:1px solid #686868;padding:15px;background:#171717;box-shadow:inset 0 0 18px #000}.remote h1{text-align:center;margin:5px 0 15px;font-family:Georgia,serif;font-size:1.35rem;font-weight:900;letter-spacing:-.04em}.top,.inputs,.tuner-controls,.deck-controls,.cd-controls,.effects{display:grid;gap:6px}.top,.inputs{grid-template-columns:repeat(5,1fr)}.input-box,.tuner-box,.deck,.cd-box,.center-box,.rear-box,.muting-box,.effects-box{border:1px solid #626262;padding:6px}.box-label{text-align:center;font-size:.66rem;letter-spacing:.08em;color:#c7c0b5;margin:0 0 6px}.tuner-line{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin:12px 0}.tuner-box{grid-column:3 / 6}.tuner-controls{grid-template-columns:repeat(3,1fr)}.decks{display:grid;grid-template-columns:repeat(5,1fr);gap:6px}.deck-a{grid-column:1 / 3}.deck-b{grid-column:4 / 6}.deck-controls{grid-template-columns:repeat(2,1fr)}.lower{display:grid;grid-template-columns:2fr 1fr 1fr 1fr;gap:6px;margin-top:12px}.cd-controls{grid-template-columns:repeat(2,1fr)}.cd-stop{grid-column:1 / 3;display:grid;place-items:center}.cd-stop .key{width:48%}.center-box,.rear-box,.muting-box{display:grid;gap:6px;align-content:start}.volume-label{margin-top:5px!important;border-top:1px solid #575757;padding-top:5px}.effects-box{width:61%;margin-top:6px}.effects{grid-template-columns:repeat(3,1fr)}.brand{width:61%;margin-top:16px;padding:13px 5px 3px;border-top:1px solid #555;color:#e4e0d7}.brand-onkyo{font-family:Georgia,serif;font-size:2.15rem;font-weight:900;letter-spacing:-.08em}.brand-ri{float:right;font-family:Georgia,serif;font-size:1.7rem;font-weight:900}.brand small{display:block;letter-spacing:.08em;font-size:.56rem}.brand b{float:right}.key{min-height:52px;padding:5px 3px;border:1px solid #6b6b6b;border-radius:3px;background:linear-gradient(135deg,#3f3f3f,#1c1c1c);box-shadow:inset 0 1px #777,0 2px 2px #000;color:#ece8df;font-size:.72rem;font-weight:700;line-height:1.05}.key:active:not([data-hold-command]),.key[data-hold-command].hold-pressed{transform:translateY(1px);background:#101010;box-shadow:inset 0 2px 3px #000}.power-key{background:linear-gradient(135deg,#984038,#57211e);border-color:#bd746b}.unlearned{opacity:.46;border-style:dashed}.saved:after{content:' ✓';color:#91d27a}.learning{background:#806326;opacity:1}.legend{margin:12px 0 0;text-align:center;font-size:.78rem;color:#c5bfb4}.learn{color:#d7cfbf}.back{margin-top:14px}@media(max-width:420px){main{padding:20px 10px}.remote{padding:10px}.key{min-height:46px;font-size:.61rem}.top,.inputs{gap:4px}}</style>");
}

String renderRemoteShell(bool learning) {
  String page = F("<div class=\"remote\"><div class=\"top\">");
  page += advancedButton("POWER", learning); page += advancedButton("SLEEP", learning); page += advancedButton("SPK-MAIN", learning); page += advancedButton("SPK-REMOTE", learning); page += advancedButton("SIMUL", learning);
  page += F("</div><div class=\"input-box\"><p class=\"box-label\">INPUT SELECTOR</p><div class=\"inputs\">");
  page += advancedButton("TUNER", learning); page += advancedButton("PHONO", learning); page += advancedButton("CD", learning); page += advancedButton("DIRECT", learning); page += advancedButton("SIMUL-UP", learning);
  page += advancedButton("VIDEO-1", learning); page += advancedButton("VIDEO-2", learning); page += advancedButton("TAPE-1", learning); page += advancedButton("TAPE-2", learning); page += advancedButton("SIMUL-DOWN", learning);
  page += F("</div></div><div class=\"tuner-line\"><div></div><div></div><div class=\"tuner-box\"><p class=\"box-label\">TUNER</p><div class=\"tuner-controls\">");
  page += advancedButton("CLASS", learning); page += advancedButton("PRESET-", learning); page += advancedButton("PRESET+", learning);
  page += F("</div></div></div><div class=\"decks\"><div class=\"deck deck-a\"><p class=\"box-label\">DECK-A</p><div class=\"deck-controls\">");
  page += advancedButton("DECK-A-REV", learning); page += advancedButton("DECK-A-PLAY", learning); page += advancedButton("DECK-A-REC", learning); page += advancedButton("DECK-A-STOP", learning); page += advancedButton("DECK-A-REW", learning); page += advancedButton("DECK-A-FF", learning);
  page += F("</div></div><div></div><div class=\"deck deck-b\"><p class=\"box-label\">DECK-B</p><div class=\"deck-controls\">");
  page += advancedButton("DECK-B-REV", learning); page += advancedButton("DECK-B-PLAY", learning); page += advancedButton("DECK-B-REC", learning); page += advancedButton("DECK-B-STOP", learning); page += advancedButton("DECK-B-REW", learning); page += advancedButton("DECK-B-FF", learning);
  page += F("</div></div></div><div class=\"lower\"><div class=\"cd-box\"><p class=\"box-label\">CD</p><div class=\"cd-controls\">");
  page += advancedButton("CD-PAUSE", learning); page += advancedButton("CD-PLAY", learning);
  page += F("<span class=\"cd-stop\">"); page += advancedButton("CD-STOP", learning); page += F("</span>");
  page += advancedButton("CD-PREV", learning); page += advancedButton("CD-NEXT", learning);
  page += F("</div></div><div class=\"center-box\"><p class=\"box-label\">CENTER</p>");
  page += advancedButton("CENTER-ON", learning); page += advancedButton("CENTER-UP", learning); page += advancedButton("CENTER-DOWN", learning);
  page += F("</div><div class=\"rear-box\"><p class=\"box-label\">REAR LEVEL</p>");
  page += advancedButton("REAR-UP", learning); page += advancedButton("REAR-DOWN", learning);
  page += F("</div><div class=\"muting-box\"><p class=\"box-label\">MUTING</p>");
  page += advancedButton("MUTE", learning);
  page += F("<p class=\"box-label volume-label\">VOLUME</p>");
  page += advancedButton("VOL+", learning); page += advancedButton("VOL-", learning);
  page += F("</div></div><div class=\"effects-box\"><div class=\"effects\">");
  page += advancedButton("SURROUND", learning); page += advancedButton("DELAY", learning); page += advancedButton("TEST", learning);
  page += F("</div></div><div class=\"brand\"><span class=\"brand-onkyo\">ONKYO</span><span class=\"brand-ri\">RI</span><small>REMOTE CONTROL TRANSMITTER <b>RC-209S</b></small></div></div>");
  return page;
}

void handleAdvancedPage() {
  String page = remotePageHead("Pilot Onkyo Advanced");
  page += remoteShellStyle();
  page += F("<style>.firmware-version{margin:14px 0 0;color:#bbb5aa;font-size:.72rem;text-align:center}.remote button{touch-action:manipulation;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}.remote button:focus{outline:none}.remote button:focus-visible{outline:2px solid #d1c8ba;outline-offset:2px}</style>");
  page += renderRemoteShell(false);
  page += F("<a href=\"/schedule\" aria-label=\"Budzik i wyłączenie\" title=\"Budzik i wyłączenie\" style=\"display:flex;align-items:center;justify-content:center;width:44px;height:44px;margin:14px auto;border:1px solid #666;border-radius:5px\"><svg aria-hidden=\"true\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.8\" stroke-linecap=\"round\"><circle cx=\"12\" cy=\"12\" r=\"9\"/><path d=\"M12 7v5l3 2\"/></svg></a>");
  page += F("<a class=\"back\" href=\"/\">Wróć do wersji Basic</a><script>document.querySelectorAll('[data-id]:not([data-hold-command])').forEach(b=>b.onclick=()=>fetch('/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(b.dataset.id)}));</script><script src=\"/volume.js\"></script>");
  page += F("<p class=\"firmware-version\">Firmware v" ONKYO_FIRMWARE_VERSION "</p><a href=\"/status\" style=\"margin-top:12px;font-size:.72rem\">Diagnostyka</a></main></body></html>");
  server.send(200, "text/html", page);
}

void handleRemoteLearnPage() {
  String page = remotePageHead("Nauka pilota Onkyo");
  page += remoteShellStyle();
  page += F("<style>.remote button{touch-action:manipulation;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}.remote button:focus{outline:none}.remote button:focus-visible{outline:2px solid #d1c8ba;outline-offset:2px}</style>");
  page += F("<h1>NAUKA PILOTA — RC-209S</h1><p class=\"hint\">Zielony znacznik oznacza przypisany kod. Po wybraniu przycisku nadajnik IR zostaje wyłączony, więc wzmacniacz nie dostanie żadnej komendy. Naciśnij odpowiednik na oryginalnym pilocie.</p><p id=\"status\">Wybierz przycisk do nauczenia.</p>");
  page += renderRemoteShell(true);
  page += F("<a href=\"/advanced\">Wróć do pilota Advanced</a><script>let current=null;const status=document.querySelector('#status');const buttons=[...document.querySelectorAll('[data-id]')];async function poll(){try{const r=await fetch('/learn/status');const d=await r.json();if(d.learning){current=d.name;buttons.forEach(b=>b.classList.toggle('learning',b.dataset.id===current));status.textContent='Czekam na sygnał: '+current}else if(current){buttons.forEach(b=>b.classList.remove('learning'));const b=buttons.find(x=>x.dataset.id===current);if(b)b.classList.add('saved');status.textContent='Kod zapisany: '+current;current=null}}catch(_){status.textContent='Brak połączenia z pilotem.'}}buttons.forEach(b=>b.onclick=async()=>{const r=await fetch('/learn/start',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(b.dataset.id)});if(!r.ok){status.textContent='Nie można rozpocząć nauki.';return}current=b.dataset.id;buttons.forEach(x=>x.classList.toggle('learning',x===b));status.textContent='Czekam na sygnał: '+current});window.addEventListener('pagehide',()=>{if(current)fetch('/learn/cancel',{method:'POST'})});setInterval(poll,600);</script></main></body></html>");
  server.send(200, "text/html", page);
}

void handleVersion() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", F("{\"version\":\"" ONKYO_FIRMWARE_VERSION "\"}"));
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_PAGE);
}

void handleManifest() {
  server.send_P(200, "application/manifest+json", PWA_MANIFEST);
}

void handleIcon() {
  server.send_P(200, "image/svg+xml", PWA_ICON);
}

void stopVolume() {
  volumeSafety.closeAll();
  heldVolumeCommand = nullptr;
}

// The IR receiver is needed only while assigning a new command.  Leaving its
// sampling timer running continuously makes it react to every nearby remote.
void enableIrReceiverForLearning() {
  if (irReceiverEnabled) return;
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  irReceiverEnabled = true;
}

void disableIrReceiver() {
  if (!irReceiverEnabled) return;
  IrReceiver.stop();
  irReceiverEnabled = false;
}

bool sendOnkyoCommand(const String &name) {
  // Never emit IR while the receiver is learning: this prevents the amplifier
  // from receiving a command selected accidentally in the web UI.
  if (!irTransmissionEnabled || learningCommand != nullptr) return false;
  RemoteFunction *function = findRemoteFunction(name);
  if (function != nullptr) {
    recordUserActivity();
    if (function->learnedProtocol != UNKNOWN) {
      IRData learnedData{};
      learnedData.protocol = function->learnedProtocol;
      learnedData.address = function->learnedAddress;
      learnedData.command = function->learnedCommand;
      learnedData.numberOfBits = function->learnedBits;
      Serial.print(">>> Sending learned ");
      Serial.println(name);
      return IrSender.write(&learnedData, NO_REPEATS) != 0;
    }
    if (function->hardcodedProtocol == UNKNOWN) return false;
    Serial.print(">>> Sending hardcoded ");
    Serial.println(name);
    if (function->hardcodedProtocol == NEC) {
      IrSender.sendNEC(function->hardcodedAddress, function->hardcodedCommand, 0);
      return true;
    }
    IRData hardcodedData{};
    hardcodedData.protocol = function->hardcodedProtocol;
    hardcodedData.address = function->hardcodedAddress;
    hardcodedData.command = function->hardcodedCommand;
    hardcodedData.numberOfBits = function->hardcodedBits;
    return IrSender.write(&hardcodedData, NO_REPEATS) != 0;
  }
  return false;
}

bool persistAlarms(const AlarmSettings &settings) {
  return alarms.storageReady && alarmPreferences.putBytes("settings", &settings, sizeof(settings)) == sizeof(settings);
}

void handleAlarmSettings() {
  const time_t now = time(nullptr);
  tm local{};
  const bool ready = timeSynchronized.load() && localtime_r(&now, &local) != nullptr && alarmDay(&local) != 0;
  char clock[32] = "";
  if (ready) strftime(clock, sizeof(clock), "%Y-%m-%d %H:%M:%S", &local);
  const auto &off = alarms.settings.slots[0];
  const auto &on = alarms.settings.slots[1];
  char offDate[16] = "", onDate[16] = "";
  if (off.dueDay) snprintf(offDate, sizeof(offDate), "%04lu-%02lu-%02lu", (unsigned long)(off.dueDay / 10000), (unsigned long)(off.dueDay / 100 % 100), (unsigned long)(off.dueDay % 100));
  if (on.dueDay) snprintf(onDate, sizeof(onDate), "%04lu-%02lu-%02lu", (unsigned long)(on.dueDay / 10000), (unsigned long)(on.dueDay / 100 % 100), (unsigned long)(on.dueDay % 100));
  char body[1024];
  snprintf(body, sizeof(body),
      "{\"clockReady\":%s,\"storageReady\":%s,\"localTime\":\"%s\",\"lastResult\":\"%s\","
      "\"off\":{\"enabled\":%s,\"time\":\"%02u:%02u\",\"date\":\"%s\"},"
      "\"on\":{\"enabled\":%s,\"time\":\"%02u:%02u\",\"date\":\"%s\",\"source\":\"%s\"}}",
      ready ? "true" : "false", alarms.storageReady ? "true" : "false", clock, alarms.result,
      off.enabled ? "true" : "false", off.hour, off.minute, offDate,
      on.enabled ? "true" : "false", on.hour, on.minute, onDate, ALARM_SOURCES[on.source]);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", body);
}

bool readAlarmTime(const char *timeKey, AlarmSlot &slot) {
  if (!server.hasArg(timeKey)) return false;
  const String value = server.arg(timeKey);
  if (value.length() != 5 || value[2] != ':') return false;
  for (unsigned int i = 0; i < 5; ++i) {
    if (i != 2 && (value[i] < '0' || value[i] > '9')) return false;
  }
  slot.hour = (value[0] - '0') * 10 + value[1] - '0';
  slot.minute = (value[3] - '0') * 10 + value[4] - '0';
  return slot.hour < 24 && slot.minute < 60;
}

void saveAlarmSettings() {
  server.sendHeader("Cache-Control", "no-store");
  const String action = server.arg("action");
  const bool on = action == "on" || action == "cancelOn";
  const bool cancel = action == "cancelOn" || action == "cancelOff";
  if (action != "on" && action != "off" && !cancel) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"Odśwież panel budzika. Wybierz Ustaw lub Anuluj.\"}");
    return;
  }
  AlarmSettings updated = alarms.settings;
  auto &slot = updated.slots[on ? 1 : 0];
  if (cancel) slot.enabled = 0;
  else {
    const time_t now = time(nullptr);
    tm local{};
    if (!timeSynchronized.load() || localtime_r(&now, &local) == nullptr || !alarmDay(&local)) {
      server.send(409, "application/json; charset=utf-8", "{\"error\":\"Zegar ESP32 czeka na NTP. Spróbuj za chwilę.\"}");
      return;
    }
    bool sourceFound = !on;
    if (on) for (uint8_t i = 0; i < ALARM_SOURCE_COUNT; ++i) {
      if (server.arg("source") == ALARM_SOURCES[i]) { slot.source = i; sourceFound = true; break; }
    }
    if (!sourceFound || !readAlarmTime(on ? "onTime" : "offTime", slot)) {
      server.send(400, "application/json; charset=utf-8", "{\"error\":\"Sprawdź godzinę i źródło.\"}");
      return;
    }
    slot.enabled = 1;
    slot.dueDay = nextAlarmDay(&local, slot.hour, slot.minute);
    for (auto &legacy : updated.slots) if (legacy.enabled && !legacy.dueDay)
      legacy.dueDay = nextAlarmDay(&local, legacy.hour, legacy.minute);
  }
  if (!validAlarms(updated)) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"Włączenie i wyłączenie nie mogą wypadać jednocześnie.\"}");
    return;
  }
  if (!persistAlarms(updated)) {
    alarms.storageReady = false;
    alarms.cancelSource();
    server.send(500, "application/json; charset=utf-8", "{\"error\":\"Nie udało się zapisać harmonogramu w pamięci ESP32.\"}");
    return;
  }
  if (on) alarms.cancelSource();
  alarms.settings = updated;
  recordUserActivity();
  handleAlarmSettings();
}

void handleAlarms() {
  const time_t now = time(nullptr);
  tm local{};
  const bool ready = timeSynchronized.load() && localtime_r(&now, &local) != nullptr;
  if (ready && alarms.storageReady) {
    auto upgraded = alarms.settings;
    bool changed = false;
    for (auto &slot : upgraded.slots) if (slot.enabled && !slot.dueDay) {
      slot.dueDay = nextAlarmDay(&local, slot.hour, slot.minute);
      changed = true;
    }
    if (changed) {
      if (!persistAlarms(upgraded)) { alarms.storageReady = false; alarms.result = "Błąd zapisu budzika"; return; }
      alarms.settings = upgraded;
    }
  }
  alarms.tick(restartUptimeMs(), &local, ready,
              otaInProgress || learningCommand != nullptr || heldVolumeCommand != nullptr || !irTransmissionEnabled,
              persistAlarms, [](const char *command) { return sendOnkyoCommand(command); });
}

void handleRestartRequest() {
  server.sendHeader("Cache-Control", "no-store");
  if (server.arg("confirm") != "restart") {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (otaInProgress || alarms.pending()) {
    server.send(409, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Trwa aktualizacja lub pobudka. Spróbuj ponownie za chwilę.\"}");
    return;
  }
  if (!manualRestartRequested) {
    stopVolume();
    recordUserActivity();
    manualRestartAtMs = restartUptimeMs() + 500;
    manualRestartRequested = true;
  }
  // Return HTTP acknowledgment before rebooting, without blocking the loop.
  server.send(202, "application/json", "{\"ok\":true,\"restarting\":true}");
}

void handleRequestedRestart() {
  if (!manualRestartRequested || otaInProgress || restartUptimeMs() < manualRestartAtMs) return;
  stopVolume();
  digitalWrite(IR_SEND_PIN, LOW);
  ESP.restart();
}

void handleLearnStart() {
  alarms.cancelSource();
  stopVolume();
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  RemoteFunction *function = findRemoteFunction(server.arg("name"));
  if (function == nullptr) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  learningCommand = function->id;
  recordUserActivity();
  irTransmissionEnabled = false;
  enableIrReceiverForLearning();
  learningStartedMs = millis();
  digitalWrite(IR_SEND_PIN, LOW);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLearnCancel() {
  recordUserActivity();
  learningCommand = nullptr;
  irTransmissionEnabled = true;
  disableIrReceiver();
  learningStartedMs = 0;
  digitalWrite(IR_SEND_PIN, LOW);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLearnStatus() {
  if (learningCommand != nullptr) {
    String response = "{\"learning\":true,\"name\":\"";
    response += learningCommand;
    response += "\"}";
    server.send(200, "application/json", response);
    return;
  }
  server.send(200, "application/json", "{\"learning\":false}");
}

void processLearnedCommand() {
  if (learningCommand == nullptr || !IrReceiver.decode()) {
    return;
  }

  const IRData &data = IrReceiver.decodedIRData;
  // Discard the tail/repeat frames of the preceding physical button press.
  // A new slot therefore waits for a fresh, deliberate signal.
  if (millis() - learningStartedMs < LEARN_RECEIVER_SETTLE_MS ||
      (data.flags & IRDATA_FLAGS_IS_REPEAT) != 0) {
    IrReceiver.resume();
    return;
  }
  if (data.protocol != UNKNOWN && (data.flags & IRDATA_FLAGS_WAS_OVERFLOW) == 0) {
    RemoteFunction *function = findRemoteFunction(learningCommand);
    if (function != nullptr) {
      function->learnedProtocol = data.protocol;
      function->learnedAddress = data.address;
      function->learnedCommand = data.command;
      function->learnedBits = data.numberOfBits;
      function->learnedValue = data.command == 0 ? 1 : static_cast<uint8_t>(data.command);
      const String keyBase = function->preferenceKey;
      preferences.putUChar((keyBase + "P").c_str(), static_cast<uint8_t>(data.protocol));
      preferences.putUShort((keyBase + "A").c_str(), data.address);
      preferences.putUShort((keyBase + "C").c_str(), data.command);
      preferences.putUChar((keyBase + "B").c_str(), data.numberOfBits);
      Serial.print(">>> Learned ");
      Serial.print(function->id);
      Serial.print(" protocol=");
      Serial.print(getProtocolString(data.protocol));
      Serial.print(" address=0x");
      Serial.print(data.address, HEX);
      Serial.print(" command=0x");
      Serial.println(data.command, HEX);
    }
    recordUserActivity();
    learningCommand = nullptr;
    irTransmissionEnabled = true;
    disableIrReceiver();
    learningStartedMs = 0;
    digitalWrite(IR_SEND_PIN, LOW);
  }
  IrReceiver.resume();
}

void handleCommand() {
  alarms.cancelSource();
  stopVolume();
  if (!server.hasArg("name") || !sendOnkyoCommand(server.arg("name"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeScript() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/javascript; charset=utf-8", VOLUME_SCRIPT);
}

uint64_t readVolumeSession() {
  if (!server.hasArg("session")) return 0;
  const String raw = server.arg("session");
  if (raw.length() == 0 || raw.length() > 20) return 0;
  for (unsigned int i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') return 0;
  }
  errno = 0;
  char *end = nullptr;
  const uint64_t session = strtoull(raw.c_str(), &end, 10);
  return errno == ERANGE || end == nullptr || *end != '\0' ? 0 : session;
}

void handleVolumePress() {
  const String direction = server.arg("direction");
  const char *command = direction == "up" ? "VOL+" : direction == "down" ? "VOL-"
                        : direction == "tuning-up" ? "TUNING+" : direction == "tuning-down" ? "TUNING-" : nullptr;
  if (command == nullptr) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  stopVolume();
  alarms.cancelSource();
  const uint64_t session = volumeSafety.prepare(direction == "up", millis());
  preparedVolumeCommand = command;
  // A short tap emits one command, without arming any repetition.
  if (session == 0 || !sendOnkyoCommand(command)) {
    volumeSafety.cancel(session);
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  char response[64];
  snprintf(response, sizeof(response), "{\"ok\":true,\"session\":\"%llu\"}",
           static_cast<unsigned long long>(session));
  server.send(200, "application/json", response);
}

void expireVolumeHold(unsigned long now) {
  if (heldVolumeCommand != nullptr &&
      (volumeSafety.reachedLimit(now) || now - lastVolumeSignalMs > VOLUME_WATCHDOG_MS)) {
    stopVolume();
  }
}

void handleVolumeStart() {
  const unsigned long now = millis();
  expireVolumeHold(now);
  const auto result = volumeSafety.start(readVolumeSession(), now);
  if (result == VolumeHoldSafety::StartResult::Rejected) {
    server.send(409, "application/json", "{\"ok\":false}");
    return;
  }
  if (result == VolumeHoldSafety::StartResult::Started) {
    if (!irTransmissionEnabled || learningCommand != nullptr || preparedVolumeCommand == nullptr) {
      stopVolume();
      server.send(409, "application/json", "{\"ok\":false}");
      return;
    }
    heldVolumeCommand = preparedVolumeCommand;
    recordUserActivity();
    lastVolumeSignalMs = now;
    lastVolumeSendMs = now;
  }
  // Duplicate starts never reset the hold deadline or emit another IR command.
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeKeepalive() {
  const unsigned long now = millis();
  expireVolumeHold(now);
  if (heldVolumeCommand == nullptr || !volumeSafety.matches(readVolumeSession())) {
    server.send(409, "application/json", "{\"ok\":false}");
    return;
  }
  recordUserActivity();
  lastVolumeSignalMs = now;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeStop() {
  const uint64_t session = readVolumeSession();
  if (session == 0) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (volumeSafety.cancel(session)) {
    recordUserActivity();
    heldVolumeCommand = nullptr;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void repeatHeldVolume() {
  if (heldVolumeCommand == nullptr) {
    return;
  }

  const unsigned long now = millis();
  expireVolumeHold(now);
  if (heldVolumeCommand == nullptr) return;

  if (now - lastVolumeSendMs >= VOLUME_REPEAT_INTERVAL_MS) {
    sendOnkyoCommand(heldVolumeCommand);
    lastVolumeSendMs = now;
  }
}

void connectToWiFi() {
  WiFi.onEvent(onWiFiDiagnostics);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.setAutoReconnect(true);
  if (!WiFi.setSleep(false)) Serial.println(F("Cannot disable Wi-Fi power saving."));
  wifiRecovery.begin(restartUptimeMs());
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(F("Connecting to Wi-Fi in the background."));
}

void startLoopWatchdog() {
  esp_task_wdt_config_t config{};
  config.timeout_ms = LOOP_WATCHDOG_MS;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  config.idle_core_mask |= 1;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  config.idle_core_mask |= 2;
#endif
  config.trigger_panic = true;
  esp_err_t result = esp_task_wdt_reconfigure(&config);
  if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_init(&config);
  if (result == ESP_OK) {
    result = esp_task_wdt_add(nullptr);
    loopWatchdogReady = result == ESP_OK;
  }
  if (!loopWatchdogReady) Serial.println(F("Cannot enable loop watchdog."));
}

void feedLoopWatchdog() {
  if (loopWatchdogReady) esp_task_wdt_reset();
}

void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setTimeout(OTA_RECEIVE_TIMEOUT_MS);
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    stopVolume();
    alarms.cancelSource();
    recordUserActivity();
    feedLoopWatchdog();
  });
  // OTA.handle() stays inside one loop iteration for the whole transfer.
  // Feed only while bytes are arriving; a stalled transfer remains bounded.
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { feedLoopWatchdog(); });
  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    recordUserActivity();
  });
  ArduinoOTA.onError([](ota_error_t) {
    otaInProgress = false;
    recordUserActivity();
  });
  ArduinoOTA.begin();

  Serial.print("OTA ready. Hostname: ");
  Serial.println(OTA_HOSTNAME);
}

void handleWiFiRecovery() {
  const uint64_t now = restartUptimeMs();
  if (deviceDiagnostics.disconnectPending.exchange(false)) {
    stopVolume();
    wifiRecovery.interrupted(now);
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected) stopVolume();
  const auto action = wifiRecovery.update(now, connected, otaInProgress);
  if (action == WiFiRecovery::Action::Connected) {
    if (networkServicesStarted) {
      server.stop();
      ArduinoOTA.end();
    }
    startOta();
    server.begin();
    networkServicesStarted = true;
    Serial.print("Wi-Fi connected. Open http://");
    Serial.println(WiFi.localIP());
  } else if (action == WiFiRecovery::Action::Retry) {
    Serial.println(F("Wi-Fi recovery: retrying connection."));
    WiFi.reconnect();
  } else if (action == WiFiRecovery::Action::ResetRadio) {
    Serial.println(F("Wi-Fi recovery: restarting radio without rebooting ESP32."));
    // Keep credentials and the running clock; re-create sockets after GOT_IP.
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(OTA_HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Onkyo IR Remote firmware v" ONKYO_FIRMWARE_VERSION));
  startLoopWatchdog();
  restartStorageReady = restartPreferences.begin("onkyo-restart", false);
  dailyRestart.begin(restartUptimeMs(), restartStorageReady ? restartPreferences.getUInt("restartDay", 0) : 0);
  if (!restartStorageReady) Serial.println(F("Daily restart disabled: storage unavailable."));
  alarms.storageReady = alarmPreferences.begin("onkyo-alarms", false);
  if (alarms.storageReady && alarmPreferences.isKey("settings")) {
    AlarmSettings saved{};
    if (alarmPreferences.getBytesLength("settings") == sizeof(saved) &&
        alarmPreferences.getBytes("settings", &saved, sizeof(saved)) == sizeof(saved) && upgradeAlarmSettings(saved)) {
      alarms.settings = saved;
    } else {
      alarms.storageReady = false;
      alarms.result = "Harmonogram zatrzymany: nieprawidłowe dane pamięci";
    }
  }

  // The transmitter is always available.  The receiver is started only for
  // an active learning session, so ordinary remotes are ignored otherwise.
  IrSender.begin(IR_SEND_PIN);
  preferences.begin("onkyo-remote", false);
  for (RemoteFunction &function : REMOTE_FUNCTIONS) {
    function.learnedValue = preferences.getUChar(function.preferenceKey, 0);
    const String keyBase = function.preferenceKey;
    const uint8_t savedProtocol = preferences.getUChar((keyBase + "P").c_str(), static_cast<uint8_t>(UNKNOWN));
    if (savedProtocol != static_cast<uint8_t>(UNKNOWN)) {
      function.learnedProtocol = static_cast<decode_type_t>(savedProtocol);
      function.learnedAddress = preferences.getUShort((keyBase + "A").c_str(), 0);
      function.learnedCommand = preferences.getUShort((keyBase + "C").c_str(), 0);
      function.learnedBits = preferences.getUChar((keyBase + "B").c_str(), 0);
    }
  }
  // Firmware Basic used these two keys. Migrate them once so yesterday's
  // learned preset buttons remain available in the Advanced remote.
  RemoteFunction *presetPrevious = findRemoteFunction("PRESET-");
  RemoteFunction *presetNext = findRemoteFunction("PRESET+");
  if (presetPrevious != nullptr && presetPrevious->learnedValue == 0) {
    presetPrevious->learnedValue = preferences.getUChar("presetPrev", 0);
    if (presetPrevious->learnedValue != 0) preferences.putUChar(presetPrevious->preferenceKey, presetPrevious->learnedValue);
  }
  if (presetNext != nullptr && presetNext->learnedValue == 0) {
    presetNext->learnedValue = preferences.getUChar("presetNext", 0);
    if (presetNext->learnedValue != 0) preferences.putUChar(presetNext->preferenceKey, presetNext->learnedValue);
  }

  connectToWiFi();
  startTimeSynchronization();
  volumeSafety.begin(esp_random());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/version", HTTP_GET, handleVersion);
  server.on("/diagnostics", HTTP_GET, handleDiagnostics);
  server.on("/status", HTTP_GET, handleDiagnosticsPage);
  server.on("/restart", HTTP_POST, handleRestartRequest);
  server.on("/schedule", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", ALARM_PAGE);
  });
  server.on("/alarms", HTTP_GET, handleAlarmSettings);
  server.on("/alarms", HTTP_POST, saveAlarmSettings);
  server.on("/advanced", HTTP_GET, handleAdvancedPage);
  server.on("/manifest.webmanifest", HTTP_GET, handleManifest);
  server.on("/icon.svg", HTTP_GET, handleIcon);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/volume.js", HTTP_GET, handleVolumeScript);
  server.on("/volume/press", HTTP_POST, handleVolumePress);
  server.on("/volume/start", HTTP_POST, handleVolumeStart);
  server.on("/volume/keepalive", HTTP_POST, handleVolumeKeepalive);
  server.on("/volume/stop", HTTP_POST, handleVolumeStop);
  server.on("/learn/start", HTTP_POST, handleLearnStart);
  server.on("/learn/cancel", HTTP_POST, handleLearnCancel);
  server.on("/learn/status", HTTP_GET, handleLearnStatus);
  // HTTP and OTA start when Wi-Fi obtains an address, also after reconnects.
}

void loop() {
  const uint64_t started = restartUptimeMs();
  handleWiFiRecovery();
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server.handleClient();
  }
  repeatHeldVolume();
  processLearnedCommand();
  handleRequestedRestart();
  if (!manualRestartRequested) handleAlarms();
  handleDailyRestart();
  const uint64_t elapsed = restartUptimeMs() - started;
  if (elapsed > deviceDiagnostics.maxLoopMs) {
    deviceDiagnostics.maxLoopMs = elapsed > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(elapsed);
  }
  feedLoopWatchdog();
  delay(1); // Allow the idle/system tasks to run even without HTTP traffic.
}
