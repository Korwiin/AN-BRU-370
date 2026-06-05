# Absolute Mouse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `USBHIDMouse` (relative) with a custom absolute-mouse HID device so macros jump directly to calibrated screen coordinates without a slow homing operation.

**Architecture:** A `AbsMouseDevice` class (defined privately in `hid.cpp`) subclasses `USBHIDDevice` and provides a 51-byte HID descriptor declaring a 0–32767 absolute pointer. `hid.cpp` tracks `s_absX/s_absY` so relative moves can be emulated. The Mouse Tune menu replaces 6 digit-editor params with 2 live-calibration sessions (encoder moves cursor, short-press locks X then saves Y) plus 2 digit-editor params for the label offset deltas. NVS keys for absolute coords renamed `aptX/aptY/amcX/amcY` so old pixel-space values are ignored on first boot.

**Tech Stack:** ESP32-S3 Arduino / PlatformIO · TinyUSB (via `USBHID.h`) · U8g2 OLED

---

## File Map

| File | Change |
|---|---|
| `include/hid.h` | Remove `USBHIDMouse Mouse`, `homeMouse()`, `moveMouseTotal()`; add `moveAbs()`, `moveRel()` |
| `src/hid.cpp` | Add `AbsMouseDevice` + HID descriptor; implement `moveAbs`, `moveRel`; update `mouseClick`; remove `Mouse` |
| `src/macros.cpp` | Replace all relative-mouse calls with `moveAbs`/`moveRel`; CDRP confirm becomes `moveAbs(100,100)` + click |
| `include/ui.h` | Add `showMouseCalibrate(int axis, uint16_t val, const char* label)` declaration |
| `src/ui.cpp` | Rewrite `showMouseTuneMenu` (6 items); add `showMouseCalibrate`; keep `showMouseTuneEdit` unchanged |
| `src/main.cpp` | Rename NVS keys; add `MOUSE_CALIBRATE_X/Y` enum states + vars; rewrite `executeMouseTuneItem`; add calibration loop handlers |

---

## Task 1: AbsoluteMouse HID device

**Files:**
- Modify: `include/hid.h`
- Modify: `src/hid.cpp`

- [ ] **Step 1: Rewrite `include/hid.h`**

Replace the entire file with:

```cpp
#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDGamepad.h>

namespace HID {
  extern USBHIDKeyboard Keyboard;
  extern USBHIDGamepad  Gamepad;

  void begin();
  bool isReady();

  void moveAbs(uint16_t x, uint16_t y);
  void moveRel(int16_t dx, int16_t dy);

  void pressKey(uint8_t key);
  void typeText(const char* text);
  void mouseClick(uint8_t button = MOUSE_LEFT);
}
```

- [ ] **Step 2: Rewrite `src/hid.cpp`**

Replace the entire file with:

