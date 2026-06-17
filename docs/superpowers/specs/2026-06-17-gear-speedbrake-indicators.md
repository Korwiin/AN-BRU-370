# Gear & Speedbrake Indicators — Design Spec

**Date:** 2026-06-17
**Status:** Approved for implementation

---

## Overview

Add three landing gear status indicators (triangle formation, left side of fuel display) and a speedbrake intensity indicator (3×3 dot grid, right side) to the Aircraft Status home screen. Both indicators use free horizontal space flanking the centered fuel number.

---

## Layout

The display is 128×32px. The fuel string in `u8g2_font_t0_22b_mr` measures **fw=55px for 5 chars** and **fw=66px for 6 chars** (font is monospaced at 11px/char). Layout is sized to the worst-case 6-char fuel string ("XX,XXX"):

```
x=0        x=31              x=97      x=128
┌──────────┬─────────────────┬──────────┐
│  Left    │   fuel centered │  Right   │
│  31px    │     (66px max)  │  31px    │
└──────────┴─────────────────┴──────────┘
```

The left and right columns occupy x=0–30 and x=98–127 respectively. The fuel centering shifts inward for narrower strings but the column boundaries are fixed at 31px each.

The bottom row (y=32 baseline) is unchanged — CH:/FL:/JAMMING remains there.

---

## Left Column — Landing Gear Triangle

### Behavior

Three filled circles drawn with `u8g2.drawDisc(x, y, 3)` (radius 3 = 7px diameter). Each circle is independent — only drawn when the corresponding DCS-BIOS gear-down-and-locked light is active (value = 1). Nothing is drawn when a gear is up or in transit.

### Positions

```
y=5  (top row):     drawDisc(15,  5, 3)   — nose gear
                    [empty middle row]
y=21 (bottom row):  drawDisc( 8, 21, 3)   — left main gear
                    drawDisc(23, 21, 3)   — right main gear
```

Triangle proportions: 16px vertical gap, 15px horizontal gap between main gear centers. Nose disc is centered over the midpoint of the two main gear discs.

### DCS-BIOS Sources

| Light | Address | Mask | Shift | Description |
|-------|---------|------|-------|-------------|
| Nose | `0x447A` | `0x4000` | 14 | `LIGHT_GEAR_N` — green, down and locked |
| Left main | `0x447A` | `0x8000` | 15 | `LIGHT_GEAR_L` — green, down and locked |
| Right main | `0x447C` | `0x0001` | 0 | `LIGHT_GEAR_R` — green, down and locked |

Note: `0x447A` is already parsed for `DCSBIOS_ADDR_MC_LIGHT` (mask `0x0001`). Gear N and L share that address — their bits are extracted in the same `processWord` branch.

---

## Right Column — Speedbrake 3×3 Grid

### Behavior

Three lines of three characters each, right-justified at x=128. Matches the real F-16 cockpit speedbrake indicator (9-dot 3×3 grid).

| Speedbrake value | Display | Notes |
|---|---|---|
| 0 (≤ threshold) | nothing | |
| 1–49% (`< 0x7FFF`) | `...` on all 3 lines | dots = light braking |
| 50%+ (`≥ 0x7FFF`) | `***` on all 3 lines | asterisks = heavy braking |

The stowed threshold is `≤ 0x0200` (avoids noise at resting position). The 50% boundary is `0x7FFF` (midpoint of the 0–65535 uint16 range).

### Positions

Font: `u8g2_font_5x7_tr`. All three lines drawn right-justified:

```
y=8  — line 1: u8g2.drawStr(128 - strWidth, 8,  str)
y=16 — line 2: u8g2.drawStr(128 - strWidth, 16, str)
y=24 — line 3: u8g2.drawStr(128 - strWidth, 24, str)
```

where `str` is `"..."`, `"***"`, or not drawn.

### DCS-BIOS Source

| Control | Address | Mask | Shift | Range |
|---------|---------|------|-------|-------|
| `SPEEDBRAKE_INDICATOR` | `0x44D4` | `0xFFFF` | 0 | 0=stowed, 65535=fully open |

Defined as `defineFloat` with range `{-1, 1}` in the Lua module, mapped to uint16 by DCS-BIOS. Value 0x0000 = stowed, 0xFFFF = fully open.

