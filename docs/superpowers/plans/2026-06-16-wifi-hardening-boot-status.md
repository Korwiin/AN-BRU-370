# WiFi Hardening & Boot Status Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the arbitrary boot progress bar with a meaningful six-phase WiFi status display, harden the connection sequence with event-driven detection and controlled retry, and make failures actionable.

**Architecture:** `WifiMgr::beginAttempt()` cycles the radio (~500 ms), does a blocking channel scan (~2-3 s), and calls `WiFi.begin()` if the SSID is found; a single `WiFi.onEvent()` handler sets volatile flags for Eth/IP/DNS asynchronously. `main.cpp` gains a `BOOT_STATUS` loop-state that drives up to 3 attempts (15 s each), reads phase flags each tick, starts DCS-BIOS on first IP, and exposes exit conditions (LP→Settings, encoder→Macro, DCS event→alert). `UI::showBootStatus()` renders the six-phase grid with `drawLine()` checkmarks and X marks.

**Tech Stack:** ESP32 Arduino WiFi events (`WiFi.onEvent`, `ARDUINO_EVENT_WIFI_*`), u8g2 OLED primitives, PlatformIO C++.

---

## File Map

| File | Change |
|---|---|
| `include/dcs_bios.h` | Add `hasData()` declaration |
| `src/dcs_bios.cpp` | Add `hasData()` implementation |
| `include/wifi_mgr.h` | Add `WifiPhase` struct; add `beginAttempt()`, `failReasonStr()`, `getPhase()` |
| `src/wifi_mgr.cpp` | Add driver config, volatile phase flags, event handler, `beginAttempt()`, `failReasonStr()`, `getPhase()`; update `pollConnect()`, `isConnected()`, `reconnect()` |
| `include/ui.h` | Add `BootStatusInfo` struct; add `showBootStatus()` |
| `src/ui.cpp` | Implement `showBootStatus()` with check/X primitives |
| `src/main.cpp` | Add `BOOT_STATUS` to `MenuState`; add boot-state vars; restructure `setup()`; add BOOT_STATUS handler; add DCS-event exit; add runtime watchdog |
| `src/ota.cpp` | Replace `WiFi.disconnect(true)+delay(200)` with `WiFi.mode(WIFI_OFF)+delay(500)` |
| `include/config.h` | Bump version `0.25`→`0.26`, BCD `0x0025`→`0x0026` |

---

## Task 1: DcsBios::hasData()

**Files:**
- Modify: `include/dcs_bios.h`
- Modify: `src/dcs_bios.cpp`

- [ ] **Step 1: Add `hasData()` to dcs_bios.h**

In `include/dcs_bios.h`, after the `isConnected()` declaration (line 34), add:

```cpp
  bool hasData();   // true while DCS-BIOS packets received (same 3s window as isConnected)
```

Full updated namespace block:
```cpp
namespace DcsBios {
  void begin(const char* mcastAddr, uint16_t listenPort,
             const char* cmdHost,   uint16_t cmdPort);
  bool update();
  bool isConnected();
  bool hasData();   // true while DCS-BIOS packets received (same 3s window as isConnected)
  void sendCommand(const char* identifier, uint16_t value);

  bool    masterCaution();
  bool    rwrMslLaunch();
  bool    storesConfigLight();
  uint8_t storesConfigSw();
}
```

- [ ] **Step 2: Implement `hasData()` in dcs_bios.cpp**

In `src/dcs_bios.cpp`, after `DcsBios::isConnected()` (after line 97), add:

```cpp
bool DcsBios::hasData() { return isConnected(); }
```

- [ ] **Step 3: Commit**

```bash
git add include/dcs_bios.h src/dcs_bios.cpp
git commit -m "feat(dcs): add hasData() for boot status DCS phase indicator"
```

---

## Task 2: WifiMgr — Phase Tracking + Event Handler + beginAttempt

