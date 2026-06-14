# Spec: AP Switch + Gamepad Removal

**Date:** 2026-06-14  
**Status:** Approved

---

## Goal

Remove the AP PITCH and AP ROLL switch features and the HID Gamepad interface entirely. The target user no longer requires these controls; DCS-BIOS provides better alternatives for in-game switch and axis binding. Four GPIO pins are freed for future use. Knowledge of the removed implementation is preserved via an archive git branch.

---

## Knowledge Preservation

Before any code changes, create branch `archive/ap-switches-gamepad` from `main` at commit `255aa1b`. This branch is a permanent, compilable bookmark to the complete working implementation. No further commits are made on it.

---

## Code Removal Scope

### HID layer (`src/hid.cpp`, `include/hid.h`)

Remove the HID Gamepad interface. The USB composite device becomes CDC + Digitizer only. Review `platformio.ini` for any gamepad-specific build flags and remove them.

### Hardware layer (`src/hardware.cpp`, `include/hardware.h`)

Remove the 3-position switch reading logic and the `pushGamepad()` function. The encoder is untouched.

### DCS-BIOS layer (`src/dcs_bios.cpp`, `include/dcs_bios.h`)

Remove:
- `DCSBIOS_ADDR_AP_SWITCHES`, `DCSBIOS_MASK_AP_PITCH`, `DCSBIOS_SHFT_AP_PITCH`, `DCSBIOS_MASK_AP_ROLL`, `DCSBIOS_SHFT_AP_ROLL`
- `apPitchSwitch()` and `apRollSwitch()` getter declarations and implementations

Keep: the decode of address `0x4400` and all `STORES_CONFIG_SW` constants — `STORES_CONFIG_SW` shares this address and must continue working.

### Pins (`include/pins.h`)

Remove `PIN_SW1_A`, `PIN_SW1_B`, `PIN_SW2_A`, `PIN_SW2_B`. Pins 1, 2, 12, 13 are freed with no replacement assigned.

### Main loop (`src/main.cpp`)

Remove gamepad button push calls and any AP switch state variables.

### Version bump (`include/config.h`)

`v0.13 → v0.14`

---

## Documentation Updates

### `docs/spec.md`
- Remove the **Gamepad Mapping** section entirely.
- In the **DCS-BIOS Integration** section, remove AP PITCH and AP ROLL rows from the decoded signals table and the corresponding rows from the commands table.

### `docs/tech-reference.md`
- Remove the **HID Gamepad Mapping** section entirely.
- Remove AP PITCH and AP ROLL rows from the DCS-BIOS decoded signals table.
- Remove the Gamepad interface from the USB composite interface list (USB Identity section).

### `docs/decision-log.md`
- Add entry: AP switches and Gamepad removed from scope (2026-06-14). Target user no longer requires them. DCS-BIOS provides better alternatives for in-game switch and axis binding. Implementation preserved on `archive/ap-switches-gamepad` branch.

---

## Out of Scope

- Reassigning the freed GPIO pins — no current plan.
- OTA firmware updates — separate backlog item; partition table changes required regardless of this removal.
- Any other DCS-BIOS signals (MC, RWR, Stores Config) — unchanged.
