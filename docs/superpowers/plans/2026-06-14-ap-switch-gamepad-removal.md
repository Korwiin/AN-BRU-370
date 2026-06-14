# AP Switch + Gamepad Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the AP PITCH/ROLL switch features and HID Gamepad interface entirely, freeing 4 GPIO pins and simplifying the USB composite device to CDC + Digitizer only.

**Architecture:** Removal proceeds layer-by-layer so each task produces a clean build. `hardware.cpp`/`hardware.h` are deleted entirely (they become empty after switch removal). The `0x4400` DCS-BIOS address decode is kept for `STORES_CONFIG_SW` — only the AP pitch/roll extraction is removed. `DCSBIOS_ADDR_AP_SWITCHES` is renamed `DCSBIOS_ADDR_STORES_CONFIG_SW` since AP switches are gone.

**Tech Stack:** ESP32-S3, PlatformIO/Arduino, C++. Build command: `pio run -e esp32s3_supermini`.

---

## File Map

| File | Action | Change |
|---|---|---|
| `src/main.cpp` | Modify | Remove `#include "hardware.h"`, 3 `Hardware::` calls |
| `src/hardware.cpp` | Delete | Entire file |
| `include/hardware.h` | Delete | Entire file |
| `src/hid.cpp` | Modify | Remove `USBHIDGamepad` instance and `Gamepad.begin()` |
| `include/hid.h` | Modify | Remove `#include <USBHIDGamepad.h>` and `extern Gamepad` |
| `src/dcs_bios.cpp` | Modify | Remove `s_apPitch`/`s_apRoll` state, pitch/roll decode, two getters |
| `include/dcs_bios.h` | Modify | Remove AP constants/declarations; rename addr constant; clean comment |
| `include/pins.h` | Modify | Remove 4 switch pin constants |
| `include/config.h` | Modify | Version bump `0.13` → `0.14` |
| `docs/spec.md` | Modify | Remove Gamepad Mapping section; trim DCS-BIOS tables |
| `docs/tech-reference.md` | Modify | Remove HID Gamepad Mapping section; trim DCS-BIOS table |
| `docs/decision-log.md` | Modify | Add removal entry |

---

## Task 0: Create archive branch

- [ ] **Step 0.1 — Tag current state as archive**

```bash
git checkout -b archive/ap-switches-gamepad
git push -u origin archive/ap-switches-gamepad   # skip if no remote
git checkout main
```

Expected: branch `archive/ap-switches-gamepad` created at `255aa1b`. `main` is still the active branch.

---

## Task 1: Remove Hardware module from `main.cpp`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1.1 — Remove `#include "hardware.h"`**

In `src/main.cpp` line 8, delete:
```cpp
#include "hardware.h"
```

- [ ] **Step 1.2 — Remove `Hardware::begin()` from `setup()`**

In `src/main.cpp` line 237, delete:
```cpp
  Hardware::begin();
```

- [ ] **Step 1.3 — Remove `Hardware::update()` and `Hardware::forceSync()` from `loop()`**

In `src/main.cpp` around lines 368–372, replace:
```cpp
  // Normal operation
  Hardware::update();

  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !s_wasDcsConnected) {
    Hardware::forceSync();
    UI::showSyncing(); delay(800); UI::showSynced();
```
with:
```cpp
  // Normal operation
  bool nowConnected = DcsBios::isConnected();
  if (nowConnected && !s_wasDcsConnected) {
    UI::showSyncing(); delay(800); UI::showSynced();
```

- [ ] **Step 1.4 — Build to verify**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`, zero errors, zero warnings. (`hardware.cpp` still exists as a separate compile unit and builds independently at this stage — that is fine.)

- [ ] **Step 1.5 — Commit**

```bash
git add src/main.cpp
git commit -m "refactor: remove Hardware module calls from main.cpp"
```

---

## Task 2: Delete `hardware.cpp` and `hardware.h`

**Files:**
- Delete: `src/hardware.cpp`
- Delete: `include/hardware.h`

- [ ] **Step 2.1 — Delete both files**

```bash
git rm src/hardware.cpp include/hardware.h
```

- [ ] **Step 2.2 — Build to verify**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`. `hid.h` still includes `USBHIDGamepad.h` and the class is still defined — no errors yet.

