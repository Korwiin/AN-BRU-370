# Spec: "Update" Menu Redesign

**Date:** 2026-06-15
**Status:** Approved

## Summary

Rename the "Fw Upd" settings menu item to "Update" and redesign the firmware update flow to
eliminate the intermediate landing screen. Selecting "Update" immediately begins an OTA check
and shows results in-place on a single persistent screen. All exits return the cursor to the
"Update" menu item.

## State Machine

`FIRMWARE_MENU` state is removed. Case 5 in the settings handler transitions directly to
`FIRMWARE_CHECKING`.

```
Settings["Update"] ──► FIRMWARE_CHECKING  (inline draw + blocking OTA::check())
                             │
                    ┌────────┼────────┐
                    ▼        ▼        ▼
             UP_TO_DATE  CONFIRM   ERROR
                 │           │        │
               SP=Back  SP=Update  SP=Back
                        LP=Cancel
                    └────────┴────────┘
                             ▼
                          SETTINGS
                       cursor: sel=5, offset=2
```

`FIRMWARE_UPDATING` and `FIRMWARE_ERROR` states are unchanged except their back exits now
set `s_menuSel=5, s_menuOffset=2`.

## Screen Layouts

Display: 128×32 px, font `u8g2_font_5x7_tr` (5px/char fixed-width), lines at y=8/16/24/32.

### During check (drawn inline before blocking `OTA::check()`)

```
y= 8: Current Firmware v0.16
y=16: Checking for updates...
```

Both strings are ~110px wide — fit within 128px with margin.

### Up to date (SP exits to Settings)

```
y= 8: Current Firmware v0.16
y=16: You are up to date.
y=24: SP=Back
```

### Available (SP=Update, LP=Cancel)

```
y= 8: Current Firmware v0.16
y=16: Firmware v0.17 avail
y=24: SP=Update   LP=Cancel
```

`"SP=Update   LP=Cancel"` = 21 chars = 105px, fits on one line.

### Error (unchanged layout, exit updated)

```
y=10: Update failed
y=21: <reason>
y=31: SP=Back
```

## UI Function Changes (`ui.cpp` / `ui.h`)

| Function | Change |
|---|---|
| `showFirmwareMenu()` | **Remove** — state eliminated |
| `showFirmwareChecking()` | **Redesign** — add `const char* currentVer` param; draw two-line header |
| `showFirmwareUpToDate()` | **Redesign** — signature `(const char* currentVer)`; new three-line layout |
| `showFirmwareConfirm()` | **Redesign** — signature `(const char* currentVer, const char* availVer)`; new three-line layout with inline SP/LP |
| `showFirmwareUpdating()` | Unchanged |
| `showFirmwareError()` | Unchanged |

## `main.cpp` Changes

### Menu label
```cpp
// before
else if (idx == 5) label = "Fw Upd";
// after
else if (idx == 5) label = "Update";
```

### Case 5 entry
```cpp
// before
case 5:  // Firmware
  s_mode = FIRMWARE_MENU;
// after
case 5:  // Firmware
  s_mode = FIRMWARE_CHECKING;
```

### Remove `FIRMWARE_MENU` handler and OLED case
Both the `s_mode == FIRMWARE_MENU` input block and the `case FIRMWARE_MENU:` OLED block
are deleted.

### `FIRMWARE_CHECKING` handler
Call `showFirmwareChecking(ver)` with the version string before the blocking `OTA::check()`.

### `FIRMWARE_UP_TO_DATE` handler
Remove auto-dismiss timer (`s_otaUpToDateSince` — no longer needed). Replace with:
```cpp
if (Encoder::shortPressed()) {
  s_mode = SETTINGS; s_menuSel = 5; s_menuOffset = 2;
}
```

### `showFirmwareConfirm` call
Pass both `currentVer` and the formatted `availVer` string.

### All firmware "back to Settings" exits
Every transition back to `SETTINGS` from any firmware state sets:
```cpp
s_menuSel = 5; s_menuOffset = 2;
```
This positions the cursor on "Update" (item 5 of 6, shown as the last of the 4 visible items
at offset 2).

## Variables Removed

- `s_otaUpToDateSince` — obsolete once auto-dismiss is replaced by SP=Back.
