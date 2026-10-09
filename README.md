# Onkyo RC-209S Wi-Fi IR Remote

ESP32-based Wi-Fi infrared remote for the Onkyo RC-209S layout. It serves two local web panels:

- **Basic** — everyday controls: power, volume, mute, selected inputs and tuner preset arrows.
- **Advanced** — the complete RC-209S-style layout, including tuner, tape decks, CD and amplifier controls.

The remote controls work inside the same local Wi-Fi network as the ESP32. They
do not use a cloud service or require an Internet connection. The scheduled
restart additionally uses NTP time synchronization.

[Polska wersja / Polish version](README.pl.md)

## Manual restart

Diagnostics (`/status`) includes **Restart ESP32**. It acknowledges `POST /restart`
(`confirm=restart`) before rebooting, stops held controls, retains flash settings
and emits no IR. OTA and a pending wake source return HTTP 409. Wait about
15 seconds then refresh diagnostics; NTP synchronizes again after boot.

## One-shot off and wake schedules

Off/wake settings are available at `/schedule`, linked from both remote
panels and from the clock icon in the Windows/Android widget's top menu.
Widgets 0.4.0 or later open their own native alarm panel without a browser; all clients
read and write the same settings on the ESP32 through `/alarms`.
Choose a wake time/source and press **Ustaw**, or an off time and its **Ustaw**.
Windows 0.4.2, Android 0.4.3 and firmware 1.3.2 use matching dark time and source
pickers. Click HH:mm to edit 24-hour digits: down advances the number, up goes
back. **Gotowe** confirms only the local choice; **Ustaw** writes the schedule.
Each action is independent and runs once: today if the chosen minute has not
started, otherwise tomorrow. The panel confirms the date/time and offers cancel.
Defaults are unset, with times 02:00 and 07:00. Wake selects an input after two seconds.
Settings survive ESP32 restart; the phone/computer need not stay on.

The ESP32 requires NTP synchronization after every boot and follows Warsaw
time including DST. Internet access is required to synchronize, not to execute
an already synchronized schedule. Missed minutes are not caught up. An action
is disabled and saved before sending POWER, so it cannot repeat after a restart
or on the next day. Pressing Set explicitly schedules another occurrence.
When local time skips a minute at spring DST change, that action is skipped.
OTA, learning and held controls also cause a due action to be skipped.

Both actions send the same POWER toggle. Off assumes an already powered-on
amplifier; wake assumes standby. There is no feedback and volume stays unchanged.
For waking to music choose a source that will actually be playing, such as TUNER.

`GET /alarms` returns clock/storage status, the last execution result since boot,
and `off`/`on` settings including their target `date`. `POST /alarms` accepts
URL-encoded `action=on` with `onTime`/`source`, `action=off` with `offTime`,
or `action=cancelOn`/`cancelOff`. Times are `HH:MM`. Sources: TUNER, CD, PHONO,
TAPE-1, TAPE-2, VIDEO-1, VIDEO-2. The two actions cannot share the same date/time.
Each write affects only the requested action. Saving settings emits no IR. A storage failure stops
the scheduler; browser errors require refreshing before another save.

## Firmware versioning

The version has one source: [FirmwareVersion.h](firmware/OnkyoRemote/FirmwareVersion.h).
It appears at the bottom of Basic and Advanced, in the serial startup log, and in
the JSON returned by `GET /version`, for example `{"version":"1.1.0"}`.
This endpoint reports the version actually running on the ESP32.

Use `MAJOR.MINOR.PATCH`: increment PATCH for fixes, MINOR for compatible new
features, and MAJOR for breaking changes. Record each release in
[CHANGELOG.md](CHANGELOG.md). Unreleased changes may accumulate under the same
pending version; after release, use a new version for subsequent firmware changes.
Updating the source or compiling it does not update the device.

## Volume-up protection

A tap sends one IR command. Repeating starts only while the same press is still
held, 350 ms after its initial response. On both Basic and Advanced, ESP32 ends
each volume-up hold at three seconds from the first command, regardless of
renewals from the phone or computer. The button also visually releases at the limit, even while the pointer is held.
Release and press again to continue.
Volume-down and tuning have no additional hold limit; all holds still stop
after more than two seconds without a valid renewal.

