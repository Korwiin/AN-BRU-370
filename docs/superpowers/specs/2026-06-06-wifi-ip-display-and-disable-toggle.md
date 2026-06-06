# WiFi IP Display and Disable Toggle

**Date:** 2026-06-06  
**Status:** Approved

## Goal

Two WiFi submenu improvements:
1. Show only the last 2 octets of the IP address, prefixed with `x.x.`, so the full address is readable within the 63px left panel.
2. Add a WiFi enable/disable toggle as the first item of the WiFi submenu. Toggling saves to NVS and reboots.

---

## Feature 1: IP Display

### Change

In `src/ui.cpp` `showWifiSubMenu()`, replace the current truncation:

```cpp
snprintf(ipLine, sizeof(ipLine), "IP:%.9s", ip);
```

with a 2-octet parse:

```cpp
const char* after2 = ip;
int dots = 0;
for (const char* p = ip; *p; p++) {
  if (*p == '.' && ++dots == 2) { after2 = p + 1; break; }
}
char ipLine[16];
snprintf(ipLine, sizeof(ipLine), "IP: x.x.%s", after2);
```

### Result

| IP | Old display | New display |
|---|---|---|
| 192.168.1.200 | `IP:192.168.1` | `IP: x.x.1.200` |
| 10.0.0.5 | `IP:10.0.0.5` | `IP: x.x.0.5` |
| *(not connected)* | `IP:` | `IP: x.x.` |

`"IP: x.x.1.200"` = 14 chars at 5x7 font — fits within the 63px left panel.

When not connected, `WifiMgr::activeIP()` returns `""` → `after2` stays pointing at `""` → displays `"IP: x.x."`. Acceptable — same as current blank-IP behavior.

### ipLine buffer

Current declaration in `showWifiSubMenu()` is `char ipLine[13]` — must increase to `char ipLine[16]` to hold `"IP: x.x."` + up to 7 chars of last 2 octets (`"255.255"`) + null = 16.

---

## Feature 2: WiFi Disable Toggle

### Data

- NVS key: `wifi_en` (int, default 1)
- Runtime variable: `static bool s_wifiEnabled` in `main.cpp`
- Loaded in `loadNvs()`: `s_wifiEnabled = prefs.getInt("wifi_en", 1);`

### Boot sequence changes (`src/main.cpp` `setup()`)

Current:
```cpp
if (!s_wifiCancelled) WifiMgr::startConnect();
unsigned long wifiStart = millis();
{ while (...) { ... } }
```

New — wrap both the `startConnect()` call and the 30s splash loop in `s_wifiEnabled`:
```cpp
if (s_wifiEnabled && !s_wifiCancelled) WifiMgr::startConnect();
if (s_wifiEnabled) {
  unsigned long wifiStart = millis();
  { while (...) { ... } }
}
```

### Background poll guard (`src/main.cpp` `loop()`)

Current:
```cpp
if (!s_dcsBiosStarted && !s_wifiCancelled) {
```
New:
```cpp
if (!s_dcsBiosStarted && !s_wifiCancelled && s_wifiEnabled) {
```

### WiFi submenu structure

5 items (was 4). Modulo updates from 4 → 5. Display shows 4 at a time (scrolling if needed).

| # | Label | Behaviour |
|---|---|---|
| 0 | `WiFi:ON` / `WiFi:OFF` | Toggle, save, reboot |
| 1 | Manual | Unchanged |
| 2 | Bluetooth | Unchanged |
| 3 | Connect | Guard: if `!s_wifiEnabled`, go directly to SETTINGS |
| 4 | Back | Unchanged |

### Toggle action (sel == 0 in WiFi submenu)

```cpp
s_wifiEnabled = !s_wifiEnabled;
{ Preferences p; p.begin("brew", false); p.putInt("wifi_en", s_wifiEnabled ? 1 : 0); p.end(); }
UI::showSaved();
ESP.restart();
```

Same save → reboot pattern as Screen Size change.

### UI function signature

`showWifiSubMenu()` gains a `bool wifiEnabled` parameter:

```cpp
void UI::showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip, bool wifiEnabled);
```

The first item in the items array becomes dynamic — rendered as `"WiFi:ON"` or `"WiFi:OFF"` based on `wifiEnabled`. The remaining 4 items are static strings.

`include/ui.h` declaration updated to match.

### Call site in main.cpp

```cpp
UI::showWifiSubMenu(s_wifiSubSel, s_wifiSubOffset,
                    WifiMgr::activeSSID(), WifiMgr::activeIP(),
                    s_wifiEnabled);
```

---

## Files Touched

| File | Change |
|---|---|
| `src/ui.cpp` | IP parse logic; `showWifiSubMenu` gains `wifiEnabled` param + first item label; `ipLine` buffer 13→16 |
| `include/ui.h` | `showWifiSubMenu` signature + `wifiEnabled` param |
| `src/main.cpp` | `s_wifiEnabled` declaration; `loadNvs()` reads `wifi_en`; `setup()` guards; `loop()` poll guard; WiFi submenu toggle handler; modulo 4→5; `showWifiSubMenu` call gains param |
