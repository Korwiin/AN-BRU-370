# Screen Resolution Entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Screen" entry as the first item in the Mouse Tune menu, letting the user enter their screen resolution (W×H, up to 9999×9999) via an 8-digit OLED editor — width on the left, height on the right, separated by "x", all on one screen. Saves immediately to NVS on last SP.

**Architecture:** Two changes: (1) UI layer adds `showScreenEdit` and updates `showMouseTuneMenu` to 7 items; (2) `main.cpp` adds `SCREEN_EDIT` enum state, two new globals (`s_screenW/H`), and wires the state machine. All existing Mouse Tune menu sel indices shift up by 1 to make room for Screen at index 0.

**Tech Stack:** ESP32-S3 Arduino / PlatformIO · U8g2 OLED (128×32 SSD1306) · ESP32 Preferences (NVS)

---

## File Map

| File | Change |
|---|---|
| `include/ui.h` | Add `showScreenEdit(int digits[8], int digitPos)` declaration |
| `src/ui.cpp` | Replace `showMouseTuneMenu` body (7 items); add `showScreenEdit` |
| `src/main.cpp` | Add `SCREEN_EDIT` enum; add 4 vars; update `loadNvs`; rewrite `executeMouseTuneItem`; update MOUSE_TUNE_MENU handler; add SCREEN_EDIT loop handler + OLED case |

---

## OLED Layout for `showScreenEdit`

128×32 display, `u8g2_font_6x10_tr` for digits (6 px wide), `u8g2_font_5x7_tr` for labels/hints.

```
y=7  : "Screen"                         ← 5x7 font
y=20 : [W₀][W₁][W₂][W₃]  x  [H₀][H₁][H₂][H₃]  ← 6x10 font digits
y=31 : "SP=nxt LP=cancel"  (or "SP=save LP=cancel" on digit 7)
```

Digit x-positions (centred in 128 px):

| Slot | char x | box x | note |
|---|---|---|---|
| W₀ | 21 | 20 | width thousands |
| W₁ | 30 | 29 | width hundreds |
| W₂ | 39 | 38 | width tens |
| W₃ | 48 | 47 | width units |
| "x" | 59 | — | literal separator |
| H₀ | 72 | 71 | height thousands |
| H₁ | 81 | 80 | height hundreds |
| H₂ | 90 | 89 | height tens |
| H₃ | 99 | 98 | height units |

Selection box: `drawBox(digit_x - 1, 9, 8, 12)` with inverted digit text.

---

## Task 1: UI layer

**Files:**
- Modify: `include/ui.h`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add `showScreenEdit` declaration to `include/ui.h`**

After the `showMouseCalibrate` line, add:

```cpp
  void showScreenEdit(int digits[8], int digitPos);
```

- [ ] **Step 2: Replace `showMouseTuneMenu` in `src/ui.cpp`**

Find the existing `UI::showMouseTuneMenu` function and replace it entirely with:

```cpp
void UI::showMouseTuneMenu(int sel, int offset) {
  static const char* items[] = {
    "Screen",
    "Cal:Pin Tool", "Cal:Map Ctr",
    "Label X", "Label Y",
    "Save+Exit", "Cancel"
  };
  static const int kItems = 7;
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

- [ ] **Step 3: Add `showScreenEdit` to `src/ui.cpp`**

Add this function after `showMouseCalibrate` (around line 291):

```cpp
void UI::showScreenEdit(int digits[8], int digitPos) {
  static const int kDx[8] = {21, 30, 39, 48, 72, 81, 90, 99};
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "Screen");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(59, 20, "x");
  for (int d = 0; d < 8; d++) {
    int x = kDx[d];
    char dc[2] = {(char)('0' + digits[d]), 0};
    if (d == digitPos) {
      u8g2.setDrawColor(1); u8g2.drawBox(x - 1, 9, 8, 12);
      u8g2.setDrawColor(0); u8g2.drawStr(x, 20, dc);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(x, 20, dc);
    }
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 31, digitPos < 7 ? "SP=nxt LP=cancel" : "SP=save LP=cancel");
  u8g2.sendBuffer();
}
```

- [ ] **Step 4: Build to verify compile**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -10
```

