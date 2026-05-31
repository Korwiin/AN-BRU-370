# WiFi Submenu Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the WiFi submenu to a split left/right layout showing current SSID and IP, replace Serial Entry with a BLE UART terminal session for credential entry, and add a Connect option.

**Architecture:** Four layers of change applied in sequence — add NimBLE library, extend WifiMgr API (activeIP, reconnect, runBleSetup with NimBLE Nordic UART Service), update UI screens (showWifiSubMenu split layout, showWifiConfirm, showBleActive), then wire the state machine in main.cpp. The BLE session uses a blocking while-loop matching the existing runEncoderEntry and runSerialSetup patterns.

**Tech Stack:** NimBLE-Arduino 1.4.x (h2zero/NimBLE-Arduino), Nordic UART Service (NUS) UUIDs, U8g2, PlatformIO espressif32.

---

### Task 0: Add NimBLE-Arduino to platformio.ini

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Add NimBLE to lib_deps**

In `platformio.ini`, update `lib_deps` under `[env:esp32s3_supermini]`:

```ini
lib_deps =
    olikraus/U8g2@^2.36.0
    h2zero/NimBLE-Arduino@^1.4.0
```

- [ ] **Step 2: Verify clean build**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run -e esp32s3_supermini
```
Expected: build succeeds. NimBLE is fetched and compiled. NimBLE's own internal warnings are fine; 0 errors required.

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "feat: add NimBLE-Arduino dependency"
```

---

### Task 1: Add `activeIP()` and `reconnect()` to WifiMgr

**Files:**
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`

- [ ] **Step 1: Declare new functions in wifi_mgr.h**

In `include/wifi_mgr.h`, add after the `activeSSID()` declaration:

```cpp
// Returns device IP as a dotted-decimal string (e.g. "192.168.1.5"), or "--" if not connected.
const char* activeIP();

// Reset connected state and reconnect with saved credentials (non-blocking start).
// startConnect() handles WiFi.disconnect internally — no double-disconnect.
void reconnect();
```

- [ ] **Step 2: Implement `activeIP()` and `reconnect()` in wifi_mgr.cpp**

Add after the `activeSSID()` function body in `src/wifi_mgr.cpp`:

```cpp
const char* WifiMgr::activeIP() {
  static char buf[16];
  if (!s_connected) return "--";
  WiFi.localIP().toString().toCharArray(buf, sizeof(buf));
  return buf;
}

void WifiMgr::reconnect() {
  s_connected = false;
  startConnect();
}
```

- [ ] **Step 3: Verify clean build**

```bash
pio run -e esp32s3_supermini
```
Expected: 0 errors.

- [ ] **Step 4: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat: add WifiMgr::activeIP() and reconnect()"
```

---

### Task 2: Add `runBleSetup()` to WifiMgr

**Files:**
- Modify: `include/wifi_mgr.h`
- Modify: `src/wifi_mgr.cpp`

- [ ] **Step 1: Declare `runBleSetup()` in wifi_mgr.h**

In `include/wifi_mgr.h`, add after the `runSerialSetup` declaration:

```cpp
// Runs BLE UART (Nordic UART Service) credential entry session.
// Disconnects WiFi, starts NimBLE, presents an interactive terminal prompt.
// oledActiveCb: called every loop tick while the BLE session is running.
// cancelCb: return true to abort (long press check).
// Returns true if credentials saved — caller must call ESP.restart().
// Returns false if cancelled; WiFi reconnect is handled internally.
bool runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)());
```

- [ ] **Step 2: Add NimBLE include and static BLE state to wifi_mgr.cpp**

At the top of `src/wifi_mgr.cpp`, add after the existing `#include` lines:

```cpp
#include <NimBLEDevice.h>

#define NUS_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic* s_bleTxChar     = nullptr;
static volatile bool          s_bleClientConn = false;
static String                 s_bleRxBuf;
static volatile bool          s_bleLineReady  = false;
```

- [ ] **Step 3: Add BLE callbacks and bleSend() helper to wifi_mgr.cpp**

Add the following before the `WifiMgr::runBleSetup` definition (after `runSerialSetup`):

