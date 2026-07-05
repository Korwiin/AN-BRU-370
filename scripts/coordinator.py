#!/usr/bin/env python3
"""
Brew370 WiFi Lockout Coordinator

Opens both USB serial ports (ANBRU-370RD dev shell + Sham_Master_100 sniffer),
runs the Eero lockout experiment, and logs all observations with wall-clock
timestamps. Zero interpretation — raw evidence stream for offline analysis.

Usage:
    python3 coordinator.py                       # auto-discover both ports
    python3 coordinator.py -s /dev/cu.X -d /dev/cu.Y
    python3 coordinator.py -r 1                  # one repetition
    python3 coordinator.py --probe-only          # sniffer-only, no experiment

Run with PlatformIO's Python: ~/.platformio/penv/bin/python3 coordinator.py

Log file: coordinator_YYYYMMDD_HHMMSS.log (same directory as this script)
"""
import argparse, glob, sys, time, os
import serial
from datetime import datetime

BAUD        = 115200
SETTLE_S    = 4.5   # seconds after opening dev-shell port before first command
CMD_TIMEOUT = 20    # seconds to wait for shell #ok/#err
CONN_WAIT_S = 3     # seconds to wait after wifi conn full — success takes ~1s, failure <1s
PROBE_GAPS  = [30, 60, 120, 240, 300]   # seconds between recovery probes
MAX_CYCLES  = 20    # stop provoking after this many cycles without lockout


def ts() -> str:
    return datetime.now().isoformat(timespec='milliseconds')


