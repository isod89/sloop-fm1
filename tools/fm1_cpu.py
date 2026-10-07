#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure the audio CPU load of a running FM-1 through its serial console (FELUCCA_CDC,
src/console.c `status`, read-only).

  tools/fm1_cpu.py [SECONDS] [--port COMx] [--csv FILE] [--label TEXT]

Samples `status` about twice a second and prints, over the samples taken while playing:
cpu_pct (the firmware's smoothed load: song.cpu_q8) min / mean / max, the worst audio half since
boot (audio_max_us, against the 5804 us a 256-frame half lasts at 44.1 kHz; the firmware sheds a
voice above 85 %), voices shed / given up. The worst half is a running maximum since power-on:
power-cycle the FM-1 before a measurement that must not include an earlier peak.
Needs pyserial (py -3 -m pip install --user pyserial).
"""
import argparse
import csv
import sys
import time

HALF_US = 256 * 1000000 / 44100          # one I2S half buffer (core.h HALF_FRAMES at FS)
SHED_PCT = 85                             # audio.c: a half over 85 % sheds a voice
FM1_VID = 0x1209


def find_port():
    from serial.tools import list_ports
    ports = [p for p in list_ports.comports() if p.vid == FM1_VID]
    if not ports:
        raise SystemExit("fm1_cpu: no FM-1 serial console found (USB connected? firmware with FELUCCA_CDC=1)")
    return ports[0].device


def status(port):
    port.reset_input_buffer()
    port.write(b"status\r")
    end, buf = time.time() + 1.0, b""
    while time.time() < end and not buf.rstrip().endswith(b">"):
        buf += port.read(port.in_waiting or 1)
    kv = {}
    for line in buf.decode("ascii", "replace").splitlines():
        parts = line.strip().split(" ", 1)
        if len(parts) == 2:
            kv[parts[0]] = parts[1]
    return kv


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("seconds", nargs="?", type=float, default=60)
    ap.add_argument("--port")
    ap.add_argument("--csv")
    ap.add_argument("--label", default="")
    a = ap.parse_args()
    import serial
    port = serial.Serial(a.port or find_port(), 115200, timeout=0.2)
    port.dtr = True                       # the console answers only with DTR (cdc.dtr)
    time.sleep(0.3)
    first = status(port)
    if "cpu_pct" not in first:
        raise SystemExit("fm1_cpu: the console did not answer `status`")
    print(f"{first.get('felucca', '')} {a.label}".strip() or "FM-1", f"on {port.port}; {a.seconds:.0f} s, play now")
    rows, t0 = [], time.time()
    while time.time() - t0 < a.seconds:
        kv = status(port)
        if "cpu_pct" in kv:
            rows.append({"t": round(time.time() - t0, 2), **kv})
        time.sleep(0.25)
    port.close()
    if a.csv:
        keys = sorted({k for r in rows for k in r}, key=lambda k: (k != "t", k))
        with open(a.csv, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, keys)
            w.writeheader()
            w.writerows(rows)
    play = [r for r in rows if r.get("playing") == "1"] or rows
    cpu = [int(r["cpu_pct"]) for r in play]
    last = rows[-1]
    worst = int(last.get("audio_max_us", 0))
    pct = worst * 100 / HALF_US
    print(f"samples {len(rows)}, playing {len([r for r in rows if r.get('playing') == '1'])}")
    print(f"cpu_pct   min {min(cpu)}  mean {sum(cpu) / len(cpu):.1f}  max {max(cpu)}")
    print(f"worst half {worst} us = {pct:.0f} % of {HALF_US:.0f} us (shedding above {SHED_PCT} %)")
    print(f"voices shed {last.get('voices_shed')}, given up {last.get('voices_given_up')}")
    return 1 if pct > SHED_PCT or int(last.get("voices_shed", 0)) else 0


if __name__ == "__main__":
    sys.exit(main())
