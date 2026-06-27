#include "wifi_mgr.h"
#include "encoder.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

#define NUS_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLECharacteristic* s_bleTxChar      = nullptr;
static volatile bool          s_bleClientConn  = false;
static volatile bool          s_bleSubscribed  = false;
static String                 s_bleRxBuf;
static volatile bool          s_bleLineReady   = false;
static portMUX_TYPE           s_bleMux         = portMUX_INITIALIZER_UNLOCKED;

static char s_ssid[64] = {0};
static char s_pass[64] = {0};
static bool s_connected = false;

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

int WifiMgr::rssi() { return WiFi.RSSI(); }

bool WifiMgr::checkInternet() {
  static bool          s_result    = false;
  static unsigned long s_lastCheck = 0;
  static bool          s_hasResult = false;
  if (!isConnected()) { s_hasResult = false; s_lastCheck = 0; return false; }
  unsigned long now = millis();
  if (s_hasResult && now - s_lastCheck < 30000UL) return s_result;
  IPAddress ip;
  s_result    = (WiFi.hostByName("raw.githubusercontent.com", ip) == 1);
  s_lastCheck = now;
  s_hasResult = true;
  return s_result;
}

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

static void registerEventHandler() {
  if (s_eventRegistered) return;
  s_eventRegistered = true;
  WiFi.onEvent([](WiFiEvent_t ev, WiFiEventInfo_t info) {
    switch (ev) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        s_phase_ssid = true;  // SSID was found; confirmed by successful association
        s_phase_eth  = true;
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

bool WifiMgr::beginAttempt(int n) {
  (void)n;
  loadCredentials();
  if (s_ssid[0] == '\0') {
    s_phase_ssidFail = true;
    return false;
  }
  registerEventHandler();

  // Reset all phase flags for this attempt
  s_phase_rf = s_phase_ssid = s_phase_eth = s_phase_ip = s_phase_dns = false;
  s_phase_rfFail = s_phase_ssidFail = false;
  s_phase_failReason = 0;
  s_connected = false;

  // Driver config
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);  // disabled during boot; re-enabled after IP acquired in main.cpp

  // Radio cycle — clears stale hardware state
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ANBRU-370");  // after mode set so it takes effect

  // Clear any DISCONNECTED events that fired during the mode cycle (e.g. ASSOC_LEAVE on retry)
  // Without this, s_phase_failReason is non-zero from the mode-OFF teardown on retry attempts,
  // causing needRetry to fire immediately before the new WiFi.begin() has time to complete.
  s_phase_failReason = 0;
  s_connected = false;

  // RF check
  String mac = WiFi.macAddress();
  bool macOk = (mac.length() >= 11 && mac != "00:00:00:00:00:00");
  bool statusOk = (WiFi.status() != WL_CONNECTED);
  if (!macOk || !statusOk) {
    s_phase_rfFail = true;
    return false;
  }
  s_phase_rf = true;

  // Full-channel scan sorted by RSSI so the driver connects to the strongest
  // Eero node in the mesh. WIFI_FAST_SCAN (the library default) stops at the
  // first AP found, which may not be the nearest node. WIFI_ALL_CHANNEL_SCAN
  // surveys all nodes before associating, eliminating the probe-request storm
  // that the previous explicit WiFi.scanNetworks() was generating during
  // Eero's WPA3 SAE session teardown window.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(s_ssid, s_pass);
  return true;
}

// Legacy shim — used by runBleSetup() post-cancel path (same translation unit; not in header)
static void startConnect() {
  loadCredentials();
  if (s_ssid[0] == '\0') return;
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
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
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

bool WifiMgr::pollConnect() {
  static bool s_reported = false;
  if (!s_phase_ip) { s_reported = false; return false; }
  if (s_reported) return false;
  s_reported = true;
  s_connected = true;
  return true;
}

void WifiMgr::cancelConnect() {
  WiFi.disconnect(true);
  s_connected = false;
  s_phase_ip  = false;
}

bool WifiMgr::isConnected() {
  return s_connected || s_phase_ip;
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
  loadCredentials();
  registerEventHandler();
  s_connected    = false;
  s_phase_ip     = false;
  s_phase_eth    = false;
  s_phase_failReason = 0;
  WiFi.setAutoReconnect(false);  // prevent framework from competing during reconnect
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ANBRU-370");
  delay(100);                    // allow lwIP netif to commit hostname before DHCP fires
  s_phase_failReason = 0;        // clear mode-cycle disconnect events
  s_connected = false;
  WiFi.setAutoReconnect(true);   // re-enable for runtime drop recovery
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.begin(s_ssid, s_pass);
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

class BleServerCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*) override {
    s_bleClientConn = true;
  }
  void onDisconnect(NimBLEServer* pServer) override {
    s_bleSubscribed  = false;
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

class BleTxSubscribeCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, ble_gap_conn_desc*, uint16_t subValue) override {
    if (subValue & 0x0001) {
      s_bleSubscribed = true;
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
  s_connected = false;
  s_bleSubscribed = false;

  NimBLEDevice::init("AN/BRU-370");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new BleServerCb());

  NimBLEService* pSvc = pServer->createService(NUS_SERVICE_UUID);
  s_bleTxChar = pSvc->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  s_bleTxChar->setCallbacks(new BleTxSubscribeCb());
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
        if (s_bleSubscribed) {
          sendBanner();
          state = GET_SSID;
        }
        break;

      case GET_SSID:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          if (line.length() == 0) {
            bleSend("SSID:\r\n");
          } else {
            strlcpy(newSSID, line.c_str(), sizeof(newSSID));
            bleSend("Password:\r\n");
            state = GET_PASS;
          }
        }
        break;

      case GET_PASS:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          strlcpy(newPass, line.c_str(), sizeof(newPass));
          const char* passDisplay = (newPass[0] == '\0') ? "(none)" : newPass;
          char msg[400];
          snprintf(msg, sizeof(msg),
            "\r\n"
            "------------------------------\r\n"
            "  SSID: %s\r\n"
            "  Pass: %s\r\n"
            "------------------------------\r\n"
            "Current: %s\r\n"
            "\r\n"
            "Y=save  r=retry  n=cancel:\r\n",
            newSSID, passDisplay,
            s_ssid[0] ? s_ssid : "(none)");
          bleSend(msg);
          state = CONFIRM;
        }
        break;

      case CONFIRM:
        if (!s_bleClientConn) { state = WAIT_CLIENT; break; }
        if (s_bleLineReady) {
          portENTER_CRITICAL(&s_bleMux);
          String line = s_bleRxBuf;
          s_bleRxBuf = ""; s_bleLineReady = false;
          portEXIT_CRITICAL(&s_bleMux);
          line.trim();
          if (line == "Y" || line == "y") {
            saveCredentials(newSSID, newPass);
            bleSend("Saved. Rebooting...\r\n");
            delay(500);
            result = true;
            done   = true;
          } else if (line == "r" || line == "R") {
            bleSend("SSID:\r\n");
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
