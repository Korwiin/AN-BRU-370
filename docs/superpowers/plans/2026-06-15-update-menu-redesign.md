# Update Menu Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename "Fw Upd" → "Update" in the settings menu and redesign the OTA flow to skip the landing screen — checking immediately on entry, showing results in-place, and returning the cursor to "Update" on all exits.

**Architecture:** `FIRMWARE_MENU` state is removed. Case 5 in `executeMenuItem()` goes straight to `FIRMWARE_CHECKING`, which draws a two-line header then blocks on `OTA::check()`. Results are shown in-place on the same screen layout. Three UI functions gain a `currentVer` parameter; `showFirmwareMenu()` is deleted.

**Tech Stack:** ESP32-S3, PlatformIO/Arduino, U8g2 (`u8g2_font_5x7_tr`), 128×32 OLED.

---

### Task 1: Update UI header and redesign screen functions

**Files:**
- Modify: `include/ui.h:50-54`
- Modify: `src/ui.cpp:422-484`

- [ ] **Step 1: Update the four firmware function declarations in `include/ui.h`**

Replace lines 50–54 (the OTA firmware update screens block):

```cpp
  // OTA firmware update screens
  void showFirmwareChecking(const char* currentVer);
  void showFirmwareUpToDate(const char* currentVer);
  void showFirmwareConfirm(const char* currentVer, const char* availVer);
  void showFirmwareUpdating(int percent);
  void showFirmwareError(const char* reason);
```

`showFirmwareMenu()` is removed entirely. `showFirmwareChecking`, `showFirmwareUpToDate`, and `showFirmwareConfirm` all gain a `currentVer` parameter. `showFirmwareConfirm` also gains `availVer`.

- [ ] **Step 2: Replace all firmware screen functions in `src/ui.cpp`**

Replace the entire block from `// ---- Firmware update screens ----` (line 422) through the closing `}` of `showFirmwareError` (line 483) with:

```cpp
// ---- Firmware update screens ----

void UI::showFirmwareChecking(const char* currentVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, "Checking for updates...");
  u8g2.sendBuffer();
}

void UI::showFirmwareUpToDate(const char* currentVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, "You are up to date.");
  u8g2.drawStr(0, 24, "SP=Back");
  u8g2.sendBuffer();
}

void UI::showFirmwareConfirm(const char* currentVer, const char* availVer) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  char line2[24];
  snprintf(line2, sizeof(line2), "Firmware %s avail", availVer);
  u8g2.drawStr(0, 8,  currentVer);
  u8g2.drawStr(0, 16, line2);
  u8g2.drawStr(0, 24, "SP=Update   LP=Cancel");
  u8g2.sendBuffer();
}

void UI::showFirmwareUpdating(int percent) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 8, "Updating...");
  u8g2.drawFrame(0, 13, 128, 8);
  int fill = (percent * 126) / 100;
  if (fill > 0) u8g2.drawBox(1, 14, fill, 6);
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", percent);
  u8g2.drawStr((128 - u8g2.getStrWidth(pct)) / 2, 30, pct);
  u8g2.sendBuffer();
}

void UI::showFirmwareError(const char* reason) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 10, "Update failed");
  u8g2.drawStr(0, 21, reason);
  u8g2.drawStr(0, 31, "SP=Back");
  u8g2.sendBuffer();
}
```

Note: `showFirmwareMenu()` is simply gone — do not replace it with anything.

- [ ] **Step 3: Commit**

```bash
git add include/ui.h src/ui.cpp
git commit -m "feat(ui): redesign firmware update screens — immediate-check layout"
```

---

### Task 2: Update `main.cpp` — state machine and menu label

**Files:**
- Modify: `src/main.cpp` (multiple locations — follow each step in order)

- [ ] **Step 1: Remove `FIRMWARE_MENU` from the `MenuState` enum**

Current (line 18):
```cpp
  FIRMWARE_MENU, FIRMWARE_CHECKING, FIRMWARE_UP_TO_DATE,
```
Replace with:
```cpp
  FIRMWARE_CHECKING, FIRMWARE_UP_TO_DATE,
```

- [ ] **Step 2: Remove the `s_otaUpToDateSince` variable (line 55)**

Current:
```cpp
static OTA::CheckResult  s_otaResult        = {};
static unsigned long     s_otaUpToDateSince  = 0;
static char              s_otaError[24]      = {0};
```
Replace with:
```cpp
static OTA::CheckResult  s_otaResult   = {};
static char              s_otaError[24] = {0};
```

- [ ] **Step 3: Change case 5 to go directly to `FIRMWARE_CHECKING`**

Current (`executeMenuItem()`, around line 119):
```cpp
    case 5:  // Firmware
      s_mode = FIRMWARE_MENU;
      return;
```
Replace with:
```cpp
    case 5:  // Firmware
      s_mode = FIRMWARE_CHECKING;
      return;
```

- [ ] **Step 4: Replace the entire firmware state-handler block**

Locate the block starting at `} else if (s_mode == FIRMWARE_MENU) {` and ending at the closing `}` of `} else if (s_mode == FIRMWARE_ERROR) {` (currently lines 597–637). Replace the whole block with:

```cpp
  } else if (s_mode == FIRMWARE_CHECKING) {
    if (!WifiMgr::isConnected()) {
      strlcpy(s_otaError, "No WiFi", sizeof(s_otaError));
      s_mode = FIRMWARE_ERROR;
    } else {
      char ver[24];
      snprintf(ver, sizeof(ver), "Current Firmware v%s", FIRMWARE_VERSION);
      UI::showFirmwareChecking(ver);
      s_otaResult = OTA::check();
      if (s_otaResult.error[0]) {
        strlcpy(s_otaError, s_otaResult.error, sizeof(s_otaError));
        s_mode = FIRMWARE_ERROR;
      } else if (s_otaResult.available) {
        s_mode = FIRMWARE_CONFIRM;
      } else {
        s_mode = FIRMWARE_UP_TO_DATE;
      }
    }

  } else if (s_mode == FIRMWARE_UP_TO_DATE) {
    if (Encoder::shortPressed()) {
      s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2;
    }

  } else if (s_mode == FIRMWARE_CONFIRM) {
    if (Encoder::shortPressed()) s_mode = FIRMWARE_UPDATING;
    if (Encoder::longPressed())  { s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2; }

  } else if (s_mode == FIRMWARE_UPDATING) {
    UI::showFirmwareUpdating(0);
    if (!OTA::perform(s_otaResult.url, otaProgressCb)) {
      strlcpy(s_otaError, OTA::performError(), sizeof(s_otaError));
      s_mode = FIRMWARE_ERROR;
    }

  } else if (s_mode == FIRMWARE_ERROR) {
    if (Encoder::shortPressed()) { s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2; }
  }
```

Key changes from original:
- `FIRMWARE_MENU` handler removed entirely
- `FIRMWARE_CHECKING` passes `"Current Firmware v<ver>"` to `showFirmwareChecking()`
- `FIRMWARE_UP_TO_DATE` removes auto-timer; SP=Back with cursor at idx=5
- All Settings exits: `s_menuSel = 5; s_menuOffset = 2;`

- [ ] **Step 5: Replace the OLED switch firmware cases**

Locate the `switch (s_mode)` block in the OLED update section (around line 649). Replace the four firmware cases with:

```cpp
      case FIRMWARE_CHECKING:  break;  // screen set inline before blocking call
      case FIRMWARE_UP_TO_DATE: {
        char ver[24];
        snprintf(ver, sizeof(ver), "Current Firmware v%s", FIRMWARE_VERSION);
        UI::showFirmwareUpToDate(ver);
        break;
      }
      case FIRMWARE_CONFIRM: {
        char curVer[24];
        snprintf(curVer, sizeof(curVer), "Current Firmware v%s", FIRMWARE_VERSION);
        char availVer[10];
        snprintf(availVer, sizeof(availVer), "v%X.%02X",
                 s_otaResult.versionBCD >> 8, s_otaResult.versionBCD & 0xFF);
        UI::showFirmwareConfirm(curVer, availVer);
        break;
      }
      case FIRMWARE_UPDATING:  break;  // screen driven by otaProgressCb
      case FIRMWARE_ERROR:     UI::showFirmwareError(s_otaError); break;
```

The `case FIRMWARE_MENU:` block that previously appeared before `FIRMWARE_CHECKING` is gone.

- [ ] **Step 6: Rename the menu label**

In `src/ui.cpp`, find the menu draw loop label for idx==5 (around line 208):
```cpp
    else if (idx == 5) label = "Fw Upd";
```
Change to:
```cpp
    else if (idx == 5) label = "Update";
```

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp src/ui.cpp
git commit -m "feat(main): remove FIRMWARE_MENU state; check on entry, cursor returns to Update"
```

---

### Task 3: Build, flash, and verify

**Files:** None modified — build and test only.

- [ ] **Step 1: Build**

```bash
cd /Volumes/home/Projects/Arduino/Brew370
pio run
```

Expected: `SUCCESS` with zero errors and zero warnings about `FIRMWARE_MENU` or `showFirmwareMenu` or `s_otaUpToDateSince`. If any "use of undeclared identifier" or "unused variable" warnings appear, fix them before flashing.

- [ ] **Step 2: Flash**

```bash
pio run --target upload
```

Expected: upload completes and device reboots.

- [ ] **Step 3: Verify — menu label**

Open the Settings menu. Scroll to item 5. Confirm it reads **"Update"** (not "Fw Upd").

- [ ] **Step 4: Verify — immediate check (WiFi connected)**

With WiFi connected, press encoder on "Update". Confirm:
- Screen immediately shows:
  ```
  Current Firmware v0.16
  Checking for updates...
  ```
  (no intermediate "press to check" screen)

- [ ] **Step 5: Verify — up-to-date result**

After check completes (device is already on latest): confirm screen updates to:
```
Current Firmware v0.16
You are up to date.
SP=Back
```
Press SP. Confirm Settings menu reopens with cursor on **"Update"** (not top of list).

- [ ] **Step 6: Verify — available result (if testable)**

If a newer version is available in the manifest, confirm screen shows:
```
Current Firmware v0.16
Firmware v0.17 avail
SP=Update   LP=Cancel
```
Press LP. Confirm Settings menu reopens with cursor on **"Update"**.

- [ ] **Step 7: Verify — no WiFi error path**

Disable WiFi (Settings → WiFi → Disable). Return to Settings, press "Update". Confirm screen transitions directly to the error screen showing `"No WiFi"` and `"SP=Back"`. Press SP. Confirm cursor returns to "Update".

- [ ] **Step 8: Bump version and commit**

In `include/config.h`, increment the version (e.g. `"0.17"` / `0x0017`) per the dual-update rule in CLAUDE.md. Commit:

```bash
git add include/config.h
git commit -m "release: bump to v0.17 — Update menu redesign"
```
