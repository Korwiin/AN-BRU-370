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
- Flash layout: `partitions.csv` in project root — do NOT swap; default partition table causes boot loops on this board.
- Board pinout: `Board Diagrams/ESP32-S3_DWEII_Pinout.jpg`

## Pin Map

See `include/pins.h` — do not hardcode GPIO numbers in sketch files.

## Project-Specific Constraints

- `include/config.h` is gitignored — never commit Wi-Fi credentials.
  Copy `include/config.h.example` to `include/config.h` before building.
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
- `FIRMWARE_VERSION` (string) and `FIRMWARE_VERSION_BCD` (BCD integer) in `include/config.h` must be updated together on every version bump. Example: `FIRMWARE_VERSION "0.06"` → `FIRMWARE_VERSION_BCD 0x0006`.
