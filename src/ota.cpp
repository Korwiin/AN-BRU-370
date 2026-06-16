#include "ota.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

OTA::CheckResult OTA::check() {
  CheckResult result = {};
  WiFi.setSleep(false);

  // DCS-BIOS uses a hard-coded IP, so WiFi connected ≠ DNS working
  IPAddress ip;
  if (!WiFi.hostByName("raw.githubusercontent.com", ip)) {
    strlcpy(result.error, "DNS fail", sizeof(result.error));
    WiFi.setSleep(true);
    return result;
  }

  // Retry once — raw.githubusercontent.com TLS handshakes fail transiently.
  // Client declared outside loop: one TLS context reused across retries
  // instead of two partial contexts competing for heap.
  // client.setTimeout caps the TLS handshake phase; http timeouts only cover TCP connect + reads.
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);  // seconds — WiFiClientSecure::setTimeout is in seconds, not ms
  int   code       = -1;
  int   codes[2]   = {0, 0};
  int   elapsed[2] = {0, 0};
  String payload;
  for (int attempt = 0; attempt < 2 && code != HTTP_CODE_OK; attempt++) {
    if (attempt > 0) delay(2000);
    HTTPClient http;
    if (!http.begin(client, OTA_MANIFEST_URL)) {
      strlcpy(result.error, "URL err", sizeof(result.error));
      WiFi.setSleep(true);
      return result;
    }
    http.setConnectTimeout(15000);
    http.setTimeout(20000);
    unsigned long t0 = millis();
    code             = http.GET();
    elapsed[attempt] = (int)((millis() - t0) / 1000);
    codes[attempt]   = code;
    if (code == HTTP_CODE_OK) {
      payload = http.getString();
    }
    http.end();
  }

  if (code != HTTP_CODE_OK) {
    snprintf(result.error, sizeof(result.error), "a1:%d/%ds a2:%d/%ds",
             codes[0], elapsed[0], codes[1], elapsed[1]);
    WiFi.setSleep(true);
    return result;
  }

  // Parse {"version": N, "url": "..."}
  const char* src = payload.c_str();

  const char* vp = strstr(src, "\"version\"");
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  vp = strchr(vp, ':');
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  result.versionBCD = (uint16_t)atoi(vp + 1);

  const char* up = strstr(src, "\"url\"");
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  up += 5;
  up = strchr(up, '"');
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  up++;
  const char* ue = strchr(up, '"');
  if (!ue) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  size_t len = (size_t)(ue - up);
  if (len >= sizeof(result.url)) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); WiFi.setSleep(true); return result; }
  memcpy(result.url, up, len);
  result.url[len] = '\0';

  result.available = (result.versionBCD > FIRMWARE_VERSION_BCD);
  WiFi.setSleep(true);
  return result;
}

static char s_performError[32] = {};
const char* OTA::performError() { return s_performError; }

bool OTA::perform(const char* url, void(*progress)(int)) {
  s_performError[0] = '\0';
  WiFi.setSleep(false);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);  // seconds — WiFiClientSecure::setTimeout is in seconds, not ms

  // Retry once — GitHub release redirects to S3 can transiently refuse
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  int code = -1;
  for (int attempt = 0; attempt < 2 && code != HTTP_CODE_OK; attempt++) {
    if (attempt > 0) { http.end(); delay(3000); }
    if (!http.begin(client, url)) {
      WiFi.setSleep(true);
      strlcpy(s_performError, "begin fail", sizeof(s_performError));
      return false;
    }
    http.setConnectTimeout(15000);
    http.setTimeout(30000);
    code = http.GET();
  }
  if (code != HTTP_CODE_OK) {
    http.end(); WiFi.setSleep(true);
    snprintf(s_performError, sizeof(s_performError), "GET %d", code);
    return false;
  }

  int totalBytes = http.getSize();
  if (!Update.begin(totalBytes > 0 ? totalBytes : UPDATE_SIZE_UNKNOWN)) {
    http.end(); WiFi.setSleep(true);
    strlcpy(s_performError, "begin OTA fail", sizeof(s_performError));
    return false;
  }

  if (progress) {
    Update.onProgress([progress, totalBytes](size_t done, size_t total) {
      if (totalBytes > 0) progress((int)((done * 100) / totalBytes));
    });
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  http.end();
  WiFi.setSleep(true);

  bool sizeOk = (totalBytes <= 0) || ((int)written == totalBytes);
  if (!sizeOk || !Update.end(true)) {
    Update.abort();
    snprintf(s_performError, sizeof(s_performError), "w%d/%d e%d",
             (int)written, totalBytes, (int)Update.getError());
    return false;
  }
  WiFi.disconnect(true);  // clean disassociation before reset; prevents router-side stale session on next boot
  delay(200);
  ESP.restart();
  return true;  // never reached
}
