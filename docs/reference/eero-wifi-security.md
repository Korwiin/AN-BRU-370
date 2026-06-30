# Eero WiFi Security — Deauth Protection, Client Steering & Reconnect Behavior

Reference document for the Brew370 AN/BRU-370 project. Compiled 2026-06-29.

---

## What We Know (Confirmed)

### Eero WPA3 Transition Mode

Eero's WPA3 implementation is **transition mode**: WPA3-capable devices connect via WPA3 SAE; WPA2-only devices fall back to WPA2. Both coexist on the same SSID simultaneously.

Source: [Eero Help — What is WPA3?](https://support.eero.com/hc/en-us/articles/360042523671-What-is-WPA3)

Key behavior documented by Eero:
- MFP (Management Frame Protection) is enabled when WPA3 is on. Eero states: *"eero uses MFP to make it more difficult for attackers to perform deauthentication attacks."*
- If a device has trouble connecting after WPA3 is enabled, Eero recommends disabling WPA3 for compatibility.
- **Documented interoperability issues**: Some IoT/legacy devices fail to connect or disconnect intermittently even in transition mode — reported by Eero community users with smart home devices (Meross, etc.).

Source: [Eero Community — Enable WPA3 blocks incompatible devices](https://community.eero.com/t/y4hm5ck/enable-wpa3-for-compatible-devices-blocks-incompatible-devices)

---

### Protected Management Frames (PMF / 802.11w)

PMF is defined in IEEE 802.11w-2009 (now part of IEEE 802.11-2020). It protects Deauthentication, Disassociation, and Robust Action frames with cryptographic signatures.

| Mode | WPA2 Behavior | WPA3 Behavior |
|---|---|---|
| PMF Optional (capable=1, required=0) | Client negotiates PMF if AP supports it | Not valid — WPA3 requires PMF |
| PMF Required (capable=1, required=1) | Client MUST use PMF or connection rejected | WPA3 SAE requires this |
| PMF Disabled (capable=0, required=0) | No management frame protection | Cannot use WPA3 |

**WPA3 requires PMF Required.** Without PMF, WPA3 SAE cannot be negotiated. This is the mechanism our `esp_wifi_disable_pmf_config()` exploits: disabling PMF forces Eero to fall back to WPA2.

Sources:
- [802.11w PMF Overview — praneethwifi.in](https://praneethwifi.in/2020/03/07/protected-management-frames-in-wpa2-802-11w-wpa3-owe/)
- [802.11w Lab Tests — dot11.exposed](https://dot11.exposed/2018/10/13/802-11w-2009-protected-management-frames-pmf-overview-and-lab-tests/)

---

### How PMF Affects Deauthentication

**Without PMF (WPA2, PMF disabled):**
- Deauth and Disassoc frames are sent in plaintext and unauthenticated
- Any device can forge a deauth frame — classic "deauth attack" surface
- AP accepts deauth from client and clears the session immediately

**With PMF (WPA2+PMF or WPA3):**
- Deauth/Disassoc frames are cryptographically protected (MIC from PTK/IGTK keys)
- Forged deauth frames are silently dropped by the AP
- **SA Query procedure**: If the AP has an active PMF session for a client MAC and receives a new Association Request, it rejects it with **Status Code 30** ("Try again later") and includes an **Association Comeback Time** (typically 1–10 seconds, default 1s on enterprise APs). The client must wait before retrying.

Source: [Cisco 802.11w Configuration Guide](https://www.cisco.com/c/en/us/support/docs/wireless-mobility/wireless-lan-wlan/212576-configure-802-11w-management-frame-prote.html)

---

### Eero Client Steering and 802.11k/v/r Roaming

#### What Eero does

Eero uses **802.11v BSS Transition Management (BTM)** for client steering — the mechanism that encourages devices to roam to a better-positioned AP node. Eero's own documentation states:

> *"The option will have the eero suggest which access point a device should connect to, but it cannot require the device to comply."*

BTM is fundamentally a *suggestion* protocol:
1. Eero sends a **BTM Request** frame listing preferred APs with signal quality data
2. The client can accept (and roam), decline, or ignore entirely
3. The AP can optionally set `disassociate-imminent=1` in the BTM frame — this means *"I will disconnect you after N seconds if you don't roam"*. This is the only force mechanism.

**Whether Eero sets `disassociate-imminent=1` is not publicly documented.** If it does, devices that ignore the BTM request will be forcefully disconnected, triggering a reconnect from scratch.

Source: [Eero Help — Why doesn't my device connect to the closest eero?](https://support.eero.com/hc/en-us/articles/360039477051-Why-doesn-t-my-device-connect-to-the-closest-eero)

#### What ESP32-S3 supports (and what it doesn't in practice)

| Protocol | Purpose | ESP-IDF Support | arduino-esp32 default |
|---|---|---|---|
| 802.11k | Neighbor reports — let client find candidate APs | Partial (v5.0+) | **Disabled** |
| 802.11v | BTM — AP suggests roam target | Available (v5.0+) | **Disabled** (`btm_enabled=0`) |
| 802.11r | Fast BSS Transition — speeds up re-authentication on roam | Available (v5.0+) | **Disabled** (`ft_enabled=0`) |

`WiFi.begin()` in arduino-esp32's `WiFiSTA.cpp` calls `wifi_sta_config()` which hardcodes `btm_enabled=0` and `ft_enabled=0`. **Our ESP32-S3 never opts in to any of these protocols.**

Known open issues confirming practical limitations:
- [esp-idf #3671](https://github.com/espressif/esp-idf/issues/3671) — "Does ESP32 support seamless roaming?" (2020, still referenced)
- [esp-idf #11936](https://github.com/espressif/esp-idf/issues/11936) — "Wifi roaming does not work (802.11k/v/r) in STA mode" (open)

Even when explicitly enabled via `esp_wifi_set_config()`, roaming behaviour is unreliable. The ESP32 only roams when the current connection fully drops — it does not proactively roam in response to BTM suggestions.

#### Implication for Brew370 disconnections

When Eero sends a BTM Request to the ESP32-S3:
1. ESP32 ignores it (no `btm_enabled`)
2. If Eero has `disassociate-imminent=1`: Eero **forcefully disconnects** the ESP32 after its countdown
3. ESP32 sees an unexpected disconnect (reason 8 `ASSOC_LEAVE` or similar)
4. ESP32 reconnects — likely to the same AP node (no 802.11k neighbor data)
5. Eero may immediately send another BTM request to steer it away again → cycle repeats

This is a plausible cause of **intermittent disconnections that don't follow the auth-failure pattern** — random-looking drops during active use when RF conditions are otherwise fine.

#### Mitigation options

| Option | Effect | Trade-offs |
|---|---|---|
| Eero **Compatibility Mode** (Settings → Advanced) | Disables client steering entirely | Also disables WPA3 and band steering |
| Enable `btm_enabled=1` in ESP-IDF config | ESP32 receives and can respond to BTM requests | Requires ESP-IDF direct API, unreliable per #11936 |
| Pin the device to a specific AP BSSID | Prevents Eero from steering (device ignores other BSSIDs) | Loses mesh flexibility; breaks if that node goes offline |
| Accept the behaviour | Most BTM disconnects are brief; reconnect is fast on WPA2 | Occasional UI disruption during DCS sessions |

**Eero Compatibility Mode** is the lowest-effort test: if random disconnections stop after enabling it, BTM steering was the cause.

---

### AUTH_EXPIRE (Reason 2) — The Root Cause of Our Connection Issues

`WIFI_REASON_AUTH_EXPIRE = 2` is the AP telling the client: *"You have an existing active session, and I'm not accepting a new auth request until it's cleaned up."*

**Mechanism in WPA3 transition mode:**

1. ESP32 connects via WPA3 SAE → Eero creates an SA (Security Association) for the client's MAC
2. ESP32 restarts (power cut, OTA, menu reboot) **without properly terminating the SA** (no deauth sent, or deauth wasn't received)
3. Eero still holds the WPA3 SAE state for that MAC
4. ESP32 tries to reconnect. Eero sees: "I have an active WPA3 SA for this MAC, this new auth request is suspicious." → sends AUTH_EXPIRE
5. WPA3 SAE teardown is slow (seconds to minutes) due to cryptographic SA state on the AP
6. Every failed retry within the cleanup window resets Eero's cleanup timer → perpetual AUTH_EXPIRE loop

**Our fix**: `esp_wifi_disable_pmf_config(WIFI_IF_STA)` after every `WiFi.begin()` disables PMF on the client side. Without PMF, Eero cannot negotiate WPA3 SAE — it falls back to WPA2. WPA2 session cleanup is simpler and faster.

---

### arduino-esp32 WPA3 Bugs (Confirmed, External Sources)

These are real, documented bugs — not Eero-specific behavior:

| Issue | Description | Status |
|---|---|---|
| [#6767](https://github.com/espressif/arduino-esp32/issues/6767) | ESP32-C3: continuous AUTH_EXPIRE, cannot connect | Resolved |
| [#6843](https://github.com/espressif/arduino-esp32/issues/6843) | AUTH_EXPIRE intermittently (~40% failure rate) with v2.0.3 | Open (ongoing) |
| [#7605](https://github.com/espressif/arduino-esp32/issues/7605) | WiFi.begin() AUTH_EXPIRE on specific boards | Open |
| [esp-idf #8192](https://github.com/espressif/esp-idf/issues/8192) | STA deauth frame causing reconnect issues | Open |

**Key finding**: These bugs affect multiple ESP32 variants (C3, S3, original). The root cause in most cases is PMF negotiation interacting badly with the WPA3 SAE handshake in arduino-esp32's `WiFiSTA.cpp`, which hardcodes `pmf_cfg.capable = true` in every `WiFi.begin()` call.

---

## What We Don't Know (Unconfirmed)

### Eero-Specific Internals Not Publicly Documented

| Question | Status |
|---|---|
| Eero's WPA3 SAE session cleanup timeout (exact seconds) | **Not documented by Eero** |
| Eero's WPA2 session inactivity timeout (exact seconds) | **Not documented by Eero** |
| Eero's Association Comeback Time configuration | **Not documented** (industry default is 1s, Eero may differ) |
| Whether Eero's session persistence differs from standard 802.11 behavior | **No external confirmation** |

The 60+ second session persistence we observed during debugging may be explained by:
1. WPA3 SAE cryptographic teardown (protocol-mandated) — **confirmed protocol behavior**
2. Our rapid-retry bombardment extending Eero's cleanup timer — **plausible, consistent with observed behavior**
3. Eero-specific "non-standard" behavior — **not externally confirmed**

---

## Practical Implications for Brew370

### Why our fix works

`esp_wifi_disable_pmf_config(WIFI_IF_STA)` after `WiFi.begin()` → PMF disabled on client → Eero cannot negotiate WPA3 → WPA2 fallback. Eero confirms "Office 2.4 GHz WPA2" in its app. WPA2 without PMF means:
- Deauth frames are unprotected (security trade-off)
- Session cleanup is simpler — AP clears state faster on deauth receipt
- No SA Query / Association Comeback Time mechanism active

This is the right trade-off for an embedded device on a private home network.

### Deauth before restart — reassessed

Standard community practice (`ArduinoOTA`, `HTTPUpdate`, `CockpitOS`) is `ESP.restart()` with no prior WiFi management. We added `WiFi.disconnect()` before restart to speed up Eero's session cleanup. After switching to `ESP.restart()` only (v0.83+), OTA restart reconnect improved — consistent with the hypothesis that our custom deauth sequence was causing problems rather than solving them.

**Current state (v0.83+)**: `ESP.restart()` only. No explicit deauth. No `WiFi.mode(WIFI_OFF)`. Result: better reconnect reliability.

### Error codes and their meaning in our context

| Code | Name | Likely cause on Eero | Deauth needed before retry? |
|---|---|---|---|
| 2 | AUTH_EXPIRE | Stale WPA3 SAE session | No — wait 20–30s quietly |
| 15 | 4WAY_HANDSHAKE_TIMEOUT | WPA2 key exchange failed | No — retry after 10s |
| 39 | TIMEOUT | Client-side timeout, AP didn't respond | No — retry after 5–10s |
| 201 | NO_AP_FOUND | AP out of range or SSID wrong | N/A |
| 202 | AUTH_FAIL | Wrong password | N/A |

### Boot retry timing rationale

Our 5-second boot retry (v0.85+) is based on error 39 being a **client-side timeout** with no known Eero SA cleanup requirement. The AP likely has no session state to clear. This is distinct from error 2 (AUTH_EXPIRE) which historically required 20–30s of quiet time.

**No external data** confirms 5 seconds is optimal for Eero + error 39. Should be validated by observing whether error 39 clears on the first retry.

---

## Sources

- [Eero — What is WPA3?](https://support.eero.com/hc/en-us/articles/360042523671-What-is-WPA3)
- [Eero Community — WPA3 blocks incompatible devices](https://community.eero.com/t/y4hm5ck/enable-wpa3-for-compatible-devices-blocks-incompatible-devices)
- [Eero Community — WPA3 issues](https://community.eero.com/t/h7hlf8v/wpa3-issues)
- [Eero Community — WPA3 compatibility problem](https://community.eero.com/t/m1yys25/compatibility-problem-with-wpa3-beta)
- [802.11w PMF Overview — praneethwifi.in](https://praneethwifi.in/2020/03/07/protected-management-frames-in-wpa2-802-11w-wpa3-owe/)
- [802.11w Lab Tests — dot11.exposed](https://dot11.exposed/2018/10/13/802-11w-2009-protected-management-frames-pmf-overview-and-lab-tests/)
- [802.11w Does It Stop Deauth Attacks? — InfiShark](https://infishark.com/blogs/learn/802-11w-protected-management-frames-does-it-stop-deauth-attacks)
- [Deauth Attack Mitigation — InfiShark](https://infishark.com/blogs/learn/deauth-attack-mitigation-802-11w-and-beyond)
- [Cisco 802.11w Configuration Guide](https://www.cisco.com/c/en/us/support/docs/wireless-mobility/wireless-lan-wlan/212576-configure-802-11w-management-frame-prote.html)
- [ESP-IDF WiFi Security — ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi-security.html)
- [arduino-esp32 Issue #6767 — AUTH_EXPIRE](https://github.com/espressif/arduino-esp32/issues/6767)
- [arduino-esp32 Issue #6843 — AUTH_EXPIRE intermittent](https://github.com/espressif/arduino-esp32/issues/6843)
- [arduino-esp32 Issue #7605 — AUTH_EXPIRE WiFi.begin()](https://github.com/espressif/arduino-esp32/issues/7605)
- [esp-idf Issue #8192 — Deauth reconnect](https://github.com/espressif/esp-idf/issues/8192)
- [Cut It: Deauth Attacks on PMF in WPA2/WPA3 — Springer](https://link.springer.com/chapter/10.1007/978-3-031-08147-7_16)
- [On the Robustness of Wi-Fi Deauth Countermeasures — wisec2022.pdf](https://papers.mathyvanhoef.com/wisec2022.pdf)
- [Eero Help — Why doesn't my device connect to the closest eero?](https://support.eero.com/hc/en-us/articles/360039477051-Why-doesn-t-my-device-connect-to-the-closest-eero)
- [Eero Help — Compatibility Mode](https://support.eero.com/hc/en-us/articles/27887486808603-Compatibility-Mode)
- [ESP-IDF Issue #3671 — Does ESP32 support seamless roaming?](https://github.com/espressif/esp-idf/issues/3671)
- [ESP-IDF Issue #11936 — WiFi roaming does not work (802.11k/v/r) in STA mode](https://github.com/espressif/esp-idf/issues/11936)
- [ESP-IDF Security and Roaming — ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi-driver/security-and-roaming.html)
- [802.11v BSS Transition Management — wifiwiki.wordpress.com](https://wifiwiki.wordpress.com/2020/04/27/802-11v-wireless-network-management/)
- [802.11k, 802.11r, 802.11v Overview — Mist/Juniper](https://www.mist.com/documentation/802-11k-802-11r-802-11v/)