Hold sessions reject delayed starts and stale renewals; an old stop cannot
cancel a newer hold. Presses and renewals do not accumulate on a slow connection;
requests time out after two seconds. There is no background connection polling. These rules
limit runaway repetition; IR provides no amplifier volume feedback, so they
cannot enforce an absolute maximum volume level.

The pages share [VolumeControls.h](firmware/OnkyoRemote/VolumeControls.h), served
at `/volume.js`; the firmware policy is in
[VolumeHoldSafety.h](firmware/OnkyoRemote/VolumeHoldSafety.h). The browser sends
`POST /volume/press` with a direction for the single command and a session token,
then uses that token for `/volume/start`, `/volume/keepalive` and `/volume/stop`.
Reload existing browser tabs after uploading this version; the previous hold
API without a session token is rejected.

## Stability diagnostics

Starting with 1.3.3, startup does not wait indefinitely for Wi-Fi. Connection
attempts are repeated every 15 seconds; after a minute without an IP address,
the radio is restarted. HTTP and OTA are recreated when connectivity returns.
A router outage does not repeatedly reboot ESP32, so its synchronized clock
and saved alarms can keep running offline. A loop watchdog resets a stalled
task after 15 seconds; OTA transfer progress feeds it during uploads. A reported
Wi-Fi disconnect closes the volume session on the next loop iteration. The
two-second client keepalive watchdog and three-second volume-up cap remain.
The `/diagnostics` JSON additionally reports `recovery`: connection retry/radio
restart counters and the loop watchdog status/timeout, all for the current boot.

Starting with 1.1.0, Wi-Fi modem sleep is disabled to reduce response latency;
this increases radio power consumption. The **Diagnostyka** link below the
version opens `/status`; `/diagnostics` returns the same measurements as JSON.
The page reads once when opened and again only on manual refresh.

It reports firmware/uptime, Wi-Fi signal in dBm, configured power saving,
connection/disconnect counts and the last disconnect reason code/time since
boot, free/minimum heap and largest free block, the reset reason, maximum loop
duration, clock synchronization and volume limits. Missing signal/disconnect
information is `null` in JSON. Counters and timing maxima describe the current
boot; they are not a persistent history. The reset reason comes from the ESP32
SDK, so a software reset does not distinguish OTA from the nightly restart.
Diagnostics do not affect the idle timer and never include Wi-Fi or OTA
credentials. They are read-only and do not send amplifier commands.

## Nightly restart

Starting with version 1.0.0, the firmware checks for an idle restart in the local
03:00–05:00 window, using Polish winter/summer time. It waits for 30 full minutes
without control activity and at least 30 minutes since boot. If the condition
is not met before 05:00, that night's restart is skipped.

IR commands, active volume renewals, releasing an active hold, learning controls
and OTA reset the idle timer. Merely opening a page or polling status does not.
Restarting is blocked during OTA, learning and active holds. The restart date is
saved to flash before reboot, limiting scheduled restarts to once per day even
after power loss; a storage failure prevents the scheduled restart.

The clock synchronizes asynchronously via `pool.ntp.org` and `time.nist.gov`.
Without a valid synchronization after boot, scheduled restarting stays disabled;
remote control continues normally. No clock waiting is added to the control loop.
Schedule constants are in [DailyRestart.h](firmware/OnkyoRemote/DailyRestart.h).


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

### Native Android widget

The [android/](android/README.md) project provides a home-screen widget with
the Basic panel buttons, controlling ESP32 without opening the web panel.
Each volume tap changes the volume by one step. On Android 16 (API 36) or
newer, holding VOL−/VOL+ repeats until release, with a 3-second limit.
Android 8–15 supports single taps. The widget uses the existing firmware 1.1.0
protocol; no firmware update is needed.
APK build and installation instructions are in the Android documentation.

### Windows PC widget

The [windows/](windows/README.md) project provides a native desktop remote
with the Basic panel and press-and-hold VOL−/VOL+ (3-second limit).
The small window can be moved, resized, and pinned above other windows.
While the app is running, the VOL−/VOL+ and MUTE media keys control Onkyo,
and PLAY/PAUSE sends POWER, including while minimized. Closing the app
restores the standard Windows key functions.
It uses firmware 1.1.0 without an ESP32 update. Portable app and test
instructions are in the Windows documentation.

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
