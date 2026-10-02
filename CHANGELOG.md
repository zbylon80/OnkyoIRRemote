# Changelog

Firmware versions use `MAJOR.MINOR.PATCH`. Entries marked **Unreleased** describe
prepared changes; they do not mean that a device has been updated.

## 1.0.0 — Unreleased

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

The baseline is commit `3e5f043`, which did not have an explicit firmware version.
The IR codes, browser button handling, repeat timing and disconnect watchdog are
unchanged. Opening pages and reading status do not count as use.
