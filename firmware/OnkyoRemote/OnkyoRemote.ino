#include <ArduinoOTA.h>
#include <IRremote.hpp>
#include <WebServer.h>
#include <WiFi.h>

#include "WiFiConfig.h"

constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_RECEIVE_PIN = 27;
constexpr uint16_t ONKYO_ADDRESS = 0x6DD2;
constexpr uint8_t ONKYO_POWER_COMMAND = 0x04;

WebServer server(80);

const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Onkyo Remote</title>
</head>
<body>
  <h1>Onkyo TX-SV919PRO</h1>
  <p>First-milestone web remote</p>
  <form action="/power" method="post">
    <button type="submit">POWER</button>
  </form>
</body>
</html>
)HTML";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_PAGE);
}

void handlePower() {
  Serial.println(">>> Sending POWER");
  IrSender.sendNEC(ONKYO_ADDRESS, ONKYO_POWER_COMMAND, 0);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
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
  server.on("/power", HTTP_POST, handlePower);
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
