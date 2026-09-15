#include <ArduinoOTA.h>
#include <IRremote.hpp>
#include <WebServer.h>
#include <WiFi.h>

#include "WiFiConfig.h"

constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_RECEIVE_PIN = 27;
constexpr uint16_t ONKYO_ADDRESS = 0x6DD2;

struct OnkyoCommand {
  const char *name;
  uint8_t value;
};

const OnkyoCommand COMMANDS[] = {
    {"POWER", 0x04}, {"VOL+", 0x02},   {"VOL-", 0x03},
    {"MUTE", 0x05},  {"TAPE-1", 0x08}, {"CD", 0x09},
    {"PHONO", 0x0A}, {"TUNER", 0x0B},  {"VIDEO-1", 0x0F},
};

WebServer server(80);

constexpr unsigned long VOLUME_REPEAT_INTERVAL_MS = 110;
constexpr unsigned long VOLUME_WATCHDOG_MS = 400;
const char *heldVolumeCommand = nullptr;
unsigned long lastVolumeSignalMs = 0;
unsigned long lastVolumeSendMs = 0;

const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Onkyo Remote</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: #101010; color: #f5f2ed; -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; }
    main { width: min(100%, 430px); min-height: 100vh; padding: 24px 18px 32px; background: #181818; }
    header { display: flex; justify-content: space-between; align-items: start; margin-bottom: 28px; }
    h1 { margin: 0; font-size: 1.1rem; letter-spacing: .08em; }
    header p, .note { margin: 4px 0 0; color: #aaa6a0; font-size: .8rem; }
    .connected { color: #91d27a; }
    button { min-height: 56px; border: 0; border-radius: 15px; background: #292929; color: inherit; font: inherit; font-size: 1rem; font-weight: 650; cursor: pointer; touch-action: manipulation; }
    button:active { transform: scale(.98); background: #393939; }
    .power { width: 100%; background: #bd4141; font-size: 1.2rem; letter-spacing: .1em; }
    .power:active { background: #d65252; }
    .label { margin: 25px 0 9px; color: #aaa6a0; font-size: .72rem; letter-spacing: .12em; }
    .volume, .sources { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .volume button { min-height: 72px; font-size: 2rem; }
    .mute { width: 100%; margin-top: 10px; }
    .status { min-height: 20px; margin: 18px 0 0; color: #d6d0c8; font-size: .84rem; text-align: center; }
    .note { text-align: center; }
    @media (min-width: 431px) { main { min-height: auto; border-radius: 28px; box-shadow: 0 18px 50px #0008; } }
  </style>
</head>
<body>
  <main>
    <header>
      <div><h1>ONKYO REMOTE</h1><p>TX-SV919PRO</p></div>
      <p class="connected">● Connected</p>
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
    </div>
    <p id="status" class="status" aria-live="polite">Ready to send commands.</p>
    <p class="note">The receiver does not report its actual state.</p>
  </main>
  <script>
    const status = document.getElementById('status');
    let holdTimer = null;
    let heldVolumeCommand = null;
    let requestInFlight = false;
    function sendCommand(command) {
      if (requestInFlight) return;
      requestInFlight = true;
      fetch('/command', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'name=' + encodeURIComponent(command) })
        .then((response) => { if (!response.ok) throw new Error('Command failed'); status.textContent = command + ' command sent.'; })
        .catch(() => { status.textContent = 'Could not reach the remote.'; })
        .finally(() => { requestInFlight = false; });
    }
    function postVolume(path, body = '') {
      return fetch(path, { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body });
    }
    function stopHolding() {
      if (holdTimer !== null) { clearInterval(holdTimer); holdTimer = null; }
      if (heldVolumeCommand !== null) {
        heldVolumeCommand = null;
        postVolume('/volume/stop').catch(() => {});
      }
    }
    document.querySelectorAll('[data-command]').forEach((button) => button.addEventListener('click', () => sendCommand(button.dataset.command)));
    document.querySelectorAll('[data-source]').forEach((button) => button.addEventListener('click', () => {
      sendCommand(button.dataset.source);
    }));
    document.querySelectorAll('[data-hold-command]').forEach((button) => {
      button.addEventListener('pointerdown', (event) => {
        event.preventDefault();
        stopHolding();
        const command = button.dataset.holdCommand;
        heldVolumeCommand = command;
        button.setPointerCapture(event.pointerId);
        const direction = command === 'VOL+' ? 'up' : 'down';
        postVolume('/volume/start', 'direction=' + direction)
          .then((response) => { if (!response.ok) throw new Error('Volume failed'); if (heldVolumeCommand === command) status.textContent = command + ' active. Release to stop.'; })
          .catch(() => { status.textContent = 'Could not reach the remote.'; stopHolding(); });
        holdTimer = setInterval(() => postVolume('/volume/keepalive').catch(() => stopHolding()), 150);
      });
      button.addEventListener('contextmenu', (event) => event.preventDefault());
    });
    document.addEventListener('pointerup', stopHolding);
    document.addEventListener('pointercancel', stopHolding);
    window.addEventListener('blur', stopHolding);
    window.addEventListener('pagehide', stopHolding);
    document.addEventListener('visibilitychange', () => { if (document.hidden) stopHolding(); });
  </script>
</body>
</html>
)HTML";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_PAGE);
}

void stopVolume() {
  heldVolumeCommand = nullptr;
}

bool sendOnkyoCommand(const String &name) {
  for (const OnkyoCommand &command : COMMANDS) {
    if (name == command.name) {
      Serial.print(">>> Sending ");
      Serial.println(command.name);
      IrSender.sendNEC(ONKYO_ADDRESS, command.value, 0);
      return true;
    }
  }
  return false;
}

void handleCommand() {
  stopVolume();
  if (!server.hasArg("name") || !sendOnkyoCommand(server.arg("name"))) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeStart() {
  if (!server.hasArg("direction")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  const String direction = server.arg("direction");
  heldVolumeCommand = direction == "up" ? "VOL+" : direction == "down" ? "VOL-" : nullptr;
  if (heldVolumeCommand == nullptr || !sendOnkyoCommand(heldVolumeCommand)) {
    stopVolume();
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  lastVolumeSignalMs = millis();
  lastVolumeSendMs = lastVolumeSignalMs;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeKeepalive() {
  if (heldVolumeCommand != nullptr) {
    lastVolumeSignalMs = millis();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleVolumeStop() {
  stopVolume();
  server.send(200, "application/json", "{\"ok\":true}");
}

void repeatHeldVolume() {
  if (heldVolumeCommand == nullptr) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastVolumeSignalMs > VOLUME_WATCHDOG_MS) {
    stopVolume();
    return;
  }

  if (now - lastVolumeSendMs >= VOLUME_REPEAT_INTERVAL_MS) {
    sendOnkyoCommand(heldVolumeCommand);
    lastVolumeSendMs = now;
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();
  Serial.print("Wi-Fi connected. Open http://");
  Serial.println(WiFi.localIP());
}

void startOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  Serial.print("OTA ready. Hostname: ");
  Serial.println(OTA_HOSTNAME);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Retain the known-working IR setup: transmitter on GPIO26, receiver on GPIO27.
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  connectToWiFi();
  startOta();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/volume/start", HTTP_POST, handleVolumeStart);
  server.on("/volume/keepalive", HTTP_POST, handleVolumeKeepalive);
  server.on("/volume/stop", HTTP_POST, handleVolumeStop);
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  repeatHeldVolume();
}
