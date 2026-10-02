# Firmware safety tests

From the project root on Windows with Python and Visual Studio C++ build tools:

```powershell
python tests/test_daily_restart.py
node tests/test_volume_controls.js
```

The tests compile the real `DailyRestart.h` policy and extract the actual
clock/restart/OTA/volume functions from the sketch. Simulated hardware records flash
writes and resets; tests never connect to the ESP32 or emit IR.

Coverage includes the exact 30-minute boundary, 03:00–05:00 window, postponement,
skipping a busy night, boot cooldown, persisted dates, backward clock changes,
long uptimes, spring/autumn local dates, unavailable clock, storage failures,
OTA/learning/hold guards, OTA error cooldown, and page reads versus active use.
The Polish timezone rule follows the POSIX timezone supported by the ESP32 SDK;
these host tests supply already-converted local time.

The same host test checks the actual volume handlers: single-command taps,
the two-second up limit despite renewals/duplicate starts, cancelled and stale
sessions, token parsing, boot isolation, millis rollover, and continuous down/
tuning holds with their disconnect watchdog. IR sending is simulated.

The Node test runs the exact served browser script with manual timers and
deferred HTTP responses. It checks release before a reply, local limits,
cleanup, stale errors, request timeouts, 64-bit token precision and the absence
of queued presses/renewals. Neither test communicates with a physical device.
