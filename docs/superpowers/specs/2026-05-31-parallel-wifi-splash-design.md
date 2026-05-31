# Design: Parallel WiFi + Splash Progress Bar

**Date:** 2026-05-31
**Scope:** `include/wifi_mgr.h`, `src/wifi_mgr.cpp`, `include/ui.h`, `src/ui.cpp`, `src/main.cpp`

---

## Goal

Start WiFi connecting in parallel with the splash screen. Show a 1px progress bar growing left-to-right across the bottom row during the 30s timeout. On connect, replace the bar with a brief "WiFi Connected" confirmation. Long press cancels WiFi; short press / encoder turn dismisses splash while WiFi keeps connecting in background.

---

## Splash Screen States

### Connecting (progress bar)
```
┌────────────────────────────────┐
│                                │
│        AN/BRU-370              │  ← large font, centered H+V (baseline y=29)
│                                │
│█████████████░░░░░░░░░░░░░░░░░░░│  ← 1px bar at y=31, fill = elapsed*128/30000
└────────────────────────────────┘
```

### Connected (brief confirmation)
```
┌────────────────────────────────┐
│                                │
│        AN/BRU-370              │
│▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  ← black strip clears overlap (y=25..31)
│       WiFi Connected           │  ← u8g2_font_4x6_tr, centered, baseline y=31
└────────────────────────────────┘
```

The large text has no descenders so rows y=30–31 are naturally clear. The black strip only appears in the connected state to prevent the small font overlapping the large font's lower pixels.

---

## Input Behaviour During Splash

| Input | Effect |
|-------|--------|
| Encoder turn or short press | Dismiss splash; WiFi keeps connecting in background; enter macro menu |
| Long press | `cancelConnect()`, set `s_wifiCancelled = true`; enter HID-only macro menu |
| 30s timeout with no connection | Enter HID-only macro menu |
| WiFi connects | Show "WiFi Connected" up to 1.5s (encoder-dismissable); call `DcsBios::begin()`; enter macro menu |

---

## WiFi Timeout

`kWifiConnectTimeoutMs = 30000UL` — defined in `wifi_mgr.h` (main.cpp uses it for the splash loop duration).

Progress bar fill: `fill = (int)((elapsed_ms * 128UL) / kWifiConnectTimeoutMs)` clamped to 128.

---

## HID-Only vs Full Mode

| Condition | DCS-BIOS | WiFi status display |
|-----------|----------|---------------------|
| WiFi connected (setup or background) | Started | WiFi:OK |
| Splash dismissed early, WiFi pending | Not yet started | WiFi:-- until connected |
| Long press cancel | Never started | WiFi:-- |
| 30s timeout | Background polling continues; starts if AP responds late | WiFi:-- until connected |

---

## Architecture Changes

### `wifi_mgr.h` / `wifi_mgr.cpp`

Replace `begin()` with three functions:

```cpp
// Load credentials and call WiFi.begin() — returns immediately (non-blocking)
void startConnect();

// Call each loop tick while connecting.
// Updates s_connected on first WL_CONNECTED. Returns true exactly once (on connect).
bool pollConnect();

// Abort connection attempt. Sets WiFi to idle.
void cancelConnect();

// Timeout constant used by main.cpp splash loop
constexpr unsigned long kWifiConnectTimeoutMs = 30000UL;
```

`isConnected()` and `activeSSID()` unchanged.

`startConnect()` contains all credential loading + `WiFi.setHostname()` + `WiFi.mode()` + `WiFi.begin()` (currently in `begin()`).

`pollConnect()`:
```cpp
bool WifiMgr::pollConnect() {
  if (s_connected) return false;
  if (WiFi.status() == WL_CONNECTED) {
    s_connected = true;
    return true;
  }
  return false;
}
```

`cancelConnect()`:
```cpp
void WifiMgr::cancelConnect() {
  WiFi.disconnect(true);
}
```

### `ui.h` / `ui.cpp`

New function (replaces current one-shot `showSplash()` during setup):

```cpp
// Redraws splash with bottom-row status.
// wifiOk=false: draws progress bar of `fill` pixels (0..128) at y=31.
// wifiOk=true:  clears bottom strip and shows "WiFi Connected" centered at y=31.
void showSplashProgress(int fill, bool wifiOk);
```

`showSplash()` is retained for any future use.

Implementation notes:
- Large title: same font selection logic as `showSplash()` (logisoso26 → t0_22b_mr fallback)
- Vertical centering: `y = (32 + u8g2.getAscent()) / 2` — unchanged
- Progress bar: `u8g2.drawBox(0, 31, fill, 1)` (only if `fill > 0 && !wifiOk`)
- WiFi Connected: `u8g2.setDrawColor(0); u8g2.drawBox(0, 25, 128, 7); u8g2.setDrawColor(1);` then draw text

### `main.cpp`

**New state variables:**
```cpp
static bool s_wifiCancelled  = false;
static bool s_dcsBiosStarted = false;
```

**`setup()` — replace current splash + WiFi block:**
```
1. WifiMgr::startConnect()
2. Record wifiStart = millis()
3. Splash loop (max kWifiConnectTimeoutMs):
   a. Poll WifiMgr::pollConnect() each tick
   b. Compute fill = elapsed * 128 / kWifiConnectTimeoutMs
   c. Call UI::showSplashProgress(fill, connected)
   d. If connected: show "WiFi Connected" for up to 1.5s (encoder-dismissable), then DcsBios::begin(), s_dcsBiosStarted=true, break
   e. If long press: WifiMgr::cancelConnect(), s_wifiCancelled=true, break
   f. If short press or encoder turn: break (WiFi keeps going)
   g. If elapsed >= timeout: break (HID-only)
4. Encoder::flush()
5. Hardware::begin()
```

**`loop()` — background WiFi connect:**
```cpp
if (!s_dcsBiosStarted && !s_wifiCancelled) {
  if (WifiMgr::pollConnect()) {
    DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                   "255.255.255.255", DCSBIOS_CMD_PORT);
    s_dcsBiosStarted = true;
  }
}
```

Remove existing `showWifiConnecting` / `showWifiConnected` / `showWifiFailed` calls from `setup()` — they are superseded by the splash progress display.

---

## Out of Scope

- `showWifiConnecting`, `showWifiFailed`, `showWifiConnected` — kept in `ui.cpp` for potential future use, just not called from `setup()` any more.
