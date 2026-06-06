# DCS-BIOS Init on Manual WiFi Connect

**Date:** 2026-06-07
**Status:** Approved

## Goal

When the user manually connects via Settings → WiFi → Connect and it succeeds, automatically start DCS-BIOS so cockpit data flows without requiring a reboot.

## Problem

Currently, a successful manual connect returns to Settings with WiFi active but `s_dcsBiosStarted = false`. The background poll in `loop()` is guarded by `!s_wifiCancelled && s_wifiEnabled` but not by connection state — however, `WifiMgr::pollConnect()` returns false once already connected, so DCS-BIOS never starts. The user must reboot to get cockpit data.

## Change

**File:** `src/main.cpp`
**Location:** `s_wifiSubSel == 3` (Connect) handler, inside the `if (connected)` branch, after `UI::showWifiConnected(...)`

Add:
```cpp
if (!s_dcsBiosStarted) {
  DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                 "255.255.255.255", DCSBIOS_CMD_PORT);
  s_dcsBiosStarted = true;
}
```

The `!s_dcsBiosStarted` guard prevents double-init if the background `loop()` poll happened to start DCS-BIOS concurrently, or if the user presses Connect on an already-connected device.

## No UI Changes Needed

The existing `loop()` sync detection (`showSyncing()` → `showSynced()`) fires as soon as `DcsBios::isConnected()` becomes true after the first export frame arrives. No additional feedback is required.

## Version Bump

0.09 → 0.10

## Files Touched

| File | Change |
|---|---|
| `src/main.cpp` | 3 lines added inside the Connect handler's `if (connected)` branch |
| `include/config.h` | Version bump (gitignored, local only) |