```cpp
#include "hid.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;
USBHIDGamepad  HID::Gamepad;

// --- Absolute mouse HID descriptor (51 bytes) ---
// Pointer device, 0-32767 logical range, 3 buttons + X/Y axes.
// Report: [buttons(1B)] [X lo][X hi] [Y lo][Y hi]  = 5 bytes total.
static const uint8_t s_absDesc[] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01,   // UsagePage(Desktop), Usage(Mouse), App Collection
    0x09,0x01, 0xA1,0x00,             // Usage(Pointer), Physical Collection
      0x05,0x09,                      // UsagePage(Button)
      0x19,0x01, 0x29,0x03,           // UsageMin(1) UsageMax(3)
      0x15,0x00, 0x25,0x01,           // LogMin(0) LogMax(1)
      0x95,0x03, 0x75,0x01,           // Count(3) Size(1)
      0x81,0x02,                      // Input(Data,Var,Abs) — 3 button bits
      0x95,0x01, 0x75,0x05,           // Count(1) Size(5)
      0x81,0x03,                      // Input(Const) — 5 padding bits
      0x05,0x01,                      // UsagePage(Desktop)
      0x09,0x30, 0x09,0x31,           // Usage(X) Usage(Y)
      0x15,0x00, 0x26,0xFF,0x7F,      // LogMin(0) LogMax(32767)
      0x75,0x10, 0x95,0x02,           // Size(16) Count(2)
      0x81,0x02,                      // Input(Data,Var,Abs) — X and Y
    0xC0,                             // End Physical Collection
  0xC0                                // End App Collection
};

class AbsMouseDevice : public USBHIDDevice {
public:
  AbsMouseDevice() {
    static bool s_init = false;
    if (!s_init) { s_init = true; _hid.addDevice(this, sizeof(s_absDesc)); }
  }
  void begin() { _hid.begin(); }
  uint16_t _onGetDescriptor(uint8_t* buf) override {
    memcpy(buf, s_absDesc, sizeof(s_absDesc));
    return sizeof(s_absDesc);
  }
  bool send(uint8_t btns, uint16_t x, uint16_t y) {
    uint8_t r[5] = {
      btns,
      (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
      (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    return _hid.SendReport(0, r, 5);
  }
private:
  USBHID _hid;
};

static AbsMouseDevice s_absMouse;
static uint16_t       s_absX = 0;
static uint16_t       s_absY = 0;
static USBHID         s_hid;   // used only for isReady() query

void HID::begin() {
  USB.manufacturerName("E4 Mafia");
  USB.productName("AN/BRU-370");
  USB.PID(0x370A);
  Keyboard.begin();
  s_absMouse.begin();
  Gamepad.begin();
  USB.begin();
}

bool HID::isReady() {
  return s_hid.ready();
}

void HID::moveAbs(uint16_t x, uint16_t y) {
  s_absX = x;
  s_absY = y;
  s_absMouse.send(0, s_absX, s_absY);
}

void HID::moveRel(int16_t dx, int16_t dy) {
  int nx = (int)s_absX + dx;
  int ny = (int)s_absY + dy;
  moveAbs((uint16_t)constrain(nx, 0, 32767),
          (uint16_t)constrain(ny, 0, 32767));
}

void HID::pressKey(uint8_t key) {
  Keyboard.press(key); delay(50); Keyboard.release(key);
}

void HID::typeText(const char* text) {
  Keyboard.print(text); delay(250);
}

void HID::mouseClick(uint8_t button) {
  s_absMouse.send(button, s_absX, s_absY);
  delay(50);
  s_absMouse.send(0, s_absX, s_absY);
  delay(250);
}
```

- [ ] **Step 3: Build to verify compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -20
```

Expected: `SUCCESS` with RAM/flash report. Failure here means a descriptor or API mismatch — do not proceed to Task 2 until this passes.

- [ ] **Step 4: Commit**

```bash
git add include/hid.h src/hid.cpp
git commit -m "feat: replace USBHIDMouse with absolute mouse HID device (0-32767 range)"
```

---

## Task 2: Update macros

**Files:**
- Modify: `src/macros.cpp`

- [ ] **Step 1: Rewrite `src/macros.cpp`**

Replace the entire file with:

```cpp
#include "macros.h"
#include "hid.h"
#include <Arduino.h>

// Mouse calibration params — loaded from NVS in setup(), defaults here.
// [0]=aptX [1]=aptY : absolute position of Pin Tool button (0-32767)
// [2]=amcX [3]=amcY : absolute position of map drop target (0-32767)
// [4]=lbX  [5]=lbY  : relative delta from pin drop to label input
int mouseParams[6] = {16384, 1000, 16384, 16384, 10, 26};

static void openMapAndSelectPin() {
  HID::Keyboard.releaseAll();
  HID::pressKey(KEY_F10);
  HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
  HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
}

static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveRel((int16_t)mouseParams[4], (int16_t)mouseParams[5]);
  HID::mouseClick();
  HID::Keyboard.releaseAll();
  HID::typeText(label);
  HID::moveRel(-60, 0);
  HID::mouseClick();
  delay(250);
  HID::pressKey(KEY_F1);
}

static void executeAWACS()       { openMapAndSelectPin(); dropPinAndLabel("magic11"); }
static void executeFCAP()        { openMapAndSelectPin(); dropPinAndLabel("fcap"); }
static void executeREAPER()      { openMapAndSelectPin(); dropPinAndLabel("1688 reaper"); }

static void executeCDRP(int idx);
static void executeCDRPAlpha()   { executeCDRP(3); }
static void executeCDRPBravo()   { executeCDRP(4); }
static void executeCDRPCharlie() { executeCDRP(5); }
static void executeCDRPDelta()   { executeCDRP(6); }
static void executeCDRPEcho()    { executeCDRP(7); }
static void executeCDRPFoxtrot() { executeCDRP(8); }
static void executeCDRPGamma()   { executeCDRP(9); }

