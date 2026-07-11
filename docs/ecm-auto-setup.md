# ECM Auto-Detection & Setup

**Feature spec** · Target firmware: v1.02 · Date: 2026-07-11 · Status: APPROVED — READY FOR IMPLEMENTATION

---

## 1. Overview

When the F-16C loadout includes an ALQ-131 ECM pod, the sim initializes ECM_1_BTN as armed and ECM power to STBY. The Brew370 setup sequence detects this state and runs three additional automated steps: arm mode buttons 2–6, advance power to OPR, and enable the CMDS JMR Source switch. The XMIT switch is left at whatever position the sim placed it.

---

## 2. ALQ-131 ECM Panel

### Mode buttons

Eight toggle buttons: ECM_1_BTN through ECM_6_BTN, ECM_FRM_BTN, ECM_SPL_BTN.

Buttons 1–5 each have four indicator LEDs. Button 6 has **no named indicator lights in DCS-BIOS**.

| LED | Color  | Meaning                              |
|-----|--------|--------------------------------------|
| S   | Amber  | Standby — button armed, pod in STBY  |
| A   | Green  | Active — button armed, pod in OPR    |
| F   | Red    | Fault                                |
| T   | Blue   | Transmitting (actively jamming)      |

While ECM power is in STBY, armed buttons show **S**. Once power advances to OPR, armed buttons transition to **A** (and **T** when actively jamming).

### Default state when pod is installed (before setup runs)

- ECM_1_BTN: armed by sim → S light on
- ECM_2_BTN through ECM_6_BTN: dark (unarmed)
- ECM_FRM_BTN, ECM_SPL_BTN: dark (not touched by setup)
- ECM_PW_SW: STBY (sim default)
- ECM_XMIT_SW: FWD+AFT (sim default, not touched by setup)

### State after setup completes

- ECM_1_BTN through ECM_6_BTN: all armed → A lights on (pod is OPR)
- ECM_FRM_BTN, ECM_SPL_BTN: intentionally left off — pilot choice
- ECM_PW_SW: OPR
- CMDS JMR Source switch: ON

### Switches

| Control | DCS-BIOS ID     | Positions              | Setup action                   |
|---------|-----------------|------------------------|-------------------------------|
| Power   | `ECM_PW_SW`     | OFF(0) / STBY(1) / OPR(2) | STBY → OPR — automated step 7 |
| XMIT    | `ECM_XMIT_SW`   | AFT(0) / FWD+AFT(1) / BARRAGE(2) | Not touched — left at sim default |
| RESET   | `ECM_RESET_BTN` | Momentary              | Not touched                   |
| BIT     | `ECM_BIT_BTN`   | Momentary              | Not touched                   |

**XMIT switch position descriptions** (per pilot, not confirmed from DCS source files — verify in-sim):

| Value | Position | Behavior |
|-------|----------|----------|
| 0 | AFT     | Aft hemisphere only |
| 1 | FWD+AFT | Forward and aft — standard combat position; DCS auto-start default |
| 2 | BARRAGE | Continuous full-sphere transmission |

---

## 3. Detection Mechanism

**Detection signal:** `LIGHT_ECM_1_S` at address `0x4480`, mask `0x2000`

- ON → pod installed, button 1 armed, power in STBY → run ECM setup steps 6–8
- OFF → no pod present → skip ECM steps, setup ends at step 5

The sim arms ECM_1_BTN and sets ECM power to STBY automatically when an ALQ-131 pod is in the loadout. No pod = all lights dark.

Address `0x4480` is already dispatched in `processWord()` for RWR MSL LAUNCH (mask `0x0004`). The ECM S-light check is an additive mask check in the same branch — no new address dispatch entry needed.

Detection is sampled once at setup initiation. The ECM state word will have been received in the initial DCS-BIOS broadcast frame before the user triggers setup.

---

## 4. Full Setup Sequence

Steps 1–5 are unchanged. Steps 6–8 execute only when `s_ecmPresent` is true. The OLED step counter shows `X/5` or `X/8` accordingly.

| Step | Label (OLED) | Command(s)                                                   | Confirmation                                  | Condition    |
|------|-------------|--------------------------------------------------------------|-----------------------------------------------|--------------|
| 1    | `HAD`       | `HDPT_SW_L 1`                                                | `hdptLeft() == true`                          | Always       |
| 2    | `TGP`       | `HDPT_SW_R 1`                                                | `hdptRight() == true`                         | Always       |
| 3    | `CMDS`      | `CMDS_MODE_KNB 3`                                            | `cmdsModeKnob() == 3`                         | Always       |
| 4    | `RWR`       | `RWR_PWR_BTN 1`                                              | `rwrPowerLight() == true`                     | Always       |
| 5    | `MWS`       | `CMDS_MWS_SOURCHE_SW 1`                                      | `mwsOn() == true`                             | Always       |
| 6    | `BTN`       | `ECM_2_BTN 1`, `ECM_3_BTN 1`, `ECM_4_BTN 1`, `ECM_5_BTN 1`, `ECM_6_BTN 1` (all sent together) | S-lights on buttons 2–5 all lit (`0x448A` masks `0x0002 \| 0x0020 \| 0x0200 \| 0x2000`) | ECM pod only |
| 7    | `PWR`       | `ECM_PW_SW 2`                                                | `ecmPowerOpr() == true` (`0x4526` mask `0x0300` >> 8 == 2) | ECM pod only |
| 8    | `JMR`       | `CMDS_JMR_SOURCHE_SW 1`                                      | `jmrSourceOn() == true` (`0x445A` mask `0x0800` >> 11 == 1) | ECM pod only |

