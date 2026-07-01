# WiFi Error Codes — AN/BRU-370

Error codes displayed on the boot status screen as `<text> Retry N` or `Error N Retry N`
when the named code has no display string. Source: `src/wifi_mgr.cpp` `failReasonStr()` +
`esp_wifi_types.h` (ESP32-S3 SDK).

---

## Codes with named display strings

These codes have explicit entries in `failReasonStr()` and appear as human-readable text
on the boot screen.

| Code | Display text | ESP-IDF constant | Explanation |
|-----:|---|---|---|
| 1 | `Reconnecting` | `WIFI_REASON_UNSPECIFIED` | arduino-esp32 auto-reconnect cycle in progress. Normal transient state; clears once connected. |
| 2 | `Auth expired` | `WIFI_REASON_AUTH_EXPIRE` | AP invalidated the 802.11 auth session. Seen on WPA3/WPA2 transition APs (e.g. Eero). PMF disable (`esp_wifi_disable_pmf_config`) forces WPA2 to prevent this. |
| 15 | `WPA2 failed` | `WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT` | 4-way WPA2 handshake timed out. Key exchange failed — wrong password unlikely (that's 202); more likely a radio glitch mid-handshake or AP timing issue. |
| 24 | `Cipher error` | `WIFI_REASON_CIPHER_SUITE_REJECTED` | AP rejected the cipher suite offered by the device. Can occur if AP enforces WPA3-only and PMF disable caused a mismatch. |
| 39 | `Conn timeout` | `WIFI_REASON_TIMEOUT` | Connection attempt timed out at the 802.11 association level. **Not in arduino-esp32's auto-retry list** — firmware applies an app-level 30 s retry to recover. |
| 200 | `AP lost` | `WIFI_REASON_BEACON_TIMEOUT` | AP beacons stopped arriving; device declared the AP gone. Seen during AP reboots or when the device moves out of range. |
| 201 | `AP not found` | `WIFI_REASON_NO_AP_FOUND` | Scan found no AP matching the stored SSID. Wrong SSID, AP offline, or device out of range. |
| 202 | `Wrong password` | `WIFI_REASON_AUTH_FAIL` | Authentication failed — most likely incorrect PSK. Check credentials via Settings → WiFi → Secrets. |
| 203 | `Assoc failed` | `WIFI_REASON_ASSOC_FAIL` | Association request was rejected by the AP. Can happen if the AP has reached its client limit or has a MAC filter. |
| 204 | `WPA2 failed` | `WIFI_REASON_HANDSHAKE_TIMEOUT` | WPA2 handshake timed out at the ESP-IDF layer (distinct from code 15, which is the 802.11 standard reason). Same effective meaning. |
| 205 | `Conn failed` | `WIFI_REASON_CONNECTION_FAIL` | Generic connection failure from ESP-IDF after all internal retries exhausted. |

---

## Codes without named strings (show as `Error N`)

These codes fall through to the `"Error %u"` fallback. Rare in normal operation.

| Code | ESP-IDF constant | Explanation |
|-----:|---|---|
| 3 | `WIFI_REASON_AUTH_LEAVE` | Device left the AP voluntarily (normal disconnect). |
| 4 | `WIFI_REASON_ASSOC_EXPIRE` | Association timed out due to inactivity. |
| 5 | `WIFI_REASON_ASSOC_TOOMANY` | AP has reached its maximum client count. |
| 6 | `WIFI_REASON_NOT_AUTHED` | AP disassociated device because it was not authenticated. |
| 7 | `WIFI_REASON_NOT_ASSOCED` | AP received a frame from a device that was not associated. |
| 8 | `WIFI_REASON_ASSOC_LEAVE` | Station left the association (normal client-initiated disconnect). |
| 9 | `WIFI_REASON_ASSOC_NOT_AUTHED` | Association rejected; device was not authenticated first. |
| 10 | `WIFI_REASON_DISASSOC_PWRCAP_BAD` | Power capability IE was unacceptable to the AP. |
| 11 | `WIFI_REASON_DISASSOC_SUPCHAN_BAD` | Supported channels IE was unacceptable to the AP. |
| 12 | `WIFI_REASON_BSS_TRANSITION_DISASSOC` | AP issued a BSS Transition Management (802.11v) disassoc — steering device to another AP. arduino-esp32 does not process BTM requests, so reconnection is driven by the firmware's auto-reconnect. |
| 13 | `WIFI_REASON_IE_INVALID` | An information element in the frame was invalid. |
| 14 | `WIFI_REASON_MIC_FAILURE` | Message integrity check (MIC) failed — possible interference or replay attack. |
| 16 | `WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT` | Group key handshake timed out. |
| 17 | `WIFI_REASON_IE_IN_4WAY_DIFFERS` | RSN IE in the 4-way handshake differed from the beacon. Race condition risk; can happen if AP configuration changes mid-handshake. |
| 18 | `WIFI_REASON_GROUP_CIPHER_INVALID` | Group cipher suite invalid. |
| 19 | `WIFI_REASON_PAIRWISE_CIPHER_INVALID` | Pairwise cipher suite invalid. |
| 20 | `WIFI_REASON_AKMP_INVALID` | Authentication and key management protocol (AKMP) was invalid. |
| 21 | `WIFI_REASON_UNSUPP_RSN_IE_VERSION` | RSN IE version not supported. |
| 22 | `WIFI_REASON_INVALID_RSN_IE_CAP` | RSN IE capabilities were invalid. |
| 23 | `WIFI_REASON_802_1X_AUTH_FAILED` | 802.1X (enterprise) authentication failed. Not applicable to home PSK networks. |
| 25 | `WIFI_REASON_TDLS_PEER_UNREACHABLE` | TDLS peer unreachable. TDLS not used in this project. |
| 26 | `WIFI_REASON_TDLS_UNSPECIFIED` | Unspecified TDLS reason. |
| 27–31 | Various | Spectrum management / QoS / roaming agreement reasons. Uncommon on home APs. |
| 32 | `WIFI_REASON_UNSPECIFIED_QOS` | Unspecified QoS-related disassociation. |
| 33 | `WIFI_REASON_NOT_ENOUGH_BANDWIDTH` | AP could not provide required QoS bandwidth. |
| 34 | `WIFI_REASON_MISSING_ACKS` | AP stopped receiving 802.11 ACKs from the device (or vice versa). RF link went silent long enough to declare the connection dead. Caused by range, interference, or a power glitch. Self-recovering. |
| 35 | `WIFI_REASON_EXCEEDED_TXOP` | Station exceeded its transmission opportunity window. |
| 36 | `WIFI_REASON_STA_LEAVING` | Station is leaving the BSS. |
| 37 | `WIFI_REASON_END_BA` | Block ACK stream ended. |
| 38 | `WIFI_REASON_UNKNOWN_BA` | Unknown block ACK stream. |
| 46 | `WIFI_REASON_PEER_INITIATED` | Peer-initiated disconnection. |
| 47 | `WIFI_REASON_AP_INITIATED` | AP-initiated disconnection (AP reboot, channel change, etc.). |
| 48–51 | Fast BSS Transition reasons | 802.11r roaming frame errors. ESP32 does not enable 802.11r. |
| 67 | `WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED` | Could not establish the transmission link. |
| 68 | `WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED` | Alternative channel is occupied. |
| 206 | `WIFI_REASON_AP_TSF_RESET` | AP reset its timing synchronization function (TSF) — usually an AP reboot or reset. |
| 207 | `WIFI_REASON_ROAMING` | Station is roaming to another AP. |
| 208 | `WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG` | AP asked the station to try again later but the comeback time was too long. |
| 209 | `WIFI_REASON_SA_QUERY_TIMEOUT` | Security Association (SA) query timed out (PMF / 802.11w). Relevant when PMF is active; suppressed here by `esp_wifi_disable_pmf_config()`. |

---

## Source files

- `src/wifi_mgr.cpp` — `WifiMgr::failReasonStr()` (named display strings)
- `~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/esp_wifi/include/esp_wifi_types.h` — full enum
- `docs/reference/eero-wifi-security.md` — Eero-specific context for codes 2, 12, 209