```cpp
class BleServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override {
    s_bleClientConn = true;
  }
  void onDisconnect(NimBLEServer* pServer) override {
    s_bleClientConn  = false;
    s_bleLineReady   = false;
    s_bleRxBuf       = "";
    pServer->startAdvertising();
  }
};

class BleRxCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    for (char c : val) {
      if (c == '\n' || c == '\r') {
        if (s_bleRxBuf.length() > 0) s_bleLineReady = true;
      } else if (!s_bleLineReady) {
        s_bleRxBuf += c;
      }
    }
  }
};

// Sends msg over BLE UART in 20-byte chunks (default NUS MTU payload size).
static void bleSend(const char* msg) {
  if (!s_bleTxChar || !s_bleClientConn) return;
  const uint8_t* data = (const uint8_t*)msg;
  size_t len = strlen(msg);
  size_t off = 0;
  while (off < len) {
    size_t chunk = (len - off < 20) ? len - off : 20;
    s_bleTxChar->setValue(data + off, chunk);
    s_bleTxChar->notify();
    delay(20);
    off += chunk;
  }
}
```

- [ ] **Step 4: Implement `runBleSetup()` in wifi_mgr.cpp**

Add after `runSerialSetup`:

```cpp
bool WifiMgr::runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)()) {
  WiFi.disconnect(true);
  delay(100);
  s_connected = false;

  NimBLEDevice::init("AN/BRU-370");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new BleServerCb());

  NimBLEService* pSvc = pServer->createService(NUS_SERVICE_UUID);
  s_bleTxChar = pSvc->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* pRx = pSvc->createCharacteristic(
    NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRx->setCallbacks(new BleRxCb());

  pSvc->start();
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(NUS_SERVICE_UUID);
  pAdv->start();

  enum { WAIT_CLIENT, GET_SSID, GET_PASS, CONFIRM } state = WAIT_CLIENT;
  char newSSID[64] = {0};
  char newPass[64] = {0};
  bool done   = false;
  bool result = false;

  while (!done) {
    if (cancelCb && cancelCb()) break;
    if (oledActiveCb) oledActiveCb();

    switch (state) {
      case WAIT_CLIENT:
        if (s_bleClientConn) {
          char msg[100];
          snprintf(msg, sizeof(msg),
            "\r\n=== AN/BRU-370 WiFi Setup ===\r\n\r\nCurrent SSID: %s\r\n\r\nEnter new SSID:\r\n",
            s_ssid);
          bleSend(msg);
          state = GET_SSID;
        }
        break;

      case GET_SSID:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          line.trim();
          if (line.length() == 0) {
            bleSend("Enter new SSID:\r\n");
          } else {
            strlcpy(newSSID, line.c_str(), sizeof(newSSID));
            bleSend("Enter Password:\r\n");
            state = GET_PASS;
          }
        }
        break;

      case GET_PASS:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          line.trim();
          strlcpy(newPass, line.c_str(), sizeof(newPass));
          char msg[160];
          snprintf(msg, sizeof(msg),
            "\r\nSSID: %s\r\nPass: %s\r\n\r\nSave & Reboot (Y), Retry (r), Cancel (n):\r\n",
            newSSID, newPass);
          bleSend(msg);
          state = CONFIRM;
        }
        break;

      case CONFIRM:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          line.trim();
          if (line == "Y" || line == "y") {
            saveCredentials(newSSID, newPass);
            bleSend("Saved. Rebooting...\r\n");
            delay(500);
            result = true;
            done   = true;
          } else if (line == "r" || line == "R") {
            char cur[100];
            snprintf(cur, sizeof(cur),
              "\r\nCurrent SSID: %s\r\n\r\nEnter new SSID:\r\n", s_ssid);
            bleSend(cur);
            state = GET_SSID;
          } else {
            bleSend("Cancelled.\r\n");
            done = true;
          }
        }
        break;
    }
    delay(10);
  }

  s_bleTxChar = nullptr;
  NimBLEDevice::deinit(true);
  delay(100);
  if (!result) startConnect();
  return result;
}
```