Expected: build fails only on `main.cpp` (undefined `SCREEN_EDIT`, undeclared `s_screenDigits` etc.) — zero errors in `ui.h` or `ui.cpp`.

- [ ] **Step 5: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat: showScreenEdit 8-digit OLED layout, Mouse Tune menu 7 items"
```

---

## Task 2: main.cpp wiring

**Files:**
- Modify: `src/main.cpp`

All six sub-steps below are in a single file. Make them in order; build once at the end.

- [ ] **Step 1: Add `SCREEN_EDIT` to the `MenuState` enum**

Find (line 13):
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, MOUSE_TUNE_EDIT, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y
};
```

Replace with:
```cpp
enum MenuState {
  MACRO_MENU, SETTINGS, BRIGHTNESS_ADJUST, SLEEP_ADJUST,
  MOUSE_TUNE_MENU, MOUSE_TUNE_EDIT, WIFI_MENU,
  MOUSE_CALIBRATE_X, MOUSE_CALIBRATE_Y,
  SCREEN_EDIT
};
```

- [ ] **Step 2: Add four new static variables**

After `static unsigned long s_lastCalibTick = 0;` (around line 50), add:

```cpp
static int s_screenW         = 1920;
static int s_screenH         = 1080;
static int s_screenDigits[8] = {0};
static int s_screenDigitPos  = 0;
```

- [ ] **Step 3: Add screen dims to `loadNvs()`**

In `loadNvs()`, after the `mouseParams[5]` line and before `prefs.end()`, add:

```cpp
  s_screenW = prefs.getInt("scrW", 1920);
  s_screenH = prefs.getInt("scrH", 1080);
```

- [ ] **Step 4: Rewrite `executeMouseTuneItem()`**

Replace the entire function with:

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
  if (s_mouseTuneSel == 1 || s_mouseTuneSel == 2) {
    s_calibIdx = s_mouseTuneSel - 1;
    s_calibX   = (uint16_t)mouseParams[s_calibIdx * 2];
    s_calibY   = (uint16_t)mouseParams[s_calibIdx * 2 + 1];
    s_lastCalibTick = millis();
    HID::moveAbs(s_calibX, s_calibY);
    s_mode = MOUSE_CALIBRATE_X;
    return;
  }
  if (s_mouseTuneSel == 3 || s_mouseTuneSel == 4) {
    s_editParamIdx = s_mouseTuneSel + 1;
    int v = mouseParams[s_editParamIdx];
    s_editDigits[0] = v / 1000;
    s_editDigits[1] = (v / 100) % 10;
    s_editDigits[2] = (v / 10)  % 10;
    s_editDigits[3] = v % 10;
    s_editDigitPos  = 0;
    s_mode = MOUSE_TUNE_EDIT;
    return;
  }
  if (s_mouseTuneSel == 5) {
    Preferences p; p.begin("brew", false);
    p.putInt("aptX", mouseParams[0]); p.putInt("aptY", mouseParams[1]);
    p.putInt("amcX", mouseParams[2]); p.putInt("amcY", mouseParams[3]);
    p.putInt("lbX",  mouseParams[4]); p.putInt("lbY",  mouseParams[5]);
    p.end();
    UI::showSaved();
    s_mode = SETTINGS;
    return;
  }
  s_mode = SETTINGS;  // sel=6: Cancel
}
```

- [ ] **Step 5: Update `MOUSE_TUNE_MENU` handler wrap and add `SCREEN_EDIT` handler**

In `loop()`, find the `MOUSE_TUNE_MENU` block:
```cpp
    s_mouseTuneSel = (s_mouseTuneSel + delta + 6) % 6;
```
Change to:
```cpp
    s_mouseTuneSel = (s_mouseTuneSel + delta + 7) % 7;
