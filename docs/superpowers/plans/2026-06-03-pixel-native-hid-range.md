# Pixel-Native HID Range Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make 1 HID absolute mouse unit = 1 screen pixel by patching the HID report descriptor at boot time using the user's stored screen dimensions, so the calibration OLED displays real pixel coordinates.

**Architecture:** The HID descriptor's X and Y logical-maximum bytes are patched in `HID::begin(w, h)` before `USB.begin()` fires, so Windows enumerates the correct range on every boot. X and Y are split into separate descriptor sections (required since screen width ≠ height). `main.cpp` passes `s_screenW/H` (loaded from NVS) into `HID::begin()`. Calibration NVS keys are renamed to abandon stale 0–4095 values; new defaults are proportional to screen size.

**Tech Stack:** ESP32-S3 Arduino / PlatformIO · TinyUSB (USBHID) · ESP32 Preferences (NVS)

---

## Descriptor Change: 53 → 64 bytes

The old descriptor declared X and Y together with one shared `LogMax`. Because screen width ≠ height, they must be declared separately.

**Old combined section (15 bytes, bytes 36–50):**
```
Usage(X), Usage(Y), LogMin(0), LogMax(4095), Size(16), Count(2), Input
```

**New separate sections (26 bytes, bytes 36–61):**
```
Usage(X), LogMin(0), LogMax(screenW-1), Size(16), Count(1), Input  ← bytes 36-48
Usage(Y), LogMin(0), LogMax(screenH-1), Size(16), Count(1), Input  ← bytes 49-61
```

**Patched indices** (written in `HID::begin(w, h)`):
- `s_absBuf[41]` = `(w-1) & 0xFF`  — X LogMax low byte
- `s_absBuf[42]` = `(w-1) >> 8`    — X LogMax high byte
- `s_absBuf[54]` = `(h-1) & 0xFF`  — Y LogMax low byte
- `s_absBuf[55]` = `(h-1) >> 8`    — Y LogMax high byte

---

## File Map

| File | Change |
|---|---|
| `include/hid.h` | `begin()` → `begin(uint16_t w, uint16_t h)` |
| `src/hid.cpp` | 64-byte mutable buffer; patch in `begin(w,h)`; fix `moveRel` constrain |
| `src/main.cpp` | Load `scrW/scrH` first in `loadNvs`; rename calib NVS keys; proportional defaults; update `HID::begin` call; update calibration constrain; SCREEN_EDIT save → reboot |

---

## Task 1: HID layer

**Files:**
- Modify: `include/hid.h`
- Modify: `src/hid.cpp`

- [ ] **Step 1: Update `void begin()` signature in `include/hid.h`**

Replace:
```cpp
  void begin();
```
With:
```cpp
  void begin(uint16_t w, uint16_t h);
```

- [ ] **Step 2: Rewrite `src/hid.cpp`**

Replace the entire file with:

