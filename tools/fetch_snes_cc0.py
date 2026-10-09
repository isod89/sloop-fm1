#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ugotworms
"""Download the CC0 recordings of the SNES engine's bank (tools/snes_bank.py) into assets/snes-cc0/:
  https://github.com/sgossner/VSCO-2-CE  (CC0 1.0)
  https://github.com/sgossner/VCSL       (CC0 1.0)
One note per instrument, chosen so that the chip (two octaves above a sample's root at most) still reaches
the instrument's top note at its rate. Only the start of each WAV is downloaded; it is kept as mono 16-bit
(at most 1 s) with manifest.json (instrument -> file, note, source) and ATTRIBUTION.txt. Files already
there are kept (delete one to fetch it again). tools/gen_brr.py builds the bank from them; an instrument
whose file is missing gets a synthesized stand-in. Each pitched recording is measured here, once (numpy):
its octave (the libraries' file names are often an octave off) and its tuning in cents, kept in the manifest;
a build measures nothing.
  tools/fetch_snes_cc0.py
"""
import io
import json
import math
import re
import struct
import sys
import urllib.parse
import urllib.request
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sampleio as sio  # noqa: E402
import snes_bank as bank  # noqa: E402

DEST = Path(__file__).resolve().parents[1] / "assets" / "snes-cc0"
RANGE = 600_000                       # bytes from the start of each WAV (1.6 s of 48 kHz 24-bit stereo is 460 KB)
KEEP_S = 1.0                          # (gen_brr.py uses at most ~0.7 s: attack, loop search, loop)
# the libraries' file names mostly count octaves one below the usual (their "C5" sounds C6), but not every
# instrument: each recording is measured (measure_note); this is the guess for picking, and the fallback when a
# sound has no clear pitch (bells, kalimba)
NAME_OCTAVE = 12
PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(name):
    m = re.fullmatch(r"([A-G])(#?)(-?\d)", name)
    return 12 * (int(m.group(3)) + 1) + PC[m.group(1)] + (1 if m.group(2) else 0)


def tree(repo, cache={}):
    if repo not in cache:
        url = f"https://api.github.com/repos/sgossner/{repo}/git/trees/master?recursive=1"
        with urllib.request.urlopen(url, timeout=120) as r:
            cache[repo] = [e["path"] for e in json.load(r)["tree"] if e["path"].lower().endswith(".wav")]
    return cache[repo]


def pick(inst):
    """(path, note) of the recording an instrument plays"""
    _, repo, folder, rx, *fixed = inst["src"]
    files = [p for p in tree(repo) if p.rsplit("/", 1)[0] == folder and re.fullmatch(rx, p.rsplit("/", 1)[1])]
    if not files:
        raise SystemExit(f"fetch: {inst['name']}: nothing in {repo}/{folder} matches {rx}")
    if fixed:
        return sorted(files)[0], fixed[0]
    notes = sorted((midi(re.fullmatch(rx, p.rsplit("/", 1)[1]).group("n")) + inst["named"], p) for p in files)
    shift = 12 * math.log2(32000 / inst["rate"])    # the root, above the note: stored below the chip's 32 kHz,
    #                                                 the sample plays faster at pitch 0x1000
    want = inst["top"] - 24
    ok = [(n, p) for n, p in notes if n + shift >= want - 0.01]
    n, p = ok[0] if ok else notes[-1]
    return p, n


def harmonic_hz(x, sr, start=0.15):
    """the fundamental of a recording: the frequency whose first five harmonics hold the most of its spectrum
    (YIN takes a strong 2nd harmonic for the note; this does not). None without numpy or a clear pitch"""
    try:
        import numpy as np
    except ImportError:
        return None
    a = int(start * sr)
    w = np.array(x[a:a + 16384] if len(x) > a + 4096 else x[-16384:], dtype=float)
    if len(w) < 2048 or not w.any():
        return None
    w = w - w.mean()
    sp = np.abs(np.fft.rfft(w * np.hanning(len(w)), 1 << 18))
    fr = np.fft.rfftfreq(1 << 18, 1.0 / sr)
    c = np.arange(25.0, min(4000.0, sr / 10), 0.25)
    s = sum(np.interp(k * c, fr, sp) for k in range(1, 6))
    return float(c[int(np.argmax(s))])


def measure_note(path, named):
    """the note a recording sounds: its name moved by the octaves its measured pitch says (within 0.4 semitone of
    whole octaves, -1 .. +2 of them), else the name with NAME_OCTAVE (no clear pitch); and what was measured"""
    sr, x = sio.read_any_wav(path)
    f = harmonic_hz(x, sr)
    if f:
        off = 69 + 12 * math.log2(f / 440.0) - named
        k = round(off / 12)
        if abs(off - 12 * k) < 0.4 and -1 <= k <= 2:
            return named + 12 * k, f"measured {off:+.2f}"
        return named + NAME_OCTAVE, f"unclear ({off:+.2f}): the name + {NAME_OCTAVE}"
    return named + NAME_OCTAVE, f"not measured (numpy): the name + {NAME_OCTAVE}"


