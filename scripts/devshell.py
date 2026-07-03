#!/usr/bin/env python3
"""Brew370 dev-shell client. Usage:
  devshell.py [-p PORT] "wifi?"           # one command, print # responses
  devshell.py [-p PORT] --fb              # fetch fb? and render 128x32 ASCII
Run with PlatformIO's python (has pyserial): ~/.platformio/penv/bin/python
"""
import argparse, glob, sys, time
import serial

def find_port():
    ports = glob.glob("/dev/cu.usbmodem*")
    if not ports:
        sys.exit("no /dev/cu.usbmodem* port found")
    return ports[0]

def send(ser, cmd, timeout=10.0):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode())
    lines, deadline = [], time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline().decode(errors="replace").rstrip()
        if not raw:
            continue
        if raw.startswith("#"):
            lines.append(raw)
            if raw == "#ok" or raw.startswith("#err"):
                return lines
    lines.append("#err client timeout")
    return lines

def render_fb(lines):
    hexdata = "".join(l.split(" ", 2)[2] for l in lines if l.startswith("#fb "))
    buf = bytes.fromhex(hexdata)
    if len(buf) != 512:
        sys.exit(f"expected 512 fb bytes, got {len(buf)}")
    for y in range(32):
        row = "".join(
            "█" if (buf[(y // 8) * 128 + x] >> (y % 8)) & 1 else " "
            for x in range(128))
        print(row)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default=None)
    ap.add_argument("--fb", action="store_true")
    ap.add_argument("cmd", nargs="?", default=None)
    args = ap.parse_args()
    port = args.port or find_port()
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, 115200, 0.25
    ser.dtr = False  # match monitor_dtr=0 / monitor_rts=0 — do not reset the board
    ser.rts = False
    ser.open()
    if args.fb:
        render_fb(send(ser, "fb?"))
    elif args.cmd:
        for l in send(ser, args.cmd):
            print(l)
    else:
        ap.error("need a command or --fb")

if __name__ == "__main__":
    main()
