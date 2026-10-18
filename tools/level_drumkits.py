#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measures every synthesised drum sound and writes tools/drumkit_levels.json (the trims, dB, that
gen_drumkits.py adds to each sound's level). Loudness: the loudest 100 ms (ITU-R BS.1770 weighting),
so a kit's kick, snare and hats land at the same place in every kit; no sound peaks above -4 dBFS.

  cc -O2 -Ibuild/gen -Ifirmware/src -Ifirmware/hal tests/drum_level.c -lm -o build/host/drum_level
  build/host/drum_level build/host/hits.raw && tools/level_drumkits.py build/host/hits.raw
  (then rebuild: the header is generated from the new trims; run it twice after big changes)
Needs numpy and scipy."""
import json
import re
import sys
from pathlib import Path

import numpy as np
from scipy.signal import lfilter

FS = 44100
N = FS * 2 // 32 * 32
HERE = Path(__file__).parent
LANES = "KICK SNARE CLAP CHH OHH TOMLO TOMHI CRASH RIDE SHAKER CONGA RIM COWBELL CLAVE KICK2 SNARE2".split()
KICK = -12.0                                   # the kick's loudness, LUFS-like (100 ms window)
REL = dict(KICK=0, KICK2=0, SNARE=-1, SNARE2=-2.5, CLAP=-2, CHH=-15, OHH=-12, TOMLO=-3, TOMHI=-3.5,
           CRASH=-10, RIDE=-13, SHAKER=-17, CONGA=-5, RIM=-6, COWBELL=-6, CLAVE=-7)
KIT = {"AMBIENT": -1.5, "JAZZ": -1.5, "VINTAGE": -0.5, "LATIN": -0.5}
PEAK = -4.0
KICK_PEAK = -1.5                               # (2.7) the kicks peak as high as the sampled kits'
KICK_UP = 2.5                                  # (2.7) and that much louder: heard on a small speaker


def loudness(x):
    b1 = [1.53512485958697, -2.69169618940638, 1.19839281085285]
    a1 = [1, -1.69065929318241, 0.73248077421585]
    b2, a2 = [1, -2, 1], [1, -1.99004745483398, 0.99007225036621]
    k = lfilter(b2, a2, lfilter(b1, a1, x)) ** 2
    w = int(0.1 * FS)
    c = np.concatenate([[0], np.cumsum(k)])
    return 10 * np.log10(((c[w:] - c[:-w]) / w).max() + 1e-12) - 0.691


def main(raw):
    names = re.findall(r'\("([^"]+)", "[^"]*", 0x?[0-9A-F]*,', (HERE / "gen_drumkits.py").read_text())
    d = np.fromfile(raw, np.int32).astype(float) / 32768.0
    assert len(d) == len(names) * 16 * N, "the hits do not match gen_drumkits.py (rebuild first)"
    path = HERE / "drumkit_levels.json"
    old = json.loads(path.read_text()) if path.exists() else {}
    out = {}
    for ki, n in enumerate(names):
        out[n] = {}
        for li, lane in enumerate(LANES):
            x = d[(ki * 16 + li) * N:(ki * 16 + li + 1) * N]
            kick = lane in ("KICK", "KICK2")
            want = KICK + REL[lane] + KIT.get(n, 0.0) + (KICK_UP if kick else 0.0)
            adj = min(want - loudness(x), (KICK_PEAK if kick else PEAK) - 20 * np.log10(np.abs(x).max() + 1e-9))
            t = old.get(n, {}).get(lane, 0.0) + adj
            out[n][lane] = round(max(-31.75, min(31.75, t)) * 4) / 4
    path.write_text(json.dumps(out, indent=1) + "\n")
    print(f"{len(names)} kits levelled -> {path}")


if __name__ == "__main__":
    main(sys.argv[1])
