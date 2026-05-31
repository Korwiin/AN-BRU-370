#include "wifi_mgr.h"
#include "encoder.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>

static char s_ssid[64] = {0};
static char s_pass[64] = {0};
static bool s_connected = false;

bool WifiMgr::begin() {
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

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ANBRU-370");
  WiFi.begin(s_ssid, s_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }
  s_connected = (WiFi.status() == WL_CONNECTED);
  return s_connected;
}

bool WifiMgr::isConnected() {
  return s_connected;
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
