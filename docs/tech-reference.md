# AN/BRU-370 Technical Reference

Constants, keys, and identifiers needed when making firmware changes. For behavioral descriptions see [spec.md](spec.md).

---

## USB Identity

| Field | Value |
|---|---|
| Manufacturer | `E4 Mafia` |
| Product | `AN/BRU-370` |
| PID | `0x370A` |
| VID | ESP32 default (Espressif) |
| Wi-Fi hostname | `ANBRU-370` |
| BLE advertised name | `AN/BRU-370` |

**Do not change the PID.** DCS and Windows HID drivers cache device identity by PID; a change breaks all existing bindings. See [decision-log.md](decision-log.md).

---

## NVS Keys

All keys are integers unless noted. Two namespaces are used.

### Namespace `brew` (device config)

| Key | Type | Default | Description |
|---|---|---|---|
| `brightness` | int | 20 | OLED brightness (0–255) |
| `encrev` | int | 1 | Encoder direction reversed (0=normal, 1=reversed) |
| `sleep` | int | 45 | OLED sleep timeout in seconds |
| `scrW` | int | 1920 | Target screen width in pixels |
| `scrH` | int | 1080 | Target screen height in pixels |
| `apxX` | int | scrW / 4 | Map Pin Tool X (absolute screen pixels) |
| `apxY` | int | scrH / 54 | Map Pin Tool Y (absolute screen pixels) |
| `amcX2` | int | scrW / 2 | Map Center X (absolute screen pixels) |
| `amcY2` | int | scrH / 2 | Map Center Y (absolute screen pixels) |
| `lbX2` | int | scrW / 2 | Pin Label X (absolute screen pixels) |
| `lbY2` | int | scrH / 2 | Pin Label Y (absolute screen pixels) |
| `cdrpX` | int | scrW / 5 | Click Out X (absolute screen pixels) |
| `cdrpY` | int | scrH / 2 | Click Out Y (absolute screen pixels) |
| `wifi_en` | int | 1 | Wi-Fi enabled (0=disabled, 1=enabled) |

Mouse position defaults are recalculated at runtime from `scrW`/`scrH`; they are not stored unless the user calibrates them.

Keys `lbX` and `lbY` (old relative-delta label offset) may exist in NVS as harmless orphans from earlier firmware. They are never read.

### Namespace `brew_wifi` (credentials)

| Key | Type | Description |
|---|---|---|
| `ssid` | String | Wi-Fi SSID (empty = not configured) |
| `pass` | String | Wi-Fi password (empty = open network or not configured) |

No compile-time credential defaults. Credentials are NVS-only; configure at runtime via Settings → Wi-Fi → Secrets → BLE TERM.

---

## DCS-BIOS Network

| Parameter | Value | Source |
|---|---|---|
| Multicast listen address | `239.255.50.10` | `DCSBIOS_MCAST_ADDR` in `config.h` |
| Listen port | `5010` | `DCSBIOS_MCAST_PORT` |
| Command host | `255.255.255.255` (broadcast) | hardcoded in `DcsBios::begin()` call |
| Command port | `7778` | `DCSBIOS_CMD_PORT` |
| Stream timeout | 3 s | hardcoded in `DcsBios::isConnected()` |

---

## DCS-BIOS Decoded Signals

All verified against `F-16C_50.json` v0.11.4.

| Signal | Address | Mask | Shift | Notes |
|---|---|---|---|---|
| STORES_CONFIG_SW | `0x4400` | `0x0080` | 7 | 0=CAT I, 1=CAT III |
| MASTER_CAUTION light | `0x447A` | `0x0001` | — | bit test |
| STORES CONFIG light | `0x4478` | `0x0001` | — | bit test |
| RWR MSL LAUNCH light | `0x4480` | `0x0004` | — | bit test |

`LIGHT_STORES_CONFIG` (`0x4478`) and `MASTER_CAUTION` (`0x447A`) are adjacent addresses that arrive in the same export frame. Any code that depends on the Stores Config light clearing must be checked **before** the `mcConfirmed` gate, or it will be skipped when MC clears in the same frame.

### Commands Sent

| Constant | DCS-BIOS identifier | Usage |
|---|---|---|
| `DCSBIOS_CMD_MC_RESET` | `MASTER_CAUTION` | Reset MC button |
| `DCSBIOS_CMD_CMDS_DISPENSE` | `CMDS_DISPENSE_BTN` | Countermeasures dispense |
| `DCSBIOS_CMD_STORES_CONFIG_SW` | `STORES_CONFIG_SW` | Stores Config switch toggle |

ANT ELEV is **not** modeled in DCS-BIOS for the F-16C.

---

## Timeout and Timing Constants

| Constant | Value | Purpose |
|---|---|---|
| `kWifiConnectTimeoutMs` | 30 000 ms | Splash-screen Wi-Fi connect window |
| `SC_RETRY_MS` | 500 ms | Stores Config switch command retry interval |
| `SC_LIGHT_TIMEOUT_MS` | 3 000 ms | Stores Config light-clear timeout before SC_GAVE_UP |
| MC debounce | 200 ms | Minimum time MC must be high before alert triggers |
| RWR debounce | 200 ms | Minimum time RWR must be high before alert triggers |
| MC flash interval | 200 ms | OLED flash toggle rate for Master Caution |
| RWR flash interval | 100 ms | OLED flash toggle rate for Missile Launch |

---

## HID Digitizer (Absolute Pointer)

The device presents a `Usage(Pen)` digitizer, not `Usage(Mouse)`. This is required — `Usage(Mouse)` is intercepted by `mouhid.sys` which applies a ~16 px minimum-displacement filter to absolute reports that cannot be disabled by any Windows setting.

| Parameter | Value |
|---|---|
| Usage Page | Digitizer |
| Usage | Pen |
| X range | 0 – 32 767 |
| Y range | 0 – 32 767 |
| Button byte bit 0 | TipSwitch (left click) |
| Button byte bit 1 | BarrelSwitch (right click) |
| Button byte bit 2 | InRange (always asserted) |

---

## Firmware Versioning

`FIRMWARE_VERSION` (string) and `FIRMWARE_VERSION_BCD` (BCD integer) in `include/config.h` must be updated together on every version bump.

Example: `"0.13"` → `0x0013`

`config.h` is committed. No setup step required — it contains no credentials.
