# Changelog

Firmware versions use `MAJOR.MINOR.PATCH`. Entries marked **Unreleased** describe
prepared changes; they do not mean that a device has been updated.

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