### Notes

- **Step 6 — ECM_6_BTN:** Button 6 has no indicator light in DCS-BIOS. Its command is sent on every attempt alongside buttons 2–5, but step 6 passes solely on the buttons 2–5 S-light confirmation.
- **Retry policy:** Steps 6–8 use the same policy as steps 1–5: 10 retries × 750 ms per step. On step 6 retry, all five button commands are re-sent. Exhaustion on any step aborts the sequence and returns to NOT_READY.
- **Max steps:** `s_ecmPresent ? 8 : 5`

---

## 5. DCS-BIOS Address Reference

| Identifier           | Address  | Mask     | Shift | R/W  | Purpose                              |
|----------------------|----------|----------|-------|------|--------------------------------------|
| `LIGHT_ECM_1_S`      | `0x4480` | `0x2000` | 13    | R    | Detection cue — pod in STBY          |
| `LIGHT_ECM_2_S`      | `0x448A` | `0x0002` | 1     | R    | Step 6 confirm — button 2 armed      |
| `LIGHT_ECM_3_S`      | `0x448A` | `0x0020` | 5     | R    | Step 6 confirm — button 3 armed      |
| `LIGHT_ECM_4_S`      | `0x448A` | `0x0200` | 9     | R    | Step 6 confirm — button 4 armed      |
| `LIGHT_ECM_5_S`      | `0x448A` | `0x2000` | 13    | R    | Step 6 confirm — button 5 armed      |
| ~~LIGHT_ECM_6_S~~    | —        | —        | —     | —    | Not defined in DCS-BIOS              |
| `ECM_2_BTN`          | `0x4526` | `0x8000` | 15    | W    | Step 6 command                       |
| `ECM_3_BTN`          | `0x4544` | `0x0100` | 8     | W    | Step 6 command                       |
| `ECM_4_BTN`          | `0x4544` | `0x0200` | 9     | W    | Step 6 command                       |
| `ECM_5_BTN`          | `0x4544` | `0x0400` | 10    | W    | Step 6 command                       |
| `ECM_6_BTN`          | `0x4544` | `0x0800` | 11    | W    | Step 6 command (unconfirmed)         |
| `ECM_PW_SW`          | `0x4526` | `0x0300` | 8     | R/W  | Step 7 command + confirm (value 2 = OPR) |
| `CMDS_JMR_SOURCHE_SW`| `0x445A` | `0x0800` | 11    | R/W  | Step 8 command + confirm (value 1 = ON) |

### processWord() dispatch additions

| Address  | Existing use                      | New addition                              |
|----------|-----------------------------------|-------------------------------------------|
| `0x4480` | RWR MSL LAUNCH (mask `0x0004`)    | `LIGHT_ECM_1_S` (mask `0x2000`) — additive |
| `0x448A` | —                                 | All four button S-lights — new address    |
| `0x4526` | —                                 | `ECM_PW_SW` readback — new address        |
| `0x445A` | MWS_SW (mask `0x1000`)            | `CMDS_JMR_SOURCHE_SW` (mask `0x0800`) — additive |

---

## 6. Implementation Scope

### Files changed

**`include/dcs_bios.h`**
- Add address/mask constants for all new signals (ECM_1_S, ECM_2–5_S, ECM_PW_SW, JMR_SW)
- Add command string defines for ECM_2–6_BTN, ECM_PW_SW, CMDS_JMR_SOURCHE_SW
- Add accessors: `bool ecmStandby()`, `bool ecmBtns2to5Armed()`, `bool ecmPowerOpr()`, `bool jmrSourceOn()`

**`src/dcs_bios.cpp`**
- Add static state vars: `s_ecm1S`, `s_ecmBtnArmed` (bitmask for buttons 2–5), `s_ecmPwSw`, `s_jmrSw`
- In `processWord()`: add checks at `0x4480` (ECM_1_S), `0x448A` (button S-lights), `0x4526` (PW_SW), `0x445A` (JMR — additive to existing MWS check)
- Implement four new accessors

**`src/main.cpp`**
- Add `s_ecmPresent` bool; set from `DcsBios::ecmStandby()` at setup entry
- Extend max step: `s_ecmPresent ? 8 : 5`
- Add `case 6:`, `case 7:`, `case 8:` to confirmed / retry / advance switch blocks
- Step 6 retry re-sends all five button commands
- OLED step counter denominator: `s_ecmPresent ? 8 : 5`; labels `BTN` / `PWR` / `JMR` for steps 6–8

### Not in scope

- ECM XMIT switch — left at sim default
- FRM and SPL buttons — pilot choice
- ECM status display on aircraft home screen
- ECM state persistence across aircraft restarts

---

## 7. Decisions

| # | Question | Decision |
|---|----------|----------|
| 1 | Should setup set the XMIT switch? | No — left at sim default. Pilot choice. |
| 2 | ECM_6_BTN has no indicator light — how to confirm step 6? | Accept silent pass. Command sent on every retry. Step passes when buttons 2–5 confirm. |
| 3 | False-negative detection if ECM state word not yet received? | Accept it. Initial DCS-BIOS frame arrives before manual setup trigger. User re-runs if needed. |
| 4 | Delay after PW → OPR before readback? | No delay. Rely on retry loop per NOT_READY standing procedure (750 ms × 10 retries). |
