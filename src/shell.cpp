#include "shell.h"

#ifdef DEV_BUILD
#include <WiFi.h>
#include <esp_wifi.h>
#include <nvs.h>
#include "encoder.h"
#include "ui.h"
#include "wifi_mgr.h"
#include "dcs_bios.h"

static Shell::Hooks s_hooks;
static char s_line[96];
static uint8_t s_lineLen = 0;
static bool s_busy = false;

static void dispatch(char* line);

// --- WiFi event ring (32 entries) ---
// Written from the arduino-esp32 WiFi event task (onWifiEvent), read from the
// main loop task (wifi?/wifi log? handlers) — guard all access with s_evtMux.
struct WifiEvt { uint32_t ms; uint16_t ev; uint8_t reason; };
static portMUX_TYPE s_evtMux = portMUX_INITIALIZER_UNLOCKED;
static WifiEvt  s_evts[32];
static uint8_t  s_evtNext = 0;
static uint8_t  s_evtCount = 0;
// Most recent disconnect reason. Deliberately NOT cleared on reconnect —
// after a drop + auto-reconnect, `wifi?` still shows why the last drop happened.
static uint8_t  s_lastReason = 0;

static const char* wifiReasonName(uint8_t r) {
  switch (r) {   // esp_wifi_types.h wifi_err_reason_t — verified against
                 // ~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/esp_wifi/include/esp_wifi_types.h
    case 1:   return "UNSPECIFIED";
    case 2:   return "AUTH_EXPIRE";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE";
    case 5:   return "ASSOC_TOOMANY";
    case 6:   return "NOT_AUTHED";
    case 7:   return "NOT_ASSOCED";
    case 8:   return "ASSOC_LEAVE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    default:  return "?";
  }
}

static const char* wifiEvtName(uint16_t ev) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_START:        return "STA_START";
    case ARDUINO_EVENT_WIFI_STA_STOP:         return "STA_STOP";
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:    return "CONNECTED";
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: return "DISCONNECTED";
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:       return "GOT_IP";
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:      return "LOST_IP";
    default: return "?";
  }
}

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  uint8_t reason = 0;
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    reason = info.wifi_sta_disconnected.reason;
  }
  portENTER_CRITICAL(&s_evtMux);
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    s_lastReason = reason;
  }
  s_evts[s_evtNext] = { (uint32_t)millis(), (uint16_t)event, reason };
  s_evtNext = (s_evtNext + 1) % 32;
  if (s_evtCount < 32) s_evtCount++;
  portEXIT_CRITICAL(&s_evtMux);
}

void Shell::begin(const Hooks& hooks) {
  s_hooks = hooks;
  WiFi.onEvent(onWifiEvent);
  Serial.println("#shell ready (type 'help')");
}

void Shell::poll() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      s_line[s_lineLen] = '\0';
      if (s_lineLen > 0) dispatch(s_line);
      s_lineLen = 0;
      continue;
    }
    if (s_lineLen < sizeof(s_line) - 1) s_line[s_lineLen++] = c;
    else s_lineLen = 0;  // overlong line: discard silently
  }
}

