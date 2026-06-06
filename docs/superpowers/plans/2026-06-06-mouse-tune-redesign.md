# Mouse Position Tuning Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the Mouse Tune submenu: rename/reorganise to 6 items, replace digit-entry label calibration with live calibration, add Click Out POS, auto-save each target to NVS on confirm, remove dead code.

**Architecture:** All changes are confined to `main.cpp`, `macros.cpp/h`, `hid.cpp/h`, `ui.cpp/h`. `mouseParams[]` expands to 8 entries. Each calibration target (calibIdx 0–3) maps to `mouseParams[calibIdx*2]` / `[calibIdx*2+1]` and writes its own NVS keys on confirm.

**Tech Stack:** ESP32-S3 / Arduino / PlatformIO / ESP32 Preferences NVS / U8g2 OLED / USB HID

**Spec:** `docs/superpowers/specs/2026-06-06-mouse-tune-redesign.md`

---

## File Map

| File | Changes |
|---|---|
| `include/macros.h` | `mouseParams[6]` → `mouseParams[8]` |
| `src/macros.cpp` | Array size; `executeCDRP` uses `mouseParams[6/7]`; `dropPinAndLabel` uses `moveAbs` |
| `src/hid.cpp` | Remove `screenW()` / `screenH()` |
| `include/hid.h` | Remove `screenW()` / `screenH()` declarations |
| `src/main.cpp` | NVS keys; rewrite `executeMouseTuneItem`; remove dead vars/state; auto-save on confirm; instructional text arrays; modulo 6 |
| `src/ui.cpp` | Title; 6-item array; remove `showMouseTuneEdit` |
| `include/ui.h` | Remove `showMouseTuneEdit` declaration |

---

## Task 1: Extend mouseParams and update macro functions

**Files:**
- Modify: `include/macros.h`
- Modify: `src/macros.cpp`
- Modify: `src/hid.cpp`
- Modify: `include/hid.h`

- [ ] **Step 1: Extend mouseParams extern in macros.h**

In `include/macros.h`, change:
```cpp
extern int mouseParams[6];
```
to:
```cpp
extern int mouseParams[8];
```

- [ ] **Step 2: Extend array initializer in macros.cpp**

In `src/macros.cpp`, change:
```cpp
int mouseParams[6] = {875, 50, 2048, 2048, 10, 26};
```
to:
```cpp
int mouseParams[8] = {875, 50, 2048, 2048, 0, 0, 0, 0};
```
Indices [4]–[7] are overwritten by `loadNvs()` before use; initializer values don't matter.

- [ ] **Step 3: Update executeCDRP to use mouseParams[6/7]**

In `src/macros.cpp`, change:
```cpp
static void executeCDRP(int idx) {
  HID::typeText(macros[idx].payload);
  HID::moveAbs(HID::screenW() / 5, HID::screenH() / 2);
  HID::mouseClick();
}
```
to:
```cpp
static void executeCDRP(int idx) {
  HID::typeText(macros[idx].payload);
  HID::moveAbs((uint16_t)mouseParams[6], (uint16_t)mouseParams[7]);
  HID::mouseClick();
}
```

- [ ] **Step 4: Update dropPinAndLabel to use moveAbs for label position**

In `src/macros.cpp`, change:
```cpp
static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveRel((int16_t)mouseParams[4], (int16_t)mouseParams[5]);
  HID::mouseClick();
```
to:
```cpp
static void dropPinAndLabel(const char* label) {
  HID::mouseClick();
  delay(400);
  HID::moveAbs((uint16_t)mouseParams[4], (uint16_t)mouseParams[5]);
  HID::mouseClick();
```

- [ ] **Step 5: Remove screenW() and screenH() from hid.cpp**

In `src/hid.cpp`, remove these two lines (added in v0.07, now replaced by NVS-loaded mouseParams[6/7]):
```cpp
uint16_t HID::screenW() { return s_maxX + 1; }
uint16_t HID::screenH() { return s_maxY + 1; }
```

- [ ] **Step 6: Remove screenW() and screenH() declarations from hid.h**

