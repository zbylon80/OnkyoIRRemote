# Onkyo IR Remote

Wi-Fi controlled IR remote for an Onkyo TX-SV919PRO receiver, running on an ESP32 DevKitC / ESP-WROOM-32.

## Project layout

- `firmware/OnkyoRemote/` — the Wi-Fi remote application.
- `diagnostics/OnkyoIRReader/` — receives and prints IR commands from the original remote.
- `diagnostics/OnkyoIRTest/` — sends a POWER command every two seconds and listens for its loopback.

## Hardware connections

| Module | Wire | ESP32 pin |
| --- | --- | --- |
| IR receiver | red VCC | 3V3 |
| IR receiver | black GND | GND |
| IR receiver | yellow OUT | GPIO27 |
| IR transmitter | green VCC | VIN / 5V |
| IR transmitter | brown GND | GND |
| IR transmitter | orange DAT | GPIO26 |

## Setup

1. Copy `firmware/OnkyoRemote/WiFiConfig.example.h` to `WiFiConfig.h` in the same folder.
2. Enter Wi-Fi credentials and a strong OTA password in `WiFiConfig.h`.
3. Open `firmware/OnkyoRemote/OnkyoRemote.ino` in Arduino IDE, select the ESP32 board and upload once over USB.
4. Future uploads may use the `onkyo-remote` network port in Arduino IDE after the board joins Wi-Fi.

`WiFiConfig.h` is ignored by Git and must never be committed.
