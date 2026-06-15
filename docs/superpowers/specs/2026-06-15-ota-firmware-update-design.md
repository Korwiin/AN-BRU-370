# OTA Firmware Update — Design Spec

**Date:** 2026-06-15
**Target firmware:** v0.15+
**Board:** ESP32-S3 SuperMini (4 MB flash, OTA partitions already configured)

---

## Overview

Manual HTTP OTA update triggered from the Settings menu. Device fetches a JSON manifest from a GitHub raw URL, compares the version field against the running `FIRMWARE_VERSION_BCD`, and if newer, downloads the binary from a GitHub Release asset and flashes it via the ESP32 `Update` library. HTTPS uses `setInsecure()` (no certificate pinning).

---

## 1. Module Interface

New files: `src/ota.cpp`, `include/ota.h`.

```cpp
namespace OTA {
  struct CheckResult {
    bool     available;
    uint16_t versionBCD;  // e.g. 0x0015
    char     url[256];    // release asset URL from manifest
  };

  CheckResult check();                        // fetch manifest, compare version
  bool        perform(void(*progress)(int));  // download + flash, calls progress(0..100)
}
```

Both functions are blocking. `check()` takes 1–3 s (HTTP round-trip). `perform()` takes tens of seconds; the `progress` callback is the only OLED update path during flashing. On successful flash `perform()` calls `ESP.restart()` and does not return. On failure it returns `false`.

HTTPS transport: `WiFiClientSecure` with `setInsecure()`. Redirect following enabled via `HTTPClient::setFollowRedirects(HTTPC_STRICT_RFC2616)` (required for GitHub Release asset CDN redirects).

JSON parsing: manual `strstr` search — no ArduinoJson dependency. Manifest is two fields; the parser extracts `version` (integer) and `url` (string).

---

## 2. Manifest Format and GitHub URLs

**Manifest file:** `ota/manifest.json` (committed to repo root, served via GitHub raw)

```json
{"version": 20, "url": "https://github.com/YOUR_GITHUB_USER/Brew370/releases/download/v0.20/firmware.bin"}
```

`version` is the decimal integer value of `FIRMWARE_VERSION_BCD` (e.g. `0x0014` = 20). Comparison: `manifest.version > FIRMWARE_VERSION_BCD` → update available.

**Manifest URL** (in `config.h`):
```c
#define OTA_MANIFEST_URL "https://raw.githubusercontent.com/YOUR_GITHUB_USER/Brew370/main/ota/manifest.json"
```

**Binary URL:** embedded in the manifest `url` field; points to a GitHub Release asset. `HTTPClient` follows the CDN redirect automatically.

**Version display string** derived from BCD at runtime — no string field in manifest:
```cpp
snprintf(buf, sizeof(buf), "%X.%02X", result.versionBCD >> 8, result.versionBCD & 0xFF);
// 0x0015 → "0.15"
```

**Initial manifest:** set to current shipped version so a fresh device sees "Up to date":
```json
{"version": 20, "url": "https://github.com/YOUR_GITHUB_USER/Brew370/releases/download/v0.14/firmware.bin"}
```

---

## 3. UX Flow / OLED States

Six new values added to the `s_mode` enum in `main.cpp`:

| State | OLED | Controls |
|---|---|---|
| `FIRMWARE_MENU` | Left: `AN/BRU-370 / v0.14`  Right: `> Check / Back` | SP → `FIRMWARE_CHECKING`; LP → `SETTINGS` |
| `FIRMWARE_CHECKING` | Full screen: `Checking...` | Blocking — no input |
| `FIRMWARE_UP_TO_DATE` | `Up to date / v0.14` | SP or 3 s timeout → `SETTINGS` |
| `FIRMWARE_CONFIRM` | `v0.15 avail / SP=Update / LP=Cancel` | SP → `FIRMWARE_UPDATING`; LP → `SETTINGS` |
| `FIRMWARE_UPDATING` | `Updating... / [progress bar] / XX%` | Blocking — no input |
| `FIRMWARE_ERROR` | `Update failed / <reason>` | SP → `SETTINGS` |

Error reasons (one-line, shown on OLED):
- `No WiFi` — WiFi not connected when Check is pressed
- `No server` — manifest HTTP error or network timeout
- `Bad manifest` — manifest parse failed
- `Flash failed` — `Update` library write error

On successful flash: `perform()` calls `Update.end()` then `ESP.restart()` — no extra confirmation screen.

---

## 4. Settings Menu Changes

Current (7 items): `Knob, Brightness, LCD Sleep, WiFi, Mouse Tune, Reboot, EXIT`

New (8 items): `Knob, Brightness, LCD Sleep, WiFi, Mouse Tune, Firmware, Reboot, EXIT`

"Firmware" is inserted at index 5; Reboot shifts to 6, EXIT to 7.

**`ui.cpp` changes:**
- Add `"Firmware"` to `s_menuItems[]`
- `kNumMenuItems`: 7 → 8
- Right-panel label: `"Fw Upd"` (fits 5x7 font in ~57 px)

**`main.cpp` changes:**
- `SETTINGS` handler: `% 7` → `% 8`
- `executeMenuItem()`: add `case 5` → `s_mode = FIRMWARE_MENU`
- Existing `case 5` (Reboot) → `case 6`; `case 6` (EXIT) → `case 7`

---

## 5. Config and Build

**`config.h.example`** — add one line:
```c
#define OTA_MANIFEST_URL "https://raw.githubusercontent.com/YOUR_GITHUB_USER/Brew370/main/ota/manifest.json"
```

**`platformio.ini`** — no changes. `HTTPClient`, `WiFiClientSecure`, and `Update` are all part of the ESP32 Arduino core.

**`ota/manifest.json`** — new file at repo root (not inside `src/`). Committed to main; updated as part of every release.

---

## 6. Release Workflow

1. Build release binary (`pio run -e esp32s3_supermini_release`)
2. Create GitHub Release tagged `vX.XX`, upload `firmware.bin` as release asset
3. Update `ota/manifest.json`: bump `version` integer and `url`
4. Bump `FIRMWARE_VERSION` and `FIRMWARE_VERSION_BCD` in `config.h`
5. Commit and push `ota/manifest.json` to main — devices will see the update on next check

---

## 7. Flash Budget

| | Size |
|---|---|
| OTA slot (app0 / app1) | 1,280 KB |
| Current firmware (v0.14) | ~968 KB |
| Estimated OTA module overhead | ~80–100 KB |
| Remaining headroom | ~212 KB |