In `include/hid.h`, remove:
```cpp
  uint16_t screenW();
  uint16_t screenH();
```

- [ ] **Step 7: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`. Zero warnings about undefined references.

- [ ] **Step 8: Commit**

```bash
git add include/macros.h src/macros.cpp src/hid.cpp include/hid.h
git commit -m "feat: extend mouseParams[8], wire executeCDRP/dropPinAndLabel to NVS params"
```

---

## Task 2: Update NVS loading for new entries

**Files:**
- Modify: `src/main.cpp` (function `loadNvs()`, lines ~57–75)

- [ ] **Step 1: Replace label and CDRP NVS reads in loadNvs()**

In `src/main.cpp`, inside `loadNvs()`, change:
```cpp
  mouseParams[4] = prefs.getInt("lbX", 10);
  mouseParams[5] = prefs.getInt("lbY", 26);
```
to:
```cpp
  mouseParams[4] = prefs.getInt("lbX2", s_screenW / 2);
  mouseParams[5] = prefs.getInt("lbY2", s_screenH / 2);
  mouseParams[6] = prefs.getInt("cdrpX", s_screenW / 5);
  mouseParams[7] = prefs.getInt("cdrpY", s_screenH / 2);
```
Old keys `lbX`/`lbY` are left as NVS orphans (harmless). New keys default to proportional screen positions matching the previous hardcoded values.

- [ ] **Step 2: Remove the stale memcpy of prevMouseParams after loadNvs**

Still in `loadNvs()`, remove:
```cpp
  memcpy(s_prevMouseParams, mouseParams, sizeof(mouseParams));
```
(This line will be unreachable once `s_prevMouseParams` is removed in Task 4, but removing it now keeps compilation clean.)

- [ ] **Step 3: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: NVS loads lbX2/lbY2 (absolute) and cdrpX/cdrpY with proportional defaults"
```

---

## Task 3: Rewrite executeMouseTuneItem()

**Files:**
- Modify: `src/main.cpp` (function `executeMouseTuneItem()`, lines ~109–154)

This replaces all 7 old cases (Screen, CalPinTool, CalMapCtr, LabelX, LabelY, Save, Cancel) with 6 new ones. The new function eliminates all references to `s_editParamIdx`, `s_editDigits`, and `s_editDigitPos`, which makes those variables safe to remove in Task 4.

- [ ] **Step 1: Replace the entire executeMouseTuneItem function**

In `src/main.cpp`, replace the full body of `executeMouseTuneItem()`:
```cpp
static void executeMouseTuneItem() {
  if (s_mouseTuneSel == 0) {
    s_screenDigits[0] = s_screenW / 1000;
    s_screenDigits[1] = (s_screenW / 100) % 10;
    s_screenDigits[2] = (s_screenW / 10)  % 10;
    s_screenDigits[3] = s_screenW % 10;
    s_screenDigits[4] = s_screenH / 1000;
    s_screenDigits[5] = (s_screenH / 100) % 10;
    s_screenDigits[6] = (s_screenH / 10)  % 10;
    s_screenDigits[7] = s_screenH % 10;
    s_screenDigitPos  = 0;
    s_mode = SCREEN_EDIT;
    return;
  }
  if (s_mouseTuneSel >= 1 && s_mouseTuneSel <= 4) {
    int ci = s_mouseTuneSel - 1;  // calibIdx: 0=PinTool 1=MapCtr 2=PinLabel 3=ClickOut
    if (ci == 2) {
      // Pin Label POS: drop a pin to open label dialog before calibrating
      HID::Keyboard.releaseAll();
      HID::pressKey(KEY_F10);
      delay(30);
      HID::moveAbs((uint16_t)mouseParams[0], (uint16_t)mouseParams[1]);
      HID::mouseClick();
      HID::moveAbs((uint16_t)mouseParams[2], (uint16_t)mouseParams[3]);
      HID::mouseClick();
      delay(400);
    }
    s_calibIdx      = ci;
    s_calibX        = (uint16_t)mouseParams[ci * 2];
    s_calibY        = (uint16_t)mouseParams[ci * 2 + 1];
    s_lastCalibTick = millis();
    HID::moveAbs(s_calibX, s_calibY);
    s_mode = MOUSE_CALIBRATE_X;
    return;
  }
  // sel == 5: Back
  s_mode = SETTINGS;
}
```

