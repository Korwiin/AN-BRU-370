# AN/BRU-370 Firmware Specification

**Device:** AN/BRU-370 cockpit panel controller for DCS World F-16C (Viper)
**Platform:** ESP32-S3 Super Mini (DWEII)
**Current firmware:** v0.87

---

## System Overview

The AN/BRU-370 is a USB composite HID + Wi-Fi peripheral that bridges physical cockpit controls to DCS World. It presents two HID interfaces: a keyboard and Pen/pointer for map interactions. Over Wi-Fi it receives cockpit state from DCS-BIOS and sends switch commands back.

Physical controls:
- **Rotary encoder** — Menu navigation, settings adjustment, and sending actions to DCS-BIOS

---

## Boot Sequence

### USB Settle (3 seconds)

On power-up the OLED shows the boot status screen with all indicators pending. This 3-second window allows USB-OTG enumeration to complete before the Wi-Fi radio starts (shared PMU; see decision log).

### No Credentials Check

After the settle period, if no Wi-Fi credentials are stored in NVS, the device shows a "No WiFi Setup" prompt and routes directly to the Wi-Fi menu. Normal boot continues once credentials are saved.

### Boot Status State

The device enters the BOOT_STATUS state machine and calls `startWifi()` once — WiFi is mandatory; there is no disable option. The OLED shows three indicators on one line, each either pending (`--`), spinning (`-\|/` at 250 ms/frame), or showing live data once that phase completes:

| Slot | Pending | Spinning | Done |
|---|---|---|---|
| WiFi | *(starts spinning immediately)* | until associated | auth mode text: `WPA2` or `WPA3` |
| IP | `--` until WiFi associates | until DHCP completes | last octet as plain digits, e.g. `42` |
| DCS | `--` until IP arrives | continuously once IP arrives — never shows a "done" state (the screen exits ~1.5 s after DCS connects, so a completed visual would never actually be seen) | — |

A status line below shows exactly one of three states, centered:

| Condition | Text |
|---|---|
| A WiFi failure reason is active | `<reason> Retry N` — N counts down from 30 to 0 each second, then retries |
| No IP yet, no failure | `Connecting ...` |
| IP acquired, DCS not yet connected | `Waiting for DCS ...` |

**There is no attempt limit or "boot failed" terminal state.** `WiFi.setAutoReconnect(true)` is enabled from the start; the arduino-esp32 stack retries indefinitely on its own. One exception: `WIFI_REASON_TIMEOUT` (39) is not in arduino-esp32's auto-retry list, so the boot handler restarts `startWifi()` itself after the 30-second countdown if that specific reason is stuck. See decision log 2026-06-29 for why the old 3-attempt-with-WIFI_OFF-cycling system was replaced.

DCS-BIOS starts automatically on IP assignment.

### Boot Exit

| Condition | Exit |
|---|---|
| IP + DCS both live for 1.5 s | Auto-exits to `homeMode()` |
| Encoder input (turn or short press) when IP + DCS live | Immediate exit to `homeMode()` |
| Long press at any time | Opens Settings |

`homeMode()` returns `BOOT_STATUS` if DCS is not connected (regardless of cause — initial boot, mid-session WiFi drop, or DCS disconnect), or `AIRCRAFT_STATUS`/`NOT_READY` if DCS is connected (depending on the MWS setup flag). `MACRO_MENU` is no longer a `homeMode()` target — it remains reachable only as a manual navigation choice from Aircraft Status while DCS is connected.

---

## OLED Sleep

The display sleeps after a configurable idle period (default 45 s). Range: 10–120 s in 5-second steps, or 0 (never sleep). Any encoder input wakes it. An active alert (MC, RWR) also wakes immediately.

---

## Aircraft Status Screen (Home)

When DCS-BIOS is connected, `homeMode()` shows the Aircraft Status screen. It displays a live readout of key cockpit instruments:

- **Fuel** — totalizer in lbs (e.g. `12,345 lb`)
- **CH:** — chaff count as reported by DCS (raw string, e.g. `  60`, `Lo10`)
- **FL:** — flare count (same format)
- **ECM** — `JAMMING` (blinking) when jammer pod is transmitting, `--` otherwise

When the DCS-BIOS count string has a `Lo` prefix, the count value blinks to indicate low quantity.

### Navigation from Aircraft Status

| Input | Effect |
|---|---|
| Short press | Switch to Macro Menu |
| Encoder turn | Switch to Macro Menu |
| Long press | Open Settings |
| DCS disconnects | Automatically switches to Macro Menu |

---

## Macro Menu

