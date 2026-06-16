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

  // Driver config — set before any radio operation
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("ANBRU-370");

  // Radio cycle — clears stale hardware state
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);

  // RF check
  String mac = WiFi.macAddress();
  bool macOk = (mac.length() >= 11 && mac != "00:00:00:00:00:00");
  bool statusOk = (WiFi.status() != WL_CONNECTED);
  if (!macOk || !statusOk) {
    s_phase_rfFail = true;
    return false;
  }
  s_phase_rf = true;

  // Blocking SSID scan (~2-3 s)
  int numNets = WiFi.scanNetworks(false, false);
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
    s_phase_ssidFail = true;
    return false;
  }
  s_phase_ssid = true;

  // All pre-checks passed — start association
  WiFi.begin(s_ssid, s_pass);
  return true;
}

// Legacy shim — used by runBleSetup() post-cancel path (same translation unit; not in header)
static void startConnect() {
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
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
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
    "\033[2J\033[H"
    "\033[0;31m[UNCLASSIFIED]\033[0m\r\n"
    "\r\n"
    "\033[1;32mAN/BRU-370 CONFIG TERMINAL\033[0m\r\n"
    "\033[1;32mF-16C Bl.50 // USAF\033[0m\r\n"
    "\r\n"
    "\033[0;31mWARNING: RESTRICTED SYSTEM\033[0m\r\n"
    "\033[0;31mUnauthorized use prohibited.\033[0m\r\n"
    "\r\n"
    "\033[0;32mCurrent SSID: %s\033[0m\r\n"
    "\r\n"
    "Enter \033[1;32mSSID\033[0;32m:\033[0m \r\n",
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
            bleSend("Enter \033[1;32mSSID\033[0;32m:\033[0m \r\n");
          } else {
            strlcpy(newSSID, line.c_str(), sizeof(newSSID));
            bleSend("Enter \033[1;32mPassword\033[0;32m:\033[0m \r\n");
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
          char msg[256];
          snprintf(msg, sizeof(msg),
            "\033[0;32m\r\nCurrent SSID: %s\r\n\r\n"
            "  \033[1;32mSSID\033[0;32m: %s\r\n"
            "  \033[1;32mPass\033[0;32m: %s\033[0m\r\n"
            "\r\n"
            "Save [\033[1;32mY\033[0;32m / \033[1;32mr\033[0;32m=retry / \033[1;32mn\033[0;32m=cancel\033[0m]:\r\n",
            s_ssid[0] ? s_ssid : "(none)", newSSID, passDisplay);
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
            bleSend("\033[0;32mEnter \033[1;32mSSID\033[0;32m:\033[0m \r\n");
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