Macro macros[] = {
  {"AWACS",        "magic11",      executeAWACS},
  {"FCAP",         "fcap",         executeFCAP},
  {"REAPER",       "1688 reaper",  executeREAPER},
  {"CDRP ALPHA",   "CDRP-ALPHA",   executeCDRPAlpha},
  {"CDRP BRAVO",   "CDRP-BETA",    executeCDRPBravo},
  {"CDRP CHARLIE", "CDRP-CHARLIE", executeCDRPCharlie},
  {"CDRP DELTA",   "CDRP-DELTA",   executeCDRPDelta},
  {"CDRP ECHO",    "CDRP-ECHO",    executeCDRPEcho},
  {"CDRP FOXTROT", "CDRP-FOXTROT", executeCDRPFoxtrot},
  {"CDRP GAMMA",   "CDRP-GAMMA",   executeCDRPGamma},
};
const int numMacros = sizeof(macros) / sizeof(macros[0]);

static void executeCDRP(int idx) {
  HID::typeText(macros[idx].payload);
  HID::moveAbs(100, 100);
  HID::mouseClick();
}

void executeMacro(int idx) {
  if (HID::isReady()) macros[idx].execute();
}
```

- [ ] **Step 2: Build to verify compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -20
```

Expected: `SUCCESS`. If you see `'Mouse' is not a member of 'HID'` the old `Mouse.release` call wasn't fully removed — check the diff.

- [ ] **Step 3: Commit**

```bash
git add src/macros.cpp
git commit -m "feat: migrate macros to absolute mouse (moveAbs/moveRel, no homing)"
```

---

## Task 3: Update UI

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add `showMouseCalibrate` declaration to `include/ui.h`**

After the `showMouseTuneEdit` line add:

```cpp
  void showMouseCalibrate(int axis, uint16_t val, const char* label);
```

The `showMouseTuneEdit` declaration stays as-is (still used for lbX/lbY digit editing).

- [ ] **Step 2: Replace `showMouseTuneMenu` body in `src/ui.cpp`**

Find and replace the entire `showMouseTuneMenu` function (lines 236–253):

```cpp
void UI::showMouseTuneMenu(int sel, int offset) {
  static const char* items[] = {
    "Cal:Pin Tool", "Cal:Map Ctr",
    "Label X", "Label Y",
    "Save+Exit", "Cancel"
  };
  static const int kItems = 6;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "MOUSE TUNE");
  for (int i = 0; i < 3; i++) {
    int idx = offset + i;
    if (idx >= kItems) break;
    int y = 18 + i * 8;
    if (idx == sel) { u8g2.drawStr(0, y, ">"); u8g2.drawStr(10, y, items[idx]); }
    else              u8g2.drawStr(10, y, items[idx]);
  }
  u8g2.sendBuffer();
}
```

- [ ] **Step 3: Add `showMouseCalibrate` to `src/ui.cpp`**

Add this function after `showMouseTuneEdit` (after line 277):

```cpp
void UI::showMouseCalibrate(int axis, uint16_t val, const char* label) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, label);
  char vbuf[14];
  snprintf(vbuf, sizeof(vbuf), "%c: %u", axis == 0 ? 'X' : 'Y', (unsigned)val);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 19, vbuf);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 31, axis == 0 ? "SP=lock  LP=cancel" : "SP=save  LP=cancel");
  u8g2.sendBuffer();
}
```

- [ ] **Step 4: Build to verify compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -20
```

Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat: add showMouseCalibrate, update Mouse Tune menu to 6 items"
```

---

## Task 4: Wire up main.cpp

**Files:**
- Modify: `src/main.cpp`

This task has the most surface area. Make each sub-step, build after step 4, then commit.

- [ ] **Step 1: Add new enum states and calibration variables**

In the `MenuState` enum (line 13), add two states at the end:

```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, MOUSE_TUNE_EDIT, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y
};
```

After the existing static variable block (after `s_dcsBiosStarted`, around line 44), add:

```cpp
static int          s_calibIdx       = 0;    // 0=PinTool 1=MapCtr
static uint16_t     s_calibX         = 0;
static uint16_t     s_calibY         = 0;
static unsigned long s_lastCalibTick = 0;
```

- [ ] **Step 2: Rename NVS keys in `loadNvs()`**

Replace the four absolute-position lines (lines 52–55):

```cpp
  mouseParams[0] = prefs.getInt("aptX", 16384);
  mouseParams[1] = prefs.getInt("aptY", 1000);
  mouseParams[2] = prefs.getInt("amcX", 16384);
  mouseParams[3] = prefs.getInt("amcY", 16384);
  mouseParams[4] = prefs.getInt("lbX",  10);
  mouseParams[5] = prefs.getInt("lbY",  26);
```