---

## DCS-BIOS Changes (`dcs_bios.h` / `dcs_bios.cpp`)

### New constants in `dcs_bios.h`

```cpp
// Landing gear down-and-locked lights — N and L share address 0x447A with MC_LIGHT
constexpr uint16_t DCSBIOS_MASK_LIGHT_GEAR_N    = 0x4000;
constexpr uint16_t DCSBIOS_MASK_LIGHT_GEAR_L    = 0x8000;
constexpr uint16_t DCSBIOS_ADDR_GEAR_LIGHT_R    = 0x447C;
constexpr uint16_t DCSBIOS_MASK_LIGHT_GEAR_R    = 0x0001;
constexpr uint16_t DCSBIOS_ADDR_SPEEDBRAKE      = 0x44D4;
```

### New state variables in `dcs_bios.cpp`

```cpp
static bool     s_gearN     = false;
static bool     s_gearL     = false;
static bool     s_gearR     = false;
static uint16_t s_speedbrake = 0;
```

### `processWord` additions

- `0x447A` branch (already exists for MC_LIGHT): add extraction of bits 14 and 15 for gear N and L
- New `0x447C` branch: extract bit 0 for gear R
- New `0x44D4` branch: store full uint16 for speedbrake

### New public functions in `DcsBios` namespace

```cpp
bool     gearNose();    // true when nose gear down and locked
bool     gearLeft();    // true when left main gear down and locked
bool     gearRight();   // true when right main gear down and locked
uint16_t speedbrake();  // 0=stowed, 65535=fully open
```

Declared in `dcs_bios.h`, implemented in `dcs_bios.cpp`.

---

## UI Changes (`ui.h` / `ui.cpp`)

### Updated `showAircraftStatus` signature

```cpp
void showAircraftStatus(uint32_t fuelLbs,
                        const char* chaff, const char* flare, bool ecmTx,
                        bool gearN, bool gearL, bool gearR,
                        uint16_t speedbrake);
```

### New static helpers in `ui.cpp`

```cpp
static void drawGearTriangle(bool n, bool l, bool r);
static void drawSpeedbrake(uint16_t val);
```

`drawGearTriangle` calls `u8g2.drawDisc` for each lit gear. `drawSpeedbrake` selects `"..."`, `"***"`, or draws nothing, then draws the chosen string right-justified at y=8, y=16, y=24.

Both helpers are called from `showAircraftStatus()` after drawing the fuel string and before the bottom zone.

Font must be restored to `u8g2_font_5x7_tr` before the helpers draw (large font is active for the fuel string).

### Remove `fw` diagnostic

The `#ifndef RELEASE_BUILD` fw measurement block added in this session is removed — measurement is complete (fw=55 for 5-char, 66px inferred for 6-char).

---

## `main.cpp` Changes

The `showAircraftStatus` callsite is updated to pass the four new arguments:

```cpp
UI::showAircraftStatus(DcsBios::fuelLbs(),
                       DcsBios::chaffStr(), DcsBios::flareStr(),
                       DcsBios::ecmTransmitting(),
                       DcsBios::gearNose(), DcsBios::gearLeft(), DcsBios::gearRight(),
                       DcsBios::speedbrake());
```

---

## CH:/FL: Spacing Fix (included in same version)

Already implemented in v0.58 session but not yet in a released version with gear indicators:

- `trimmed()` lambda removed from `showAircraftStatus`
- CH: draws raw 4-char DCS buffer directly; leading spaces act as natural label/digit gap
- FL: centered using fixed `"FL:    "` reference width so label position never shifts during blink

---

## Version

Target: **v0.59**

---

## Success Criteria

- All 3 gear discs appear when gear is down and locked in DCS; disappear when gear is raised
- Each gear disc is independent — 2 discs show if one gear is up
- Triangle formation is visually clear on the physical OLED
- Speedbrake shows `...` at any partial deployment, `***` at 50%+ deployment, nothing when stowed
- CH: and FL: labels have consistent spacing from values at all count values including 60, 255, Lo10
- No pixel overlap between indicators and the centered fuel string at any fuel level
- Build fits within flash budget
