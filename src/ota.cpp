#include "ota.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

OTA::CheckResult OTA::check() {
  CheckResult result = {};

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, OTA_MANIFEST_URL)) {
    strlcpy(result.error, "No server", sizeof(result.error));
    return result;
  }
  http.setTimeout(8000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    strlcpy(result.error, "No server", sizeof(result.error));
    return result;
  }

  String payload = http.getString();
  http.end();

  // Parse {"version": N, "url": "..."}
  const char* src = payload.c_str();

  const char* vp = strstr(src, "\"version\"");
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  vp = strchr(vp, ':');
  if (!vp) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  result.versionBCD = (uint16_t)atoi(vp + 1);

  const char* up = strstr(src, "\"url\"");
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  up += 5;
  up = strchr(up, '"');
  if (!up) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  up++;
  const char* ue = strchr(up, '"');
  if (!ue) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  size_t len = (size_t)(ue - up);
  if (len >= sizeof(result.url)) { strlcpy(result.error, "Bad manifest", sizeof(result.error)); return result; }
  memcpy(result.url, up, len);
  result.url[len] = '\0';

  result.available = (result.versionBCD > FIRMWARE_VERSION_BCD);
  return result;
}

bool OTA::perform(const char* url, void(*progress)(int)) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  http.setTimeout(30000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  int totalBytes = http.getSize();
  if (!Update.begin(totalBytes > 0 ? totalBytes : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[512];
  int written = 0;

  while (http.connected() && (totalBytes < 0 || written < totalBytes)) {
    int avail = stream->available();
    if (avail > 0) {
      int toRead = min(avail, (int)sizeof(buf));
      size_t n = stream->readBytes(buf, toRead);
      size_t wrote = Update.write(buf, n);
      if (wrote != n) { http.end(); Update.abort(); return false; }
      written += (int)wrote;
      if (totalBytes > 0 && progress) {
        progress((written * 100) / totalBytes);
      }
    } else {
      delay(1);
    }
  }

  http.end();
  if (!Update.end(true)) return false;
  ESP.restart();
  return true;  // never reached
}