- [ ] **Step 3: Rewrite `executeMouseTuneItem()`**

Replace the entire `executeMouseTuneItem` function (lines 94–132):

```cpp
static void executeMouseTuneItem() {
  if (s_mouseTuneSel == 0 || s_mouseTuneSel == 1) {
    // Live calibration: enter X-axis phase
    s_calibIdx = s_mouseTuneSel;
    s_calibX   = (uint16_t)mouseParams[s_calibIdx * 2];
    s_calibY   = (uint16_t)mouseParams[s_calibIdx * 2 + 1];
    s_lastCalibTick = millis();
    HID::moveAbs(s_calibX, s_calibY);
    s_mode = MOUSE_CALIBRATE_X;
    return;
  }
  if (s_mouseTuneSel == 2 || s_mouseTuneSel == 3) {
    // Digit editor for lbX (sel=2 → paramIdx=4) or lbY (sel=3 → paramIdx=5)
    s_editParamIdx = s_mouseTuneSel + 2;
    int v = mouseParams[s_editParamIdx];
    s_editDigits[0] = v / 1000;
    s_editDigits[1] = (v / 100) % 10;
    s_editDigits[2] = (v / 10)  % 10;
    s_editDigits[3] = v % 10;
    s_editDigitPos  = 0;
    s_mode = MOUSE_TUNE_EDIT;
    return;
  }
  if (s_mouseTuneSel == 4) {  // Save+Exit
    Preferences p; p.begin("brew", false);
    p.putInt("aptX", mouseParams[0]); p.putInt("aptY", mouseParams[1]);
    p.putInt("amcX", mouseParams[2]); p.putInt("amcY", mouseParams[3]);
    p.putInt("lbX",  mouseParams[4]); p.putInt("lbY",  mouseParams[5]);
    p.end();
    UI::showSaved();
    s_mode = SETTINGS;
    return;
  }
  // sel=5: Cancel
  s_mode = SETTINGS;
}
```

- [ ] **Step 4: Update `MOUSE_TUNE_MENU` handler in `loop()`**

Find the `MOUSE_TUNE_MENU` block in `loop()` (around line 312):

```cpp
  } else if (s_mode == MOUSE_TUNE_MENU) {
    s_mouseTuneSel = (s_mouseTuneSel + delta + 6) % 6;
    if (s_mouseTuneSel < s_mouseTuneOffset) s_mouseTuneOffset = s_mouseTuneSel;
    if (s_mouseTuneSel >= s_mouseTuneOffset + 3) s_mouseTuneOffset = s_mouseTuneSel - 2;
    if (Encoder::shortPressed()) { UI::flashScreen(); executeMouseTuneItem(); }
    if (Encoder::longPressed()) {
      memcpy(mouseParams, s_prevMouseParams, sizeof(mouseParams));
      s_mode = SETTINGS; UI::flashScreen();
    }
```

The only change is `% 6` (was `% 10`). The rest stays the same.

- [ ] **Step 5: Add `MOUSE_CALIBRATE_X` and `MOUSE_CALIBRATE_Y` handlers in `loop()`**

Add these two blocks after the `MOUSE_TUNE_EDIT` block and before the `WIFI_MENU` block:

```cpp
  } else if (s_mode == MOUSE_CALIBRATE_X || s_mode == MOUSE_CALIBRATE_Y) {
    if (delta != 0) {
      unsigned long now = millis();
      unsigned long dt  = now - s_lastCalibTick;
      s_lastCalibTick   = now;
      int step;
      if      (dt <  60) step = 500;
      else if (dt < 100) step = 100;
      else if (dt < 200) step = 20;
      else               step = 1;
      if (s_mode == MOUSE_CALIBRATE_X) {
        s_calibX = (uint16_t)constrain((int)s_calibX + delta * step, 0, 32767);
      } else {
        s_calibY = (uint16_t)constrain((int)s_calibY + delta * step, 0, 32767);
      }
      HID::moveAbs(s_calibX, s_calibY);
      UI::showMouseCalibrate(
        s_mode == MOUSE_CALIBRATE_X ? 0 : 1,
        s_mode == MOUSE_CALIBRATE_X ? s_calibX : s_calibY,
        s_calibIdx == 0 ? "Pin Tool" : "Map Ctr"
      );
    }
    if (Encoder::shortPressed()) {
      if (s_mode == MOUSE_CALIBRATE_X) {
        s_mode = MOUSE_CALIBRATE_Y;
        UI::showMouseCalibrate(1, s_calibY, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
      } else {
        mouseParams[s_calibIdx * 2]     = (int)s_calibX;
        mouseParams[s_calibIdx * 2 + 1] = (int)s_calibY;
        s_mode = MOUSE_TUNE_MENU;
      }
    }
    if (Encoder::longPressed()) {
      s_mode = MOUSE_TUNE_MENU;  // discard — mouseParams unchanged
    }
```

