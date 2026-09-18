# Onkyo RC-209S Wi-Fi IR Remote

ESP32-based Wi-Fi infrared remote for the Onkyo RC-209S layout. It serves two local web panels:

- **Basic** — everyday controls: power, volume, mute, selected inputs and tuner preset arrows.
- **Advanced** — the complete RC-209S-style layout, including tuner, tape decks, CD and amplifier controls.

The remote works only inside the same local Wi-Fi network as the ESP32. It does not use a cloud service or require an Internet connection.

[Polska wersja / Polish version](README.pl.md)


## 1. What you need

- an **ESP32 DevKitC / ESP-WROOM-32** board;
- an IR transmitter;
- a 38 kHz IR receiver for learning codes from the original remote;
- jumper wires;
- a USB cable for the first firmware upload;
- a computer with Arduino IDE 2.x and a phone or computer on the same Wi-Fi network.

## 2. Wiring

| Module | Wire | ESP32 pin | Purpose |
| --- | --- | --- | --- |
| IR receiver | red | `3V3` | power |
| IR receiver | black | `GND` | ground |
| IR receiver | yellow | `GPIO27` | received signal |
| IR transmitter | green | `VIN` / `5V` | power |
| IR transmitter | brown | `GND` | ground |
| IR transmitter | orange | `GPIO26` | transmitted signal |

All modules need a common ground. Power the IR receiver from `3V3`, not `5V`.

## 3. Prepare Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install **esp32 by Espressif Systems** from Boards Manager.
3. Install **IRremote** from Library Manager.
4. Open `firmware/OnkyoRemote/OnkyoRemote.ino`.
5. Select **ESP32 Dev Module** and the correct USB port.

## 4. Configure Wi-Fi and OTA

1. Copy `firmware/OnkyoRemote/WiFiConfig.example.h` to `WiFiConfig.h` in the same folder.
2. Enter your local Wi-Fi details and a strong OTA password.
3. Never commit or share `WiFiConfig.h`; it contains passwords.

The file is already excluded by `.gitignore`.

## 5. First upload

1. Connect the ESP32 over USB.
2. Click **Upload** in Arduino IDE.
3. Open Serial Monitor at `115200` baud.
4. After Wi-Fi connects, the firmware prints the device's local IP address.

The IP can change after a router restart. Check Serial Monitor, your router's DHCP list, or create a DHCP reservation for the ESP32.

## 6. Use the remote

Your phone or computer must use the same Wi-Fi network as the ESP32.

| Panel | Address | Purpose |
| --- | --- | --- |
| Basic | `http://ESP32_IP/` | everyday controls |
| Advanced | `http://ESP32_IP/advanced` | complete RC-209S-style layout |

For the current installation, use `http://192.168.1.46/` and `http://192.168.1.46/advanced`.

Point the IR transmitter toward the receiver on the amplifier. In Basic, the `◀` and `▶` buttons beside `VIDEO-1` switch saved tuner stations.

## 7. Install as an app

### Android / Chrome

1. Open the panel in Chrome.
2. Open the `⋮` menu.
3. Choose **Install app** or **Add to Home screen**.

### Windows / Chrome or Edge

1. Open the panel in the browser.
2. Use the install-app icon in the address bar or the browser menu.
3. Optionally pin the installed app to Start or the taskbar.

## 8. Learn Advanced functions on a new device

The complete set of 46 RC-209S codes is hardcoded in the firmware. On a new or erased ESP32, simply configure Wi-Fi and upload `OnkyoRemote.ino` — the Advanced panel works immediately, without the original remote or a learning procedure.

Learning remains in the source only for a deliberate future reassignment. Its page is intentionally hidden in normal use. To enable it temporarily:

1. Temporarily add this line in `setup()` in `OnkyoRemote.ino`:

   ```cpp
   server.on("/remotelearn", HTTP_GET, handleRemoteLearnPage);
   ```

2. Upload the firmware and open `http://ESP32_IP/remotelearn`.
3. Select a function on the layout, aim the original RC-209S at the IR receiver, and press the matching physical button.
4. A green mark means the code was saved. It overrides the matching firmware value and persists in ESP32 storage through restarts.
5. Remove or comment the line again, re-upload, and use the Advanced panel.

The ESP32 IR transmitter is disabled while learning, so the amplifier cannot receive an accidental command.

## 9. Wi-Fi firmware updates (OTA)

After the first USB upload, further firmware updates can be performed over the local Wi-Fi network. The ESP32 must be powered on and connected.

In Arduino IDE, select the network port, usually shown as `onkyo-remote at IP_ADDRESS`, then click **Upload**. Arduino IDE will request the OTA password from local `WiFiConfig.h`.

If OTA fails, restart the ESP32 and try again. USB remains the most reliable recovery method.

## 10. Troubleshooting

