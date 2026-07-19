# CLAUDE.md — Brew370

Extends `/Volumes/home/Projects/Arduino/CLAUDE.md` and `~/.claude/CLAUDE.md`.

## Board Identity

**ESP32-S3 Super Mini** (DWEII, ESP32S3FH4R2) — 240 MHz dual-core Xtensa LX7,
4 MB flash, 2 MB PSRAM, USB-C, 3.3 V logic. PlatformIO board: `esp32-s3-devkitm-1`
(confirmed working in ESP32-S3_Test project).

- Native USB: GPIO 19 (D-), GPIO 20 (D+) — do not use as GPIO.
- ADC1 (Wi-Fi safe): GPIO 1–10. ADC2 (GPIO 11–20) unusable when Wi-Fi active.
- Strapping pins to avoid: GPIO 0 (BOOT), GPIO 45, GPIO 46.
- PSRAM: 2 MB, enabled via `-D BOARD_HAS_PSRAM` build flag.
- Flash layout: `boards/partitions_4mb.csv` — do NOT swap; default partition table causes boot loops on this board.
- Board pinout: `Board Diagrams/ESP32-S3_DWEII_Pinout.jpg`

## Second Target — ANBRU-430

**Waveshare ESP32-S3-Touch-LCD-4.3B-BOX** (N16R8: 16 MB flash, 8 MB octal PSRAM,
800×480 RGB panel, GT911 touch, CH422G expander, one native USB-C).

- Envs: `anbru430` (production, USB HID, no CDC) and `anbru430_dev` (DEV_BUILD:
  HID stubbed, CDC test shell, button-free flashing). Never ship a dev build.
- `board_build.arduino.memory_type = qio_opi` and `boards/partitions_16mb.csv`
  are mandatory. LVGL heap must live in PSRAM, never internal RAM — internal-heap
  LVGL (whether a builtin TLSF pool or stdlib CLIB malloc) starves mbedTLS and
  breaks every OTA HTTPS check. Mechanism: `-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CUSTOM`
  + `src/anbru430/lv_mem_psram.c` implementing the LVGL custom-stdlib contract
  (`lv_mem_init/deinit`, `lv_mem_add_pool/remove_pool`, `lv_malloc_core`,
  `lv_realloc_core`, `lv_free_core`, `lv_mem_monitor_core`, `lv_mem_test_core`)
  backed by `heap_caps_malloc/realloc/free(...,
  MALLOC_CAP_SPIRAM)`. Two prior incidents: 3a shipped a static pool in internal
  RAM, and v0.05 shipped stdlib CLIB malloc + 20-line bounce buffers — both left
  internal heap around 55 KB free, well under what mbedTLS needs, so OTA checks
  failed instantly (`a1:-1/0s`).
- Identity: PID `0x430A`, product/BLE/hostname `ANBRU-430`, own version line in
  `include/anbru430/config.h` (`FIRMWARE_VERSION` + `FIRMWARE_VERSION_INT` together),
  OTA channel `ota/manifest-anbru430.json` + tags `anbru430-vX.YY`.
- BLE credential setup must run before `HID::begin()` — TinyUSB kills Bluedroid
  advertising on IDF 5.5.4. Runtime BLE entry = NVS `blereq` flag + reboot.
- CH422G backlight is binary (no PWM): Brightness = LVGL dim overlay, power
  saving = LCD Sleep only.

## Pin Map

See `include/oled/pins.h` — do not hardcode GPIO numbers in sketch files.

## Project-Specific Constraints

- `include/oled/config.h` is committed. It contains no credentials — Wi-Fi credentials are NVS-only.
  No setup step needed; the file is part of the repo and builds as-is.
- DCS-BIOS over Wi-Fi (UDP multicast 239.255.50.10:5010 recv, UDP 7778 send).
  No third-party DCS-BIOS library — direct WiFiUDP only.
- USB composite: CDC + HIDKeyboard + HID Digitizer (pen). Build flags
  `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1` are both required.
- Absolute pointing device uses `Usage Page(Digitizer) / Usage(Pen)` — NOT `Usage(Mouse)`.
  Windows `mouhid.sys` (mouse driver) applies a ~16px minimum-displacement filter to
  `Usage(Mouse)` absolute reports that cannot be disabled by any user setting. `hidpen.sys`
  (digitizer driver) maps absolute coordinates 1:1. Do NOT revert to Usage(Mouse).
  Button byte: bit0=TipSwitch (L-click), bit1=BarrelSwitch (R-click), bit2=InRange (always 1).
- ADC1 only (GPIO 1–10) for the pot. ADC2 is unreliable with Wi-Fi active.
- DCS-BIOS identifiers verified in `docs/superpowers/plans/2026-05-30-brew370-initial.md` Task 0.
- `FIRMWARE_VERSION` (string) and `FIRMWARE_VERSION_BCD` (BCD integer) in `include/oled/config.h` must be updated together on every version bump. Example: `FIRMWARE_VERSION "0.06"` → `FIRMWARE_VERSION_BCD 0x0006`.
