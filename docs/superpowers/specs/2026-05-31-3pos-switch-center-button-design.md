# Design: 3-Position Switch Center Button + Button Reorder

**Date:** 2026-05-31
**Scope:** `src/hardware.cpp` only

---

## Goal

Add a gamepad button for the center (middle) position of each 3-pos switch, and reorder buttons so Pitch occupies 1–3 and Roll occupies 4–6.

---

## Background

Both AP switches (SW1 PITCH, SW2 ROLL) are SPDT 3-position toggles wired as:
- Pin A LOW → pos 0 (physically DOWN)
- Both HIGH → pos 1 (physically CENTER)
- Pin B LOW → pos 2 (physically UP)

`readSwitch()` already returns `1` for center — no hardware or GPIO change needed.

Currently the center position sends no gamepad button. DCS cannot distinguish center from "no input."

---

## Button Mapping (6 bits, mutually exclusive per switch)

| Bit | DCS button | Switch | Physical | DCS action |
|-----|-----------|--------|----------|------------|
| 0 | 1 | SW1 pos 0 | down | PITCH ATT HOLD |
| 1 | 2 | SW1 pos 1 | center | A/P OFF |
| 2 | 3 | SW1 pos 2 | up | PITCH ALT HOLD |
| 3 | 4 | SW2 pos 0 | down | ROLL STRG SEL |
| 4 | 5 | SW2 pos 1 | center | ROLL ATT HOLD |
| 5 | 6 | SW2 pos 2 | up | ROLL HDG SEL |

Only one bit per switch is set at any time (mutually exclusive).

---

## Change

Replace the `pushGamepad()` comment block and button-bit logic in `src/hardware.cpp`:

**Old (4 bits, no center):**
```cpp
// Bit 0 = SW1 pos 0 (AP PITCH ATT HOLD — held while switch is up)
// Bit 1 = SW1 pos 2 (AP PITCH ALT HOLD — held while switch is down)
// Bit 2 = SW2 pos 0 (AP ROLL  STRG SEL — held while switch is up)
// Bit 3 = SW2 pos 2 (AP ROLL  HDG SEL  — held while switch is down)
// Center (pos 1) = no bit set for that switch
...
if (sw1 == 0) btns |= (1u << 0);
if (sw1 == 2) btns |= (1u << 1);
if (sw2 == 0) btns |= (1u << 2);
if (sw2 == 2) btns |= (1u << 3);
```

**New (6 bits, center included):**
```cpp
// Bit 0 (btn 1) = SW1 pos 0 down   — PITCH ATT HOLD
// Bit 1 (btn 2) = SW1 pos 1 center — A/P OFF
// Bit 2 (btn 3) = SW1 pos 2 up     — PITCH ALT HOLD
// Bit 3 (btn 4) = SW2 pos 0 down   — ROLL STRG SEL
// Bit 4 (btn 5) = SW2 pos 1 center — ROLL ATT HOLD
// Bit 5 (btn 6) = SW2 pos 2 up     — ROLL HDG SEL
...
if (sw1 == 0) btns |= (1u << 0);
if (sw1 == 1) btns |= (1u << 1);
if (sw1 == 2) btns |= (1u << 2);
if (sw2 == 0) btns |= (1u << 3);
if (sw2 == 1) btns |= (1u << 4);
if (sw2 == 2) btns |= (1u << 5);
```

No other files change.

---

## DCS Rebinding Required

After flashing, existing DCS bindings for SW2 (previously buttons 3 and 4) must be updated to buttons 4 and 6. New center buttons 2 (A/P OFF) and 5 (ROLL ATT HOLD) need new bindings.