- [ ] **Step 6: Add calibration states to the OLED switch**

In the OLED `switch (s_mode)` block at the bottom of `loop()`, add after `MOUSE_TUNE_EDIT`:

```cpp
      case MOUSE_CALIBRATE_X:
        UI::showMouseCalibrate(0, s_calibX, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
        break;
      case MOUSE_CALIBRATE_Y:
        UI::showMouseCalibrate(1, s_calibY, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
        break;
```

- [ ] **Step 7: Build to verify compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -20
```

Expected: `SUCCESS`. Common failures:
- `'MOUSE_CALIBRATE_X' undeclared` — enum not updated yet (Step 1)
- `'showMouseCalibrate' not declared` — Task 3 not done yet

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp
git commit -m "feat: live mouse calibration — encoder moves cursor, short-press locks X/saves Y"
```

---

## Task 5: Final clean build + version bump

- [ ] **Step 1: Full clean build**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run --target clean && pio run 2>&1 | tail -20
```

Expected: `SUCCESS`.

- [ ] **Step 2: Bump firmware version**

In [include/config.h](include/config.h), increment `FIRMWARE_VERSION` to `"0.04"`.

- [ ] **Step 3: Commit version bump**

```bash
git add include/config.h
git commit -m "chore: bump firmware version to 0.04"
```

---

## Self-Review

**Spec coverage check:**

| Requirement | Task |
|---|---|
| Replace USBHIDMouse with absolute device | Task 1 |
| 0–32767 coordinate range | Task 1 (descriptor) |
| `moveAbs(x,y)` and `moveRel(dx,dy)` API | Task 1 |
| Tracked `s_absX/s_absY` for relative emulation | Task 1 |
| Remove `homeMouse()` / `moveMouseTotal()` | Task 1 + Task 2 |
| Map-drop macros use `moveAbs` | Task 2 |
| CDRP confirm: `moveAbs(100,100)` + click | Task 2 |
| `lbX/lbY` moves use `moveRel` | Task 2 |
| NVS keys renamed `aptX/aptY/amcX/amcY` | Task 4 step 2 |
| New defaults for absolute coord range | Task 4 step 2 |
| Mouse Tune menu: 6 items | Task 3 + Task 4 |
| Live calibration: two-step X then Y | Task 4 step 5 |
| Velocity-based encoder acceleration | Task 4 step 5 |
| Calibration saves to `mouseParams` on confirm | Task 4 step 5 |
| Save+Exit persists to NVS | Task 4 step 3 |
| Long-press MOUSE_TUNE_MENU cancels to prev state | Task 4 step 4 (existing behaviour preserved) |
| `showMouseCalibrate` OLED function | Task 3 + Task 4 step 6 |

**Placeholder scan:** None found — every step contains complete code.

**Type consistency check:**
- `HID::moveAbs(uint16_t, uint16_t)` — declared in hid.h Task 1, called as `HID::moveAbs((uint16_t)mouseParams[0], ...)` in macros.cpp Task 2, called as `HID::moveAbs(s_calibX, s_calibY)` in main.cpp Task 4. ✓
- `HID::moveRel(int16_t, int16_t)` — declared Task 1, called with `(int16_t)mouseParams[4]` in macros.cpp and `(-60, 0)` in dropPinAndLabel. ✓
- `UI::showMouseCalibrate(int, uint16_t, const char*)` — declared ui.h Task 3, defined ui.cpp Task 3, called main.cpp Task 4. ✓
- `s_calibIdx * 2` indexes into `mouseParams[0/1]` (Pin Tool) and `mouseParams[2/3]` (Map Ctr). ✓
- `s_editParamIdx = s_mouseTuneSel + 2` maps sel=2 → paramIdx=4 (lbX), sel=3 → paramIdx=5 (lbY). `showMouseTuneEdit` uses labels[] at index 4 ("Label X") and 5 ("Label Y") — both already correct in existing code. ✓
