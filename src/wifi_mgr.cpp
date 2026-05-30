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