**Files:**
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`

### Step 2a: Update wifi_mgr.h

- [ ] **Step 1: Add WifiPhase struct and new declarations to wifi_mgr.h**

Replace the entire content of `include/wifi_mgr.h` with:

```cpp
#pragma once
#include <Arduino.h>

namespace WifiMgr {
  // Per-attempt phase flags — set by event handler or beginAttempt().
  // Read via getPhase() which takes a snapshot of the volatile flags.
  struct WifiPhase {
    bool rf;              // radio initialised + WL_IDLE_STATUS confirmed
    bool ssid;            // our SSID found in blocking scan
    bool eth;             // Layer 2 association (WIFI_STA_CONNECTED event)
    bool ip;              // DHCP complete (WIFI_STA_GOT_IP event)
    bool dns;             // DNS server assigned (checked at GOT_IP)
    bool rfFail;          // RF check failed (bad MAC or stale WL_CONNECTED)
    bool ssidFail;        // our SSID not found in scan
    uint8_t failReasonCode; // from WIFI_STA_DISCONNECTED reason field (0 = none)
  };

  // Start one connection attempt. Blocks ~500 ms (radio cycle) + ~2-3 s (scan).
  // Resets all phase flags, cycles radio, checks RF, scans for SSID.
  // If SSID found, calls WiFi.begin() and returns true; caller polls getPhase().ip.
  // Returns false if RF check or SSID scan fails (check getPhase().rfFail/.ssidFail).
  // n: attempt number 1-3, stored for display purposes.
  bool beginAttempt(int n);

  // Snapshot of current phase flags (safe to call from loop() tick).
  WifiPhase getPhase();

  // Human-readable failure string derived from phase flags and failReasonCode.
  // Returns nullptr if no failure has occurred yet.
  const char* failReasonStr();

  // Returns true when IP is assigned (uses phase flag; updated by event handler).
  bool isConnected();

  // Returns true exactly once after isConnected() becomes true.
  // Resets if WiFi drops and reconnects. Used to trigger DCS-BIOS start.
  bool pollConnect();

  // Abort connection attempt. Clears WiFi state.
  void cancelConnect();

  const char* activeSSID();

  // Returns device IP as "192.168.1.5" or "--" if not connected.
  const char* activeIP();

  // Silent runtime reconnect (no phase screen). Called by watchdog and Settings→Connect.
  // Does not reset ssid/rf phase display — reconnects directly.
  void reconnect();

  void saveCredentials(const char* ssid, const char* pass);
  void clearOverride();

  // Blocking encoder-driven single-field entry. Returns false if cancelled (long press).
  bool runEncoderEntry(const char* fieldName,
                       char* result, size_t maxLen,
                       int8_t (*deltaFn)(),
                       bool   (*shortFn)(),
                       bool   (*longFn)(),
                       void   (*oledFn)(const char* field, const char* buf, const char* sel));

  // Serial credential entry flow. Returns true if credentials saved.
  bool runSerialSetup(void (*oledCb)(), bool (*cancelCb)());

  // BLE UART (Nordic UART Service) credential entry session.
  // Returns true if credentials saved — caller must call ESP.restart().
  bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
}
```

### Step 2b: Update wifi_mgr.cpp

- [ ] **Step 2: Add phase flag state and event handler registration to wifi_mgr.cpp**

At the top of `src/wifi_mgr.cpp`, after the existing static declarations (after `static bool s_connected = false;`, around line 21), add:

```cpp
// Phase flags — set from WiFi event task; volatile for safe cross-task reads
static volatile bool    s_phase_rf         = false;
static volatile bool    s_phase_ssid       = false;
static volatile bool    s_phase_eth        = false;
static volatile bool    s_phase_ip         = false;
static volatile bool    s_phase_dns        = false;
static volatile bool    s_phase_rfFail     = false;
static volatile bool    s_phase_ssidFail   = false;
static volatile uint8_t s_phase_failReason = 0;
static bool             s_eventRegistered  = false;