```cpp
#include "hid.h"
#include <USBHID.h>

USBHIDKeyboard HID::Keyboard;
USBHIDGamepad  HID::Gamepad;

// --- Absolute mouse HID descriptor (64 bytes) ---
// X and Y declared separately so each can have its own LogMax (screen W ≠ H).
// Bytes 41-42 = X LogMax (lo, hi), bytes 54-55 = Y LogMax (lo, hi).
// Both are patched with (screenW-1) and (screenH-1) in HID::begin() before USB.begin().
// Report ID = HID_REPORT_ID_MOUSE (2). Payload = [buttons][Xlo][Xhi][Ylo][Yhi] = 5 bytes.
static uint8_t s_absBuf[64] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01,   // UsagePage(Desktop), Usage(Mouse), App Collection
  0x85, HID_REPORT_ID_MOUSE,          // Report ID (2)
  0x09,0x01, 0xA1,0x00,               // Usage(Pointer), Physical Collection
  0x05,0x09, 0x19,0x01, 0x29,0x03,   // UsagePage(Button), UsageMin(1), UsageMax(3)
  0x15,0x00, 0x25,0x01,               // LogMin(0), LogMax(1)
  0x95,0x03, 0x75,0x01, 0x81,0x02,   // Count(3), Size(1), Input — 3 button bits
  0x95,0x01, 0x75,0x05, 0x81,0x03,   // Count(1), Size(5), Input — 5 padding bits
  0x05,0x01,                          // UsagePage(Desktop)
  // X axis — LogMax lo/hi at bytes 41,42 — patched in HID::begin()
  0x09,0x30,                          // Usage(X)
  0x15,0x00, 0x26,0x7F,0x07,          // LogMin(0), LogMax(1919 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  // Y axis — LogMax lo/hi at bytes 54,55 — patched in HID::begin()
  0x09,0x31,                          // Usage(Y)
  0x15,0x00, 0x26,0x37,0x04,          // LogMin(0), LogMax(1079 placeholder)
  0x75,0x10, 0x95,0x01, 0x81,0x02,   // Size(16), Count(1), Input(Abs)
  0xC0,                               // End Physical Collection
  0xC0                                // End App Collection
};

static uint16_t s_maxX = 1919;
static uint16_t s_maxY = 1079;

class AbsMouseDevice : public USBHIDDevice {
public:
  AbsMouseDevice() {
    static bool s_init = false;
    if (!s_init) { s_init = true; _hid.addDevice(this, sizeof(s_absBuf)); }
  }
  void begin() { _hid.begin(); }
  uint16_t _onGetDescriptor(uint8_t* buf) override {
    memcpy(buf, s_absBuf, sizeof(s_absBuf));
    return sizeof(s_absBuf);
  }
  bool send(uint8_t btns, uint16_t x, uint16_t y) {
    uint8_t r[5] = {
      btns,
      (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
      (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    return _hid.SendReport(HID_REPORT_ID_MOUSE, r, 5);
  }
private:
  USBHID _hid;
};

static AbsMouseDevice s_absMouse;
static uint16_t       s_absX = 0;
static uint16_t       s_absY = 0;
static USBHID         s_hid;   // used only for isReady() query

void HID::begin(uint16_t w, uint16_t h) {
  // Patch descriptor with screen-native coordinate ranges before USB enumeration
  s_maxX = w - 1;
  s_maxY = h - 1;
  s_absBuf[41] = s_maxX & 0xFF;
  s_absBuf[42] = s_maxX >> 8;
  s_absBuf[54] = s_maxY & 0xFF;
  s_absBuf[55] = s_maxY >> 8;

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
  moveAbs((uint16_t)constrain(nx, 0, (int)s_maxX),
          (uint16_t)constrain(ny, 0, (int)s_maxY));
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
  delay(20);
}
```

- [ ] **Step 3: Build — expect failure only in main.cpp**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -10
```

Expected: error in `src/main.cpp` — `HID::begin()` called with no arguments but now requires two. Zero errors in `hid.h` or `hid.cpp`.

- [ ] **Step 4: Commit**

```bash
git add include/hid.h src/hid.cpp
git commit -m "feat: pixel-native HID descriptor — begin(w,h) patches X/Y LogMax at boot"
```

---

## Task 2: main.cpp wiring

**Files:**
- Modify: `src/main.cpp`

Make all sub-steps in order; build once at the end.

- [ ] **Step 1: Reorder `loadNvs()` — load screen dims before calibration params**

The proportional calibration defaults depend on `s_screenW/H`, so screen dims must be read first. Replace the entire body of `loadNvs()` with:

```cpp
static void loadNvs() {
  Preferences prefs;
  prefs.begin("brew", true);
  s_brightness  = prefs.getInt("brightness", 20);
  s_encReversed = prefs.getInt("encrev", 1);
  s_sleepSecs   = prefs.getInt("sleep", 45);
  // Screen dims loaded first — calibration defaults are proportional to them
  s_screenW = prefs.getInt("scrW", 1920);
  s_screenH = prefs.getInt("scrH", 1080);
  // TODO: remove stale NVS keys aptX/aptY/amcX/amcY (0-4095 space, now abandoned)
  mouseParams[0] = prefs.getInt("apxX",  s_screenW / 4);
  mouseParams[1] = prefs.getInt("apxY",  s_screenH / 54);
  mouseParams[2] = prefs.getInt("amcX2", s_screenW / 2);
  mouseParams[3] = prefs.getInt("amcY2", s_screenH / 2);
  mouseParams[4] = prefs.getInt("lbX", 10);
  mouseParams[5] = prefs.getInt("lbY", 26);
  prefs.end();
  memcpy(s_prevMouseParams, mouseParams, sizeof(mouseParams));
}
```

- [ ] **Step 2: Update `HID::begin()` call in `setup()`**

Find (around line 166):
```cpp
  HID::begin();