| Symptom | Check |
| --- | --- |
| The panel does not open | same Wi-Fi network, current IP address, ESP32 power |
| ESP32 is powered but unreachable | press `EN` / `RESET`, then wait for Wi-Fi reconnect |
| The amplifier does not react | transmitter direction, `GPIO26` wire, common ground |
| The original remote cannot be read | receiver on `GPIO27`, `3V3` power, common ground |
| An Advanced function does not work | re-upload the current firmware; also check the IR transmitter and its direction |

The complete code map for this installation is in [Saved IR code map](#11-mapa-zapisanych-kodów-ir--saved-ir-code-map).

## 11. Saved IR code map

The table below is the complete configuration hardcoded in the firmware. Hexadecimal values use the `0x` prefix.

| Function | Protocol | Address | Command | Bits |
| --- | --- | --- | --- | --- |
| POWER | NEC | `0x6DD2` | `0x04` | 32 |
| SLEEP | NEC | `0x6DD2` | `0x5D` | 32 |
| SPEAKERS MAIN | NEC | `0x6DD2` | `0x59` | 32 |
| SPEAKERS REMOTE | NEC | `0x6DD2` | `0x5A` | 32 |
| SIMUL SOURCE | NEC | `0x6DD2` | `0xCC` | 32 |
| SIMUL SOURCE UP | NEC | `0x6DD2` | `0xC2` | 32 |
| SIMUL SOURCE DOWN | NEC | `0x6DD2` | `0xC3` | 32 |
| TUNER | NEC | `0x6DD2` | `0x0B` | 32 |
| PHONO | NEC | `0x6DD2` | `0x0A` | 32 |
| CD (input) | NEC | `0x6DD2` | `0x09` | 32 |
| DIRECT | NEC | `0x6DD2` | `0x44` | 32 |
| VIDEO-1 | NEC | `0x6DD2` | `0x0F` | 32 |
| VIDEO-2 | NEC | `0x6DD2` | `0x0E` | 32 |
| TAPE-1 | NEC | `0x6DD2` | `0x08` | 32 |
| TAPE-2 | NEC | `0x6DD2` | `0x07` | 32 |
| CLASS | NEC | `0x6DD2` | `0x4A` | 32 |
| PRESET ◀ | NEC | `0x6DD2` | `0x01` | 32 |
| PRESET ▶ | NEC | `0x6DD2` | `0x00` | 32 |
| DECK A ◀ | NEC | `0x6DD2` | `0x4F` | 32 |
| DECK A ▶ | PulseDistance | `0x0000` | `0x0000` | 7 |
| DECK A REC / PAUSE | NEC | `0x6DD2` | `0x50` | 32 |
| DECK A STOP | NEC | `0x6DD2` | `0x4D` | 32 |
| DECK A REW | NEC | `0x6DD2` | `0x52` | 32 |
| DECK A FF | NEC | `0x6DD2` | `0x51` | 32 |
| DECK B ◀ | NEC | `0x6DD2` | `0x16` | 32 |
| DECK B ▶ | NEC | `0x6DD2` | `0x15` | 32 |
| DECK B REC / PAUSE | NEC | `0x6DD2` | `0x18` | 32 |
| DECK B STOP | NEC | `0x6DD2` | `0x13` | 32 |
| DECK B REW | NEC | `0x6DD2` | `0x1A` | 32 |
| DECK B FF | NEC | `0x6DD2` | `0x19` | 32 |
| CD PAUSE | NEC | `0x6DD2` | `0x1F` | 32 |
| CD PLAY | NEC | `0x6DD2` | `0x1B` | 32 |
| CD STOP | NEC | `0x6DD2` | `0x1C` | 32 |
| CD ◀◀ | NEC | `0x6DD2` | `0x1E` | 32 |
| CD ▶▶ | NEC | `0x6DD2` | `0x1D` | 32 |
| CENTER OFF/ON | NEC | `0x6DD2` | `0x98` | 32 |
| CENTER UP | NEC | `0x6DD2` | `0x80` | 32 |
| CENTER DOWN | NEC | `0x6DD2` | `0x81` | 32 |
| REAR LEVEL UP | NEC | `0x6DD2` | `0x42` | 32 |
| REAR LEVEL DOWN | NEC | `0x6DD2` | `0x43` | 32 |
| MUTING | NEC | `0x6DD2` | `0x05` | 32 |
| VOLUME UP | NEC | `0x6DD2` | `0x02` | 32 |
| VOLUME DOWN | NEC | `0x6DD2` | `0x03` | 32 |
| SURROUND MODE | NEC | `0x6DD2` | `0x4C` | 32 |
| DELAY TIME | NEC | `0x6DD2` | `0x53` | 32 |
| TEST | NEC | `0x6DD2` | `0x9A` | 32 |
## Project files

| File or folder | Contents |
| --- | --- |
| `firmware/OnkyoRemote/OnkyoRemote.ino` | firmware and embedded web panels |
| `firmware/OnkyoRemote/WiFiConfig.example.h` | password-free configuration template |
| `diagnostics/OnkyoIRReader/` | simple IR receive diagnostic sketch |
| `diagnostics/OnkyoIRTest/` | IR send/receive test sketch |
