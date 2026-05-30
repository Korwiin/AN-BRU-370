#include "wifi_mgr.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>

static char s_ssid[64] = {0};
static char s_pass[64] = {0};

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
  WiFi.begin(s_ssid, s_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
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
