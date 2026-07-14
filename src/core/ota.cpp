#include "ota.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>

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
  result.versionInt = (uint16_t)atoi(vp + 1);

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

  result.available = (result.versionInt > FIRMWARE_VERSION_INT);
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

  // Use IDF OTA APIs directly — the Arduino Update library declares a partition-sized
  // write on UPDATE_SIZE_UNKNOWN, causing esp_ota_end() in IDF 5.5.4 to reject the
  // image when written < declared_size. OTA_WITH_SEQUENTIAL_WRITES erases sectors
  // on-demand and lets esp_ota_end() validate from the image header instead.
  const esp_partition_t* ota_part = esp_ota_get_next_update_partition(NULL);
  if (!ota_part) {
    http.end(); WiFi.setSleep(true);
    strlcpy(s_performError, "no OTA part", sizeof(s_performError));
    return false;
  }

  esp_ota_handle_t ota_handle = 0;
  esp_err_t err = esp_ota_begin(ota_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
  if (err != ESP_OK) {
    http.end(); WiFi.setSleep(true);
    snprintf(s_performError, sizeof(s_performError), "ota_begin %d", (int)err);
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  unsigned long stallAt = millis();

  while (http.connected() || stream->available()) {
    int avail = stream->available();
    if (avail <= 0) {
      if (millis() - stallAt > 30000UL) {
        esp_ota_abort(ota_handle);
        http.end(); WiFi.setSleep(true);
        snprintf(s_performError, sizeof(s_performError), "stall w%dk", (int)written / 1024);
        return false;
      }
      delay(10);
      continue;
    }
    stallAt = millis();

    int toRead = min(avail, (int)sizeof(buf));
    if (totalBytes > 0) toRead = min(toRead, totalBytes - (int)written);
    int n = stream->readBytes(buf, toRead);
    if (n <= 0) break;

    err = esp_ota_write(ota_handle, buf, n);
    if (err != ESP_OK) {
      esp_ota_abort(ota_handle);
      http.end(); WiFi.setSleep(true);
      snprintf(s_performError, sizeof(s_performError), "write e%d w%dk", (int)err, (int)written / 1024);
      return false;
    }
    written += n;
    if (progress && totalBytes > 0)
      progress((int)((written * 100) / totalBytes));

    if (totalBytes > 0 && (int)written >= totalBytes) break;
  }
  http.end();

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    WiFi.setSleep(true);
    snprintf(s_performError, sizeof(s_performError), "ota_end %d w%dk", (int)err, (int)written / 1024);
    return false;
  }

  err = esp_ota_set_boot_partition(ota_part);
  if (err != ESP_OK) {
    WiFi.setSleep(true);
    snprintf(s_performError, sizeof(s_performError), "set_boot %d", (int)err);
    return false;
  }

  WiFi.setSleep(true);
  ESP.restart();
  return true;  // never reached
}