- [ ] **Step 2: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`. (Dead vars still declared — removed in Task 4.)

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: rewrite executeMouseTuneItem — 6 items, live calibration for all targets"
```

---

## Task 4: Remove dead code

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/ui.cpp`
- Modify: `include/ui.h`

- [ ] **Step 1: Remove dead state variables from main.cpp**

In `src/main.cpp`, remove these four declarations (around lines 29–35):
```cpp
static int  s_editParamIdx        = 0;
static int  s_editDigits[4]       = {0,0,0,0};
static int  s_editDigitPos        = 0;
static int  s_prevMouseParams[6];
```

- [ ] **Step 2: Remove MOUSE_TUNE_EDIT from MenuState enum**

In `src/main.cpp`, change:
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, MOUSE_TUNE_EDIT, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT
};
```
to:
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT
};
```

- [ ] **Step 3: Remove memcpy from executeMenuItem() case 4**

In `src/main.cpp` inside `executeMenuItem()`, change:
```cpp
    case 4:  // Mouse Tune
      memcpy(s_prevMouseParams, mouseParams, sizeof(mouseParams));
      s_mouseTuneSel = 0; s_mouseTuneOffset = 0;
      s_mode = MOUSE_TUNE_MENU;
      return;
```
to:
```cpp
    case 4:  // Mouse Tune
      s_mouseTuneSel = 0; s_mouseTuneOffset = 0;
      s_mode = MOUSE_TUNE_MENU;
      return;
```

- [ ] **Step 4: Remove memcpy from MOUSE_TUNE_MENU long-press handler**

In `src/main.cpp` inside the `MOUSE_TUNE_MENU` branch of `loop()`, change:
```cpp
    if (Encoder::longPressed()) {
      memcpy(mouseParams, s_prevMouseParams, sizeof(mouseParams));
      s_mode = SETTINGS; UI::flashScreen();
    }
```
to:
```cpp
    if (Encoder::longPressed()) {
      s_mode = SETTINGS; UI::flashScreen();
    }
```

- [ ] **Step 5: Remove MOUSE_TUNE_EDIT branch from loop()**

In `src/main.cpp`, remove the entire `else if (s_mode == MOUSE_TUNE_EDIT)` block (~lines 344–357):
```cpp
  } else if (s_mode == MOUSE_TUNE_EDIT) {
    s_editDigits[s_editDigitPos] = (s_editDigits[s_editDigitPos] + delta + 10) % 10;
    if (Encoder::shortPressed()) {
      if (s_editDigitPos < 3) {
        s_editDigitPos++;
      } else {
        mouseParams[s_editParamIdx] =
          s_editDigits[0]*1000 + s_editDigits[1]*100 +
          s_editDigits[2]*10  + s_editDigits[3];
        s_mode = MOUSE_TUNE_MENU;
        UI::flashScreen();
      }
    }
    if (Encoder::longPressed()) { s_mode = MOUSE_TUNE_MENU; UI::flashScreen(); }
```

- [ ] **Step 6: Remove showMouseTuneEdit from ui.cpp**

In `src/ui.cpp`, remove the entire `UI::showMouseTuneEdit()` function (~lines 257–279):
```cpp
void UI::showMouseTuneEdit(int paramIdx, int digits[4], int digitPos) {
  ...
}
```

- [ ] **Step 7: Remove showMouseTuneEdit declaration from ui.h**

In `include/ui.h`, remove:
```cpp
  static void showMouseTuneEdit(int paramIdx, int digits[4], int digitPos);
```

- [ ] **Step 8: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`. No references to removed symbols.

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp src/ui.cpp include/ui.h
git commit -m "refactor: remove MOUSE_TUNE_EDIT state, dead vars, showMouseTuneEdit"
```

---

## Task 5: Update UI menu (title, items, modulo)

**Files:**
- Modify: `src/ui.cpp` (function `showMouseTuneMenu()`)
- Modify: `src/main.cpp` (MOUSE_TUNE_MENU scroll logic in `loop()`)

- [ ] **Step 1: Update showMouseTuneMenu in ui.cpp**

In `src/ui.cpp`, replace the full `showMouseTuneMenu()` function:
```cpp
void UI::showMouseTuneMenu(int sel, int offset) {
  static const char* items[] = {
    "Screen Size",
    "Map Pin Tool POS", "Map Center POS",
    "Pin Label POS", "Click Out POS",
    "Back"
  };
  static const int kItems = 6;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "MOUSE POSITION TUNING");
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

- [ ] **Step 2: Update menu modulo in loop()**

In `src/main.cpp`, inside the `MOUSE_TUNE_MENU` branch, change:
```cpp
    s_mouseTuneSel = (s_mouseTuneSel + delta + 7) % 7;
```
to:
```cpp
    s_mouseTuneSel = (s_mouseTuneSel + delta + 6) % 6;
```

- [ ] **Step 3: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add src/ui.cpp src/main.cpp
git commit -m "feat: Mouse Tune UI — title MOUSE POSITION TUNING, 6-item menu"
```

---

## Task 6: Auto-save calibration to NVS on confirm

**Files:**
- Modify: `src/main.cpp` (MOUSE_CALIBRATE_Y short-press confirm block, ~lines 400–409)

- [ ] **Step 1: Add NVS write to calibration Y-confirm**

In `src/main.cpp`, inside the `MOUSE_CALIBRATE_X || MOUSE_CALIBRATE_Y` branch, change the short-press Y-confirm path from:
```cpp
      } else {
        mouseParams[s_calibIdx * 2]     = (int)s_calibX;
        mouseParams[s_calibIdx * 2 + 1] = (int)s_calibY;
        s_mode = MOUSE_TUNE_MENU;
      }
