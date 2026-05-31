# Settings Menu Wide-Screen Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reformat `showSettingsMenu` and `showBrightnessAdjust` to use a 64px/64px left-right split layout on the 128×32 SSD1306 display.

**Architecture:** Left panel shows static status info (title, version, WiFi, DCS); right panel shows the scrolling menu (4 items visible, was 3). Brightness screen splits actions left, bar+title+value right. Font switches from `u8g2_font_6x10_tr` to `u8g2_font_5x7_tr` to fit 4 lines at 8px spacing in 32px height.

**Tech Stack:** C++, U8g2 library, PlatformIO (`esp32s3_supermini` env)

---

## File Map

- Modify: `include/ui.h` — update `showSettingsMenu` signature (drop `usbReady`, add `wifiOk`/`dcsOk`)
- Modify: `src/ui.cpp` — rewrite `showSettingsMenu` and `showBrightnessAdjust`
- Modify: `src/main.cpp` — update call site (lines 362–363) and scroll guard (line 250)

---

### Task 1: Update `showSettingsMenu` signature in `ui.h`

**Files:**
- Modify: `include/ui.h:28`

- [ ] **Step 1: Edit the declaration**

In `include/ui.h`, replace line 28:
```cpp
  void showSettingsMenu(int sel, int offset, int hand, bool usbReady);
```
with:
```cpp
  void showSettingsMenu(int sel, int offset, int hand, bool wifiOk, bool dcsOk);
```

- [ ] **Step 2: Build — expect a compile error**

```bash
pio run -e esp32s3_supermini
```
Expected: error about `showSettingsMenu` definition mismatch in `ui.cpp` and call-site mismatch in `main.cpp`. This confirms the header change propagated.

---

### Task 2: Implement new `showSettingsMenu` in `ui.cpp`

**Files:**
- Modify: `src/ui.cpp:130-148`

- [ ] **Step 1: Replace the function body**

In `src/ui.cpp`, replace the entire `showSettingsMenu` function (lines 130–148):
```cpp
void UI::showSettingsMenu(int sel, int offset, int hand, bool wifiOk, bool dcsOk) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — status (x=0..63)
  u8g2.drawStr(0, 8,  "AN/BRU-370");
  char ver[12];
  snprintf(ver, sizeof(ver), "v%s", FIRMWARE_VERSION);
  u8g2.drawStr(0, 16, ver);
  u8g2.drawStr(0, 24, wifiOk ? "WiFi:OK" : "WiFi:--");
  u8g2.drawStr(0, 32, dcsOk  ? "DCS:OK"  : "DCS:--");

  // Right panel — 4-item scrolling menu (x=65..127)
  for (int i = 0; i < 4; i++) {
    int idx = offset + i;
    if (idx >= kNumMenuItems) break;
    int y = 8 + i * 8;
    const char* label;
    if (idx == 1) label = (hand == 0) ? "Hand:Left" : "Hand:Right";
    else          label = s_menuItems[idx];
    if (idx == sel) {
      u8g2.drawStr(65, y, ">");
      u8g2.drawStr(71, y, label);
    } else {
      u8g2.drawStr(71, y, label);
    }
  }
  u8g2.sendBuffer();
}
```

- [ ] **Step 2: Build — expect one remaining error (main.cpp call site)**

```bash
pio run -e esp32s3_supermini
```
Expected: error only in `main.cpp` about wrong number of arguments to `showSettingsMenu`. `ui.cpp` itself should be error-free.

---

### Task 3: Update `main.cpp` call site and scroll guard

**Files:**
- Modify: `src/main.cpp:250` (scroll guard)
- Modify: `src/main.cpp:362-363` (OLED update call)

- [ ] **Step 1: Fix the scroll guard (line 250)**

Replace:
```cpp
    if (s_menuSel >= s_menuOffset + 3) s_menuOffset = s_menuSel - 2;
```
with:
```cpp
    if (s_menuSel >= s_menuOffset + 4) s_menuOffset = s_menuSel - 3;
```

- [ ] **Step 2: Fix the call site (lines 362–363)**

Replace:
```cpp
      case SETTINGS:          UI::showSettingsMenu(s_menuSel, s_menuOffset,
                                s_handedness, HID::isReady()); break;
```
with:
```cpp
      case SETTINGS:          UI::showSettingsMenu(s_menuSel, s_menuOffset,
                                s_handedness, WifiMgr::isConnected(),
                                DcsBios::isConnected()); break;
```

- [ ] **Step 3: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: build succeeds with no errors or new warnings.

- [ ] **Step 4: Commit**

```bash
git add include/ui.h src/ui.cpp src/main.cpp
git commit -m "feat: settings menu wide-screen layout — split status/menu panels"
```

---

### Task 4: Implement new `showBrightnessAdjust` in `ui.cpp`

**Files:**
- Modify: `src/ui.cpp:150-162`

- [ ] **Step 1: Replace the function body**

In `src/ui.cpp`, replace the entire `showBrightnessAdjust` function (lines 150–162):
```cpp
void UI::showBrightnessAdjust(int value) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Left panel — actions (x=0..61)
  u8g2.drawStr(0, 10, "SP=Save");
  u8g2.drawStr(0, 24, "LP=Cncl");

  // Right panel — title, bar, value (x=65..127)
  u8g2.drawStr(65, 8, "Brightness");
  u8g2.drawFrame(65, 11, 62, 8);
  int fill = (value * 60) / 255;
  if (fill > 0) u8g2.drawBox(66, 12, fill, 6);
  char buf[5];
  snprintf(buf, sizeof(buf), "%d", value);
  int w = u8g2.getStrWidth(buf);
  u8g2.drawStr(65 + (62 - w) / 2, 28, buf);

  u8g2.sendBuffer();
}
```

- [ ] **Step 2: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: build succeeds with no errors or new warnings.

- [ ] **Step 3: Commit**

```bash
git add src/ui.cpp
git commit -m "feat: brightness adjust wide-screen layout — split actions/bar panels"
```

---

## Hardware Verification

After both commits, flash and verify on device:

```bash
pio run -e esp32s3_supermini -t upload
```

**Settings menu check:**
- Left half shows `AN/BRU-370`, firmware version, `WiFi:OK`/`WiFi:--`, `DCS:OK`/`DCS:--`
- Right half shows 4 menu items; `>` tracks the selected item
- Scrolling through all 7 items advances the window correctly (no items skip or repeat)

**Brightness check:**
- Left half shows `SP=Save` and `LP=Cncl`
- Right half shows `Brightness` title, bar fills left-to-right as encoder turns, numeric value below bar