static void registerEventHandler() {
  if (s_eventRegistered) return;
  s_eventRegistered = true;
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    switch (ev) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        s_phase_eth = true;
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        s_phase_ip  = true;
        s_phase_dns = (WiFi.dnsIP() != IPAddress(0, 0, 0, 0));
        s_connected = true;
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        s_phase_failReason = info.wifi_sta_disconnected.reason;
        s_phase_ip  = false;
        s_phase_eth = false;
        s_connected = false;
        break;
      default: break;
    }
  });
}
```

- [ ] **Step 3: Replace `WifiMgr::startConnect()` and add new functions**

Replace the existing `WifiMgr::startConnect()` function (lines 37-44 in wifi_mgr.cpp) with the new API. Also update `pollConnect()`, `isConnected()`, and `reconnect()`:

```cpp
bool WifiMgr::beginAttempt(int n) {
  (void)n;  // reserved for display — phase struct holds attempt context
  loadCredentials();
  registerEventHandler();

  // Reset all phase flags for this attempt
  s_phase_rf = s_phase_ssid = s_phase_eth = s_phase_ip = s_phase_dns = false;
  s_phase_rfFail = s_phase_ssidFail = false;
  s_phase_failReason = 0;
  s_connected = false;

  // Driver config — set before any radio operation
  WiFi.persistent(false);       // we own credentials in NVS; disable driver cache
  WiFi.setAutoConnect(false);   // we own the boot sequence
  WiFi.setAutoReconnect(true);  // free runtime reconnection on beacon loss
  WiFi.setHostname("ANBRU-370");

  // Radio cycle — clears stale hardware state from previous boot/OTA
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);

  // RF check: both conditions must be true
  String mac = WiFi.macAddress();
  bool macOk = (mac.length() >= 11 && mac != "00:00:00:00:00:00");
  bool statusOk = (WiFi.status() != WL_CONNECTED);  // no stale association
  if (!macOk || !statusOk) {
    s_phase_rfFail = true;
    return false;
  }
  s_phase_rf = true;

  // Blocking scan — checks all channels, typically 2-3 s
  int numNets = WiFi.scanNetworks(false /* blocking */, false /* hidden */);
  if (numNets < 0) {
    s_phase_ssidFail = true;
    WiFi.scanDelete();
    return false;
  }
  bool found = false;
  for (int i = 0; i < numNets && !found; i++) {
    if (strcmp(WiFi.SSID(i).c_str(), s_ssid) == 0) found = true;
  }
  WiFi.scanDelete();
  if (!found) {
    s_phase_ssid     = false;
    s_phase_ssidFail = true;
    return false;
  }
  s_phase_ssid = true;

  // All pre-checks passed — start association
  WiFi.begin(s_ssid, s_pass);
  return true;
}

// Legacy shim used by runBleSetup() post-cancel path and Settings→Connect manual path
void WifiMgr::startConnect() {
  loadCredentials();
  registerEventHandler();
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  s_connected = false;
  s_phase_ip = false;
  WiFi.disconnect(true);
  delay(100);
  WiFi.setHostname("ANBRU-370");
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);
}

WifiMgr::WifiPhase WifiMgr::getPhase() {
  WifiPhase ph;
  ph.rf             = s_phase_rf;
  ph.ssid           = s_phase_ssid;
  ph.eth            = s_phase_eth;
  ph.ip             = s_phase_ip;
  ph.dns            = s_phase_dns;
  ph.rfFail         = s_phase_rfFail;
  ph.ssidFail       = s_phase_ssidFail;
  ph.failReasonCode = s_phase_failReason;
  return ph;
}

const char* WifiMgr::failReasonStr() {
  switch (s_phase_failReason) {
    case 200: return "AP unreachable";
    case 201: return "SSID not found";
    case 202: return "Wrong password";
    case 204: return "Auth timeout";
    default:  break;
  }
  if (s_phase_rfFail)   return "RF init failed";
  if (s_phase_ssidFail) return "SSID not found";
  if (s_phase_failReason > 0) {
    static char buf[18];
    snprintf(buf, sizeof(buf), "WiFi error %u", (unsigned)s_phase_failReason);
    return buf;
  }
  return nullptr;
}
```

- [ ] **Step 4: Update pollConnect(), isConnected(), cancelConnect(), reconnect()**

Replace the existing `pollConnect()`, `isConnected()`, `cancelConnect()`, and `reconnect()` functions with:

```cpp
bool WifiMgr::pollConnect() {
  static bool s_reported = false;
  if (!s_phase_ip) { s_reported = false; return false; }
  if (s_reported) return false;
  s_reported = true;
  s_connected = true;
  return true;
}