// Split "wifi conn full" into verb="wifi", rest="conn full" (rest may be "").
static void dispatch(char* line) {
  if (s_busy) { Serial.println("#err busy"); return; }
  s_busy = true;

  char* rest = strchr(line, ' ');
  if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; }
  else rest = line + strlen(line);

  if (strcmp(line, "ping") == 0) {
    Serial.println("#pong");
    Serial.println("#ok");
  } else if (strcmp(line, "help") == 0) {
    Serial.println("#ping|help|state?|enc <n>|enc sp|enc lp|fb?");
    Serial.println("#wifi?|wifi log?|wifi on|wifi off|wifi conn [full|soft]");
    Serial.println("#wifi auto on|off|wifi boot|wifi scan|wifi nvs?|wifi restore");
    Serial.println("#ok");
  } else if (strcmp(line, "state?") == 0) {
    Serial.printf("#state %s id=%u sel=%d wifi=%d dcs=%d\n",
                  s_hooks.modeName(), s_hooks.modeId(), s_hooks.menuSel(),
                  WifiMgr::isConnected() ? 1 : 0, DcsBios::isConnected() ? 1 : 0);
    Serial.println("#ok");
  } else if (strcmp(line, "wifi?") == 0) {
    portENTER_CRITICAL(&s_evtMux);
    uint8_t lastReason = s_lastReason;
    portEXIT_CRITICAL(&s_evtMux);
    Serial.printf("#wifi status=%d ssid=%s bssid=%s ch=%d rssi=%d ip=%s auto=%d lastReason=%u:%s\n",
                  (int)WiFi.status(),
                  WifiMgr::isConnected() ? WiFi.SSID().c_str() : WifiMgr::activeSSID(),
                  WiFi.BSSIDstr().c_str(),
                  WiFi.channel(),
                  WiFi.RSSI(),
                  WiFi.localIP().toString().c_str(),
                  WifiMgr::getAutoReconnect() ? 1 : 0,
                  lastReason, wifiReasonName(lastReason));
    Serial.println("#ok");
  } else if (strcmp(line, "wifi") == 0) {
    if (strcmp(rest, "log?") == 0) {
      WifiEvt snap[32];
      uint8_t snapNext, snapCount;
      portENTER_CRITICAL(&s_evtMux);
      memcpy(snap, s_evts, sizeof(snap));
      snapNext = s_evtNext;
      snapCount = s_evtCount;
      portEXIT_CRITICAL(&s_evtMux);

      uint8_t start = (snapCount < 32) ? 0 : snapNext;
      for (uint8_t i = 0; i < snapCount; i++) {
        const WifiEvt& e = snap[(start + i) % 32];
        if (e.reason)
          Serial.printf("#evt %lu %s reason=%u:%s\n",
                        (unsigned long)e.ms, wifiEvtName(e.ev), e.reason, wifiReasonName(e.reason));
        else
          Serial.printf("#evt %lu %s\n", (unsigned long)e.ms, wifiEvtName(e.ev));
      }
      Serial.println("#ok");
    } else if (strcmp(rest, "off") == 0) {
      WiFi.disconnect(true /*wifioff*/);
      WiFi.mode(WIFI_OFF);
      Serial.println("#ok");
    } else if (strcmp(rest, "on") == 0) {
      WiFi.mode(WIFI_STA);   // radio on, no connect. TX power is set inside
                             // WifiMgr::beginConnect (after mode STA — v0.96 fix);
                             // bare 'on' is for scan/probe only.
      Serial.println("#ok");
    } else if (strcmp(rest, "conn") == 0 || strcmp(rest, "conn full") == 0) {
      if (!WifiMgr::beginConnect(true)) Serial.println("#err no credentials in NVS");
      else Serial.println("#ok");
    } else if (strcmp(rest, "conn soft") == 0) {
      if (!WifiMgr::beginConnect(false)) Serial.println("#err no credentials in NVS");
      else Serial.println("#ok");
    } else if (strcmp(rest, "auto on") == 0) {
      WifiMgr::setAutoReconnect(true);  Serial.println("#ok");
    } else if (strcmp(rest, "auto off") == 0) {
      WifiMgr::setAutoReconnect(false); Serial.println("#ok");
    } else if (strcmp(rest, "boot") == 0) {
      Serial.println("#wifi running production boot sequence (blocking)");
      // Must be the LAST use of line/rest in this handler: wifiBoot() blocks in
      // connectWifi(), whose Shell::poll() reuses s_line — the buffer these
      // pointers alias. Reading them after this call would parse corrupted data.
      s_hooks.wifiBoot();
      Serial.printf("#wifi boot done, connected=%d\n", WifiMgr::isConnected() ? 1 : 0);
      Serial.println("#ok");
    } else if (strcmp(rest, "scan") == 0) {
      if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
      int n = WiFi.scanNetworks();   // blocking, ~2-3 s
      for (int i = 0; i < n; i++) {
        Serial.printf("#ap \"%s\" %s ch=%d rssi=%d auth=%d\n",
                      WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(),
                      WiFi.channel(i), WiFi.RSSI(i), (int)WiFi.encryptionType(i));
      }
      WiFi.scanDelete();
      Serial.printf("#scan %d networks\n", n);
      Serial.println("#ok");
    } else if (strcmp(rest, "nvs?") == 0) {
      // Driver-internal persisted WiFi state (namespace nvs.net80211).
      // Read-only: report which keys exist and their sizes; never print contents.
      nvs_handle_t h;
      esp_err_t err = nvs_open("nvs.net80211", NVS_READONLY, &h);
      if (err != ESP_OK) {
        Serial.printf("#nvs namespace absent (err 0x%x)\n", err);
        Serial.println("#ok");
      } else {
        static const char* kKeys[] = { "sta.ssid", "sta.pswd", "sta.pmk", "sta.apinfo" };
        for (auto key : kKeys) {
          size_t len = 0;
          if (nvs_get_blob(h, key, nullptr, &len) == ESP_OK)
            Serial.printf("#nvs %s blob %u bytes\n", key, (unsigned)len);
          else if (nvs_get_str(h, key, nullptr, &len) == ESP_OK)
            Serial.printf("#nvs %s str %u bytes\n", key, (unsigned)len);
          else
            Serial.printf("#nvs %s absent\n", key);
        }
        nvs_close(h);
        Serial.println("#ok");
      }
    } else if (strcmp(rest, "wipe") == 0) {
      // Raw erase of the driver's NVS namespace. Only effective BEFORE the WiFi
      // driver initializes this boot (dev boot never touches WiFi) — a running
      // driver with FLASH storage rewrites the keys (observed with esp_wifi_restore).
      nvs_handle_t h;
      esp_err_t err = nvs_open("nvs.net80211", NVS_READWRITE, &h);
      if (err == ESP_OK) {
        err = nvs_erase_all(h);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
      }
      if (err == ESP_OK) { Serial.println("#wifi nvs namespace erased"); Serial.println("#ok"); }
      else Serial.printf("#err nvs wipe 0x%x\n", err);
    } else if (strcmp(rest, "mac rand") == 0) {
      // Randomized locally-administered STA MAC for this boot only (RAM).
      // Discriminator for AP-side MAC blocks. Call before conn.
      if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
      uint8_t mac[6];
      esp_fill_random(mac, 6);
      mac[0] = (mac[0] | 0x02) & 0xFE;  // locally administered, unicast
      esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, mac);
      if (err == ESP_OK) {
        Serial.printf("#wifi sta mac now %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.println("#ok");
      } else Serial.printf("#err esp_wifi_set_mac 0x%x\n", err);
    } else if (strncmp(rest, "txpwr ", 6) == 0) {
      // Runtime TX power override in dBm (driver units are 0.25 dBm, range 8..84).
      // beginConnect() re-applies WIFI_POWER_MINUS_1dBm, so issue this AFTER conn.
      int dbm = atoi(rest + 6);
      if (dbm < 2 || dbm > 21) {
        Serial.println("#err usage: wifi txpwr <2..21 dBm>");
      } else {
        esp_err_t err = esp_wifi_set_max_tx_power((int8_t)(dbm * 4));
        int8_t rb = 0;
        esp_wifi_get_max_tx_power(&rb);
        if (err == ESP_OK) { Serial.printf("#wifi txpwr set, readback %.2f dBm\n", rb / 4.0); Serial.println("#ok"); }
        else Serial.printf("#err set_max_tx_power 0x%x\n", err);
      }
    } else if (strcmp(rest, "allscan") == 0) {
      // Override the arduino-esp32 default WIFI_FAST_SCAN (first SSID match —
      // on a mesh this picks whichever node answers first, not the strongest).
      // Persists until reboot; affects subsequent `wifi conn`.
      WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
      WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
      Serial.println("#wifi scan=all-channel sort=by-signal");
      Serial.println("#ok");
    } else if (strcmp(rest, "restore") == 0) {
      // esp_wifi_restore(): erase the driver's persisted settings (incl. sta.pmk).
      // Needs an initialized driver; our own creds live in a separate namespace.
      if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
      esp_err_t err = esp_wifi_restore();
      if (err == ESP_OK) Serial.println("#wifi nvs restored to defaults");
      else               Serial.printf("#err esp_wifi_restore 0x%x\n", err);
      if (err == ESP_OK) Serial.println("#ok");
    } else {
      Serial.printf("#err unknown wifi subcmd '%s'\n", rest);
    }
  } else if (strcmp(line, "enc") == 0) {
    if (strcmp(rest, "sp") == 0)      { Encoder::inject(0, 1); Serial.println("#ok"); }
    else if (strcmp(rest, "lp") == 0) { Encoder::inject(0, 2); Serial.println("#ok"); }
    else {
      char* end = nullptr;
      long n = strtol(rest, &end, 10);
      if (end != rest && n >= -100 && n <= 100 && n != 0) {
        Encoder::inject((int8_t)n, 0);
        Serial.println("#ok");
      } else Serial.println("#err usage: enc <±n>|sp|lp");
    }
  } else if (strcmp(line, "fb?") == 0) {
    uint16_t len = 0;
    const uint8_t* buf = UI::frameBuffer(len);
    for (uint16_t off = 0; off < len; off += 64) {
      char hex[129];
      for (int i = 0; i < 64; i++) sprintf(&hex[i * 2], "%02x", buf[off + i]);
      Serial.printf("#fb %u %s\n", off, hex);
    }
    Serial.println("#ok");
  } else {
    Serial.printf("#err unknown cmd '%s'\n", line);
  }

  s_busy = false;
}
#endif  // DEV_BUILD