The Macro Menu scrolls through 10 preconfigured DCS map macros. When DCS reconnects from the Macro Menu, the display switches back to the Aircraft Status screen. If the Macro Menu is idle for 15 seconds while DCS is connected, it also auto-returns to Aircraft Status.

| Input | Effect |
|---|---|
| Encoder turn | Scroll macros |
| Short press | Execute selected macro |
| Long press | Open Settings |

### Macros

| # | Name | Action |
|---|---|---|
| 0 | AWACS | Drop map pin labeled `magic11` |
| 1 | FCAP | Drop map pin labeled `fcap` |
| 2 | REAPER | Drop map pin labeled `1688 reaper` |
| 3–9 | CDRP ALPHA–GAMMA | Type CDRP team name + confirm click |

All map macros open the F10 map, click the Pin Tool, move to the drop target, place a pin, type the label, and press F1 to confirm.

---

## Settings Menu

Accessed by **long pressing the encoder** from any home screen (Aircraft Status, Macro Menu, or Boot Status).

**Left panel** — static status
- Device name: `AN/BRU-370`
- Firmware version
- `WiFi: OK` / `WiFi: --`
- `DCS: OK` / `DCS: --`

**Right panel** — 8 items, 4 visible, scrollable

| # | Item | Behavior |
|---|---|---|
| 0 | Knob | Toggles encoder rotation direction. Saves immediately. |
| 1 | Brightness | Live OLED brightness bar. Short press saves, long press cancels. |
| 2 | LCD Sleep | Adjusts sleep timeout. Short press saves, long press cancels. |
| 3 | WiFi | Opens Wi-Fi submenu. |
| 4 | Mouse Tune | Opens Mouse Position Tuning submenu. |
| 5 | Firmware | Checks for OTA firmware update. |
| 6 | Reboot | Restarts the ESP32. |
| 7 | EXIT | Returns to `homeMode()` — `BOOT_STATUS` if DCS is not connected, otherwise `AIRCRAFT_STATUS`/`NOT_READY`. |

---

## Wi-Fi Subsystem

### Wi-Fi Menu

**Left panel** — live status
- Label: `WiFi`
- Current SSID
- IP address (last two octets: `x.x.N.N`)
- RSSI and internet reachability check

**Right panel** — 3 items (WiFi is a mandatory core function — no disable toggle; see decision log 2026-06-29)

| # | Item | Behavior |
|---|---|---|
| 0 | Secrets | Opens the Secrets submenu. |
| 1 | Connect | Reconnects with saved credentials (15 s timeout). Starts DCS-BIOS on success. |
| 2 | Back | Returns to Settings. Long press also returns. |

### Secrets Submenu

Manages stored credentials. Shows current SSID and password status (set/empty).

| # | Item | Behavior |
|---|---|---|
| 0 | Zeroize | Clears both NVS credentials and reboots. |
| 1 | BLE TERM | Launches BLE terminal for credential entry (see below). |
| 2 | Scan | Not yet implemented. |
| 3 | Back | Returns to Wi-Fi menu. Long press also returns. |

### BLE Terminal Credential Entry

Selecting **BLE TERM**:
1. Wi-Fi disconnects to free the shared radio.
2. The device advertises as `AN/BRU-370` using Nordic UART Service (NUS).
3. OLED shows connection state: `WAITING` then `CONNECTED`.
4. A BLE terminal app connects (Serial Bluetooth Terminal / Bluefruit Connect).
5. Device prompts for SSID then password (plain text, 30-char lines).
6. On `Y` confirmation: credentials saved to NVS, device reboots.
7. On `n` or long press on device: BLE stops, returns to Secrets menu without saving.

### Background Wi-Fi

After boot, if Wi-Fi connects while in a non-BOOT_STATUS state (e.g. after a manual Connect from the menu), DCS-BIOS starts automatically on the first successful connection. If WiFi drops mid-session, `homeMode()` routes back to `BOOT_STATUS` (showing the same live indicators as initial boot) rather than to a degraded "no WiFi" screen.