- [ ] **Step 2.3 — Commit**

```bash
git commit -m "refactor: delete hardware module (AP switches + gamepad logic)"
```

---

## Task 3: Remove Gamepad from `hid.h` and `hid.cpp`

**Files:**
- Modify: `include/hid.h`
- Modify: `src/hid.cpp`

- [ ] **Step 3.1 — Update `include/hid.h`**

Replace the entire file with:
```cpp
#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#ifndef MOUSE_LEFT
#define MOUSE_LEFT 0x01
#endif

namespace HID {
  extern USBHIDKeyboard Keyboard;

  void begin(uint16_t w, uint16_t h);
  bool isReady();

  void moveAbs(uint16_t x, uint16_t y);
  void moveRel(int16_t dx, int16_t dy);

  void pressKey(uint8_t key);
  void typeText(const char* text);
  void mouseClick(uint8_t button = MOUSE_LEFT);
}
```

- [ ] **Step 3.2 — Update `src/hid.cpp`**

Replace the top of the file (lines 1–6, before the descriptor comment) with:
```cpp
#include "hid.h"
#include "config.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;
```

Remove `Gamepad.begin();` from `HID::begin()`. The function currently ends:
```cpp
  Keyboard.begin();
  s_absMouse.begin();
  Gamepad.begin();
  USB.begin();
```
Change to:
```cpp
  Keyboard.begin();
  s_absMouse.begin();
  USB.begin();
```

- [ ] **Step 3.3 — Build to verify**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`, zero errors, zero warnings.

- [ ] **Step 3.4 — Commit**

```bash
git add include/hid.h src/hid.cpp
git commit -m "refactor: remove HID Gamepad interface from USB composite"
```

---

## Task 4: Remove AP switch decode from `dcs_bios.h` and `dcs_bios.cpp`

**Files:**
- Modify: `include/dcs_bios.h`
- Modify: `src/dcs_bios.cpp`

- [ ] **Step 4.1 — Rewrite `include/dcs_bios.h`**

Replace the entire file with:
```cpp
#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

// DCS-BIOS F-16C verified identifiers (F-16C_50.json v0.11.4)
// RWR MSL LAUNCH at 0x4480 (F_16C_50_LIGHT_RWR_MSL_LAUNCH, Addresses.h line 15956).

constexpr uint16_t DCSBIOS_ADDR_MC_LIGHT    = 0x447A;
constexpr uint16_t DCSBIOS_MASK_MC_LIGHT    = 0x0001;

constexpr uint16_t DCSBIOS_ADDR_RWR_MSL_LAUNCH  = 0x4480;
constexpr uint16_t DCSBIOS_MASK_RWR_MSL_LAUNCH  = 0x0004;

constexpr uint16_t DCSBIOS_ADDR_STORES_CONFIG_LIGHT = 0x4478;
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_LIGHT = 0x0001;

// STORES_CONFIG_SW at 0x4400 (previously shared with AP switches)
constexpr uint16_t DCSBIOS_ADDR_STORES_CONFIG_SW = 0x4400;
constexpr uint16_t DCSBIOS_MASK_STORES_CONFIG_SW = 0x0080;
constexpr uint8_t  DCSBIOS_SHFT_STORES_CONFIG_SW = 7;

#define DCSBIOS_CMD_MC_RESET           "MASTER_CAUTION"
#define DCSBIOS_CMD_CMDS_DISPENSE      "CMDS_DISPENSE_BTN"
#define DCSBIOS_CMD_STORES_CONFIG_SW   "STORES_CONFIG_SW"

constexpr uint32_t SC_RETRY_MS         = 500;
constexpr uint32_t SC_LIGHT_TIMEOUT_MS = 3000;

namespace DcsBios {
  void begin(const char* mcastAddr, uint16_t listenPort,
             const char* cmdHost,   uint16_t cmdPort);
  bool update();
  bool isConnected();
  void sendCommand(const char* identifier, uint16_t value);