```
to:
```cpp
      } else {
        mouseParams[s_calibIdx * 2]     = (int)s_calibX;
        mouseParams[s_calibIdx * 2 + 1] = (int)s_calibY;
        {
          Preferences p; p.begin("brew", false);
          switch (s_calibIdx) {
            case 0: p.putInt("apxX",  mouseParams[0]); p.putInt("apxY",  mouseParams[1]); break;
            case 1: p.putInt("amcX2", mouseParams[2]); p.putInt("amcY2", mouseParams[3]); break;
            case 2: p.putInt("lbX2",  mouseParams[4]); p.putInt("lbY2",  mouseParams[5]); break;
            case 3: p.putInt("cdrpX", mouseParams[6]); p.putInt("cdrpY", mouseParams[7]); break;
          }
          p.end();
        }
        s_mode = MOUSE_TUNE_MENU;
      }
```

- [ ] **Step 2: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: auto-save calibration to NVS on Y-confirm, keyed by calibIdx"
```

---

## Task 7: Calibration instructional text

**Files:**
- Modify: `src/main.cpp` (three `showMouseCalibrate` call sites)

- [ ] **Step 1: Add instructional text arrays to main.cpp**

In `src/main.cpp`, before `setup()`, add:
```cpp
static const char* kCalibLabelX[] = {
  "Mv L/R to Map Label",
  "Mv L/R to Map Center",
  "Mv L/R to Text Field",
  "Mv L/R to Click Out"
};
static const char* kCalibLabelY[] = {
  "Mv U/D to Map Label",
  "Mv U/D to Map Center",
  "Mv U/D to Text Field",
  "Mv U/D to Click Out"
};
```

- [ ] **Step 2: Update showMouseCalibrate call inside the calibration update block (delta != 0)**

In `src/main.cpp`, inside the `MOUSE_CALIBRATE_X || MOUSE_CALIBRATE_Y` delta block, change:
```cpp
      UI::showMouseCalibrate(
        s_mode == MOUSE_CALIBRATE_X ? 0 : 1,
        s_mode == MOUSE_CALIBRATE_X ? s_calibX : s_calibY,
        s_calibIdx == 0 ? "Pin Tool" : "Map Ctr"
      );
```
to:
```cpp
      UI::showMouseCalibrate(
        s_mode == MOUSE_CALIBRATE_X ? 0 : 1,
        s_mode == MOUSE_CALIBRATE_X ? s_calibX : s_calibY,
        s_mode == MOUSE_CALIBRATE_X ? kCalibLabelX[s_calibIdx] : kCalibLabelY[s_calibIdx]
      );
```

