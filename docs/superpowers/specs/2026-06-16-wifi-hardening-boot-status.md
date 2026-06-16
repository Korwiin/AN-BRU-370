# WiFi Hardening & Boot Status Screen Design

**Date:** 2026-06-16
**Status:** Approved for implementation

---

## Goal

Replace the arbitrary boot progress bar with a meaningful six-phase WiFi status display. Harden the WiFi connection sequence with event-driven detection, controlled retry, and proper driver configuration. Make connection failures actionable rather than silent.

---

## Context

WiFi is critical infrastructure on this device — DCS-BIOS, OTA updates, and the operational status panel all depend on it. Current failure modes are opaque: "WIFI FAILED" with no reason, no retry, and no indication of which phase failed. Post-OTA reboots leave the WiFi radio in a stale hardware state that requires a 5-minute power cycle to recover.

---

## Driver Configuration (once at startup, before any WiFi call)

```cpp
WiFi.persistent(false);       // we own credentials in NVS; disable driver cache
WiFi.setAutoConnect(false);   // we own the boot sequence; disable auto-connect
WiFi.setAutoReconnect(true);  // keep — free runtime reconnection on beacon loss
```

**Why each:**
- `persistent(false)`: the driver maintains its own NVS credential cache (separate from our `Preferences` storage). Leaving it enabled creates a second credential copy that drifts out of sync when the user updates via BLE/Serial setup.
- `setAutoConnect(false)`: driver auto-connect fires before `setup()` runs — before OLED is initialized, before we can show feedback, and with no control over retry logic.
- `setAutoReconnect(true)`: this is a runtime feature (reconnects when a live session drops), not a boot feature. It doesn't interfere with our boot sequence and provides free mid-session resilience.

---

## Six-Phase Model

| # | Label | Detection Method | Blocks Retry? |
|---|---|---|---|
| 1 | RF | MAC valid + `WiFi.status() == WL_IDLE_STATUS` after mode cycle | Yes |
| 2 | SSID | `ARDUINO_EVENT_WIFI_SCAN_DONE` event — our SSID present in results | Yes |
| 3 | Eth | `ARDUINO_EVENT_WIFI_STA_CONNECTED` event (Layer 2, before DHCP) | Yes |
| 4 | IP | `ARDUINO_EVENT_WIFI_STA_GOT_IP` event | Yes |
| 5 | DNS | `WiFi.dnsIP() != IPAddress(0,0,0,0)` — checked immediately after IP event | No — advisory |
| 6 | DCS | First valid UDP packet received on `239.255.50.10:5010` | No — advisory |

DNS and DCS are **non-blocking advisory indicators**. The device is fully operational for HID and DCS-BIOS at IP ✓. DNS is only needed for OTA; DCS reflects game state, not device health.

---

## Phase 1: RF Init (per attempt)

Full radio cycle required at the start of every attempt — no phase can be skipped or resumed mid-sequence:

```
WiFi.mode(WIFI_OFF)
delay(500ms)
WiFi.mode(WIFI_STA)
```

**RF ✓ criteria (both must be true):**
- `WiFi.macAddress()` returns a valid non-zero MAC (driver + hardware up)
- `WiFi.status() == WL_IDLE_STATUS` (no stale association state from previous session)

**RF ✗:** MAC is all-zeros, empty, or status is `WL_CONNECTED` immediately (stale state detected). Increment attempt counter; if attempts remain, retry from top.

---

## Phase 2–4: SSID, Eth, IP (event-driven)

Register a `WiFi.onEvent()` handler before calling `WiFi.begin()`. The handler sets flags read by the boot state machine:

```cpp
WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            // check if our SSID appears in scan results → set s_phase_ssid
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            // Layer 2 association confirmed → set s_phase_eth
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            // DHCP complete → set s_phase_ip
            // immediately check WiFi.dnsIP() → set s_phase_dns
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // store info.wifi_sta_disconnected.reason → s_wifiFailReason
            // reason 201 = NO_AP_FOUND (SSID not in range)
            // reason 202 = AUTH_FAIL   (wrong password)
            // reason 204 = HANDSHAKE_TIMEOUT (WPA2 failed)
            // reason 200 = BEACON_TIMEOUT (AP disappeared mid-session)
            break;
    }
});
```

`WiFi.begin(ssid, pass)` is called after the handler is registered. The 15-second per-attempt timeout is tracked in the boot state machine using `millis()`.

---

## Phase 5: DNS (advisory, instant)

Checked immediately when the `STA_GOT_IP` event fires:

```cpp
s_phase_dns = (WiFi.dnsIP() != IPAddress(0, 0, 0, 0));
```

No network call. Confirms DHCP assigned a DNS server. Actual DNS resolution is deferred to `OTA::check()` which already calls `WiFi.hostByName()` when the user initiates an update.

---

## Phase 6: DCS (advisory, indefinite)