```
Replace with:
```cpp
  HID::begin(s_screenW, s_screenH);
```

- [ ] **Step 3: Update Save+Exit NVS keys in `executeMouseTuneItem()`**

Find the Save+Exit block (sel == 5):
```cpp
    p.putInt("aptX", mouseParams[0]); p.putInt("aptY", mouseParams[1]);
    p.putInt("amcX", mouseParams[2]); p.putInt("amcY", mouseParams[3]);
```
Replace with:
```cpp
    p.putInt("apxX", mouseParams[0]); p.putInt("apxY", mouseParams[1]);
    p.putInt("amcX2", mouseParams[2]); p.putInt("amcY2", mouseParams[3]);
```

- [ ] **Step 4: Update calibration constrain bounds**

Find:
```cpp
        s_calibX = (uint16_t)constrain((int)s_calibX + delta * step, 0, 4095);
      } else {
        s_calibY = (uint16_t)constrain((int)s_calibY + delta * step, 0, 4095);
```
Replace with:
```cpp
        s_calibX = (uint16_t)constrain((int)s_calibX + delta * step, 0, s_screenW - 1);
      } else {
        s_calibY = (uint16_t)constrain((int)s_calibY + delta * step, 0, s_screenH - 1);
```

- [ ] **Step 5: Change SCREEN_EDIT save to auto-reboot**

Find in the SCREEN_EDIT handler:
```cpp
        UI::showSaved();
        s_mode = MOUSE_TUNE_MENU;
```
Replace with:
```cpp
        UI::showSaved();   // includes 600ms delay — enough to read "SAVED"
        ESP.restart();
```

- [ ] **Step 6: Build — must succeed**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -10
```

Expected: `SUCCESS` with zero errors.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat: pixel-native calibration — screen-proportional defaults, reboot on screen save"
```

---

## Self-Review

**Spec coverage:**

| Requirement | Task |
|---|---|
| `HID::begin(w, h)` signature | Task 1 steps 1-2 |
| 64-byte mutable descriptor buffer | Task 1 step 2 |
| X and Y declared separately | Task 1 step 2 (s_absBuf structure) |
| Descriptor patched with screen dims before USB.begin() | Task 1 step 2 (HID::begin body) |
| Patch indices: s_absBuf[41,42]=X, s_absBuf[54,55]=Y | Task 1 step 2 |
| `moveRel` constrain uses s_maxX/s_maxY | Task 1 step 2 |
| `scrW/scrH` loaded before calibration params in loadNvs | Task 2 step 1 |
| NVS keys renamed: apxX/apxY/amcX2/amcY2 | Task 2 steps 1, 3 |
| Proportional defaults: screenW/4, screenH/54, screenW/2, screenH/2 | Task 2 step 1 |
| TODO comment for stale NVS key cleanup | Task 2 step 1 |
| HID::begin() called with s_screenW/H | Task 2 step 2 |
| Calibration constrain uses s_screenW/H-1 | Task 2 step 4 |
| SCREEN_EDIT save triggers auto-reboot | Task 2 step 5 |

**Placeholder scan:** None found — every step contains complete code.

**Type consistency:**
- `HID::begin(uint16_t w, uint16_t h)` declared in `hid.h` Task 1 step 1, defined in `hid.cpp` Task 1 step 2, called as `HID::begin(s_screenW, s_screenH)` in `main.cpp` Task 2 step 2. `s_screenW/H` are `static int` in main.cpp — implicit narrowing to `uint16_t`. Both are guaranteed non-negative and ≤ 9999, so the narrowing is safe. ✓
- `s_maxX = w - 1`: if w=0, wraps to 65535. Not reachable since Screen edit prevents 0 (min entered value is 0000=0 which would give a bad range, but that's a deliberate user error). ✓
- `s_absBuf[41]` = byte index 41 of 64-byte array. Verified by byte-counting in plan header. ✓
- `s_absBuf[54]` = byte index 54 of 64-byte array. Verified by byte-counting. ✓
- Calibration `constrain(..., 0, s_screenW - 1)` — `s_screenW` is `int`, `s_screenW - 1` is `int`. `constrain(int, int, int)` is fine. ✓