- [ ] **Step 3: Update showMouseCalibrate call at X→Y transition (short press on X)**

In `src/main.cpp`, inside the short-press X-confirm path, change:
```cpp
        UI::showMouseCalibrate(1, s_calibY, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
```
to:
```cpp
        UI::showMouseCalibrate(1, s_calibY, kCalibLabelY[s_calibIdx]);
```

- [ ] **Step 4: Update showMouseCalibrate calls in the OLED render switch**

In `src/main.cpp`, inside the OLED render switch at the bottom of `loop()`, change:
```cpp
      case MOUSE_CALIBRATE_X:
        UI::showMouseCalibrate(0, s_calibX, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
        break;
      case MOUSE_CALIBRATE_Y:
        UI::showMouseCalibrate(1, s_calibY, s_calibIdx == 0 ? "Pin Tool" : "Map Ctr");
        break;
```
to:
```cpp
      case MOUSE_CALIBRATE_X:
        UI::showMouseCalibrate(0, s_calibX, kCalibLabelX[s_calibIdx]);
        break;
      case MOUSE_CALIBRATE_Y:
        UI::showMouseCalibrate(1, s_calibY, kCalibLabelY[s_calibIdx]);
        break;
```

- [ ] **Step 5: Build and verify clean compile**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`.

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp
git commit -m "feat: instructional text per calibration target and axis"
```

---

## Task 8: Version bump and final build

**Files:**
- Modify: `include/config.h`
- Modify: `include/config.h.example`

- [ ] **Step 1: Bump firmware version**

In both `include/config.h` and `include/config.h.example`, change:
```cpp
#define FIRMWARE_VERSION     "0.07"
#define FIRMWARE_VERSION_BCD  0x0007  // BCD: 0xMMmm — keep in sync with FIRMWARE_VERSION
```
to:
```cpp
#define FIRMWARE_VERSION     "0.08"
#define FIRMWARE_VERSION_BCD  0x0008  // BCD: 0xMMmm — keep in sync with FIRMWARE_VERSION
```

- [ ] **Step 2: Final build**

```bash
pio run 2>&1 | tail -8
```
Expected: `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add include/config.h include/config.h.example
git commit -m "feat: mouse position tuning redesign complete — v0.08"
```

---

## Manual Verification Checklist

After flashing, verify in order:

1. **Menu navigation** — long press from macro menu → Settings → Mouse Tune → OLED shows "MOUSE POSITION TUNING" with 6 items (Screen Size, Map Pin Tool POS, Map Center POS, Pin Label POS, Click Out POS, Back). Scrolling wraps at 6.
2. **Back** — select Back → returns to Settings menu.
3. **Screen Size** — enters 8-digit editor, confirm reboots with new resolution.
4. **Map Pin Tool POS** — enters live calibration, OLED shows "Mv L/R to Map Label" / "Mv U/D to Map Label". Confirm saves, device persists across reboot.
5. **Map Center POS** — same flow, "Mv L/R to Map Center" / "Mv U/D to Map Center". Confirm saves.
6. **Pin Label POS** — DCS must be open in map mode. Selecting this drops a pin at map center (cursor moves, pin placed, label dialog appears). OLED shows "Mv L/R to Text Field" / "Mv U/D to Text Field". Nudge cursor to text field, confirm. Verify AWACS macro now clicks the text field correctly.
7. **Click Out POS** — enters live calibration, "Mv L/R to Click Out" / "Mv U/D to Click Out". Confirm saves. Run a CDRP macro and verify confirm click lands at calibrated position.
8. **Long press to cancel** — enter any calibration, move cursor, long press → cursor stays moved but NVS not written (old value persists after reboot).
9. **Windows Device Manager** — firmware version shows 0.08.