A 60-second runtime watchdog checks for two distinct failure modes: driver desync (the app's connection-state tracking disagrees with `WiFi.status()`) triggers a lightweight `reconnect()`; a confirmed long outage (60+ seconds disconnected) triggers `reconnectFull()`, which does a full radio reset — safe at that point because any AP-side session has long since expired. See decision log 2026-06-29 for why a single short-interval watchdog was replaced with this two-tier approach.

---

## Mouse Position Tuning

Title: **MOUSE POSITION TUNING** (Settings → Mouse Tune)

6-item scrolling menu, 3 visible at a time:

| # | Item | What it calibrates |
|---|---|---|
| 0 | Screen Size | Target display resolution for absolute coordinate scaling |
| 1 | Map Pin Tool POS | Location of the F10 map pin-tool button |
| 2 | Map Center POS | Location of the map drop target |
| 3 | Pin Label POS | Location of the label text field (opens a live pin first) |
| 4 | Click Out POS | Location clicked to dismiss a dialog (CDRP confirm click) |
| 5 | Back | Returns to Settings |

### Screen Size Entry

Digits are entered one at a time (8 digits: WWWW×HHHH). Short press advances through digits; last digit saves and reboots the device.

### Position Calibration Flow (items 1–4)

1. Cursor moves to the current saved position.
2. Encoder turns adjust X; short press locks X and moves to Y axis.
3. Encoder turns adjust Y; short press saves to NVS and returns to menu.
4. Long press on either axis cancels without saving.

Velocity-sensitive steps: `dt < 60 ms → step 40`, `< 100 ms → 10`, `< 200 ms → 3`, else `1`.

**Pin Label POS** drops a live pin before entering calibration so the label dialog is open while positioning.

---

## OTA Firmware Update

Settings → Firmware checks the OTA manifest at the GitHub URL in `config.h`. Requires Wi-Fi.

| State | Display | Input |
|---|---|---|
| Checking | Current version + RSSI | (blocking) |
| Up to date | "Up to date" + version | Short press → back to Settings |
| Update available | Current + new version | Short press → install; Long press → cancel |
| Updating | Progress bar 0–100% | (blocking) |
| Error (check failed) | Error message | Short press → back to Settings |
| Error (perform failed) | Error message | Short press → retry; Long press → cancel |

On successful update the device reboots into the new firmware automatically.

---

## DCS-BIOS Integration

DCS-BIOS streams cockpit state over UDP multicast. The device receives the stream, decodes relevant words, and sends switch commands back as plain-text UDP.

The connection is considered active if a valid export frame arrived within the last 3 seconds.

### Decoded Signals

| Signal | Purpose |
|---|---|
| MASTER CAUTION light | Triggers MC takeover alert |
| RWR MISSILE LAUNCH light | Triggers missile launch alert |
| STORES CONFIG light | Triggers Stores Config smart-send flow |
| STORES CONFIG switch position | Determines correct toggle target |
| Fuel 10K / 1K / 100 dials | Aircraft Status fuel display |
| Chaff amount string | Aircraft Status CH: field |
| Flare amount string | Aircraft Status FL: field |
| ECM TX light | Aircraft Status ECM field |

### Commands Sent

| Command | Trigger |
|---|---|
| `MASTER_CAUTION` | Short press while MC alert active |
| `CMDS_DISPENSE_BTN` | Short press while missile launch alert active |
| `STORES_CONFIG_SW` | Short press while Stores Config alert active |

---

## OLED Takeover Alerts

When an alert fires, it preempts the normal display and the encoder action changes. Higher-priority alerts always take the display.

### Priority 1 — RWR Missile Launch

**Trigger:** `LIGHT_RWR_MSL_LAUNCH` active for ≥ 200 ms
**Display:** `MISSILE / LAUNCH` flashing at ~100 ms
**Short press:** Sends a CMDS dispense pulse (repeatable)
**Clears when:** RWR signal drops
**Sleep wake:** Yes — OLED wakes immediately

### Priority 2 — Master Caution

**Trigger:** `MASTER_CAUTION` light active for ≥ 200 ms
**Display:** `MASTER / CAUTION` flashing at ~200 ms
**Short press:** Sends MC reset pulse
**Clears when:** MC light drops
**Sleep wake:** Yes — OLED wakes immediately

### Priority 3 (within MC) — Stores Config Smart-Send

When MC is active and the STORES CONFIG light is also on, the Stores Config flow preempts the plain MC display.

**Display:** `STORES CONFIG` flashing (MC rate)
**Short press:** Sends `STORES_CONFIG_SW` to opposite of current position; enters state machine.

**State machine:**

```
SC_IDLE → (press) → SC_WAITING_SW → (switch confirms) → SC_WAITING_LIGHT → (light clears) → SC_IDLE
                          ↓ retry every 500 ms                  ↓ 3 s timeout
                       (resend command)                      SC_GAVE_UP
```

- **SC_WAITING_SW:** Polls until switch reaches target. Resends every 500 ms.
- **SC_WAITING_LIGHT:** Waits for Stores Config light to clear. Times out after 3 s.
- **SC_GAVE_UP:** Falls back to standard MC display; player can short-press to reset MC. Resets to SC_IDLE when the light clears.

SC_IDLE resets if MC clears, DCS-BIOS stream is lost, or SC light clears while idle.
