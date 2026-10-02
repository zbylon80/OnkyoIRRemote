# Changelog

Firmware versions use `MAJOR.MINOR.PATCH`. Entries marked **Unreleased** describe
prepared changes; they do not mean that a device has been updated.

## 1.1.0 — 2026-10-02

- Disable Wi-Fi modem sleep to reduce network latency; this increases radio
  power consumption.
- Correct OTA timeout units: tolerate five seconds without incoming data
  instead of the accidental 60 milliseconds.
- Add an on-demand diagnostics page at `/status` linked from both panels, and
  JSON at `/diagnostics` with Wi-Fi signal/power saving, connection counters,
  last disconnect code/time, uptime, memory, reset reason and maximum loop time.
  Diagnostics do not poll in the background, count as control use or store
  credentials. Counters reset on reboot.
- Extend the volume-up hold limit to three seconds. Use a single firmware
  constant for the ESP32 policy and browser timer. Keep the two-second
  disconnect watchdog and request timeouts.
- Control the pressed appearance through the actual hold state, so reaching
  the limit visually releases the button even while the pointer is held.
  Release is still required before another up hold can start.

## 1.0.0 — 2026-10-02

- Introduce explicit firmware versioning, defined in `FirmwareVersion.h`.
- Show the firmware version at the bottom of Basic and Advanced.
- Expose `GET /version` with a JSON version value and disable caching for it.
- Print the firmware version in the serial startup log.
- Schedule a restart between 03:00 and 05:00 Europe/Warsaw after 30 minutes
  without use, and at least 30 minutes after boot. Skip the night if no idle
  slot is available.
- Synchronize the clock in the background through NTP; skip scheduled restarts
  until a valid clock has been synchronized since boot. Control still works
  without Internet access.
- Block scheduled restarts during OTA, code learning or an active hold.
- Persist the scheduled restart date before rebooting, allowing at most one
  scheduled restart per local calendar day. Skip restarting if storage fails.
- Limit each volume-up hold to two seconds from its first IR command, enforced
  by ESP32 independently of browser renewals. Volume-down and tuning retain
  continuous holds and the two-second disconnect watchdog.
- Send one IR command for a tap; arm repetition only after the initial response
  and a 350 ms hold delay. Share the controls between Basic and Advanced.
- Identify holds with boot-specific sessions; reject late starts, stale renewals
  and cancelled sessions without affecting newer holds. Duplicate starts do not
  resend IR or extend the deadline.
- Keep at most one press and renewal request in flight, with two-second request
  timeouts and cleanup on release, lost pointer capture and leaving the page.
- Require release after the up limit and ignore duplicate pointer-down events.
- Use the correct tuning directions in Advanced through the shared controls.

The baseline is commit `3e5f043`, which did not have an explicit firmware version.
The IR codes, 110 ms repeat interval and two-second disconnect watchdog remain
unchanged. Opening pages and reading status do not count as use.
