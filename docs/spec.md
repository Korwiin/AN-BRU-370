# AN/BRU-370 Firmware Specification

**Device:** AN/BRU-370 cockpit panel controller for DCS World F-16C (Viper)  
**Platform:** ESP32-S3 Super Mini (DWEII)  
**Current firmware:** v0.13

---

## System Overview

The AN/BRU-370 is a USB composite HID + Wi-Fi peripheral that bridges physical cockpit controls to DCS World. It presents as two HID interfaces simultaneously: a CDC serial port (for programming) and an absolute digitizer (for map interactions). Over Wi-Fi it receives cockpit state from DCS-BIOS and sends switch commands back.

Physical controls:
- **Rotary encoder** — navigation, value adjustment, and absolute pointer control

---

## Boot Sequence

On power-up the OLED shows the splash screen ("AN/BRU-370" in large font centered vertically).

If Wi-Fi is enabled, the device begins connecting immediately. A 1-pixel progress bar grows left-to-right across the bottom of the splash screen during the 30-second connection window.

| Input during splash | Effect |
|---|---|
| Encoder turn or short press | Dismisses splash early; Wi-Fi continues connecting in background |
| Long press | Cancels Wi-Fi entirely; device enters HID-only mode |
| No input, 30s elapsed | Splash dismisses; background polling continues |

On successful connect: "WiFi Connected" text replaces the progress bar for up to 1.5 seconds (dismissable by encoder). DCS-BIOS starts automatically.

After the splash the device enters the main operating loop.

---

## OLED Sleep

The display sleeps after a configurable idle period (default 45 seconds, adjustable 5–120 s in Settings). Any encoder input or an active alert wakes it.

---

## Settings Menu

Accessed by **long pressing the encoder** from the main loop.

The menu uses a split layout on the 128×32 OLED:

**Left panel (static status)**
- Device name: `AN/BRU-370`
- Firmware version
- `WiFi: OK` or `WiFi: --`
- `DCS: OK` or `DCS: --`

**Right panel (scrolling menu, 4 items visible at a time)**

| Item | Behavior |
|---|---|
| Knob | Toggles encoder rotation direction (CW = up or CW = down). Saves immediately. |
| Brightness | Live OLED brightness bar. Short press saves, long press cancels. |
| LCD Sleep | Adjusts sleep timeout in seconds. Short press saves, long press cancels. |
| WiFi | Opens WiFi submenu. |
| Mouse Tune | Opens Mouse Position Tuning submenu. |
| Reboot | Restarts the ESP32. |
| EXIT | Returns to main operating loop. |

---

## WiFi Subsystem

### WiFi Submenu

**Left panel (status)**
- Label: `WiFi`
- Current SSID (truncated if needed)
- IP address showing last two octets only: `x.x.N.N`

**Right panel (5 items)**

| Item | Behavior |
|---|---|
| WiFi:ON / WiFi:OFF | Toggles Wi-Fi enable state, saves to NVS, reboots. |
| Manual | Encoder-based character entry for SSID and password. Saves credentials to NVS. |
| Bluetooth | BLE terminal credential entry (see below). |
| Connect | Reconnects with saved credentials (15s timeout). Starts DCS-BIOS if connect succeeds. Disabled if Wi-Fi is toggled OFF. |
| Back | Returns to Settings. Long press also returns. |

### Manual Entry

The encoder scrolls through a character set. Short press advances to the next field (SSID → Password → Confirm). Long press cancels at any point. Credentials are saved to NVS on confirm.

### BLE Terminal Entry

Selecting **Bluetooth** shows a confirmation screen (short press proceeds, long press cancels). On confirm:

1. Wi-Fi disconnects to free the radio.
2. The device advertises as `AN/BRU-370` using Nordic UART Service (NUS) over BLE.
3. The OLED shows a "BLE Active" screen with `LP=Cancel`.
4. A BLE terminal app (Serial Bluetooth Terminal on Android, Bluefruit Connect on iOS) connects and is prompted to enter SSID then password.
5. On confirmation `Y`: credentials saved, device reboots into the new network.
6. On `n` or cancel (long press on device): BLE stops, Wi-Fi reconnects, returns to menu.

### Background Polling

If the splash was dismissed before Wi-Fi connected, the main loop continues polling in the background. DCS-BIOS starts automatically on the first successful connect, unless Wi-Fi was cancelled or disabled.

