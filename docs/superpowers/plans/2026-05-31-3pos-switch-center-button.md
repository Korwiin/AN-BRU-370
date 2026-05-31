# 3-Position Switch Center Button Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a held gamepad button for the center position of each 3-pos switch and reorder all six buttons so Pitch occupies buttons 1–3 and Roll occupies buttons 4–6.

**Architecture:** Single edit to `pushGamepad()` in `src/hardware.cpp` — expand from 4 bits (no center) to 6 bits (one per position per switch). `readSwitch()` already returns `1` for center; no GPIO or pin changes needed.

**Tech Stack:** C++, Arduino ESP32, PlatformIO (`esp32s3_supermini` env)

---

## File Map

- Modify: `src/hardware.cpp` — `pushGamepad()` comment block and button-bit logic (lines 24–38)

---

### Task 1: Update `pushGamepad()` button mapping

**Files:**
- Modify: `src/hardware.cpp:24-38`

- [ ] **Step 1: Replace the comment block and bit logic**

In `src/hardware.cpp`, replace the entire comment block and `btns` assignment inside `pushGamepad()`:

**Find (lines 24–38):**
```cpp
// Gamepad button mapping:
//   Bit 0 = SW1 pos 0 (AP PITCH ATT HOLD — held while switch is up)
//   Bit 1 = SW1 pos 2 (AP PITCH ALT HOLD — held while switch is down)
//   Bit 2 = SW2 pos 0 (AP ROLL  STRG SEL — held while switch is up)
//   Bit 3 = SW2 pos 2 (AP ROLL  HDG SEL  — held while switch is down)
//   Center (pos 1) = no bit set for that switch
// x-axis: pot ADC 0-4095 mapped to int8_t -127..127
// API: Gamepad.send(x, y, z, rz, rx, ry, hat, buttons)
static void pushGamepad(uint8_t sw1, uint8_t sw2, uint16_t potRaw) {
  if (!HID::isReady()) return;
  uint32_t btns = 0;
  if (sw1 == 0) btns |= (1u << 0);
  if (sw1 == 2) btns |= (1u << 1);
  if (sw2 == 0) btns |= (1u << 2);
  if (sw2 == 2) btns |= (1u << 3);
```

**Replace with:**
```cpp
// Gamepad button mapping (one bit held per switch position):
//   Bit 0 (btn 1) = SW1 pos 0 down   — PITCH ATT HOLD
//   Bit 1 (btn 2) = SW1 pos 1 center — A/P OFF
//   Bit 2 (btn 3) = SW1 pos 2 up     — PITCH ALT HOLD
//   Bit 3 (btn 4) = SW2 pos 0 down   — ROLL STRG SEL
//   Bit 4 (btn 5) = SW2 pos 1 center — ROLL ATT HOLD
//   Bit 5 (btn 6) = SW2 pos 2 up     — ROLL HDG SEL
// x-axis: pot ADC 0-4095 mapped to int8_t -127..127
// API: Gamepad.send(x, y, z, rz, rx, ry, hat, buttons)
static void pushGamepad(uint8_t sw1, uint8_t sw2, uint16_t potRaw) {
  if (!HID::isReady()) return;
  uint32_t btns = 0;
  if (sw1 == 0) btns |= (1u << 0);
  if (sw1 == 1) btns |= (1u << 1);
  if (sw1 == 2) btns |= (1u << 2);
  if (sw2 == 0) btns |= (1u << 3);
  if (sw2 == 1) btns |= (1u << 4);
  if (sw2 == 2) btns |= (1u << 5);
```

- [ ] **Step 2: Build clean**

```bash
pio run -e esp32s3_supermini
```
Expected: `SUCCESS`, no errors, no new warnings.

- [ ] **Step 3: Commit**

```bash
git add src/hardware.cpp
git commit -m "feat: add center-position gamepad buttons, reorder pitch 1-3 roll 4-6"
```

---

## Hardware Verification

Flash and verify on device:

```bash
pio run -e esp32s3_supermini -t upload
```

Use a gamepad tester (e.g. Windows joystick properties or [gamepad-tester.com](https://gamepad-tester.com)):

- SW1 UP → button 3 held, buttons 1 and 2 off
- SW1 CENTER → button 2 held, buttons 1 and 3 off
- SW1 DOWN → button 1 held, buttons 2 and 3 off
- SW2 UP → button 6 held, buttons 4 and 5 off
- SW2 CENTER → button 5 held, buttons 4 and 6 off
- SW2 DOWN → button 4 held, buttons 5 and 6 off

**DCS rebinding required after flash:**
- SW2 STRG SEL: was button 3 → now button 4
- SW2 HDG SEL: was button 4 → now button 6
- SW1 A/P OFF center: new → bind to button 2
- SW2 ATT HOLD center: new → bind to button 5
