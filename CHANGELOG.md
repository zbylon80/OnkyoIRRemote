# Changelog

Firmware versions use `MAJOR.MINOR.PATCH`. Entries marked **Unreleased** describe
prepared changes; they do not mean that a device has been updated.

## Android 0.4.2 — 2026-10-08

- Match the Windows dark time editor and source picker. Show large HH:mm
  buttons; support typed digits and down/up step buttons with wraparound.
  Keep local confirmation/cancellation separate from Set on the ESP32.
- Preserve both alarm drafts, the selected source and open time-editor input
  across activity recreation. Keep storage/NTP/error guards and no write retries.
- Built the versioned APK and passed lint and emulator tests, including time
  validation, down/up wraparound, source selection, local Cancel, independent
  Set/Cancel and restoring an open editor after activity recreation. Inspected
  panel and picker screenshots. Phone installation remains a package update.

## 1.3.2 — 2026-10-08

- Replace browser-native time/source lists with matching dark pickers and large
  24-hour HH:mm buttons. Down advances digits, up decrements them; support
  direct typing, arrow keys and focused-field mouse-wheel changes.
- Validate local edits, dismiss with Cancel/Escape, and write only with Set.
  Keep the existing one-shot scheduler, stored settings and safety behavior.
- Passed host/browser tests and 320 px browser inspection, compiled and
  successfully uploaded by OTA. Verified HTTP 200, running version 1.3.2,
  synchronized clock and unchanged stored schedules.

## Windows 0.4.2 — 2026-10-08

- Make the time editor follow scrolling order: down advances the number and
  up goes to the preceding number. Apply the same direction to step buttons,
  arrow keys and the mouse wheel, with wraparound at 23/59 retained.

## Windows 0.4.1 — 2026-10-08

- Replace native hour/minute dropdowns with a large HH:mm button and a dark
  time editor. Support direct typing, step buttons, arrow keys and focused-field
  mouse-wheel changes; retain 24-hour validation and independent alarm drafts.
- Give source selection, its dropdown, text selection, focus and scrollbars a
  matching dark appearance. Keep editing separate from the ESP32 Set action;
  Cancel/Escape dismiss local changes without writing an alarm.
- Passed Windows protocol/WPF/media-key checks, clock-editing validation and
  cancellation checks, and small-panel scrolling. Inspected panel and popup
  previews, and prepared the portable EXE/ZIP.

## Android 0.4.1 — 2026-10-07

- Separate settings and alarm shortcuts across the widget header: a smaller
  muted gear on the left and a larger clock on a green button on the right.
  Keep independent 48 × 48 dp touch targets and existing native panel routing.
- Built the versioned APK, passed lint and existing emulator smoke tests, and
  visually checked the widget at its minimum and enlarged sizes.

## Widgets 0.4.0 — 2026-10-07

- Open native one-shot alarm panels from the Windows/Android widget clock
  buttons. Read/write the existing ESP32 `/alarms` API without a browser or
  firmware changes; retain 24-hour selectors, independent Set/Cancel actions
  and the other action's unsaved input.
- Show confirmed dates, allow cancellation without NTP, require a fresh read
  after uncertain results, and serialize requests with manual controls. Never
  retry writes automatically. Android preserves drafts across recreation.
- Publish Windows portable builds into version-specific directories.
- Verified Windows protocol/WPF/media-key tests and alarm layouts at three
  sizes, plus Android build/lint and emulator tests of the widget clock opening
  the native panel, 24-hour digits, independent Set/Cancel, preserved drafts and
  NTP/error guards. Tested against loopback servers without physical IR.
- Prepared Windows EXE/ZIP and a versioned Android debug APK. Installation on
  the user's Windows desktop/phone remains a manual package update.

## 1.3.1 — 2026-10-07

- Replace the locale-dependent time input with separate hour (00–23) and minute
  (00–59) selectors. Always show 24-hour time, including on browsers configured
  for AM/PM, while preserving the existing scheduling and cancellation API.
- Verified browser-script tests, compilation, successful OTA, HTTP 200/version
  and unchanged device schedules. Checked hour/minute selection and layout at
  320 px on the running page without saving test changes.

## 1.3.0 — 2026-10-07

- Replace misleading daily enable checkboxes with two independent one-shot
  actions: wake time/source + Set, and off time + Set. Show the scheduled date
  and an optional cancel button; preserve the other action and unsaved input.
- Schedule the next occurrence of the chosen time (today if still ahead,
  otherwise tomorrow). Persist consumption before POWER, so neither restart
  nor the following day repeats a completed action. A new Set can arm it again.
- Migrate saved times/sources without requiring a storage reset. Existing active
  daily actions become one-shot actions after NTP synchronization.
- Scheduling now uses `POST /alarms` actions `on`, `off`, `cancelOn`, `cancelOff`.
  Reject new schedules while the clock is unavailable; cancellation remains
  available. Legacy page writes require refreshing the panel.
- Verified host scheduler/API/page tests, firmware compilation and successful
  OTA upload. On the device, verified HTTP 200/version, setting both actions,
  retention after returning to Basic, and independent cancellation. Test
  schedules were cancelled without transmitting IR.

## 1.2.1 — 2026-10-07

- Replace Basic's static Connected label with an accessible clock icon linking
  to `/schedule`; use the same icon for the Advanced shortcut.
- Move the Windows/Android schedule shortcut into each widget's top menu,
  using a vector clock icon with an accessible label (widgets 0.3.1).
- Verified Windows layout at three sizes, Android build/lint, firmware build
  and OTA upload, HTTP 200/version, and clock navigation on the running page.

## 1.2.0 — 2026-10-07

- Add a manual ESP32 restart button to diagnostics and `POST /restart`.
  Acknowledge before rebooting, stop held controls and retain stored settings.
  Reject during OTA/pending wake source; never retry reboot requests automatically.

- Add two independent daily schedules stored in ESP32 flash: off (POWER) and
  wake (POWER, then the chosen source after two seconds). Defaults are disabled,
  with times 02:00 and 07:00. No change to volume or amplifier state detection.
- Add `/schedule` to Basic/Advanced and a browser shortcut in the settings of
  Windows/Android widgets (0.3.0). All clients edit the same device settings.
- Add read/write `/alarms` API with atomic settings persistence and validation.
  Use Warsaw local time and NTP, persist execution dates before sending IR,
  never catch up missed minutes or repeat an action on the same local day.
- Skip scheduled actions during OTA, IR learning and held controls. Manual
  commands cancel a pending source selection. Protect pending/upcoming alarms
  from the existing idle restart. Keep volume watchdog and hold limits.
- Uploaded successfully over OTA; verified firmware version, both page routes,
  alarm settings read/save and an actual manual restart with HTTP recovery and
  retained settings. Host scheduler/HTTP/browser tests passed.

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