bool WifiMgr::isConnected() {
  return s_connected || s_phase_ip;
}

void WifiMgr::cancelConnect() {
  WiFi.disconnect(true);
  s_connected = false;
  s_phase_ip  = false;
}

void WifiMgr::reconnect() {
  loadCredentials();
  registerEventHandler();
  s_connected    = false;
  s_phase_ip     = false;
  s_phase_eth    = false;
  s_phase_failReason = 0;
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);
}
```

- [ ] **Step 5: Compile check (firmware won't flash yet — just verify no build errors)**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run 2>&1 | tail -20
```

Expected: compilation may fail due to undefined `showBootStatus` and `BOOT_STATUS` references — that's OK at this stage. The wifi_mgr files themselves should compile clean. If there are errors specifically in wifi_mgr.cpp or dcs_bios.cpp, fix them before proceeding.

- [ ] **Step 6: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat(wifi): add phase tracking, event handler, beginAttempt() for 6-phase boot status"
```

---

## Task 3: UI::showBootStatus()

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add BootStatusInfo struct and showBootStatus() to ui.h**

In `include/ui.h`, after the `#include <Wire.h>` line and before `namespace UI {`, add:

```cpp
// Passed to UI::showBootStatus() each frame during boot.
struct BootStatusInfo {
  bool rf, ssid, eth, ip, dns, dcs;
  bool rfFail, ssidFail;   // show ✗ instead of -- for these phases
  int attempt;             // 1-3; 0 = USB settle (show all --)
  bool failed;             // all 3 attempts exhausted
  const char* failReason;  // nullptr if no failure yet
};
```

Then inside `namespace UI {`, add the declaration:

```cpp
  // Renders six-phase WiFi/DCS boot status screen. Call each loop() tick during BOOT_STATUS.
  void showBootStatus(const BootStatusInfo& s);
```

- [ ] **Step 2: Implement showBootStatus() in ui.cpp**

In `src/ui.cpp`, before `UI::showSplash()` (around line 19), add these two static helpers and the `showBootStatus` implementation:

