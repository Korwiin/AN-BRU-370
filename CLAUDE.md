# CLAUDE.md — Brew370

Extends `/Volumes/home/Projects/Arduino/CLAUDE.md` and `~/.claude/CLAUDE.md`.

## Board Identity

**ESP32-S3 Super Mini** (DWEII, ESP32S3FH4R2) — 240 MHz dual-core Xtensa LX7,
4 MB flash, 2 MB PSRAM, USB-C, 3.3 V logic. PlatformIO board: `lolin_s3_mini`
(verify if upload fails).

- Native USB: GPIO 19 (D-), GPIO 20 (D+) — do not use as GPIO.
- ADC1 (Wi-Fi safe): GPIO 1–10. ADC2 (GPIO 11–20) unusable when Wi-Fi active.
- Strapping pins to avoid: GPIO 0 (BOOT), GPIO 45, GPIO 46.
- Board pinout: `Board Diagrams/ESP32-S3_DWEII_Pinout.jpg`

## Pin Map

See `include/pins.h` — do not hardcode GPIO numbers in sketch files.

## Project-Specific Constraints

- `include/config.h` is gitignored — never commit Wi-Fi credentials.
  Copy `include/config.h.example` to `include/config.h` before building.
- DCS-BIOS over Wi-Fi (UDP multicast 239.255.50.10:5010 recv, UDP 7778 send).
  No third-party DCS-BIOS library — direct WiFiUDP only.
- USB composite: CDC + HIDKeyboard + HIDMouse + HIDGamepad. Build flags
  `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1` are both required.
- ADC1 only (GPIO 1–10) for the pot. ADC2 is unreliable with Wi-Fi active.
- ANT ELEV is NOT in DCS-BIOS F-16C. Uses Gamepad x-axis bound in DCS controls menu.
- AP switches use Gamepad buttons (held). DCS-BIOS is receive-only for sync comparison + MASTER CAUTION.
- DCS-BIOS identifiers verified in `docs/superpowers/plans/2026-05-30-brew370-initial.md` Task 0.