def measure_cents(path, note):
    """the recording's tuning against its note: the strongest of its first four harmonics near k x the note's
    frequency (+-50 cents), in cents; 0 without numpy or a clear peak"""
    try:
        import numpy as np
    except ImportError:
        return 0
    sr, x = sio.read_any_wav(path)
    a = int(0.15 * sr)
    w = np.array(x[a:a + 16384] if len(x) > a + 4096 else x[-16384:], dtype=float)
    w = w - w.mean()
    sp = np.abs(np.fft.rfft(w * np.hanning(len(w)), 1 << 18))
    fr = np.fft.rfftfreq(1 << 18, 1.0 / sr)
    f, best = 440.0 * 2 ** ((note - 69) / 12), (0.0, 0)
    for k in range(1, 5):
        sel = (fr > k * f * 2 ** (-50 / 1200)) & (fr < k * f * 2 ** (50 / 1200))
        if sel.any():
            i = int(np.argmax(np.where(sel, sp, 0)))
            if sp[i] > best[0]:
                best = (sp[i], round(1200 * math.log2(fr[i] / (k * f))))
    return int(best[1])


def download(repo, path):
    url = f"https://raw.githubusercontent.com/sgossner/{repo}/master/" + urllib.parse.quote(path)
    req = urllib.request.Request(url, headers={"Range": f"bytes=0-{RANGE - 1}"})
    with urllib.request.urlopen(req, timeout=180) as r:
        return r.read()


def save(dest, data):
    """the start of a WAV -> mono 16-bit, at most KEEP_S, at its own rate"""
    tmp = dest.with_suffix(".part")
    tmp.write_bytes(data)
    try:
        sr, x = sio.read_any_wav(tmp)
    finally:
        tmp.unlink()
    x = x[:int(KEEP_S * sr)]
    pk = max(1e-9, max(abs(v) for v in x))
    with wave.open(str(dest), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(b"".join(struct.pack("<h", int(round(v / pk * 30000))) for v in x))
    return sr, len(x)


def main():
    DEST.mkdir(parents=True, exist_ok=True)
    man_p = DEST / "manifest.json"
    man = json.loads(man_p.read_text(encoding="utf-8")) if man_p.exists() else {}
    for inst in bank.INSTRUMENTS + bank.DRUMS:
        if inst["src"][0] != "cc0":
            continue
        name, repo = inst["name"], inst["src"][1]
        dest = DEST / (re.sub(r"[^A-Z0-9]+", "_", name.upper()).strip("_") + ".wav")
        path, note = pick(inst)
        fixed = len(inst["src"]) > 4                      # a note given in the bank: as it is (drums, timpani)
        if dest.exists() and man.get(name, {}).get("path") == path and man[name].get("keep") == KEEP_S and                 man[name].get("v") == 3:
            continue
        if not (dest.exists() and man.get(name, {}).get("path") == path):
            sr, n = save(dest, download(repo, path))
        how = "given"
        if not fixed and inst in bank.INSTRUMENTS:
            named = midi(re.fullmatch(inst["src"][3], path.rsplit("/", 1)[1]).group("n"))
            note, how = measure_note(dest, named)
            if note - named != inst["named"]:
                how += f" (the bank expects +{inst['named']}: set its named=)"
        cents = 0 if inst in bank.DRUMS else measure_cents(dest, note)
        man[name] = {"file": dest.name, "note": note, "cents": cents, "repo": repo, "path": path, "keep": KEEP_S, "v": 3}
        print(f"fetch: {name:8} <- {repo}: {path} (note {note} {cents:+d} ct: {how})")
        man_p.write_text(json.dumps(man, indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    names = {i["name"] for i in bank.INSTRUMENTS + bank.DRUMS if i["src"][0] == "cc0"}
    for name in [k for k in man if k not in names]:      # instruments the bank no longer has
        (DEST / man.pop(name)["file"]).unlink(missing_ok=True)
        print(f"fetch: {name} dropped")
    man_p.write_text(json.dumps(man, indent=1, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    lines = ["Felucca SNES engine - source recordings (tools/fetch_snes_cc0.py)",
             "Licence: CC0 1.0 Universal (public domain dedication)",
             "  https://creativecommons.org/publicdomain/zero/1.0/",
             "Sources: Versilian Studios (Sam Gossner)",
             "  VSCO-2 Community Edition  https://github.com/sgossner/VSCO-2-CE  (repository licence: CC0-1.0)",
             "  VCSL                      https://github.com/sgossner/VCSL       (repository licence: CC0-1.0)",
             "Each file: the start of the recording, mixed to mono, normalized, 16-bit.", ""]
    lines += [f"{v['file']}  <-  sgossner/{v['repo']}: {v['path']}" for k, v in sorted(man.items())]
    (DEST / "ATTRIBUTION.txt").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"fetch: {len(man)} recordings in {DEST}")


if __name__ == "__main__":
    main()