```cpp
// Draws a 5×5 checkmark. x,y is bottom-left of the 5×5 cell (y is baseline).
static void drawCheck(int x, int y) {
  u8g2.drawLine(x,   y-2, x+2, y);
  u8g2.drawLine(x+2, y,   x+5, y-4);
}

// Draws a 5×5 X mark. x,y is bottom-left of the 5×5 cell (y is baseline).
static void drawCross(int x, int y) {
  u8g2.drawLine(x,   y-4, x+4, y);
  u8g2.drawLine(x+4, y-4, x,   y);
}

// Draws a phase indicator symbol at (x, baseline y).
// done=true → ✓; fail=true → ✗; neither → "--" (or ".." blink if inProgress).
static void drawPhase(int x, int y, bool done, bool fail, bool inProgress) {
  if (fail) {
    drawCross(x, y);
  } else if (done) {
    drawCheck(x, y);
  } else if (inProgress && ((millis() % 600) < 300)) {
    u8g2.drawStr(x, y, "..");
  } else {
    u8g2.drawStr(x, y, "--");
  }
}

void UI::showBootStatus(const BootStatusInfo& s) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Line 1 (y=8): title + firmware version
  u8g2.drawStr(0, 8, "AN/BRU-370");
  {
    char ver[10];
    snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
    int vw = u8g2.getStrWidth(ver);
    u8g2.drawStr(128 - vw, 8, ver);
  }

  // Lines 2-3: phase grid
  // Row 1 (y=16): RF  SSID  Eth
  // Row 2 (y=24): IP  DNS   DCS
  //
  // Column x-positions (5px font, labels + 3px gap before symbol):
  //  "RF:"  = 3ch×5 = 15px → sym at x=16
  //  "SSID:"= 5ch×5 = 25px → starts at x=33, sym at x=58
  //  "Eth:" = 4ch×5 = 20px → starts at x=85, sym at x=105

  u8g2.drawStr(0,  16, "RF:");
  u8g2.drawStr(33, 16, "SSID:");
  u8g2.drawStr(85, 16, "Eth:");

  //  "IP:"  = 3ch×5 = 15px → sym at x=16
  //  "DNS:" = 4ch×5 = 20px → starts at x=33, sym at x=54
  //  "DCS:" = 4ch×5 = 20px → starts at x=74, sym at x=95

  u8g2.drawStr(0,  24, "IP:");
  u8g2.drawStr(33, 24, "DNS:");
  u8g2.drawStr(74, 24, "DCS:");

  bool connecting = (s.attempt > 0 && !s.failed && !s.ip);

  // RF
  drawPhase(16, 16, s.rf,   s.rfFail,   connecting && !s.rf && !s.rfFail);
  // SSID — in-progress while RF done and scan running
  drawPhase(58, 16, s.ssid, s.ssidFail, connecting && s.rf && !s.ssid && !s.ssidFail);
  // Eth
  drawPhase(105,16, s.eth,  false,      connecting && s.ssid && !s.eth);
  // IP
  drawPhase(16, 24, s.ip,   false,      connecting && s.eth && !s.ip);
  // DNS (advisory — no ✗, only ✓ or --)
  drawPhase(54, 24, s.dns,  false,      false);
  // DCS (advisory — no ✗, cycles with isConnected window)
  drawPhase(95, 24, s.dcs,  false,      false);

  // Line 4 (y=32): status / retry / failure
  if (s.attempt == 0) {
    // USB settle — blank
  } else if (s.failed) {
    char line[32];
    const char* reason = s.failReason ? s.failReason : "WiFi error";
    snprintf(line, sizeof(line), "%.16s LP=Set", reason);
    u8g2.drawStr(0, 32, line);
  } else if (s.ip) {
    u8g2.drawStr(0, 32, "LP=Settings");
  } else if (s.attempt > 1) {
    char line[28];
    snprintf(line, sizeof(line), "Attempt %d/3  Retrying...", s.attempt);
    u8g2.drawStr(0, 32, line);
  } else {
    u8g2.drawStr(0, 32, "Connecting...");
  }

  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat(ui): add showBootStatus() with six-phase grid and check/X primitives"
```

---

## Task 4: main.cpp — BOOT_STATUS State Machine

**Files:**
- Modify: `src/main.cpp`

### Step 4a: Add BOOT_STATUS to MenuState and state variables

- [ ] **Step 1: Add `BOOT_STATUS` to the `MenuState` enum and boot-state variables**

In `src/main.cpp`, change the `MenuState` enum declaration (lines 14-21) to include `BOOT_STATUS` as the first entry:

```cpp
enum MenuState {
  BOOT_STATUS,
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT,
  FIRMWARE_CHECKING, FIRMWARE_UP_TO_DATE,
  FIRMWARE_CONFIRM, FIRMWARE_UPDATING, FIRMWARE_ERROR
};
```

Then change the initial `s_mode` declaration (line 27) to:

```cpp
static MenuState s_mode           = BOOT_STATUS;
```

After the `static bool s_dcsBiosStarted = false;` line (~line 52), add the boot-state tracking variables:

```cpp
static int           s_bootAttempt  = 0;   // 0=not started; 1-3=current attempt
static unsigned long s_bootDeadline = 0;   // millis()+15s set after beginAttempt() succeeds
static bool          s_bootFailed   = false;
```

### Step 4b: Restructure setup()

- [ ] **Step 2: Replace the WiFi blocking loops in setup() with a simple USB-settle loop**