class Coordinator:

    def __init__(self, shell_port: str, sniffer_port: str, logpath: str):
        self._lf = open(logpath, 'w', buffering=1)
        self._sb = b''  # shell readline buffer (partial lines)
        self.log('COORD', f'shell={shell_port} sniffer={sniffer_port} log={logpath}')

        # ── sniffer (ESP32-C3) ──
        self.log('COORD', 'opening sniffer (board resets on port open, settling 2s)')
        self._sniff = serial.Serial(sniffer_port, BAUD, timeout=0)
        time.sleep(2.0)
        self._log_port(self._sniff, 'SNIFFER', label='boot')

        # ── dev shell (ESP32-S3) ──
        self.log('COORD', f'opening dev-shell (board resets on port open, settling {SETTLE_S}s)')
        self._shell = serial.Serial(shell_port, BAUD, timeout=0)
        self._idle(SETTLE_S)   # drain sniffer throughout settle period
        self._log_port(self._shell, 'SHELL', label='boot')

    # ── logging ───────────────────────────────────────────────────────────────

    def log(self, src: str, msg: str):
        entry = f'{ts()} [{src:7s}] {msg}'
        print(entry)
        self._lf.write(entry + '\n')

    # ── internal I/O helpers ──────────────────────────────────────────────────

    def _log_port(self, ser: serial.Serial, src: str, label: str = ''):
        """Read all pending bytes from ser and log each non-empty line."""
        data = ser.read(2048)
        if not data:
            return
        for line in data.decode(errors='replace').splitlines():
            line = line.strip()
            if line:
                self.log(src, f'({label}) {line}' if label else line)
        if src == 'SHELL':
            self._sb = b''  # discard any partial buffer accumulated during settle

    # Known MACs — set after channel sync, used to flag direction in monitor mode
    _ap_mac:  str = ''
    _sta_mac: str = ''

    def _drain_sniffer(self):
        data = self._sniff.read(4096)
        if not data:
            return
        for line in data.decode(errors='replace').splitlines():
            line = line.strip()
            if not line:
                continue
            self.log('SNIFFER', line)
            # Flag any AP-initiated disconnect — the key diagnostic event
            if '#frame' in line and self._ap_mac:
                if (('DEAUTH' in line or 'DISASSOC' in line)
                        and f'src={self._ap_mac}' in line):
                    rsn = ''
                    for part in line.split():
                        if part.startswith('rsn='):
                            rsn = part
                    self.log('COORD', f'*** AP-INITIATED {("DEAUTH" if "DEAUTH" in line else "DISASSOC")} {rsn} ***')

    def sniff_cmd(self, cmd: str):
        """Send a command to the sniffer and log its response."""
        self.log('SNIFFER', f'>> {cmd}')
        self._sniff.write((cmd + '\n').encode())
        time.sleep(0.3)
        self._drain_sniffer()

    def _idle(self, seconds: float):
        """Wait while continuously draining the sniffer."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            self._drain_sniffer()
            time.sleep(0.05)

    # ── dev-shell command interface ───────────────────────────────────────────

    def shell(self, cmd: str, timeout: float = CMD_TIMEOUT) -> list:
        """
        Send cmd to the dev shell; drain sniffer while waiting for #ok/#err.
        Returns all shell response lines including the terminal #ok/#err.
        """
        self.log('SHELL', f'>> {cmd}')
        self._shell.write((cmd + '\n').encode())
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            self._drain_sniffer()
            chunk = self._shell.read(512)
            if chunk:
                self._sb += chunk
                while b'\n' in self._sb:
                    raw, self._sb = self._sb.split(b'\n', 1)
                    line = raw.decode(errors='replace').strip()
                    if line:
                        self.log('SHELL', line)
                        lines.append(line)
                        if line.startswith('#ok') or line.startswith('#err'):
                            return lines
            else:
                time.sleep(0.02)
        self.log('COORD', f'TIMEOUT waiting for response to: {cmd!r}')
        return lines

    # ── channel sync ─────────────────────────────────────────────────────────

    def _sync_sniffer_channel(self) -> int:
        """
        Connect to AP briefly, read the actual channel from wifi?,
        configure the sniffer to match, then disconnect.
        Returns the channel number, or 0 if the connect failed.
        """
        self.log('COORD', 'channel sync: connecting to read AP channel')
        self.shell('wifi conn full', timeout=5)
        self._idle(10)  # channel sync gets a full 10s — only runs once
        connected, reason = self._wifi_status()
        if not connected:
            self.log('COORD', f'channel sync: connect failed reason={reason} — sniffer channel unchanged')
            return 0
        channel = 0
        lines = self.shell('wifi?', timeout=5)
        for l in lines:
            for part in l.split():
                if part.startswith('ch='):
                    try:
                        channel = int(part[3:])
                    except ValueError:
                        pass
        for l in lines:
            for part in l.split():
                if part.startswith('bssid=') and len(part) == 23:
                    self._ap_mac = part[6:]
                if part.startswith('ip=') and part != 'ip=0.0.0.0':
                    pass  # not useful here
        # STA MAC appears in wifi? as the src of DISASSOC on next wifi off;
        # capture it from the first sniffer DISASSOC frame instead (see _drain_sniffer)
        if channel:
            self.log('COORD', f'channel sync: AP ch={channel} ap_mac={self._ap_mac}')
            self.sniff_cmd(f'ch {channel}')
        else:
            self.log('COORD', 'channel sync: could not parse channel from wifi? — sniffer channel unchanged')
        self.shell('wifi off')
        self._idle(2)
        # STA MAC is visible as src in the DISASSOC frames just emitted
        return channel

    # ── experiment primitives ─────────────────────────────────────────────────

    def _wifi_status(self) -> tuple:
        """Return (connected: bool, last_reason: str) from wifi? snapshot."""
        lines = self.shell('wifi?', timeout=5)
        connected = any('status=3 ' in l or l.endswith('status=3') for l in lines)
        reason = next(
            (l.split('lastReason=')[-1].strip().split()[0]
             for l in lines if 'lastReason=' in l),
            '')
        return connected, reason

    def _connect_attempt(self) -> tuple:
        """
        Issue wifi conn full, wait CONN_WAIT_S seconds (sniffer drained throughout),
        check wifi?, read wifi log? for the record.
        Returns (connected: bool, last_reason: str).
        """
        resp = self.shell('wifi conn full', timeout=5)
        if any('no credentials' in l for l in resp):
            raise RuntimeError('Dev shell has no WiFi credentials — run wifi boot manually first')
        self._idle(CONN_WAIT_S)
        connected, reason = self._wifi_status()
        self.shell('wifi log?', timeout=5)   # captured in log; not parsed here
        return connected, reason

    # ── monitor mode ─────────────────────────────────────────────────────────

    def monitor(self, poll_s: int = 60):
        """
        Production-observation mode.

        1. Channel sync (connect once to get AP channel, disconnect).
        2. Run wifi boot on the dev shell — puts the ESP32 in the same WiFi
           state as production firmware (blocking connect, auto-reconnect on).
        3. Log all sniffer frames indefinitely.  Any AP-initiated DEAUTH or
           DISASSOC is flagged with *** so it stands out in the log.
        4. Poll wifi log? every poll_s seconds to capture the ESP32-side event
           ring alongside the wire-level evidence.
        5. Ctrl-C to stop.

        Key diagnostic:
          AP-initiated DEAUTH  src=<Eero> — AP kicked the client; note rsn= code
          AP-initiated DISASSOC src=<Eero> — same, softer form
          Client DISASSOC      src=<ANBRU> — firmware called disconnect
          Silence → AUTH seq=1 — reconnect attempt after timeout / driver reset
        """
        self.log('COORD', f'=== MONITOR MODE poll_interval={poll_s}s ===')

        # Channel sync also populates self._ap_mac
        self._sync_sniffer_channel()

        # Put dev board into production WiFi state
        self.log('COORD', 'wifi boot — entering production WiFi state (blocking up to 65s)')
        resp = self.shell('wifi boot', timeout=70)
        if any('no credentials' in l for l in resp):
            raise RuntimeError('No WiFi credentials in NVS — configure via BLE first')
        connected, _ = self._wifi_status()
        if not connected:
            self.log('COORD', 'WARNING: wifi boot completed but not connected — monitoring anyway')
        else:
            self.log('COORD', 'Connected. Monitoring wire traffic — Ctrl-C to stop.')

        last_poll = time.time()
        try:
            while True:
                self._drain_sniffer()
                if time.time() - last_poll >= poll_s:
                    self.shell('wifi log?', timeout=5)
                    last_poll = time.time()
                time.sleep(0.05)
        except KeyboardInterrupt:
            self.log('COORD', 'monitor stopped — final state:')
            self.shell('wifi?', timeout=5)
            self.shell('wifi log?', timeout=5)

    # ── lockout experiment ────────────────────────────────────────────────────

    def run(self, reps: int = 3, probe_only: bool = False) -> list:
        """
        Lockout experiment: rapid connect/disconnect cycles to provoke the
        Eero rate limiter, then probe recovery timing.
        """
        self.log('COORD', f'=== LOCKOUT EXPERIMENT reps={reps} probe_only={probe_only} ===')

        if probe_only:
            self.log('COORD', 'probe_only mode: logging sniffer passively — Ctrl-C to stop')
            try:
                while True:
                    self._drain_sniffer()
                    time.sleep(0.05)
            except KeyboardInterrupt:
                self.log('COORD', 'stopped')
            return []

        # Sanity-check: dev shell is alive
        resp = self.shell('ping', timeout=5)
        if not any('#pong' in l for l in resp):
            raise RuntimeError('Dev shell not responding to ping')

        # Sync sniffer to actual AP channel before any experiment traffic
        self._sync_sniffer_channel()

        results = []

        for rep in range(1, reps + 1):
            self.log('COORD', f'--- rep {rep}/{reps} ---')
            self.shell('wifi auto off')
            self.shell('wifi off')
            self._idle(1)

            # ── Phase 1: provoke lockout ──────────────────────────────────────
            lockout_cycle = None
            t_lockout     = None
            fail_reason   = None

            for cycle in range(1, MAX_CYCLES + 1):
                self.log('COORD', f'cycle {cycle}')
                connected, reason = self._connect_attempt()

                if connected:
                    self.log('COORD', f'cycle {cycle}: CONNECTED')
                    self.shell('wifi off')
                    self._idle(0.5)
                else:
                    self.log('COORD', f'cycle {cycle}: FAILED reason={reason!r}')
                    lockout_cycle = cycle
                    t_lockout     = time.time()
                    fail_reason   = reason
                    break

            if lockout_cycle is None:
                self.log('COORD', f'rep {rep}: lockout NOT triggered in {MAX_CYCLES} cycles')
                results.append({'rep': rep, 'lockout_cycle': None,
                                'fail_reason': None, 'recovery_s': None})
                continue

            self.log('COORD',
                     f'rep {rep}: lockout at cycle={lockout_cycle} reason={fail_reason!r}')

            # ── Phase 2: probe recovery ───────────────────────────────────────
            recovery_s = None
            for gap in PROBE_GAPS:
                self.log('COORD', f'waiting {gap}s before probe')
                self._idle(gap)
                self.log('COORD', 'probe')
                connected, reason = self._connect_attempt()
                elapsed = time.time() - t_lockout

                if connected:
                    self.log('COORD', f'RECOVERED after {elapsed:.0f}s total')
                    self.shell('wifi off')
                    self._idle(0.5)
                    recovery_s = elapsed
                    break
                else:
                    self.log('COORD',
                             f'still locked at {elapsed:.0f}s reason={reason!r}')

            if recovery_s is None:
                self.log('COORD', f'rep {rep}: not recovered after all probes')

            results.append({
                'rep':           rep,
                'lockout_cycle': lockout_cycle,
                'fail_reason':   fail_reason,
                'recovery_s':    recovery_s,
            })

            if rep < reps:
                self.log('COORD', 'cooling down 90s before next rep')
                self._idle(90)

        # ── summary ───────────────────────────────────────────────────────────
        self.log('COORD', '=== RESULTS ===')
        for r in results:
            if r['lockout_cycle']:
                rec = f"{r['recovery_s']:.0f}s" if r['recovery_s'] else 'not recovered'
                self.log('COORD',
                         f"rep={r['rep']} lockout_cycle={r['lockout_cycle']} "
                         f"reason={r['fail_reason']!r} recovery={rec}")
            else:
                self.log('COORD', f"rep={r['rep']} no lockout")

        return results

    def close(self):
        self._drain_sniffer()
        for closeable in (self._shell, self._sniff, self._lf):
            try:
                closeable.close()
            except Exception:
                pass


# ── port discovery ─────────────────────────────────────────────────────────────

def identify_ports() -> tuple:
    """
    Open each /dev/cu.usbmodem* port sequentially, wait for the boot banner,
    and identify by content:
      - '#sniffer' in banner → Sham_Master_100 sniffer (ESP32-C3)
      - '#shell ready'       → ANBRU-370RD dev shell (ESP32-S3)
    Returns (shell_port, sniffer_port).
    """
    ports = sorted(glob.glob('/dev/cu.usbmodem*'))
    print(f'Found {len(ports)} usbmodem port(s): {ports}')
    if len(ports) < 2:
        sys.exit(
            f'Need 2 usbmodem ports, found {len(ports)}. '
            'Connect both devices and retry, or specify with -s/-d.')

    identified = {}
    for port in ports:
        print(f'Probing {port} (opening + {SETTLE_S}s settle)...', flush=True)
        try:
            s = serial.Serial(port, BAUD, timeout=0)
            time.sleep(SETTLE_S)
            data = b''
            deadline = time.time() + 1.5
            while time.time() < deadline:
                chunk = s.read(512)
                if chunk:
                    data += chunk
                else:
                    time.sleep(0.05)
            s.close()
            text = data.decode(errors='replace')
            print(f'  banner: {text[:200].strip()!r}')
            if '#sniffer' in text:
                identified['sniffer'] = port
                print('  → SNIFFER')
            elif '#shell ready' in text:
                identified['shell'] = port
                print('  → SHELL')
            else:
                print('  → unrecognized banner')
        except Exception as e:
            print(f'  error: {e}')

    shell   = identified.get('shell')
    sniffer = identified.get('sniffer')
    if not shell or not sniffer:
        sys.exit(
            f'Could not identify both ports. Got: {identified}\n'
            'Use -s SNIFFER -d DEV to specify manually.')
    return shell, sniffer


# ── entry point ────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description='Brew370 WiFi lockout coordinator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    ap.add_argument('-s', '--sniffer', metavar='PORT',
                    help='Sniffer serial port (Sham_Master_100 ESP32-C3)')
    ap.add_argument('-d', '--dev', metavar='PORT',
                    help='Dev-shell serial port (ANBRU-370RD ESP32-S3)')
    ap.add_argument('-r', '--reps', type=int, default=3, metavar='N',
                    help='Experiment repetitions (default 3)')
    ap.add_argument('--monitor', action='store_true',
                    help='Monitor mode: wifi boot then observe wire traffic (Ctrl-C to stop)')
    ap.add_argument('--poll', type=int, default=60, metavar='S',
                    help='wifi log? poll interval in monitor mode, seconds (default 60)')
    ap.add_argument('--probe-only', action='store_true',
                    help='Sniffer-only: no dev-shell interaction, just log frames')
    ap.add_argument('-o', '--output', metavar='FILE',
                    help='Log file (default: coordinator_TIMESTAMP.log in scripts/)')
    args = ap.parse_args()

    shell_port   = args.dev
    sniffer_port = args.sniffer

    if not shell_port or not sniffer_port:
        shell_port, sniffer_port = identify_ports()

    logpath = args.output or os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        f'coordinator_{datetime.now().strftime("%Y%m%d_%H%M%S")}.log')

    coord = Coordinator(shell_port, sniffer_port, logpath)
    try:
        if args.monitor:
            coord.monitor(poll_s=args.poll)
        else:
            coord.run(reps=args.reps, probe_only=args.probe_only)
    except KeyboardInterrupt:
        coord.log('COORD', 'interrupted by user')
    except Exception as e:
        coord.log('COORD', f'FATAL: {e}')
        raise
    finally:
        coord.close()

    print(f'\nLog: {logpath}')


if __name__ == '__main__':
    main()
