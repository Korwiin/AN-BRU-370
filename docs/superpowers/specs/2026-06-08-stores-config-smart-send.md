# Stores Config Smart Send — Design Spec

**Date:** 2026-06-08
**Status:** Approved for implementation

---

## Goal

When the Stores Config caution fires, one encoder press sends the correct switch command to DCS-BIOS, then the firmware monitors for confirmation automatically. If delivery fails it retries; if the switch confirms but the light lingers it falls back to standard MC display. On full success both elapsed times are displayed on the OLED for 4 seconds to inform timeout tuning.

---

## Background

`STORES_CONFIG_SW` is a two-position toggle switch decoded from DCS-BIOS address `0x4400`. DCS-BIOS sends the full state dump at connection time, so by the time any Stores Config caution fires the device already knows the switch position with certainty (not `0xFF`). The Stores Config light (`LIGHT_STORES_CONFIG`, `0x4478`) being on means the switch is in the wrong position — the opposite position is always the correct target.

---

## State Machine

The Stores Config sub-state lives inside the existing `mcConfirmed` block. It has four states:

```
SC_IDLE → (press) → SC_WAITING_SW → (sw confirms) → SC_WAITING_LIGHT → (light clears) → SC_SHOW_TIMING → SC_IDLE
                        ↓ T1 retry                        ↓ T2 expires
                     (resend same target)             SC_GAVE_UP → (show MC fallback)
```

### SC_IDLE
- OLED: flashing "STORES CONFIG" (existing behavior)
- Encoder short-press: lock `s_scTarget = storesConfigSw() ^ 1`, record `s_scTPress = millis()`, record `s_scTLastSend = millis()`, send `STORES_CONFIG_SW <s_scTarget>`, transition to `SC_WAITING_SW`

### SC_WAITING_SW
- OLED: continue flashing "STORES CONFIG"
- Each loop: if `storesConfigSw() == s_scTarget` → record `s_scSwMs = millis() - s_scTPress`, transition to `SC_WAITING_LIGHT`
- Each loop: if `millis() - s_scTLastSend >= T1` → resend `STORES_CONFIG_SW <s_scTarget>`, update `s_scTLastSend`
- Encoder: ignored

### SC_WAITING_LIGHT
- OLED: continue flashing "STORES CONFIG"
- Each loop: if `storesConfigLight() == false` → record `s_scLtMs = millis() - s_scTPress`, record `s_scShowUntil = millis() + 4000`, transition to `SC_SHOW_TIMING`
- Each loop: if `millis() - (s_scTPress + s_scSwMs) >= T2` → transition to `SC_GAVE_UP`
- Encoder: ignored

### SC_GAVE_UP
- The switch reached the target position but the light did not clear within `SC_LIGHT_TIMEOUT_MS`
- The `storesConfigLight()` check is bypassed — this state falls through to the standard MASTER CAUTION display so the player can short-press to reset MC manually
- Clears back to `SC_IDLE` only when `storesConfigLight()` finally goes false (condition resolved externally), or when `mcConfirmed` becomes false

### SC_SHOW_TIMING
- OLED: static display showing both elapsed times (see UI section below)
- Checked at the **top of the `mcConfirmed` block, before the `storesConfigLight()` test**, so it persists even after MC and the light both clear
- Encoder: ignored
- On timer expiry (`millis() >= s_scShowUntil`): reset to `SC_IDLE`, return to normal loop
- RWR remains higher priority — if `rwrConfirmed` fires during SC_SHOW_TIMING the RWR block preempts as normal (it runs before MC in `loop()`)

---

## State Reset

`s_scState` resets to `SC_IDLE` and `s_scTarget` resets to `0xFF` on any of:
- `storesConfigLight()` goes false while in `SC_IDLE` (condition cleared before press)
- `storesConfigLight()` goes false while in `SC_GAVE_UP` (condition finally resolved externally)
- The `else` branch runs (light off, MC still active — no Stores Config condition)
- `mcConfirmed` becomes false (MC cleared or stream loss)
- `SC_SHOW_TIMING` timer expires

---

## Timeout Constants

Defined in `include/dcs_bios.h` alongside other DCS-BIOS constants.

| Constant | Initial Value | Purpose |
|---|---|---|
| `SC_RETRY_MS` | `500` | Resend interval while waiting for switch ack |
| `SC_LIGHT_TIMEOUT_MS` | `3000` | Max wait for light to clear after switch confirms |

Both values are conservative placeholders. The timing display data (collected over real DCS sessions) will reveal the typical SW and LT durations. Once a baseline is established, `SC_LIGHT_TIMEOUT_MS` should be set to roughly 2× the maximum observed LT value.

---

## UI: Timing Display

New function `UI::showStoresConfigTiming(uint32_t swMs, uint32_t ltMs)` added to `ui.cpp` / `ui.h`.

Layout on 128×32 OLED:
```
SW:  312ms
LT:  445ms
```

- Line 1 at y=14: `"SW: %lums"` formatted from `swMs`
- Line 2 at y=30: `"LT: %lums"` formatted from `ltMs`
- Font: `u8g2_font_9x15B_tr` (matches other takeover screens; max expected value ~3000ms = 9 chars, fits at 9px/char)
- No flashing — static display for the full 4 seconds
- `setDrawColor` restored to 1 before `sendBuffer()` (standard pattern)

---

## Failure Handling

| Condition | Behavior |
|---|---|
| Switch never acks (UDP loss) | Resend every `SC_RETRY_MS` until switch confirms or stream drops |
| Switch acks, light stays on > `SC_LIGHT_TIMEOUT_MS` | Enter `SC_GAVE_UP`; bypass Stores Config branch, show standard MASTER CAUTION; player can short-press to reset MC |
| Stream drops at any point | Existing 3s `isConnected()` gate forces `mc = false`, `mcConfirmed` becomes false, all SC state resets |
| MC clears during `SC_SHOW_TIMING` | Timing display continues until 4s timer expires (state checked before `mcConfirmed` gate), then loop returns to normal |

---

## Variables Added to `main.cpp`

| Variable | Type | Purpose |
|---|---|---|
| `s_scState` | `uint8_t` (0–3) or enum | Current sub-state |
| `s_scTarget` | `uint8_t` | Locked target switch position |
| `s_scTPress` | `unsigned long` | `millis()` when command first sent |
| `s_scTLastSend` | `unsigned long` | `millis()` of most recent send (for T1) |
| `s_scSwMs` | `uint32_t` | Elapsed ms when switch confirmed |
| `s_scLtMs` | `uint32_t` | Elapsed ms when light cleared |
| `s_scShowUntil` | `unsigned long` | `millis()` deadline for timing display |

---

## Out of Scope

- No change to RWR missile launch behavior
- No change to standard MC reset behavior
- No NVS persistence of timing samples
- No per-session averaging or history
- `SC_RETRY_MS` and `SC_LIGHT_TIMEOUT_MS` are compile-time constants, not NVS-adjustable