Replace the entire `setup()` function body from after `UI::setContrast(s_brightness);` to the end of setup (the two WiFi-connection while-loops and their wrappers, lines 200-252) with:

```cpp
  // USB settle — PMU shared between USB-OTG and WiFi; don't start WiFi until
  // OTG enumeration completes. Show the boot status screen (all --) while waiting.
  {
    unsigned long settleStart = millis();
    BootStatusInfo bsi = {};  // all false/zero
    while (millis() - settleStart < 3000UL) {
      UI::showBootStatus(bsi);
      Encoder::readDelta();  // keep encoder fresh so flush() works
      if (Encoder::longPressed()) { s_wifiCancelled = true; break; }
      delay(10);
    }
    Encoder::flush();
  }
  // WiFi startup is handled by the BOOT_STATUS state in loop().
  s_lastActivity = millis();
```

### Step 4c: Add BOOT_STATUS handler in loop()

- [ ] **Step 3: Add the BOOT_STATUS handler to the loop() menu state machine**

In `src/main.cpp` `loop()`, find the section that handles DCS events (the `rwrConfirmed` block, around line 288). Add this to the top of each DCS alert block to exit BOOT_STATUS on the first DCS event:

In the `if (rwrConfirmed)` block, add as the first line:
```cpp
    if (s_mode == BOOT_STATUS) s_mode = MACRO_MENU;
```

In the `if (mcConfirmed)` block, add as the first line:
```cpp
    if (s_mode == BOOT_STATUS) s_mode = MACRO_MENU;
```

Then, in the mode-specific section of loop() — find the `if (s_mode == MACRO_MENU)` block (~line 393) and add the BOOT_STATUS handler **before** it:

```cpp
  if (s_mode == BOOT_STATUS) {
    // Skip boot sequence if WiFi is disabled or cancelled during USB settle
    if (!s_wifiEnabled || s_wifiCancelled) {
      s_mode = MACRO_MENU;
      return;
    }

    WifiMgr::WifiPhase ph = WifiMgr::getPhase();

    // Attempt 0 → start attempt 1. beginAttempt() blocks ~3s (radio cycle + scan).
    if (s_bootAttempt == 0) {
      if (WifiMgr::beginAttempt(1)) {
        s_bootAttempt = 1;
        s_bootDeadline = millis() + 15000UL;
      } else {
        s_bootAttempt = 1;  // mark as attempted; needRetry fires on next tick
      }
      ph = WifiMgr::getPhase();  // refresh after blocking call
    }

    // Detect failure conditions and retry
    if (!s_bootFailed && !ph.ip) {
      bool needRetry = ph.rfFail || ph.ssidFail ||
                       (ph.failReasonCode != 0) ||
                       (s_bootDeadline > 0 && millis() > s_bootDeadline);
      if (needRetry) {
        if (s_bootAttempt < 3) {
          if (WifiMgr::beginAttempt(s_bootAttempt + 1)) {
            s_bootAttempt++;
            s_bootDeadline = millis() + 15000UL;
          } else {
            s_bootAttempt++;  // still failed; retry again next tick or exhaust
          }
          ph = WifiMgr::getPhase();
        } else {
          s_bootFailed = true;
        }
      }
    }

    // Start DCS-BIOS on first IP assignment
    if (ph.ip && !s_dcsBiosStarted) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }

    // Exit conditions
    if (Encoder::longPressed()) {
      s_mode = SETTINGS; s_menuSel = 0; s_menuOffset = 0;
      return;
    }
    if (ph.ip && (delta != 0 || Encoder::shortPressed())) {
      s_mode = MACRO_MENU;
      return;
    }

    // Draw boot status screen
    bool dcs = DcsBios::hasData();
    BootStatusInfo bsi = {
      ph.rf, ph.ssid, ph.eth, ph.ip, ph.dns, dcs,
      ph.rfFail, ph.ssidFail,
      s_bootAttempt, s_bootFailed,
      WifiMgr::failReasonStr()
    };
    UI::showBootStatus(bsi);
    return;

  } else if (s_mode == MACRO_MENU) {
```

