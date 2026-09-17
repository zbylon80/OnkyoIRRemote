# Onkyo IR Remote — instructions for future work

## Locate the real project first

- The working repository is `C:\Users\zbylo\Projects\OnkyoIRRemote`.
- The similarly named `C:\Users\zbylo\Documents\ChatGPT\OnkyoIRRemote` directory may be an empty task workspace. Do not conclude that source code is missing without checking the project path above.
- Firmware source: `firmware\OnkyoRemote\OnkyoRemote.ino`.

## When a user asks to fix a running ESP32 feature

- Treat the request as authorization to implement, compile, and upload the firmware when the device is available. Do not stop at a source edit or instruct the user to use Arduino IDE.
- Start with concise, factual progress updates; do not speculate about the cause before inspecting the source.
- Preserve the current safety behavior unless the user asks to remove it. For hold/repeat controls, a finite watchdog must remain after a Wi-Fi/client disconnect.

## Build and OTA upload workflow

1. Find the device with the bundled Arduino CLI:
   `C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe board list`
2. Compile the sketch for `esp32:esp32:esp32` into `firmware\OnkyoRemote\build\esp32.esp32.esp32`.
3. Use the current `WiFiConfig.h` only to pass the OTA password internally. Never print, log, or repeat credentials.
4. Upload the fresh build over the detected network port with `arduino-cli upload --input-dir <build-directory>`.
5. Confirm a successful upload from the CLI exit code and then verify the device returns HTTP 200 from its root page.

## Avoid wasted time and token-heavy failures

- A compile command may return a process/session identifier after the first wait interval. Continue waiting on that same session until its real exit code is available; do not assume an empty initial response means compilation succeeded.
- Run one compilation at a time. Do not start another compile while a previous one is still running.
- Prefer `--quiet` for normal compilation. Do not use `--verbose` unless diagnosing a failed build; its large output can obscure the actual result.
- A clean, single-threaded ESP32 build can take several minutes. Wait for completion rather than abandoning it and using an old binary.
- Before upload, compare the binary timestamp with the source timestamp. Never flash an older artifact.
- If a previous attempt was interrupted and left `arduino-cli` or `xtensa-esp32-elf-*` processes running, inspect their PIDs and stop only the processes started by the failed attempt before retrying.
- If `--output-dir` fails to supply upload artifacts, use the explicit existing build directory with `--build-path` during compile and `--input-dir` during upload.

## Report completion accurately

- Say the firmware was uploaded only after the OTA command exits successfully.
- State the behavioral/safety trade-off in plain language: for example, how long a repeat command may continue after a true disconnect.