  bool    masterCaution();
  bool    rwrMslLaunch();
  bool    storesConfigLight();
  uint8_t storesConfigSw();
}
```

- [ ] **Step 4.2 — Update `src/dcs_bios.cpp`**

Remove the two AP state variables (lines 10–11):
```cpp
static uint8_t s_apPitch = 0xFF;
static uint8_t s_apRoll  = 0xFF;
```

Replace the `processWord()` function body. Find:
```cpp
static void processWord(uint16_t addr, uint16_t word) {
  if (addr == DCSBIOS_ADDR_AP_SWITCHES) {
    uint8_t p = (word & DCSBIOS_MASK_AP_PITCH) >> DCSBIOS_SHFT_AP_PITCH;
    uint8_t r = (word & DCSBIOS_MASK_AP_ROLL)  >> DCSBIOS_SHFT_AP_ROLL;
    if (p <= 2) s_apPitch = p;
    if (r <= 2) s_apRoll  = r;
    s_storesConfigSw = (word & DCSBIOS_MASK_STORES_CONFIG_SW) >> DCSBIOS_SHFT_STORES_CONFIG_SW;
  }
```
Replace with:
```cpp
static void processWord(uint16_t addr, uint16_t word) {
  if (addr == DCSBIOS_ADDR_STORES_CONFIG_SW) {
    s_storesConfigSw = (word & DCSBIOS_MASK_STORES_CONFIG_SW) >> DCSBIOS_SHFT_STORES_CONFIG_SW;
  }
```

Remove the two getter implementations at the bottom of the file:
```cpp
uint8_t DcsBios::apPitchSwitch() { return s_apPitch; }
uint8_t DcsBios::apRollSwitch()  { return s_apRoll;  }
```

- [ ] **Step 4.3 — Build to verify**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`, zero errors, zero warnings.

- [ ] **Step 4.4 — Commit**

```bash
git add include/dcs_bios.h src/dcs_bios.cpp
git commit -m "refactor: remove AP switch DCS-BIOS decode; rename addr constant for STORES_CONFIG_SW"
```

---

## Task 5: Remove switch pin constants from `pins.h`

**Files:**
- Modify: `include/pins.h`

- [ ] **Step 5.1 — Remove the four switch pin blocks**

In `include/pins.h`, remove:
```cpp
// AP PITCH switch (3-pos, ATT HOLD / A/P OFF / ALT HOLD)
// COM -> GND; throws -> GPIO with INPUT_PULLUP
// {LOW,HIGH}=pos0  {HIGH,HIGH}=pos1(center/off)  {HIGH,LOW}=pos2
constexpr uint8_t PIN_SW1_A      = 1;   // INPUT_PULLUP
constexpr uint8_t PIN_SW1_B      = 2;   // INPUT_PULLUP

// AP ROLL switch (3-pos, STRG SEL / ATT HOLD / HDG SEL)
constexpr uint8_t PIN_SW2_A      = 12;  // INPUT_PULLUP
constexpr uint8_t PIN_SW2_B      = 13;  // INPUT_PULLUP
```

Also update the file header comment from:
```cpp
// All front-side pins. ADC1 only (Wi-Fi safe) reserved for future use.
```
to:
```cpp
// All front-side pins. GPIO 1, 2, 12, 13 freed (formerly AP switches).
```

- [ ] **Step 5.2 — Build to verify**

```bash
pio run -e esp32s3_supermini
```

Expected: `SUCCESS`, zero errors, zero warnings.

- [ ] **Step 5.3 — Commit**

```bash
git add include/pins.h
git commit -m "refactor: remove AP switch pin constants; note freed GPIO 1,2,12,13"
```

---

## Task 6: Version bump and flash

**Files:**
- Modify: `include/config.h`

- [ ] **Step 6.1 — Bump version in `include/config.h`**

Change:
```cpp
#define FIRMWARE_VERSION     "0.13"
#define FIRMWARE_VERSION_BCD  0x0013
```
to:
```cpp
#define FIRMWARE_VERSION     "0.14"
#define FIRMWARE_VERSION_BCD  0x0014
```

- [ ] **Step 6.2 — Build and flash**

```bash
pio run -e esp32s3_supermini --target upload
```

Expected: `SUCCESS`. Device reboots. Confirm on splash screen or Settings left panel that version shows `v0.14`.

- [ ] **Step 6.3 — Behavioral smoke test**

Verify in DCS World (or by simulating DCS-BIOS UDP traffic) that the following still work:
- Master Caution OLED alert fires and encoder press resets it
- RWR Missile Launch OLED alert fires and encoder press fires CMDS dispense
- Stores Config alert fires and encoder press sends `STORES_CONFIG_SW` command

- [ ] **Step 6.4 — Commit**

```bash
git add include/config.h
git commit -m "chore: bump firmware to v0.14 (AP switches + gamepad removed)"
```

---

## Task 7: Update documentation

**Files:**
- Modify: `docs/spec.md`
- Modify: `docs/tech-reference.md`
- Modify: `docs/decision-log.md`

- [ ] **Step 7.1 — Update `docs/spec.md`**

Delete the entire **Gamepad Mapping** section (the table of 6 buttons).

In the **DCS-BIOS Integration** section, remove the two AP rows from the decoded signals table:
```
| AP PITCH switch position | Reflected to gamepad buttons for sync comparison |
| AP ROLL switch position | Reflected to gamepad buttons for sync comparison |
```

- [ ] **Step 7.2 — Update `docs/tech-reference.md`**

Delete the entire **HID Gamepad Mapping** section (the table of 6 buttons).

In the **DCS-BIOS Decoded Signals** table, remove the AP PITCH and AP ROLL rows:
```
| AP PITCH switch | `0x4400` | `0x0300` | 8 | 0=ATT HOLD, 1=A/P OFF, 2=ALT HOLD |
| AP ROLL switch | `0x4400` | `0x0C00` | 10 | 0=STRG SEL, 1=ATT HOLD, 2=HDG SEL |
```

Update the `STORES_CONFIG_SW` row to reflect the new constant name and remove the "Shares AP address" note:
```
| STORES_CONFIG_SW | `0x4400` | `0x0080` | 7 | 0=CAT I, 1=CAT III |
```

- [ ] **Step 7.3 — Update `docs/decision-log.md`**

Add a new row to the table:
```
| 2026-06-14 | AP PITCH/ROLL switches and HID Gamepad interface removed | Target user no longer requires these controls; DCS-BIOS provides better alternatives for in-game switch and axis binding. Implementation preserved on `archive/ap-switches-gamepad` branch. | Conditional compilation (#ifdef) — adds clutter for no planned re-use timeline |
```

- [ ] **Step 7.4 — Commit**

```bash
git add docs/spec.md docs/tech-reference.md docs/decision-log.md
git commit -m "docs: update spec and tech-reference for v0.14 AP switch removal"
```

---

## Self-Review

**Spec coverage:**
- ✅ Archive branch — Task 0
- ✅ Remove HID Gamepad interface — Task 3
- ✅ Remove hardware layer — Tasks 1 + 2
- ✅ Remove AP DCS-BIOS decode, keep `0x4400` for STORES_CONFIG_SW — Task 4
- ✅ Remove switch pin constants — Task 5
- ✅ Version bump `0.13 → 0.14` — Task 6
- ✅ Update spec.md, tech-reference.md, decision-log.md — Task 7

**Placeholder scan:** No TBDs. All code blocks are complete and reference only symbols defined in the plan or confirmed to exist in the current codebase.

**Type consistency:** `DCSBIOS_ADDR_STORES_CONFIG_SW` introduced in `dcs_bios.h` (Task 4.1) and used in `processWord()` in `dcs_bios.cpp` (Task 4.2) — consistent. `HID::Gamepad` removed from `hid.h` (Task 3.1) and `hid.cpp` (Task 3.2) — no remaining references after `hardware.cpp` is deleted in Task 2.
