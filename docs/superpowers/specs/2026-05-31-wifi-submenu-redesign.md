# Spec: WiFi Submenu Redesign

**Date:** 2026-05-31
**Status:** Approved

## Problem

The existing WiFi submenu uses a plain header+list layout that doesn't match the split left/right style of every other settings screen. It also includes a "Serial Entry" option that is broken in practice — users can't find the serial port when the device is in USB HID composite mode. The menu offers no way to reconnect without rebooting.

## Goals

1. Redesign `showWifiSubMenu` to match the parent settings menu split layout.
2. Replace "Serial Entry" with a Bluetooth terminal entry using NimBLE Nordic UART Service.
3. Add a "Connect" option to reconnect with saved credentials without rebooting.
4. Show current SSID and IP on the left panel so the user knows the current state at a glance.

## UI Layout

`showWifiSubMenu` takes `sel`, `offset`, `ssid` (current SSID string), and `ip` (current IP string or `"--"`).

**Left panel (x=0–63, font 5x7):**
```
WiFi
SSID: MyNet
IP: 192.168.1.5
```
SSID truncated to ~10 chars. IP shows `--` when not connected.

**Right panel (x=65–127, font 5x7), 4-item scrolling list:**
```
Manual
Bluetooth
Connect
Back
```
`s_wifiSubSel` wraps over 4 items. `s_wifiSubOffset` scrolls when selection goes out of the 4-row window (same pattern as `showSettingsMenu`).

## New Screens

### Confirmation screen — `showWifiConfirm()`

Shown before BLE starts. Full 128×32, font 5x7:
- Left panel: `"BLE Setup"`
- Right panel lines: `"WiFi will"` / `"disconnect."` / `"SP=OK LP=No"`

### BLE Active screen — `showBleActive()`

Shown while NimBLE session is running. Full 128×32, font 5x7:
- Left panel: `"BLE Active"`
- Right panel lines: `"AN/BRU-370"` / `"Waiting..."` / `"LP=Cancel"`

## BLE Terminal Flow

**Library:** `h2zero/NimBLE-Arduino` added to `platformio.ini` `lib_deps`. Nordic UART Service (NUS) UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, TX char `...CA9F`, RX char `...CAA0` (standard NUS UUIDs).

**Device advertised name:** `AN/BRU-370`

**Trigger:** User selects "Bluetooth" in the WiFi menu.

**Confirmation:** `showWifiConfirm()` displayed. SP proceeds; LP cancels and returns to WiFi menu.

**Session start:**
1. `WiFi.disconnect(true)` — radio freed for BLE.
2. NimBLE server starts, advertises as `AN/BRU-370`.
3. OLED shows `showBleActive()`. LP on device at any time stops BLE, reconnects WiFi, returns to menu.

**Terminal prompt sent to phone on connect:**
```
=== AN/BRU-370 WiFi Setup ===

Current SSID: MyNetwork

Enter new SSID:
```

**Flow:**
1. Device waits for SSID line (newline-terminated). Empty line retries.
2. Device echoes SSID, sends `Enter Password:`.
3. Device waits for password line.
4. Device sends confirmation:
   ```
   SSID: MyNetwork
   Pass: hunter2

   Save & Reboot (Y), Retry (r), Cancel (n):
   ```
5. Response handling:
   - `Y` / `y` → `WifiMgr::saveCredentials()` + `ESP.restart()`
   - `r` / `R` → loop back to step 1 (re-prompt SSID)
   - `n` / `N` or empty → NimBLE stops, `WifiMgr::startConnect()`, return to WiFi menu
6. LP on device at any point → same as `n`.

**Phone-side apps:** Serial Bluetooth Terminal (Android), Bluefruit Connect (iOS). Any NUS-compatible BLE terminal works.

## Connect Option

Calls `WifiMgr::reconnect()` (new function: resets `s_connected = false` then calls `startConnect()` — `startConnect()` already handles `WiFi.disconnect` + `WiFi.begin`). Polls using the existing splash loop pattern, showing `showWifiConnecting(ssid)`, then `showWifiConnected` or `showWifiFailed`. Returns to WiFi menu after result.

## New / Changed API

### `wifi_mgr.h` additions
```cpp
// Reset connected state and restart connection with saved credentials (non-blocking).
// startConnect() already handles WiFi.disconnect internally.
void reconnect();

// Returns the device's current IP as a string, or "--" if not connected.
const char* activeIP();

// Runs BLE UART credential entry session.
// oledActiveCb: called every loop tick to redraw the BLE Active screen.
// cancelCb: return true to abort (LP check).
// Returns true if credentials were saved (triggers reboot); false if cancelled.
bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
```

### `ui.h` additions / changes
```cpp
// Redesigned: split layout, shows current SSID and IP on left panel.
void showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip);

void showWifiConfirm();   // "Are you sure?" before BLE starts
void showBleActive();     // displayed while NimBLE session is running
```

### `main.cpp` changes
- `s_wifiSubSel` wraps over 4 (not 3).
- Add `s_wifiSubOffset` for scrolling (currently none needed since all 4 fit; kept for consistency).
- `WIFI_MENU` handler: cases 0=Manual, 1=Bluetooth, 2=Connect, 3=Back.
- `showWifiSubMenu` call passes `WifiMgr::activeSSID()` and `WifiMgr::activeIP()`.
- Remove "Serial Entry" case entirely.

### `platformio.ini`
```
h2zero/NimBLE-Arduino@^1.4.0
```
added to `lib_deps`.

## Out of Scope

- Displaying password on the left panel (unnecessary and a security concern).
- BLE pairing/bonding (open NUS service is sufficient for local credential entry).
- Persisting BLE state across reboots.