---

## Mouse Position Tuning

Title: **MOUSE POSITION TUNING** (accessed from Settings → Mouse Tune)

6-item scrolling menu, 3 visible at a time:

| # | Item | What it calibrates |
|---|---|---|
| 0 | Screen Size | Display resolution used for absolute coordinate scaling |
| 1 | Map Pin Tool POS | Location of the F10 map pin-tool button |
| 2 | Map Center POS | Location of the map center point |
| 3 | Pin Label POS | Location of the label text field when a pin dialog opens |
| 4 | Click Out POS | Location clicked to dismiss a dialog (CDRP macro) |
| 5 | Back | Returns to Settings |

### Calibration Flow (items 1–4)

Selecting a position target enters live calibration:
1. The cursor moves to the current saved position.
2. Encoder left/right adjusts the X coordinate; short press locks X and moves to Y.
3. Encoder up/down adjusts the Y coordinate; short press saves both coordinates to NVS and returns to the menu.
4. Long press on either axis cancels without saving.

**Pin Label POS** additionally drops a pin before entering calibration, so the label dialog is open while the user positions the cursor over the text field.

Each confirmed calibration saves immediately to NVS — there is no separate Save step.

---

## DCS-BIOS Integration

DCS-BIOS streams cockpit state over UDP multicast. The device receives the stream, decodes relevant words, and sends switch commands back as plain-text UDP.

The connection is considered active if a valid export frame arrived within the last 3 seconds. "DCS: OK" on the status panel reflects this.

### Decoded signals

| Signal | What it controls |
|---|---|
| MASTER CAUTION light | Triggers MC takeover alert |
| RWR MISSILE LAUNCH light | Triggers missile launch alert |
| STORES CONFIG light | Triggers Stores Config smart-send flow |
| STORES CONFIG switch position | Used to determine correct target position |

### Commands sent

| Command | Trigger |
|---|---|
| `MASTER_CAUTION` | Encoder short press while MC alert is active |
| `CMDS_DISPENSE_BTN` | Encoder short press while missile launch alert is active |
| `STORES_CONFIG_SW` | Encoder short press while Stores Config alert is active |

---

## OLED Takeover Alerts

When an alert fires, it preempts the normal display and the encoder action changes. Alerts are prioritized: a higher-priority alert always takes the display.

### Priority 1 — RWR Missile Launch

**Trigger:** `LIGHT_RWR_MSL_LAUNCH` active for ≥ 200 ms  
**Display:** `MISSILE / LAUNCH` flashing at ~100 ms rate  
**Encoder short press:** Sends a CMDS dispense pulse (repeatable)  
**Clears when:** RWR signal drops

If the OLED is sleeping when a missile launch fires, it wakes immediately.

### Priority 2 — Master Caution

**Trigger:** `MASTER_CAUTION` light active for ≥ 200 ms  
**Display:** `MASTER / CAUTION` flashing at ~200 ms rate  
**Encoder short press:** Sends MC reset command  
**Clears when:** MC light drops

If the OLED is sleeping when MC fires, it wakes immediately.

### Priority 3 (within MC) — Stores Config Smart-Send

When MC is active and the STORES CONFIG light is also on, the device enters the Stores Config flow instead of the plain MC display.

**Display:** `STORES CONFIG` flashing (same as MC rate)  
**Encoder short press:** Sends `STORES_CONFIG_SW` to the opposite of the current switch position, then monitors for confirmation.

**State machine:**

```
SC_IDLE → (press) → SC_WAITING_SW → (switch confirms) → SC_WAITING_LIGHT → (light clears) → SC_IDLE
                          ↓ retry every 500 ms                  ↓ 3 s timeout expires
                       (resend command)                      SC_GAVE_UP
```

- **SC_WAITING_SW:** Polls for the switch to reach the target position. Resends the command every 500 ms if not yet confirmed.
- **SC_WAITING_LIGHT:** Waits for the Stores Config light to clear. Times out after 3 seconds from the initial press.
- **SC_GAVE_UP:** Light did not clear in time. Falls back to standard MC display so the player can short-press to reset MC manually. Resets to SC_IDLE when the light eventually clears or MC drops.

The Stores Config state resets to SC_IDLE if MC clears, DCS-BIOS stream is lost, or the light clears while in SC_IDLE.
