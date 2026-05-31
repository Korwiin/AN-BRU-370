#include "wifi_mgr.h"
#include "encoder.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

#define NUS_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic* s_bleTxChar     = nullptr;
static volatile bool          s_bleClientConn = false;
static String                 s_bleRxBuf;
static volatile bool          s_bleLineReady  = false;

static char s_ssid[64] = {0};
static char s_pass[64] = {0};
static bool s_connected = false;

static void loadCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String nvsSsid = prefs.getString("ssid", "");
  String nvsPass = prefs.getString("pass", "");
  prefs.end();
  if (nvsSsid.length() > 0) {
    nvsSsid.toCharArray(s_ssid, sizeof(s_ssid));
    nvsPass.toCharArray(s_pass, sizeof(s_pass));
  } else {
    strlcpy(s_ssid, WIFI_SSID_DEFAULT, sizeof(s_ssid));
    strlcpy(s_pass, WIFI_PASS_DEFAULT, sizeof(s_pass));
  }
}

void WifiMgr::startConnect() {
  loadCredentials();
  WiFi.disconnect(true);  // clear stale hardware state from previous boot
  delay(100);
  WiFi.setHostname("ANBRU-370");
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);
}

bool WifiMgr::pollConnect() {
  if (s_connected) return false;
  if (WiFi.status() == WL_CONNECTED) {
    s_connected = true;
    return true;
  }
  return false;
}

void WifiMgr::cancelConnect() {
  WiFi.disconnect(true);
  s_connected = false;
}

bool WifiMgr::isConnected() {
  return s_connected;
}

const char* WifiMgr::activeSSID() {
  return s_ssid;
}

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

void WifiMgr::saveCredentials(const char* ssid, const char* pass) {
  Preferences prefs;
  prefs.begin("brew_wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void WifiMgr::clearOverride() {
  Preferences prefs;
  prefs.begin("brew_wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
}

bool WifiMgr::runEncoderEntry(const char* fieldName,
                               char* result, size_t maxLen,
                               int8_t (*deltaFn)(),
                               bool   (*shortFn)(),
                               bool   (*longFn)(),
                               void   (*oledFn)(const char*, const char*, const char*)) {
  // Character set: A-Z, a-z, 0-9, symbols, then DEL and DONE
  static const char k_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !@#$%-_.";
  // k_chars length = 26+26+10+9 = 71 printable, plus DEL(idx 71) and DONE(idx 72)
  constexpr int kTotal    = 73;
  constexpr int kIdxDEL   = 71;
  constexpr int kIdxDONE  = 72;

  // Discard any accumulated encoder state from the triggering press
  // so the blocking inner loop starts with a clean slate.
  Encoder::flush();

  int  charIdx = 0;
  char buf[64] = {0};
  int  bufLen  = 0;

  while (true) {
    int8_t d = deltaFn();
    charIdx = (charIdx + d + kTotal) % kTotal;

    char selLabel[5];
    if      (charIdx == kIdxDEL)  strlcpy(selLabel, "DEL",  sizeof(selLabel));
    else if (charIdx == kIdxDONE) strlcpy(selLabel, "OK",   sizeof(selLabel));
    else                          { selLabel[0] = k_chars[charIdx]; selLabel[1] = 0; }

    oledFn(fieldName, buf, selLabel);

    if (shortFn()) {
      if (charIdx == kIdxDEL) {
        if (bufLen > 0) buf[--bufLen] = 0;
      } else if (charIdx == kIdxDONE) {
        strlcpy(result, buf, maxLen);
        return true;
      } else {
        if (bufLen < (int)maxLen - 1) {
          buf[bufLen++] = k_chars[charIdx];
          buf[bufLen]   = 0;
        }
      }
    }

    if (longFn()) return false;

    delay(5);
  }
}

bool WifiMgr::runSerialSetup(void (*oledCb)(), bool (*cancelCb)()) {
  Serial.println("\n--- Brew370 Wi-Fi Setup ---");
  unsigned long deadline = millis() + 120000UL;  // 2-minute timeout

  char ssid[64], pass[64];

  while (millis() < deadline) {
    if (cancelCb && cancelCb()) return false;
    if (oledCb) oledCb();

    Serial.println("Enter Wi-Fi SSID:");
    while (!Serial.available()) {
      if (millis() > deadline) return false;
      if (cancelCb && cancelCb()) return false;
      if (oledCb) oledCb();
      delay(50);
    }
    String inSSID = Serial.readStringUntil('\n');
    inSSID.trim();
    if (inSSID.length() == 0) continue;
    inSSID.toCharArray(ssid, sizeof(ssid));

    Serial.println("Enter Wi-Fi Password:");
    while (!Serial.available()) {
      if (millis() > deadline) return false;
      if (cancelCb && cancelCb()) return false;
      if (oledCb) oledCb();
      delay(50);
    }
    String inPass = Serial.readStringUntil('\n');
    inPass.trim();
    inPass.toCharArray(pass, sizeof(pass));

    // Repeat back for confirmation
    Serial.printf("\nSSID:     %s\n", ssid);
    Serial.printf("Password: %s\n", pass);
    Serial.println("Confirm? (Y/n):");

    while (!Serial.available()) {
      if (millis() > deadline) return false;
      if (cancelCb && cancelCb()) return false;
      if (oledCb) oledCb();
      delay(50);
    }
    String confirm = Serial.readStringUntil('\n');
    confirm.trim();

    if (confirm == "" || confirm == "Y" || confirm == "y") {
      saveCredentials(ssid, pass);
      Serial.println("Saved. Reconnect on next boot.");
      return true;
    }
    Serial.println("Re-enter credentials.");
  }
  Serial.println("Timeout - serial setup cancelled.");
  return false;
}

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
