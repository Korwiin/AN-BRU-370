# Design: Settings Menu Wide-Screen Layout

**Date:** 2026-05-30
**Scope:** `src/ui.cpp`, `include/ui.h`, `src/main.cpp`
**Display:** SSD1306 128×32 px (U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C)

---

## Goal

Reformat `showSettingsMenu` and `showBrightnessAdjust` to use a left/right split layout
that takes advantage of the 128px width. No divider line — content placement creates
natural separation.

---

## Font

All affected screens switch from `u8g2_font_6x10_tr` to `u8g2_font_5x7_tr`.
This allows 4 lines at y = 8, 16, 24, 32 (8px spacing), filling the 32px display exactly.

---

## Settings Menu (`showSettingsMenu`)

### Left panel (x = 0..63) — static status

| Line | y  | Content                          |
|------|----|----------------------------------|
| 1    | 8  | `AN/BRU-370` (static title)      |
| 2    | 16 | `v{FIRMWARE_VERSION}` (dynamic)  |
| 3    | 24 | `WiFi:OK` or `WiFi:--`           |
| 4    | 32 | `DCS:OK` or `DCS:--`             |

### Right panel (x = 65..127) — scrolling menu

- 4 items visible at once (was 3).
- Selected item: `>` at x=65, label at x=71.
- Non-selected item: label at x=71.
- Lines at y = 8, 16, 24, 32.

### ASCII mockup

```
┌──────────────────────────────────┐
│AN/BRU-370   >Reboot              │
│v0.01         Hand:Left           │
│WiFi:OK       Brightness          │
│DCS:--        Sleep               │
└──────────────────────────────────┘
```

### Function signature change

```cpp
// Old
void showSettingsMenu(int sel, int offset, int hand, bool usbReady);

// New
void showSettingsMenu(int sel, int offset, int hand, bool wifiOk, bool dcsOk);
```

### Call-site changes in `main.cpp`

- Pass `WifiMgr::isConnected()` and `DcsBios::isConnected()` instead of `HID::isReady()`.
- Scroll guard: `+3`/`-2` → `+4`/`-3`.

```cpp
// Old
if (s_menuSel >= s_menuOffset + 3) s_menuOffset = s_menuSel - 2;

// New
if (s_menuSel >= s_menuOffset + 4) s_menuOffset = s_menuSel - 3;
```

---

## Brightness Screen (`showBrightnessAdjust`)

### Left panel (x = 0..61)

| y  | Content    |
|----|------------|
| 10 | `SP=Save`  |
| 24 | `LP=Cncl`  |

### Right panel (x = 65..127)

| Element | Position                                      |
|---------|-----------------------------------------------|
| Title   | `Brightness` at y=8, x=65                    |
| Bar frame | `drawFrame(65, 11, 62, 8)`                |
| Bar fill  | `drawBox(66, 12, (value*60)/255, 6)`      |
| Value   | numeric, centered in x=65..127, at y=28       |

### ASCII mockup

```
┌──────────────────────────────────┐
│SP=Save      Brightness           │
│             [████████░░░]        │
│LP=Cncl           128             │
│                                  │
└──────────────────────────────────┘
```

---

## Out of Scope

- `showSleepAdjust` — not requested, left unchanged.
- `showMouseTuneMenu` / `showMouseTuneEdit` — not requested, left unchanged.
- All other UI screens — not requested, left unchanged.