Note: the above replaces the raw `if (s_mode == MACRO_MENU)` with `} else if (s_mode == MACRO_MENU)` — ensure the chain is unbroken.

### Step 4d: Add background pollConnect() guard + runtime watchdog

- [ ] **Step 4: Guard the background pollConnect() call and add the runtime watchdog**

The existing loop() background pollConnect (lines ~255-261) fires after the boot state, which is now handled by BOOT_STATUS. Guard it and add the watchdog. Find:

```cpp
  if (!s_dcsBiosStarted && !s_wifiCancelled && s_wifiEnabled) {
    if (WifiMgr::pollConnect()) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }
  }
```

Replace with:

```cpp
  // Background DCS-BIOS start for reconnects after boot (boot path uses BOOT_STATUS state)
  if (s_mode != BOOT_STATUS && !s_dcsBiosStarted && !s_wifiCancelled && s_wifiEnabled) {
    if (WifiMgr::pollConnect()) {
      DcsBios::begin(DCSBIOS_MCAST_ADDR, DCSBIOS_MCAST_PORT,
                     "255.255.255.255", DCSBIOS_CMD_PORT);
      s_dcsBiosStarted = true;
    }
  }

  // Runtime WiFi watchdog — every 5 s, after boot completes.
  // setAutoReconnect(true) handles transient beacon loss; this is the fallback
  // if the driver gives up and isConnected() disagrees with WiFi.status().
  static unsigned long s_lastWatchdog = 0;
  if (s_mode != BOOT_STATUS && millis() - s_lastWatchdog >= 5000UL) {
    s_lastWatchdog = millis();
    if (WifiMgr::isConnected() && WiFi.status() != WL_CONNECTED) {
      WifiMgr::reconnect();
    }
  }
```

- [ ] **Step 5: Add missing `#include "wifi_mgr.h"` BootStatusInfo include path**

`BootStatusInfo` is defined in `ui.h`, which is already included in main.cpp. Verify line ~6 has `#include "ui.h"` — no change needed.

- [ ] **Step 6: Remove now-dead constant from wifi_mgr.h**

The old `kWifiConnectTimeoutMs` constant is no longer used. Remove it from `include/wifi_mgr.h` (the line starting `constexpr unsigned long kWifiConnectTimeoutMs`). It was already removed in the Task 2 header rewrite — verify it's gone.

- [ ] **Step 7: Full compile check**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run 2>&1 | tail -40
```

Expected: build succeeds with no errors. Fix any undeclared identifier or type errors before continuing. Common issues:
- `BootStatusInfo` not in scope → check `#include "ui.h"` is present in main.cpp
- `startConnect()` undeclared → add `void startConnect();` to wifi_mgr.h (the `runBleSetup` path needs it)
- Missing `WifiMgr::WifiPhase` → ensure `include/wifi_mgr.h` is included in main.cpp

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): add BOOT_STATUS state machine with 6-phase WiFi display and retry logic"
```

---

## Task 5: OTA Pre-Restart Fix

**Files:**
- Modify: `src/ota.cpp`

- [ ] **Step 1: Replace the pre-restart disconnect with full radio shutdown**

In `src/ota.cpp`, find the pre-restart sequence near the bottom of `OTA::perform()` (lines 142-144):

```cpp
  WiFi.disconnect(true);  // clean disassociation before reset; prevents router-side stale session on next boot
  delay(200);
  ESP.restart();
```

Replace with:

```cpp
  WiFi.mode(WIFI_OFF);  // triggers esp_wifi_stop(); sends deauth frame to AP before reset
  delay(500);
  ESP.restart();