- [ ] **Step 5: Verify clean build**

```bash
pio run -e esp32s3_supermini
```
Expected: 0 errors.

- [ ] **Step 6: Commit**

```bash
git add include/wifi_mgr.h src/wifi_mgr.cpp
git commit -m "feat: add WifiMgr::runBleSetup() with NimBLE UART terminal session"
```

---

### Task 3: Redesign `showWifiSubMenu()` with split layout

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`
- Modify: `src/main.cpp` (update call site and add s_wifiSubOffset)

- [ ] **Step 1: Update `showWifiSubMenu` signature in ui.h**

In `include/ui.h`, replace:

```cpp
void showWifiSubMenu(int sel);
```

with:

```cpp
// Split layout matching showSettingsMenu: left panel shows WiFi/SSID/IP; right panel scrolls 4 options.
void showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip);
```

- [ ] **Step 2: Rewrite `showWifiSubMenu` in ui.cpp**

In `src/ui.cpp`, replace the entire `showWifiSubMenu` function body:

```cpp
void UI::showWifiSubMenu(int sel, int offset, const char* ssid, const char* ip) {
  static const char* items[] = {"Manual", "Bluetooth", "Connect", "Back"};
  static const int kItems = 4;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — WiFi status (x=0..63)
  char ssidLine[13], ipLine[13];
  snprintf(ssidLine, sizeof(ssidLine), "S:%.10s", ssid);
  snprintf(ipLine,   sizeof(ipLine),   "IP:%.9s",  ip);
  u8g2.drawStr(0,  8, "WiFi");
  u8g2.drawStr(0, 16, ssidLine);
  u8g2.drawStr(0, 24, ipLine);

  // Right panel — 4-item scrolling menu (x=65..127)
  for (int i = 0; i < 4; i++) {
    int idx = offset + i;
    if (idx >= kItems) break;
    int y = 8 + i * 8;
    if (idx == sel) {
      u8g2.drawStr(65, y, ">");
      u8g2.drawStr(71, y, items[idx]);
    } else {
      u8g2.drawStr(71, y, items[idx]);
    }
  }
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Add `s_wifiSubOffset` to main.cpp and update the OLED call**

In `src/main.cpp`, find:

```cpp
static int  s_wifiSubSel          = 0;
```

Add on the next line:

```cpp
static int  s_wifiSubOffset       = 0;
```

Then find the OLED update switch:

```cpp
case WIFI_MENU:         UI::showWifiSubMenu(s_wifiSubSel); break;
```

Replace with:

```cpp
case WIFI_MENU:         UI::showWifiSubMenu(s_wifiSubSel, s_wifiSubOffset,
                          WifiMgr::activeSSID(), WifiMgr::activeIP()); break;
```

- [ ] **Step 4: Verify clean build**

```bash
pio run -e esp32s3_supermini
```
Expected: 0 errors.

- [ ] **Step 5: Commit**

```bash
git add include/ui.h src/ui.cpp src/main.cpp
git commit -m "feat: redesign showWifiSubMenu with split layout"
```

---

### Task 4: Add `showWifiConfirm()` and `showBleActive()` screens

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Declare new screens in ui.h**

In `include/ui.h`, add after `showSerialActive()`:

```cpp
void showWifiConfirm();  // "Are you sure?" before BLE session starts
void showBleActive();    // displayed while NimBLE UART session is running
```

- [ ] **Step 2: Implement both screens in ui.cpp**

In `src/ui.cpp`, add after `showSerialActive()`:

```cpp
void UI::showWifiConfirm() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "BLE");
  u8g2.drawStr(0, 16, "Setup");
  u8g2.drawStr(65,  8, "WiFi will");
  u8g2.drawStr(65, 16, "disconnect.");
  u8g2.drawStr(65, 24, "SP=OK");
  u8g2.drawStr(65, 32, "LP=No");
  u8g2.sendBuffer();
}

void UI::showBleActive() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0,  8, "BLE");
  u8g2.drawStr(0, 16, "Active");
  u8g2.drawStr(65,  8, "AN/BRU-370");
  u8g2.drawStr(65, 16, "Waiting...");
  u8g2.drawStr(65, 24, "LP=Cancel");
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Verify clean build**

```bash
pio run -e esp32s3_supermini
```
Expected: 0 errors.

- [ ] **Step 4: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat: add showWifiConfirm and showBleActive screens"
```

---

### Task 5: Update main.cpp WIFI_MENU state machine

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Update scroll wrap from 3 to 4 items**

In `src/main.cpp`, inside the `WIFI_MENU` handler, replace:

```cpp
    s_wifiSubSel = (s_wifiSubSel + delta + 3) % 3;
```

with:

```cpp
    s_wifiSubSel = (s_wifiSubSel + delta + 4) % 4;
    if (s_wifiSubSel < s_wifiSubOffset) s_wifiSubOffset = s_wifiSubSel;
    if (s_wifiSubSel >= s_wifiSubOffset + 4) s_wifiSubOffset = s_wifiSubSel - 3;
```

- [ ] **Step 2: Replace the entire short-press handler in WIFI_MENU**

In `src/main.cpp`, replace this block (from `if (Encoder::shortPressed())` through the closing long-press line of the WIFI_MENU section):

```cpp
    if (Encoder::shortPressed()) {
      if (s_wifiSubSel == 0) {
        // Serial Entry
        bool saved = WifiMgr::runSerialSetup(
          []() { UI::showSerialActive(); },
          []() { return Encoder::longPressed(); }
        );
        if (saved) UI::showSaved();
        s_mode = SETTINGS;
      } else if (s_wifiSubSel == 1) {
        // Manual Entry — encoder character scroll
        char newSSID[33] = {0};
        char newPass[64] = {0};
        bool gotSSID = WifiMgr::runEncoderEntry(
          "SSID", newSSID, sizeof(newSSID),
          []() { return Encoder::readDelta(); },
          []() { return Encoder::shortPressed(); },
          []() { return Encoder::longPressed(); },
          [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
        );
        if (gotSSID) {
          bool gotPass = WifiMgr::runEncoderEntry(
            "Password", newPass, sizeof(newPass),
            []() { return Encoder::readDelta(); },
            []() { return Encoder::shortPressed(); },
            []() { return Encoder::longPressed(); },
            [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
          );
          if (gotPass) {
            WifiMgr::saveCredentials(newSSID, newPass);
            UI::showSaved();
          }
        }
        s_mode = SETTINGS;
      } else {
        // Back
        s_mode = SETTINGS;
      }
      s_wifiSubSel = 0;
    }
    if (Encoder::longPressed()) { s_wifiSubSel = 0; s_mode = SETTINGS; }
```

with:

```cpp
    if (Encoder::shortPressed()) {
      if (s_wifiSubSel == 0) {
        // Manual Entry — encoder character scroll
        char newSSID[33] = {0};
        char newPass[64] = {0};
        bool gotSSID = WifiMgr::runEncoderEntry(
          "SSID", newSSID, sizeof(newSSID),
          []() { return Encoder::readDelta(); },
          []() { return Encoder::shortPressed(); },
          []() { return Encoder::longPressed(); },
          [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
        );
        if (gotSSID) {
          bool gotPass = WifiMgr::runEncoderEntry(
            "Password", newPass, sizeof(newPass),
            []() { return Encoder::readDelta(); },
            []() { return Encoder::shortPressed(); },
            []() { return Encoder::longPressed(); },
            [](const char* f, const char* b, const char* s) { UI::showCharEntry(f, b, s); }
          );
          if (gotPass) {
            WifiMgr::saveCredentials(newSSID, newPass);
            UI::showSaved();
          }
        }
        s_mode = SETTINGS;
      } else if (s_wifiSubSel == 1) {
        // Bluetooth — confirm WiFi disconnect, then run BLE UART session
        Encoder::flush();
        bool confirmed = false;
        while (true) {
          UI::showWifiConfirm();
          if (Encoder::shortPressed()) { confirmed = true; break; }
          if (Encoder::longPressed())  break;
          delay(10);
        }
        if (confirmed) {
          bool saved = WifiMgr::runBleSetup(
            []() { UI::showBleActive(); },
            []() { return Encoder::longPressed(); }
          );
          if (saved) ESP.restart();
        }
        s_mode = SETTINGS;
      } else if (s_wifiSubSel == 2) {
        // Connect — reconnect with saved credentials
        WifiMgr::reconnect();
        unsigned long t0 = millis();
        bool connected = false;
        while (millis() - t0 < 15000UL) {
          UI::showWifiConnecting(WifiMgr::activeSSID());
          if (WifiMgr::pollConnect()) { connected = true; break; }
          delay(100);
        }
        if (connected) {
          UI::showWifiConnected(WifiMgr::activeSSID());
        } else {
          UI::showWifiFailed(WifiMgr::activeSSID());
          delay(1500);
        }
        s_mode = SETTINGS;
      } else {
        // Back
        s_mode = SETTINGS;
      }
      s_wifiSubSel   = 0;
      s_wifiSubOffset = 0;
    }
    if (Encoder::longPressed()) {
      s_wifiSubSel = 0; s_wifiSubOffset = 0; s_mode = SETTINGS;
    }
```

- [ ] **Step 3: Confirm Serial Entry is fully removed**

```bash
grep -n "Serial Entry\|showSerialActive\|runSerialSetup" /Volumes/home/Projects/Arduino/Brew370/src/main.cpp
```
Expected: no matches.

- [ ] **Step 4: Verify clean build**

```bash
pio run -e esp32s3_supermini
```
Expected: 0 errors, no warnings about `showSerialActive` or `runSerialSetup` being unused (they remain in wifi_mgr for potential future use — that's fine).

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat: update WIFI_MENU for Bluetooth entry and Connect option"
```

---

### Task 6: Flash and verify on hardware

- [ ] **Step 1: Flash to device**

```bash
pio run -e esp32s3_supermini -t upload
```

- [ ] **Step 2: Verify WiFi submenu layout**

Long-press encoder → Settings → scroll to WiFi → short press.
Expected: left panel shows "WiFi" / "S:xxxx" (current SSID) / "IP:x.x.x.x" (or "IP:--"). Right panel shows "> Manual" / "Bluetooth" / "Connect" / "Back" with cursor tracking encoder rotation.

- [ ] **Step 3: Verify Manual entry still works**

Select Manual → enter a test SSID (e.g. "Test") and password ("abc") using the encoder scroll → confirm SAVED screen appears.

- [ ] **Step 4: Verify Connect option**

Select Connect → OLED shows "WIFI CONNECTING" with SSID → transitions to "WIFI CONNECTED" or "WIFI FAILED" → returns to Settings automatically.

- [ ] **Step 5: Verify Bluetooth confirm screen and LP cancel**

Select Bluetooth → confirm screen shows "BLE" / "Setup" on left, "WiFi will" / "disconnect." / "SP=OK" / "LP=No" on right.
Long-press → returns to Settings without starting BLE.

- [ ] **Step 6: Verify BLE UART session**

Select Bluetooth → short press confirm → OLED shows "BLE Active" / "AN/BRU-370" / "Waiting..." / "LP=Cancel".
On phone: open **Serial Bluetooth Terminal** (Android) or **Bluefruit Connect** (iOS), scan, connect to "AN/BRU-370".
Expected terminal output:
```
=== AN/BRU-370 WiFi Setup ===

Current SSID: <existing-ssid>

Enter new SSID:
```
Type an SSID and send → prompted for password → type password and send → confirmation shown with Y/r/n prompt.

- [ ] **Step 7: Verify Retry flow**

At the Y/r/n prompt, send `r` → SSID prompt reappears. Re-enter SSID and password.

- [ ] **Step 8: Verify Save & Reboot**

At the Y/r/n prompt, send `Y` → "Saved. Rebooting..." appears in terminal, device reboots and connects to new credentials.

- [ ] **Step 9: Verify LP cancel during BLE session**

Repeat step 6 to get to BLE Active screen. Long-press encoder before connecting phone.
Expected: BLE stops, WiFi reconnects, OLED returns to Settings menu.
