# Mouse Position Tuning Redesign

**Date:** 2026-06-06  
**Status:** Approved  

## Goal

Rename, reorganise, and extend the Mouse Tune submenu so that:
- Every position target is calibrated live (no digit entry)
- Each calibration auto-saves to NVS on confirm — no separate Save step
- A new CDRP click-out position is user-configurable
- The label position becomes screen-absolute instead of a relative delta
- Dead UI states (`MOUSE_TUNE_EDIT`) are removed

---

## Menu Structure

Title: **MOUSE POSITION TUNING** (replaces "MOUSE TUNE")  
6 items (was 7), scrollable 3 at a time, modulo 6.

| # | Label | Was |
|---|---|---|
| 0 | Screen Size | Screen |
| 1 | Map Pin Tool POS | Cal:Pin Tool |
| 2 | Map Center POS | Cal:Map Ctr |
| 3 | Pin Label POS | Label X + Label Y (merged, new flow) |
| 4 | Click Out POS | *(new)* |
| 5 | Back | Cancel |

"Save+Exit" is removed. Each calibration auto-saves to NVS on confirm.

---

## mouseParams Data Model

Array expands from 6 → 8 entries. Indexed by `calibIdx * 2` (X) and `calibIdx * 2 + 1` (Y).

| Index | calibIdx | Meaning | NVS key | Default |
|---|---|---|---|---|
| [0] | 0 | Pin Tool X | `apxX` | `screenW / 4` |
| [1] | 0 | Pin Tool Y | `apxY` | `screenH / 54` |
| [2] | 1 | Map Center X | `amcX2` | `screenW / 2` |
| [3] | 1 | Map Center Y | `amcY2` | `screenH / 2` |
| [4] | 2 | Pin Label X | `lbX2` | `screenW / 2` |
| [5] | 2 | Pin Label Y | `lbY2` | `screenH / 2` |
| [6] | 3 | Click Out X | `cdrpX` | `screenW / 5` |
| [7] | 3 | Click Out Y | `cdrpY` | `screenH / 2` |

**Label position semantics change:** [4]/[5] are now screen-absolute pixel coordinates (same coordinate space as all other params). Previously they were a relative delta from map center. New NVS keys `lbX2`/`lbY2` avoid inheriting stale relative values from `lbX`/`lbY`.

`macros.h` extern declaration changes from `mouseParams[6]` to `mouseParams[8]`.  
`dropPinAndLabel()` changes `moveRel(mouseParams[4], mouseParams[5])` → `moveAbs(mouseParams[4], mouseParams[5])`.

---

## Calibration Flow (items 1–4)

All four position targets use the same live calibration states (`MOUSE_CALIBRATE_X` → `MOUSE_CALIBRATE_Y`).

### Entry (items 1, 2, 4 — standard)
```
s_calibIdx = <0, 1, or 3>
s_calibX = mouseParams[calibIdx * 2]
s_calibY = mouseParams[calibIdx * 2 + 1]
HID::moveAbs(s_calibX, s_calibY)
s_mode = MOUSE_CALIBRATE_X
```

### Entry (item 3 — Pin Label POS)
Before entering calibration, drop a pin to open the label dialog:
```
HID::Keyboard.releaseAll()
HID::pressKey(KEY_F10)
delay(30)
HID::moveAbs(mouseParams[0], mouseParams[1])   // pin tool button
HID::mouseClick()
HID::moveAbs(mouseParams[2], mouseParams[3])   // map center
HID::mouseClick()                              // drop pin → label dialog opens
delay(400)
s_calibIdx = 2
s_calibX = mouseParams[4]
s_calibY = mouseParams[5]
HID::moveAbs(s_calibX, s_calibY)
s_mode = MOUSE_CALIBRATE_X
```
DCS must be in map mode. No guard added — same implicit assumption as running any macro.

### Confirm (short press on Y axis)
```
mouseParams[calibIdx * 2]     = s_calibX
mouseParams[calibIdx * 2 + 1] = s_calibY
// write this calibIdx's two NVS keys immediately
s_mode = MOUSE_TUNE_MENU
```