```

- [ ] **Step 2: Commit**

```bash
git add src/ota.cpp
git commit -m "fix(ota): full radio shutdown before OTA restart to prevent stale AP session"
```

---

## Task 6: Version Bump + Build

**Files:**
- Modify: `include/config.h`

- [ ] **Step 1: Bump firmware version to 0.26**

In `include/config.h`, find and update:

```cpp
#define FIRMWARE_VERSION     "0.25"
#define FIRMWARE_VERSION_BCD  0x0025
```

Change to:

```cpp
#define FIRMWARE_VERSION     "0.26"
#define FIRMWARE_VERSION_BCD  0x0026
```

- [ ] **Step 2: Full build**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run 2>&1 | tail -20
```

Expected: `SUCCESS` with RAM/flash within budget.

- [ ] **Step 3: Flash and smoke test**

```bash
pio run -t upload 2>&1 | tail -20
```

Observe on OLED:
1. Boot status screen appears with `AN/BRU-370` title and firmware version
2. `RF: --` → (after radio cycle) `RF: ✓`
3. `SSID: --` → (after scan ~2-3s) `SSID: ✓` or `SSID: ✗`
4. `Eth: --` → (after association) `Eth: ✓`
5. `IP: --` → (after DHCP) `IP: ✓`
6. `DNS: ✓` appears simultaneously with IP
7. Line 4 shows `LP=Settings`
8. Rotating encoder after IP moves to Macro menu
9. LP at any point goes to Settings

- [ ] **Step 4: Test failure path**

Configure a wrong password in Settings → WiFi → Manual. Reboot. Observe:
- `RF: ✓` → `SSID: ✓` → `Eth: ✓` → wait → line 4 shows `Wrong password LP=Set` after 15 s
- Two more retries shown as `Attempt 2/3 Retrying...` / `Attempt 3/3 Retrying...`
- Final state shows failure reason, LP goes to Settings

- [ ] **Step 5: Commit and push**

```bash
git add include/config.h
git commit -m "chore: bump version to v0.26 (WiFi hardening + boot status screen)"
git push
```

---

## Self-Review: Spec Coverage Check

| Spec requirement | Task covering it |
|---|---|
| `WiFi.persistent(false)` / `setAutoConnect(false)` / `setAutoReconnect(true)` | Task 2, `beginAttempt()` |
| Phase 1 RF: MAC valid + WL_IDLE_STATUS | Task 2, `beginAttempt()` |
| Phase 2 SSID: scan results | Task 2, `beginAttempt()` blocking scan |
| Phase 3 Eth: WIFI_STA_CONNECTED event | Task 2, event handler |
| Phase 4 IP: WIFI_STA_GOT_IP event | Task 2, event handler |
| Phase 5 DNS: `WiFi.dnsIP()` at GOT_IP | Task 2, event handler |
| Phase 6 DCS: `DcsBios::hasData()` | Task 1 |
| Max 3 attempts, 15 s each | Task 4, BOOT_STATUS handler |
| Full radio reset between attempts | Task 2, `beginAttempt()` |
| Phase indicators reset per attempt | Task 2, flag reset at start of `beginAttempt()` |
| SSID/AUTH/TIMEOUT failure reason strings | Task 2, `failReasonStr()` |
| Boot status screen layout (128×32, 4 lines) | Task 3, `showBootStatus()` |
| `✓` / `✗` / `--` / `..` blink symbols | Task 3, `drawCheck()`/`drawCross()`/`drawPhase()` |
| Line 4: Connecting / Attempt N/3 / LP=Settings / reason | Task 3, `showBootStatus()` line 4 logic |
| LP exits to Settings at any time | Task 4, BOOT_STATUS handler |
| Encoder exits to Macro when IP ✓ | Task 4, BOOT_STATUS handler |
| DCS event exits boot status directly | Task 4, DCS block guard `s_mode = MACRO_MENU` |
| Boot status never reappears after exit | Task 4 — `s_mode` is not reset to BOOT_STATUS after exit |
| OTA pre-restart radio shutdown | Task 5, `ota.cpp` |
| Runtime WiFi watchdog every 5 s | Task 4, watchdog block |
| DCS indicator cycles ✓→-- when DCS goes offline | Inherent in `DcsBios::hasData()` = `isConnected()` 3 s window |
