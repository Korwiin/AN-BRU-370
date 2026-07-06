#include "wifi_mgr.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>

#define NUS_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLECharacteristic* s_bleTxChar     = nullptr;
static volatile bool       s_bleClientConn = false;
static volatile bool       s_bleSubscribed = false;
static volatile bool       s_bleLineReady  = false;
static String              s_bleRxBuf;
static portMUX_TYPE        s_bleMux        = portMUX_INITIALIZER_UNLOCKED;

static char s_ssid[64] = {0};
static char s_pass[64] = {0};
static bool s_autoReconnect = true;

static void loadCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String nvsSsid = prefs.getString("ssid", "");
  String nvsPass = prefs.getString("pass", "");
  prefs.end();
  nvsSsid.toCharArray(s_ssid, sizeof(s_ssid));
  nvsPass.toCharArray(s_pass, sizeof(s_pass));
}

bool WifiMgr::hasCredentials() {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid.length() > 0;
}

bool WifiMgr::isBleClientConnected() { return s_bleClientConn; }

void WifiMgr::nvsCredentials(char* ssidOut, size_t ssidLen, uint8_t* passStatus) {
  Preferences prefs;
  prefs.begin("brew_wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  strlcpy(ssidOut, ssid.c_str(), ssidLen);
  if      (ssid.length() == 0) *passStatus = 0;
  else if (pass.length() == 0) *passStatus = 1;
  else                          *passStatus = 2;
}

bool WifiMgr::beginConnect(bool full) {
  loadCredentials();
  if (s_ssid[0] == '\0') return false;

  // IDF 5.x: WiFi.mode(WIFI_OFF) fully tears down the netif stack and the
  // subsequent mode(WIFI_STA)+begin() doesn't reliably restore it in the
  // same boot cycle.  Always ensure STA mode is active, then disconnect.
  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
    delay(100);
  }
  WiFi.disconnect(false, !full);  // wipeCredentials=true only on full reset

  WiFi.persistent(false);
  WiFi.setAutoReconnect(s_autoReconnect);
  WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
  // Must run after mode(WIFI_STA): setTxPower() returns false ("Neither AP or
  // STA has been started") if the driver isn't started yet.
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(s_ssid, s_pass);

#ifndef RELEASE_BUILD
  Serial.printf("[%lums] beginConnect(full=%d) ssid=%s autoReconnect=%d\n",
                (unsigned long)millis(), (int)full, s_ssid, (int)s_autoReconnect);
#endif
  return true;
}

void WifiMgr::setAutoReconnect(bool on) {
  s_autoReconnect = on;
  WiFi.setAutoReconnect(on);
}

bool WifiMgr::getAutoReconnect() {
  return s_autoReconnect;
}

bool WifiMgr::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* WifiMgr::activeSSID() {
  return s_ssid;
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

// ---- BLE UART implementation (Bluedroid) ----

class BleServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    s_bleClientConn = true;
  }
  void onDisconnect(BLEServer* pServer) override {
    s_bleClientConn = false;
    s_bleSubscribed = false;
    s_bleLineReady  = false;
    s_bleRxBuf      = "";
    pServer->startAdvertising();
  }
};

class BleRxCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String val = pChar->getValue();
    for (size_t i = 0; i < val.length(); i++) {
      char c = val[i];
      if (c == '\n' || c == '\r') {
        portENTER_CRITICAL(&s_bleMux);
        if (s_bleRxBuf.length() > 0) s_bleLineReady = true;
        portEXIT_CRITICAL(&s_bleMux);
      } else {
        portENTER_CRITICAL(&s_bleMux);
        if (!s_bleLineReady) s_bleRxBuf += c;
        portEXIT_CRITICAL(&s_bleMux);
      }
    }
  }
};

class BleTxCb : public BLECharacteristicCallbacks {
  void onSubscribe(BLECharacteristic*, ble_gap_conn_desc*, uint16_t subValue) override {
    s_bleSubscribed = (subValue & 0x0001) != 0;
  }
};

static bool bleSubscribed() { return s_bleSubscribed; }

static void bleSend(const char* msg) {
  if (!s_bleTxChar || !s_bleClientConn || !bleSubscribed()) return;
  const uint8_t* data = (const uint8_t*)msg;
  size_t len = strlen(msg);
  size_t off = 0;
  while (off < len) {
    size_t chunk = (len - off < 20) ? len - off : 20;
    s_bleTxChar->setValue(const_cast<uint8_t*>(data + off), chunk);
    s_bleTxChar->notify();
    delay(20);
    off += chunk;
  }
}

static void sendBanner() {
  char msg[512];
  snprintf(msg, sizeof(msg),
    "==============================\r\n"
    "  AN/BRU-370 CONFIG TERMINAL\r\n"
    "     F-16C Bl.50 // USAF\r\n"
    "==============================\r\n"
    "  WARNING: RESTRICTED SYSTEM\r\n"
    " Unauthorized use prohibited.\r\n"
    "==============================\r\n"
    "Current SSID: %s\r\n"
    "\r\n"
    "SSID:\r\n",
    s_ssid[0] ? s_ssid : "(none)");
  bleSend(msg);
}

bool WifiMgr::runBleSetup(void (*oledActiveCb)(), bool (*cancelCb)()) {
  WiFi.disconnect(true);
  delay(100);

  s_bleSubscribed = false;
  BLEDevice::init("AN/BRU-370");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BleServerCb());

  BLEService* pSvc = pServer->createService(NUS_SERVICE_UUID);

  s_bleTxChar = pSvc->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  s_bleTxChar->setCallbacks(new BleTxCb());

  BLECharacteristic* pRx = pSvc->createCharacteristic(
    NUS_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRx->setCallbacks(new BleRxCb());

  pSvc->start();
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
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
        if (bleSubscribed()) { sendBanner(); state = GET_SSID; }
        break;

      case GET_SSID:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf; s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          if (line.length() == 0) { bleSend("SSID:\r\n"); }
          else { strlcpy(newSSID, line.c_str(), sizeof(newSSID)); bleSend("Password:\r\n"); state = GET_PASS; }
        }
        break;

      case GET_PASS:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf; s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          strlcpy(newPass, line.c_str(), sizeof(newPass));
          const char* passDisplay = (newPass[0] == '\0') ? "(none)" : newPass;
          char msg[400];
          snprintf(msg, sizeof(msg),
            "\r\n------------------------------\r\n"
            "  SSID: %s\r\n  Pass: %s\r\n"
            "------------------------------\r\n"
            "Current: %s\r\n\r\nY=save  r=retry  n=cancel:\r\n",
            newSSID, passDisplay, s_ssid[0] ? s_ssid : "(none)");
          bleSend(msg);
          state = CONFIRM;
        }
        break;

      case CONFIRM:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf; s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          if (line == "Y" || line == "y") {
            saveCredentials(newSSID, newPass);
            bleSend("Saved. Rebooting...\r\n");
            delay(500);
            result = true; done = true;
          } else if (line == "r" || line == "R") {
            bleSend("SSID:\r\n"); state = GET_SSID;
          } else {
            bleSend("Cancelled.\r\n"); done = true;
          }
        }
        break;
    }
    delay(10);
  }

  s_bleTxChar     = nullptr;
  s_bleSubscribed = false;
  BLEDevice::deinit(true);
  delay(100);

  // Re-start WiFi connection after BLE session (whether saved or cancelled).
  // If no credentials exist yet, beginConnect returns false silently.
  if (!result) WifiMgr::beginConnect(true);
  return result;
}