NVS keys written per calibIdx:

| calibIdx | Keys |
|---|---|
| 0 | `apxX`, `apxY` |
| 1 | `amcX2`, `amcY2` |
| 2 | `lbX2`, `lbY2` |
| 3 | `cdrpX`, `cdrpY` |

### Cancel (long press, either axis)
Discard `s_calibX`/`s_calibY`, return to `MOUSE_TUNE_MENU`. No NVS write.

---

## OLED Calibration Instructional Text

`showMouseCalibrate(axis, value, label)` — `label` is now the instructional line (line 1).  
Font `u8g2_font_5x7_tr`, max ~21 chars. All strings fit.

| calibIdx | X label (axis=0) | Y label (axis=1) |
|---|---|---|
| 0 | `Mv L/R to Map Label` | `Mv U/D to Map Label` |
| 1 | `Mv L/R to Map Center` | `Mv U/D to Map Center` |
| 2 | `Mv L/R to Text Field` | `Mv U/D to Text Field` |
| 3 | `Mv L/R to Click Out` | `Mv U/D to Click Out` |

Stored as two `const char*` arrays indexed by `s_calibIdx` in `main.cpp`. Passed to `showMouseCalibrate()` at both the display-update call sites (inside MOUSE_CALIBRATE_X/Y update block and in the OLED render switch).

---

## executeCDRP Update

`executeCDRP()` in `macros.cpp` currently uses `HID::screenW() / 5, HID::screenH() / 2` (hardcoded from this session's fix). After this change it uses `mouseParams[6], mouseParams[7]`, which NVS loads with defaults `screenW / 5` and `screenH / 2` — identical behaviour until the user customises.

---

## Back Item

Returns to `SETTINGS`. No state restoration needed — all confirmed changes were already persisted to NVS, and cancelled calibrations were never written.

`s_prevMouseParams` array and the two `memcpy` calls (snapshot on entry, restore on long press) are removed.

---

## Removed Code

| Symbol | Reason |
|---|---|
| `MOUSE_TUNE_EDIT` (MenuState) | Replaced by live calibration for all targets |
| `showMouseTuneEdit()` (ui.cpp) | No longer reachable |
| `showMouseTuneEdit()` declaration (ui.h) | Same |
| `s_prevMouseParams[6]` (main.cpp) | No cancel-restore needed with auto-save model |
| `memcpy(s_prevMouseParams, ...)` entry snapshot | Same |
| `memcpy(mouseParams, s_prevMouseParams, ...)` long-press restore | Same |
| `s_editParamIdx`, `s_editDigits[4]`, `s_editDigitPos` (main.cpp) | Only used by MOUSE_TUNE_EDIT |
| `HID::screenW()`, `HID::screenH()` (hid.cpp / hid.h) | Added this session for executeCDRP; replaced by mouseParams[6/7] |
| NVS keys `lbX`, `lbY` | Superseded by `lbX2`, `lbY2` (old keys left in NVS as harmless orphans) |

---

## Files Touched

| File | Change |
|---|---|
| `include/macros.h` | `mouseParams[6]` → `mouseParams[8]` |
| `src/macros.cpp` | Array size, `executeCDRP` uses `mouseParams[6/7]`, `dropPinAndLabel` uses `moveAbs` for label |
| `src/main.cpp` | Menu modulo 6, `executeMouseTuneItem` rewrite, calibIdx 2/3 entry, auto-save per confirm, label arrays, remove prevMouseParams, remove MOUSE_TUNE_EDIT branch |
| `src/ui.cpp` | Title string, menu items array (6), remove `showMouseTuneEdit` |
| `include/ui.h` | Remove `showMouseTuneEdit` declaration |
| `src/hid.cpp` | Remove `screenW()` and `screenH()` |
| `include/hid.h` | Remove `screenW()` and `screenH()` declarations |
