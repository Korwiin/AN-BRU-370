# Brew370 Dev Test Shell

A serial command shell built only into `esp32s3_dev` (`DEV_BUILD` defined). It lets a
host script (or a human with a serial monitor) drive the firmware — inject encoder
events, query menu state, dump the OLED framebuffer, control WiFi, and observe HID
macro output — without a person turning a knob or a mouse actually moving on the PC.
`esp32s3_supermini_release` never compiles this code; `Shell::poll()` is an inline
no-op there (same pattern as `include/hid.h`'s production/dev split).

Source: `include/shell.h`, `src/shell.cpp` (all inside `#ifdef DEV_BUILD`). Host client:
`scripts/devshell.py`.

## Protocol

- Line-based, over the native-USB CDC serial port (HWCDC), 115200 baud.
- Every response line starts with `#`.
- Every command ends with exactly one `#ok` or `#err <reason>` line.
- `#hid ...` lines (HID intent logging, see below) are **asynchronous telemetry** —
  they have no `#ok` of their own and can arrive interleaved with, or after, the
  `#ok` of the command that triggered them (e.g. a macro fired by `enc sp`). Don't
  assume one `#ok` per logical action; only rely on `#ok`/`#err` as the terminator
  for the literal command just sent.
- One command in flight at a time: if a command is sent while another is still
  executing (e.g. `wifi boot`, which blocks), the shell replies `#err busy` rather
  than queuing or interleaving dispatch.

## Boot difference from production

In `DEV_BUILD`, `setup()` does **not** run the blocking WiFi connect sequence. It
prints `#boot shell-ready, wifi idle` and returns immediately — the board comes up
with WiFi off and sits in `WAITING_DCS`. This is intentional: WiFi becomes
shell-initiated (`wifi conn`, `wifi on`, etc.) so tests can control exactly when the
radio turns on.

To exercise the exact production boot sequence (`WifiMgr::beginConnect(true)` +
`connectWifi()`, including its OLED screens and blocking encoder-cancel behavior),
send `wifi boot` — it replays that sequence verbatim via a hook installed in
`main.cpp`'s `Shell::Hooks`.

```
$ ~/.platformio/penv/bin/python scripts/devshell.py "state?"
#shell ready (type 'help')
#state WAITING_DCS id=0 sel=0 wifi=0 dcs=0
#ok
```

`wifi=0` at boot confirms no auto-connect happened.

## Command Table

| Command | Effect | Response |
|---|---|---|
| `ping` | liveness check | `#pong` / `#ok` |
| `help` | list commands | 3×`#...` lines / `#ok` |
| `state?` | current UI/connectivity state | `#state <name> id=<n> sel=<n> wifi=<0\|1> dcs=<0\|1>` / `#ok` |
| `enc <±n>` | inject `n` rotation steps (`-100..100`, nonzero) | `#ok` or `#err usage: enc <±n>\|sp\|lp` |
| `enc sp` | inject a short button press | `#ok` |
| `enc lp` | inject a long button press | `#ok` |
| `fb?` | dump the 512-byte OLED framebuffer as hex | 8×`#fb <offset> <128 hex chars>` / `#ok` |
| `wifi?` | one-line WiFi status snapshot | `#wifi status=... lastReason=<n>:<name>` / `#ok` |
| `wifi log?` | dump the 32-entry WiFi event ring | N×`#evt <ms> <name> [reason=<n>:<name>]` / `#ok` |
| `wifi on` | radio on (`WIFI_STA` mode), no connect attempt | `#ok` |
| `wifi off` | disconnect + radio off | `#ok` |
| `wifi conn` / `wifi conn full` | full connect (`WifiMgr::beginConnect(true)`) | `#ok` or `#err no credentials in NVS` |
| `wifi conn soft` | soft connect (`WifiMgr::beginConnect(false)`) | `#ok` or `#err no credentials in NVS` |
| `wifi auto on` / `wifi auto off` | toggle WiFi auto-reconnect | `#ok` |
| `wifi boot` | replay the production blocking boot-connect sequence | `#wifi running production boot sequence (blocking)` then `#wifi boot done, connected=<0\|1>` / `#ok` |

**Note on `wifi boot`:** If the AP is unreachable, `wifi boot` blocks indefinitely (production behavior: retries until physical SP/LP input); wire commands get `#err busy` and `enc` injection cannot escape it — recover by re-opening the serial port, which resets the board.

| `wifi scan` | blocking AP scan (~2-3 s) | N×`#ap "<ssid>" <bssid> ch=<n> rssi=<n> auth=<n>` then `#scan <n> networks` / `#ok` |

Unknown top-level verb → `#err unknown cmd '<verb>'`. Unknown `wifi` subcommand →
`#err unknown wifi subcmd '<rest>'`.

### `ping` / `help` (family: command pump)

```
$ ~/.platformio/penv/bin/python scripts/devshell.py "ping"
#shell ready (type 'help')
#pong
#ok

$ ~/.platformio/penv/bin/python scripts/devshell.py "help"
#shell ready (type 'help')
#ping|help|state?|enc <n>|enc sp|enc lp|fb?
#wifi?|wifi log?|wifi on|wifi off|wifi conn [full|soft]
#wifi auto on|off|wifi boot|wifi scan
#ok
```
(Task 1 report.)

### `state?`

```
$ sleep 3 && ~/.platformio/penv/bin/python scripts/devshell.py "state?"
#shell ready (type 'help')
#state WAITING_DCS id=0 sel=0 wifi=0 dcs=0
#ok
```
(Task 2 report.)

### `enc` (menu navigation by wire)

```
$ ~/.platformio/penv/bin/python scripts/devshell.py "state?" "enc lp" "state?" "enc -2" "state?" "enc sp" "state?" "enc lp" "state?"

>> state?
#state WAITING_DCS id=0 sel=0 wifi=0 dcs=0
#ok
>> enc lp
#ok
>> state?
#state SETTINGS id=5 sel=0 wifi=0 dcs=0
#ok
>> enc -2
#ok
>> state?
#state SETTINGS id=5 sel=2 wifi=0 dcs=0
#ok
>> enc sp
#ok
>> state?
#state SLEEP_ADJUST id=7 sel=2 wifi=0 dcs=0
#ok
>> enc lp
#ok
>> state?
#state SETTINGS id=5 sel=2 wifi=0 dcs=0
#ok
```
(Task 4 report — note the `enc -2`, not `enc 2`; see "Persisted knob direction" below.)

### `fb?` (framebuffer dump)

```
$ ~/.platformio/penv/bin/python scripts/devshell.py --fb
                    █  █        █   █     █                   █                 ███   ██   ██
                    █  █            █                        █ █                █  █ █  █ █  █
                    █  █  ███  ██  ███   ██  ███   ███       █    ██  ███       █  █ █     █
                    ████ █  █   █   █     █  █  █ █  █      ███  █  █ █  █      █  █ █      █
                    ████ █ ██   █   █     █  █  █  ██        █   █  █ █         █  █ █  █ █  █  ██   ██   ██
                    █  █  █ █  ███   ██  ███ █  █ █          █    ██  █         ███   ██   ██   ██   ██   ██
                                                   ███
   (blank rows)
                                █    ███                  ██        █    █     █
                                █    █  █                █  █       █    █
                                █    █  █      ████       █    ██  ███  ███   ██  ███   ███  ███
                                █    ███                   █  █ ██  █    █     █  █  █ █  █ ██
                                █    █         ████      █  █ ██    █    █     █  █  █  ██    ██
                                ████ █                    ██   ██    ██   ██  ███ █  █ █    ███
```
"Waiting for DCS..." / "LP=Settings", rendered from the raw 512-byte U8g2 page buffer
(128×32, page-major). `--fb` can follow commands in the same session:
`devshell.py "enc lp" --fb` renders the Settings menu after navigating to it.
(Task 5 report.)

### `wifi` family

```
=== wifi? ===
#wifi status=255 ssid= bssid= ch=0 rssi=0 ip=0.0.0.0 auto=1 lastReason=0:?
#ok
=== wifi scan ===
#ap "SO10322" D4:3F:32:AF:95:26 ch=6 rssi=-48 auth=7
...
#scan 12 networks
#ok
=== wifi auto off ===
#ok
=== wifi conn ===
#ok
=== sleep 8 ===
=== wifi? (after 8s) ===
#wifi status=3 ssid=SO10322 bssid=D4:3F:32:AF:95:26 ch=6 rssi=-52 ip=192.168.1.57 auto=0 lastReason=0:?
#ok
=== wifi log? ===
#evt 827 ?
#evt 861 STA_START
#evt 7669 ?
#evt 7699 STA_STOP
#evt 7812 ?
#evt 7814 STA_START
#evt 8792 CONNECTED
#evt 8830 GOT_IP
#ok
=== wifi off ===
#ok
=== wifi log? (after off) ===
...
#evt 15843 DISCONNECTED reason=8:ASSOC_LEAVE
#evt 15844 STA_STOP
#ok
```
(Task 3 report — single persistent serial session; see "one session" note below.)

Observed disconnect reason on `wifi off`: `8:ASSOC_LEAVE`. The two `?`-named events
above are `ARDUINO_EVENT_WIFI_READY` (0) and `ARDUINO_EVENT_WIFI_SCAN_DONE` (1) — see
"Known cosmetic limitations" below.

### WiFi reason-code table (`wifiReasonName()`, `src/shell.cpp`)

Verified against `esp_wifi_types.h` (`wifi_err_reason_t`) in the installed ESP-IDF SDK.

| Code | Name |
|---|---|
| 1 | `UNSPECIFIED` |
| 2 | `AUTH_EXPIRE` |
| 3 | `AUTH_LEAVE` |
| 4 | `ASSOC_EXPIRE` |
| 5 | `ASSOC_TOOMANY` |
| 6 | `NOT_AUTHED` |
| 7 | `NOT_ASSOCED` |
| 8 | `ASSOC_LEAVE` |
| 15 | `4WAY_HANDSHAKE_TIMEOUT` |
| 200 | `BEACON_TIMEOUT` |
| 201 | `NO_AP_FOUND` |
| 202 | `AUTH_FAIL` |
| 203 | `ASSOC_FAIL` |
| 204 | `HANDSHAKE_TIMEOUT` |
| 205 | `CONNECTION_FAIL` |
| (any other) | `?` |

`wifi?`'s `lastReason` field shows the reason of the **most recent disconnect** and is
deliberately **not cleared on reconnect** — after a drop followed by an auto-reconnect,
`wifi?` still reports why the last drop happened. This is forensics-oriented by design,
not a bug: it lets you ask "why did it last drop" at any point in time, independent of
current connection state.

### HID intent logging (macro observation, no physical mouse/keyboard)

In `DEV_BUILD`, `include/hid.h` / `src/hid.cpp`'s dev half never touches USB HID
(USB HID and CDC Serial cannot coexist on this board's TinyUSB stack — see project
memory `feedback_no_serial_with_hid.md`). Instead every HID call logs a `#hid ...`
line and `HID::isReady()` returns `true`, so macro flows execute in full and are
observable purely from the serial stream.

```
>> enc sp
#ok
>> state?
#state MACRO_MENU id=4 sel=0 wifi=1 dcs=1
#ok
>> enc sp
#ok
>> state?
#hid kb releaseAll
#hid kb press 0x80
#hid kb press 0xCB
#hid kb releaseAll
#hid moveAbs 1147,17
#hid click 0x01
#hid moveAbs 2560,720
#hid click 0x01
#hid moveAbs 2607,771
#hid click 0x01
#hid kb releaseAll
#hid typeText "magic11"
#hid moveRel -60,0
#hid click 0x01
#hid pressKey 0xC2
#state MACRO_MENU id=4 sel=0 wifi=1 dcs=1
#ok
```
(Task 6 report — the `#hid` lines from the macro fired by the first `enc sp` arrived
folded into the *next* command's response, not their own `#ok`-terminated block. This
is normal: `#hid` is async telemetry, not a command reply.)

Note: reaching `MACRO_MENU` requires `DcsBios::isConnected()` **and** the MWS flag set
(`DcsBios::mwsOn()`) — see "MACRO_MENU is gated" below for the recipe used above.

## `scripts/devshell.py` Usage

Run with PlatformIO's bundled Python (has `pyserial`):

```bash
~/.platformio/penv/bin/python scripts/devshell.py "wifi?"
~/.platformio/penv/bin/python scripts/devshell.py --fb
~/.platformio/penv/bin/python scripts/devshell.py "enc lp" --fb
```

- Auto-discovers the port (`glob /dev/cu.usbmodem*`), or pass `-p <port>`.
- **Opening the serial port resets the board** (native USB port-open side effect on
  this ESP32-S3). The script sleeps 4 s after `ser.open()` to let boot/settle finish
  before sending the first command — this is required, not cosmetic (an earlier bench
  run without it got the `#shell ready` boot banner folded into the first command's
  response).
- Because of the reset-on-open behavior, **any stateful sequence must be sent as one
  invocation**: `devshell.py "cmd1" "cmd2" "cmd3" ...` runs all commands over a single
  open session, printing `>> <cmd>` before each command's responses. Two separate
  `devshell.py` calls do NOT share state — the board reboots between them.
- `--fb` can be combined after one or more commands (`devshell.py "enc lp" --fb`) to
  navigate first, then render the resulting screen, all in the same session.
- `--fb` alone (no commands) just fetches and renders the current framebuffer.

## Bench Realities / Gotchas

**Persisted knob direction negates injected deltas.** This board's `s_encReversed`
flag (NVS key `encrev`, toggled by the Settings menu's "Knob" item) flips the sign of
**every** delta passed to the menu state machine — including deltas injected via
`enc <n>`, since injection deliberately shares the same `readDelta()` path as
hardware input. A bench run sending `enc 2` to scroll down 2 items landed on
`sel=7` ("Reboot") instead of the intended `sel=2` ("LCD Sleep") because this board's
`encrev` was persisted `true`. The subsequent `enc sp` would have fired
`rebootWithCountdown()` — a bench run came within one command of an unintended
reboot. **Before scripting a menu-navigation sequence:** check the Settings screen's
"Knob" item (or infer direction empirically via a `state?`-observed single-step probe)
and negate your `enc` argument if the board is in reversed mode.

**MACRO_MENU is gated behind DCS + MWS.** `enc sp` from `WAITING_DCS` does **not**
reach `MACRO_MENU` directly — `WAITING_DCS`'s short-press only triggers `SETTINGS` on
long-press; `MACRO_MENU` is reachable only from `AIRCRAFT_STATUS`, which itself
requires `DcsBios::isConnected()` **and** `DcsBios::mwsOn()` (MWS_SW flag set),
otherwise the state machine routes through `NOT_READY` → `SETUP_RUNNING` instead.
With no live DCS session on the bench, Task 6's working recipe was to connect WiFi via
the shell, then inject one real DCS-BIOS UDP multicast export frame by hand to set
`MWS_SW`:

```
connect: wifi conn (then wait for GOT_IP)
send one UDP datagram to 239.255.50.10:5010 (DCSBIOS_MCAST_ADDR / DCSBIOS_MCAST_PORT, config.h):
  4x 0x55            sync
  addr = 0x445A LE    (DCSBIOS_ADDR_MWS_SW)
  len  = 0x0002 LE
  data = 0x1000 LE    (DCSBIOS_MASK_MWS_SW — sets MWS_SW=1)
```
i.e. the raw bytes `55 55 55 55 5A 44 02 00 00 10`. This is the actual DCS-BIOS wire
protocol (no firmware changes) and gets the board legitimately to `AIRCRAFT_STATUS`
with `mwsOn()` satisfied, from which `enc sp` reaches `MACRO_MENU` as documented above.

**`wifi log?` reason/event decoding is best-effort.** `WIFI_READY` (event code 0) and
`SCAN_DONE` (event code 1) are not covered by `wifiEvtName()`'s switch and decode as
`?` — this is the switch's designed fallthrough, not a bug, and is cosmetic (the
timestamp and ordering are still correct; only the name is unresolved).

**`#hid` lines are asynchronous.** They have no `#ok` of their own and can interleave
with or trail the response of the command that triggered them (see the HID example
above). A capture script waiting strictly for one `#ok` per command may need a
trailing no-op command (e.g. `state?`) to flush pending `#hid` lines into view.

## Eero-Lockout Experiment Recipe

Background: this network's Eero router has, in past sessions, imposed a temporary
reconnect lockout after repeated disconnect/reconnect cycles. The shell's WiFi event
ring plus reason-code decoding make it possible to characterize this by wire without
a person watching the OLED.

```
1. wifi auto off
   Firmware's own auto-reconnect must not fight the experiment's manual conn/off cycle.

2. Provoke: repeat "wifi conn" → wait a few seconds for GOT_IP → "wifi off", watching
   "wifi log?" after each cycle for the disconnect reason code. Normal reason on a
   clean `wifi off` is 8:ASSOC_LEAVE (client-initiated). Keep cycling.

3. Detect lockout onset: watch for a reason code change on connect attempts — e.g.
   `AUTH_FAIL` (202) or `AUTH_EXPIRE` (2) appearing where `CONNECTED`/`GOT_IP` used to
   follow a `wifi conn`. When this first appears, record T0 = the `#evt <ms>` timestamp
   of that ring entry (or wall-clock time when it was observed, cross-referenced against
   the millis()-based ring).

4. Probe for blackout end: send `wifi conn` at increasing intervals (30 s, 60 s, 120 s,
   240 s) after T0. The first attempt whose `wifi log?` shows `GOT_IP` marks the end of
   the blackout.

5. Blackout duration = (probe success timestamp) − T0. Repeat the full cycle 3 times for
   consistency before treating a duration as characteristic rather than one-off noise.
```

Each step's commands must be issued within one `devshell.py` invocation per
step-group where state must persist (e.g. the provoke loop in step 2, or a given probe
attempt in step 4) — remember the port-open reset means separate invocations do not
share WiFi/ring state until `wifi conn`/`wifi off` re-establish it.
