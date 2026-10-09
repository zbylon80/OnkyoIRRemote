# Firmware safety tests

From the project root on Windows with Python and Visual Studio C++ build tools:

```powershell
python tests/test_daily_restart.py
node tests/test_volume_controls.js
node tests/test_diagnostics_page.js
node tests/test_alarm_page.js
node tests/test_log_page.js
```

The tests compile the real `DailyRestart.h` policy and extract the actual
clock/restart/OTA/volume functions from the sketch. Simulated hardware records flash
writes and resets; tests never connect to the ESP32 or emit IR.

Recovery checks also use the real `WiFiRecovery.h` policy and extracted Wi-Fi,
watchdog and OTA functions. They cover boot without a router, retry spacing,
radio reset without ESP32 reboot/flash/IR, HTTP/OTA restoration, a disconnect
and reconnect between iterations, stale holds, OTA guards, long uptimes and
watchdog initialization/failure. The simulated watchdog checks configuration
and feeding; an actual hardware timeout/reset is not induced by host tests.

Log checks cover the real fixed-capacity ring, chronological overwrite,
checksums/schema rejection, restoring from a flash-only snapshot after RAM loss,
boot numbering, write deadlines/busy guards, failed-write backoff, streamed JSON
and read-only retrieval. Browser checks cover ordering, missing clock/RSSI,
storage warnings, download link, timeout recovery and no background polling.

The host tests also compile the real one-shot alarm scheduler and extract its
HTTP handlers: independent Set/Cancel actions, today/tomorrow and year/leap-date
rollover, consumption persisted before POWER, two-second source delay, no next-day
replay, migration, missed actions, rollback, storage failures and input validation.
The page test exercises actual JavaScript Set/Cancel flows without checkboxes,
preservation of the other action's draft, and NTP/storage/error guards.

Manual restart tests cover deliberate POST input, delayed reset after the HTTP
acknowledgment, duplicate requests, OTA/wake guards, stopping held controls and
the absence of IR. Diagnostics JavaScript never retries a restart automatically.

Coverage includes the exact 30-minute boundary, 03:00–05:00 window, postponement,
skipping a busy night, boot cooldown, persisted dates, backward clock changes,
long uptimes, spring/autumn local dates, unavailable clock, storage failures,
OTA/learning/hold guards, OTA error cooldown, and page reads versus active use.
The Polish timezone rule follows the POSIX timezone supported by the ESP32 SDK;
these host tests supply already-converted local time.

The same host test checks the actual volume handlers: single-command taps,
the three-second up limit despite renewals/duplicate starts, cancelled and stale
sessions, token parsing, boot isolation, millis rollover, and continuous down/
tuning holds with their disconnect watchdog. IR sending is simulated.

The Node test runs the exact served browser script with manual timers and
deferred HTTP responses. It checks release before a reply, local limits,
cleanup, stale errors, request timeouts, 64-bit token precision and the absence
of queued presses/renewals. Neither test communicates with a physical device.

Diagnostics tests execute the real network event callback and HTTP handler,
parse its JSON, check Wi-Fi/heap/reset data and verify reads leave the inactivity
timer unchanged. The page test covers manual refresh, timeout recovery and
rendering without background polling. Volume tests also verify visual pressed
state is cleared at three seconds while release is still required.