`DcsBios::hasData()` — a new function returning `true` after the first valid DCS-BIOS UDP packet is parsed. The UDP socket is opened as soon as IP ✓ (current behaviour). `DCS: --` is shown until the first packet arrives; `DCS: ✓` once it does.

DCS state is mission-dependent. The indicator will cycle `✓ → --` if DCS goes offline (plane destroyed, mission ended) and back to `✓` when data resumes.

---

## Retry Logic

- **Max attempts:** 3
- **Timeout per attempt:** 15 seconds (from `WiFi.begin()` to IP ✓)
- **Between attempts:** full radio reset — `WiFi.mode(WIFI_OFF)` → 500ms → `WiFi.mode(WIFI_STA)`
- **All indicators reset to `--`** at the start of each attempt (accurate — every phase re-runs)
- **After 3 failures:** stay on status screen showing final ✗ state; no further auto-retry; LP=Settings

---

## Boot Status Screen Layout

128×32 OLED, `u8g2_font_5x7_tr`, 4 lines at y=8/16/24/32.

```
┌────────────────────────────────┐
│ AN/BRU-370           v0.25     │  y=8  — title + firmware version
│ RF:✓   SSID:✓   Eth:✓         │  y=16 — phases 1–3
│ IP:✓   DNS:✓    DCS:--         │  y=24 — phases 4–6
│ Attempt 2/3  Connecting...     │  y=32 — retry state / status
└────────────────────────────────┘
```

**Line 4 content by state:**

| State | Line 4 |
|---|---|
| Connecting (attempt 1) | `Connecting...` |
| Retrying | `Attempt 2/3  Retrying...` |
| Connected, waiting for DCS | `LP=Settings` |
| Failed (all attempts) | `<reason>  LP=Settings` |

**Failure reason strings (from disconnect reason code):**

| Reason code | Display string |
|---|---|
| 201 `NO_AP_FOUND` | `SSID not found` |
| 202 `AUTH_FAIL` | `Wrong password` |
| 204 `HANDSHAKE_TIMEOUT` | `Auth timeout` |
| 200 `BEACON_TIMEOUT` | `AP unreachable` |
| other | `WiFi error <code>` |

**Phase indicator symbols** — drawn as primitives (no font switch required):
- `--` — not started / advisory not yet received (two dashes from `drawStr`)
- `✓` — two `drawLine()` calls forming a checkmark at 5×5px
- `✗` — two `drawLine()` calls forming an X at 5×5px
- In-progress: label blinks or `..` suffix while waiting (TBD during implementation)

---

## Boot Status Screen Exit Conditions

| Action | Condition | Destination |
|---|---|---|
| LP | any time | Settings menu |
| Encoder turn | DCS: ✓ only | Macro menu (normal operation) |
| DCS event fires (MC, stores, missile) | implies DCS: ✓ | That alert screen directly |

Encoder turns while `DCS: --` are ignored — the user has no panel state to act on yet.

After any exit, the status screen does not reappear unless the device reboots.

---

## Post-OTA Restart (update to `OTA::perform()`)

Replace current `WiFi.disconnect(true) + delay(200)` with a full radio shutdown:

```cpp
WiFi.mode(WIFI_OFF);
delay(500);
ESP.restart();
```

`WiFi.mode(WIFI_OFF)` triggers `esp_wifi_stop()` and ensures the AP receives a proper deauthentication frame before the reset, preventing the router from holding a stale session that blocks the next boot's association attempt.

---

## Runtime WiFi Watchdog

In `loop()`, periodically check connection health. `WiFi.setAutoReconnect(true)` handles transient beacon loss; the watchdog is the fallback if the driver gives up:

```cpp
// every 5 seconds
if (WifiMgr::isConnected() && WiFi.status() != WL_CONNECTED) {
    WifiMgr::reconnect();  // triggers full retry sequence
}
```

On reconnect: DCS-BIOS rejoins multicast group automatically (existing behaviour). DCS indicator resets to `--` until first packet on new connection.

---

## Files Affected

| File | Change |
|---|---|
| `src/wifi_mgr.cpp` | Add driver config calls; add event handler; restructure `startConnect()` into phase-aware retry loop |
| `include/wifi_mgr.h` | Add `WifiPhase` struct or flags for phase state; expose `failReason()` |
| `src/main.cpp` | Add `BOOT_STATUS` state; wire exit conditions; add runtime watchdog |
| `src/ui.cpp` | Replace `showSplashProgress()` with `showBootStatus()`; add check/X primitive drawing |
| `include/ui.h` | Update `showSplashProgress` → `showBootStatus` signature |
| `src/dcs_bios.cpp` | Add `DcsBios::hasData()` flag |
| `include/dcs_bios.h` | Expose `hasData()` |
| `src/ota.cpp` | Replace pre-restart disconnect with `WiFi.mode(WIFI_OFF)` + 500ms |

---

## Out of Scope

- WiFi network scanning / AP list display
- Static IP configuration
- WPA3 / enterprise authentication
- Multiple WiFi profiles
- Captive portal handling