```

Then find the `MOUSE_CALIBRATE_X || MOUSE_CALIBRATE_Y` block. Immediately **before** it (after the closing `}` of `MOUSE_TUNE_EDIT`), insert the new `SCREEN_EDIT` handler:

```cpp
  } else if (s_mode == SCREEN_EDIT) {
    s_screenDigits[s_screenDigitPos] = (s_screenDigits[s_screenDigitPos] + delta + 10) % 10;
    if (Encoder::shortPressed()) {
      if (s_screenDigitPos < 7) {
        s_screenDigitPos++;
      } else {
        s_screenW = s_screenDigits[0]*1000 + s_screenDigits[1]*100 +
                    s_screenDigits[2]*10   + s_screenDigits[3];
        s_screenH = s_screenDigits[4]*1000 + s_screenDigits[5]*100 +
                    s_screenDigits[6]*10   + s_screenDigits[7];
        Preferences p; p.begin("brew", false);
        p.putInt("scrW", s_screenW); p.putInt("scrH", s_screenH);
        p.end();
        UI::showSaved();
        s_mode = MOUSE_TUNE_MENU;
      }
    }
    if (Encoder::longPressed()) { s_mode = MOUSE_TUNE_MENU; UI::flashScreen(); }
```

- [ ] **Step 6: Add `SCREEN_EDIT` case to the OLED switch**

In the OLED `switch (s_mode)` block at the bottom of `loop()`, after the `MOUSE_CALIBRATE_Y` case, add:

```cpp
      case SCREEN_EDIT: UI::showScreenEdit(s_screenDigits, s_screenDigitPos); break;
```

- [ ] **Step 7: Build — must succeed**

```bash
cd /Volumes/home/Projects/Arduino/Brew370 && pio run 2>&1 | tail -10
```

Expected: `SUCCESS` with zero errors.

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp
git commit -m "feat: Screen resolution entry in Mouse Tune — 8-digit editor, saves to NVS"
```

---

## Self-Review

**Spec coverage:**

| Requirement | Task |
|---|---|
| "Screen" first in Mouse Tune menu | Task 1 step 2 |
| 4-digit width + "x" + 4-digit height on one screen | Task 1 step 3 |
| Encoder scrolls each digit 0–9 | Task 2 step 5 (SCREEN_EDIT handler) |
| SP advances through all 8 digits | Task 2 step 5 |
| Last SP saves to NVS + returns to menu | Task 2 step 5 |
| LP cancels, returns to menu, no save | Task 2 step 5 |
| Default 1920 × 1080 | Task 2 steps 2, 3 |
| NVS keys `scrW` / `scrH` | Task 2 steps 3, 5 |
| Existing menu sel indices shifted +1 | Task 2 step 4 |
| Cal:Pin Tool sel 1 → calibIdx 0 | Task 2 step 4: `s_calibIdx = s_mouseTuneSel - 1` |
| Cal:Map Ctr sel 2 → calibIdx 1 | Task 2 step 4: same formula |
| Label X sel 3 → paramIdx 4 | Task 2 step 4: `s_mouseTuneSel + 1` = 4 ✓ |
| Label Y sel 4 → paramIdx 5 | Task 2 step 4: `s_mouseTuneSel + 1` = 5 ✓ |
| Save+Exit sel 5 (was 4) | Task 2 step 4 ✓ |
| Cancel sel 6 (was 5) | Task 2 step 4 ✓ |
| OLED renders SCREEN_EDIT state | Task 2 step 6 |

**Placeholder scan:** None found.

**Type consistency:**
- `showScreenEdit(int digits[8], int digitPos)` — declared `ui.h` Task 1 step 1, defined `ui.cpp` Task 1 step 3, called `main.cpp` Task 2 step 6. ✓
- `s_screenDigits[8]` — declared Task 2 step 2, populated Task 2 step 4 (indices 0–7), read Task 2 step 5 (reconstruction), passed to `showScreenEdit` Task 2 step 6. ✓
- `s_mouseTuneSel + 1` in editParamIdx mapping: sel=3 → 4 (lbX), sel=4 → 5 (lbY). mouseParams[4]=lbX, mouseParams[5]=lbY. ✓
- `s_mouseTuneSel - 1` in calibIdx mapping: sel=1 → 0 (Pin Tool), sel=2 → 1 (Map Ctr). mouseParams[0/1]=ptX/Y, mouseParams[2/3]=mcX/Y. ✓
