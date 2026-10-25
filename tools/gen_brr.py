#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ugotworms
"""The SNES engine's instruments (firmware/src/eng_snes.c), in the S-DSP's BRR format: tools/snes_bank.py's.

  tools/gen_brr.py OUT.h                   the firmware's: the built-in waves (bank.FIRMWARE) and every preset
  tools/gen_brr.py --bank PREFIX [--own-only]
                                           the bank for the user slots: every other instrument, the kits and your
                                           own samples; PREFIX.N.hdr / PREFIX.N.bin per slot it needs (one to four).
                                           tools/fm1_sample_upload.py snes SLOT builds it and loads it

BRR: blocks of 9 bytes, a header (shift << 4 | filter << 2 | loop << 1 | end) and 16 four-bit samples
decoded through one of four predictive filters. The encoder tries every filter and shift of each block
against the decoder below, which is the hardware's bit for bit (after decode_brr of snes_spc's SPC_DSP.cpp,
by Shay Green, LGPL-2.1-or-later; with its 16-bit wrap), so the history it tracks is the one the S-DSP will
rebuild. The first block, and the loop's
first block, use filter 0 (no history): a key-on starts from the last note's history, and a loop must
decode the same on every pass.

A recording becomes an instrument the way SNES composers made them: its onset found, a sample rate chosen
near the bank's so that a whole number of cycles fills a whole number of BRR blocks (the loop then has no
seam in pitch), the loop placed after the attack where the wave best meets itself one loop later, its decay
(struck notes) flattened across the loop and its end crossfaded into its start; then encoded. The root: the
note that plays at pitch 0x1000 (the stored rate scaled to the chip's 32 kHz). Recordings: assets/snes-cc0
(tools/fetch_snes_cc0.py, its notes and tuning measured there once: a build measures nothing; a missing one gets
a synthesized stand-in), build/genwav (tools/gen_waves.py's drums, made by gen_samples.py,
which tools/build.py runs first; --bank runs gen_waves.py when they are missing).

Your own samples: assets/snes-local/ (ignored by git) may hold .spc files (every BRR sample in the sound RAM's
directory, its root measured) and .brr files (raw blocks; NAME.brr, with an optional NAME.txt of "root=<MIDI note>
loop=<block>"). They follow the bank's instruments and kits in INST (no preset: INST picks them). An .spc's choice
and roots: spc_samples (NAME.txt: keep=, skip=, root<entry>=). Only from games you own, for yourself: a bank made
with them is not yours to share. --own-only leaves the standard bank out (the presets past the built-in waves then
play whatever INST holds there).

Results are cached in build/brr_cache (keyed by the instrument and its recording): rebuilds are quick.
"""
import hashlib
import json
import math
import re
import struct
import sys
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sampleio as sio  # noqa: E402
import snes_bank as bank  # noqa: E402

SRC = Path(__file__).resolve().parents[1]
CC0 = SRC / "assets" / "snes-cc0"
LOCAL = SRC / "assets" / "snes-local"
GENWAV = SRC / "build" / "genwav"
CACHE = SRC / "build" / "brr_cache"
FS = 32000                                   # the S-DSP's rate: pitch 0x1000 plays a sample as stored
PEAK = 0.85                                  # of full scale, in the decoder's (x2) domain
VERSION = 4                                  # of the processing below (a change invalidates the cache)


# ------------------------------------------------------------------ BRR ---
def wrap16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def dec1(n, shift, filt, p1, p2s):
    """one sample as the S-DSP decodes it: nibble n (-8..7), the stored history p1, p2s (x2 domain)"""
    s = (n << shift) >> 1
    if shift >= 13:
        s = -2048 if s < 0 else 0
    p2 = p2s >> 1
    if filt >= 2:
        s += p1
        s -= p2
        if filt == 2:
            s += p2 >> 4
            s += (p1 * -3) >> 6
        else:
            s += (p1 * -13) >> 7
            s += (p2 * 3) >> 4
    elif filt == 1:
        s += p1 >> 1
        s += (-p1) >> 5
    s = max(-32768, min(32767, s))
    return wrap16(s * 2)


def enc_block(target, p1, p2, filters):
    """best (error, header bits, nibbles, p1, p2) of 16 target samples (x2 domain) from history p1, p2"""
    best = None
    for filt in filters:
        for shift in range(13):
            err, nibs, a, b = 0, [], p1, p2
            for t in target:
                base = dec1(0, shift, filt, a, b)
                n0 = int(round((t - base) / float(1 << shift)))
                bn, bv, be = 0, 0, None
                for n in (n0 - 1, n0, n0 + 1):
                    if -8 <= n <= 7:
                        v = dec1(n, shift, filt, a, b)
                        e = (v - t) * (v - t)
                        if be is None or e < be:
                            bn, bv, be = n, v, e
                if be is None:                  # all candidates outside -8..7: the nearest end
                    bn = -8 if n0 < -8 else 7
                    bv = dec1(bn, shift, filt, a, b)
                    be = (bv - t) * (bv - t)
                err += be
                if best and err >= best[0]:
                    break
                nibs.append(bn)
                a, b = bv, a
            else:
                if not best or err < best[0]:
                    best = (err, shift << 4 | filt << 2, nibs, a, b)
    return best


def brr_encode(x, loop):
    """x: floats -1..1 (a multiple of 16 long), loop: the loop's first sample (a multiple of 16) or None"""
    assert len(x) % 16 == 0 and (loop is None or loop % 16 == 0)
    tgt = [max(-32000, min(32000, int(round(v * 32767)))) for v in x]
    out, p1, p2, nb, sq = bytearray(), 0, 0, len(x) // 16, 0
    for k in range(nb):
        free = k != 0 and (loop is None or k != loop // 16)
        err, hdr, nibs, p1, p2 = enc_block(tgt[16 * k:16 * k + 16], p1, p2, range(4) if free else (0,))
        sq += err
        if k == nb - 1:
            hdr |= 1 | (2 if loop is not None else 0)
        out.append(hdr)
        for i in range(0, 16, 2):
            out.append((nibs[i] & 15) << 4 | (nibs[i + 1] & 15))
    rms = math.sqrt(sq / max(1, len(x))) / 32767
    return bytes(out), rms


def brr_decode(data, loop_block, n_out):
    """the S-DSP's samples of a BRR sample (looping as it would), n_out of them, floats"""
    out, p1, p2, blk, nb = [], 0, 0, 0, len(data) // 9
    while len(out) < n_out and blk < nb:
        h = data[9 * blk]
        for i in range(16):
            b = data[9 * blk + 1 + i // 2]
            n = (b >> 4) if i % 2 == 0 else (b & 15)
            n = n - 16 if n & 8 else n
            v = dec1(n, h >> 4, (h >> 2) & 3, p1, p2)
            p1, p2 = v, p1
            out.append(v / 32768.0)
        if h & 1:
            if not h & 2 or loop_block is None:
                break
            blk = loop_block
        else:
            blk += 1
    return out[:n_out]


# ------------------------------------------------------------- signals ---
def resample(x, sr, to):
    """windowed-sinc (Blackman, 16 zero crossings a side), low-passed at 0.92 of the lower Nyquist"""
    if abs(sr - to) < 1e-9:
        return list(x)
    ratio = sr / to                                     # input samples per output sample
    fc = 0.92 * min(1.0, 1.0 / ratio)                   # cutoff, of the input's Nyquist
    half = int(math.ceil(16 / fc))
    n_out = int(len(x) / ratio)
    out = []
    for j in range(n_out):
        c = j * ratio
        i0 = int(c)
        acc = 0.0
        for i in range(max(0, i0 - half + 1), min(len(x), i0 + half + 1)):
            t = c - i
            u = t * fc
            s = fc if t == 0 else math.sin(math.pi * u) / (math.pi * t)
            w = 0.42 + 0.5 * math.cos(math.pi * t / half) + 0.08 * math.cos(2 * math.pi * t / half)
            acc += x[i] * s * w
        out.append(acc)
    return out


def norm(x, peak=PEAK):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def additive(n, partials):
    return [sum(a * math.sin(2 * math.pi * k * i / n) for k, a in partials) for i in range(n)]


def read_wav(path):
    sr, x = sio.read_any_wav(Path(path))
    return sr, x


def note_hz(n):
    return 440.0 * 2 ** ((n - 69) / 12)


def plan_loop(f0, rate, loop_s):
    """(rate, loop length, cycles): a rate near `rate` at which `cycles` periods of f0 are a whole number of
    BRR blocks, the loop at least loop_s long"""
    best = None
    for m in range(1, 4096):
        length = 16 * m
        k = max(1, round(length * f0 / rate))
        r = length * f0 / k
        dev = abs(r / rate - 1)
        if length / rate >= loop_s and dev <= 0.02:
            return r, length, k
        if best is None or dev < best[0]:
            best = (dev, r, length, k)
        if length / rate > 4 * loop_s + 0.1:
            break
    return best[1], best[2], best[3]


def place_loop(y, att, length, search, period):
    """the loop's start (>= att): where y best meets itself one loop later, over two periods around it"""
    w = max(8, min(length // 2, int(2 * period)))
    best, bs = None, att
    end = min(att + search, len(y) - length - w - 1)
    for s in range(att, max(att + 1, end)):
        e = 0.0
        for j in range(-w, w):
            d = y[s + j] - y[s + length + j]
            e += d * d
        if best is None or e < best:
            best, bs = e, s
    return bs


def make_loop(y, s, att16, length, xfade, flatten):
    """the stored sample: att16 samples of attack ending at s, then the loop y[s:s+length] with its decay
    flattened (struck notes) and its end crossfaded into what precedes its start"""
    head = y[max(0, s - att16):s]
    head = [0.0] * (att16 - len(head)) + head
    loop = y[s:s + length]
    if flatten:                                         # the decay across the loop, undone (the envelope's SR
        h = length // 2                                 # decays the note instead)
        a = math.sqrt(sum(v * v for v in loop[:h]) / h + 1e-12)
        b = math.sqrt(sum(v * v for v in loop[h:]) / (length - h) + 1e-12)
        r = math.log(a / b) / h if b > 0 and a > b else 0.0
        loop = [v * math.exp(r * j) for j, v in enumerate(loop)]
    pre = y[s - xfade:s] if s >= xfade else [0.0] * xfade
    if flatten and pre:                                 # (its level brought to the loop's end)
        g = math.exp(r * length) if r else 1.0
        pre = [v * g for v in pre]
    for j in range(xfade):
        w = 0.5 - 0.5 * math.cos(math.pi * (j + 0.5) / xfade)
        loop[length - xfade + j] = (1 - w) * loop[length - xfade + j] + w * pre[j]
    return head + loop


# ----------------------------------------------------------------- synth ---
def synth(what, inst):
    """(samples, loop start or None, root16, rate) of a made wave"""
    if what == "square":
        x = [0.75 if i < 16 else -0.75 for i in range(32)] * 2      # 0.75: exact in a filter-0 block
        return x, 0, root_of_cycle(32), FS
    if what == "pulse25":
        x = [0.75 if i < 8 else -0.75 for i in range(32)] * 2
        return x, 0, root_of_cycle(32), FS
    if what == "saw":
        return norm(additive(64, [(h, (-1) ** (h + 1) / h) for h in range(1, 32)]) * 2), 0, root_of_cycle(64), FS
    if what == "sine":
        return norm(additive(64, [(1, 1.0), (2, 0.18), (3, 0.06)])), 0, root_of_cycle(64), FS
    if what == "strings":                               # three detuned saws in one 4096-sample loop
        n = 4096
        return norm(additive(n, [(k * h, 1.0 / h / (1 + (h / 9.0) ** 2))
                                 for k in (63, 64, 65) for h in range(1, 30) if k * h * FS / n < 15000])), 0, \
            root_of_cycle(64), FS
    if what == "synbass":                               # a low-passed saw and its sub-octave
        n = 128
        return norm(additive(n, [(2 * h, 1.0 / h / (1 + (h / 5.0) ** 2)) for h in range(1, 32)] + [(1, 0.6)])), 0, \
            root_of_cycle(n), FS
    if what == "organ":                                 # drawbars 16' 8' 5 1/3' 4' 2'
        n = 128
        return norm(additive(n, [(1, 0.8), (2, 1.0), (3, 0.6), (4, 0.5), (8, 0.3)])), 0, root_of_cycle(n), FS
    if what == "choir":                                 # "aah": harmonics under vowel formants, three voices
        n, f0 = 4096, 64 * FS / 4096
        formants = [(800, 80, 1.0), (1150, 90, 0.5), (2900, 120, 0.25), (3900, 130, 0.1)]

        def amp(f):
            return sum(g / (1 + ((f - fc) / bw) ** 2) for fc, bw, g in formants) + 0.02
        parts = [(k * h, amp(k * h * FS / n) / math.sqrt(h)) for k in (63, 64, 65) for h in range(1, 40)
                 if k * h * FS / n < 12000]
        return norm(additive(n, parts)), 0, root_of_cycle(64), FS
    if what == "orchhit":                               # an orchestra stab: a chord of bright saws, octaves
        rate = inst["rate"]                             # and a fifth over a timpani-like thump, falling fast
        n = int(inst["length"] * rate) // 16 * 16
        f0 = note_hz(60)
        ratios = [0.5, 1.0, 1.5, 2.0, 3.0, 4.0]
        x = []
        for i in range(n):
            t = i / rate
            env = math.exp(-t * 7.0) * min(1.0, t * 400)
            s = 0.0
            for r in ratios:
                for h in range(1, 12):
                    if f0 * r * h < rate * 0.45:
                        s += math.sin(2 * math.pi * f0 * r * h * t * (1 + 0.003 * h)) / h / r
            s += 1.5 * math.sin(2 * math.pi * 55 * t) * math.exp(-t * 12)
            x.append(s * env)
        x = norm(x)
        return x + [0.0] * 16, None, round(16 * (60 + 12 * math.log2(FS / rate))), rate
    raise SystemExit(f"gen_brr: no synth {what}")


def root_of_cycle(cycle):
    """the note of a cycle of `cycle` samples at 32 kHz, in 1/16 semitones"""
    return int(round((69 + 12 * math.log2(FS / cycle / 440.0)) * 16))


# ------------------------------------------------------------ recordings ---
def recording(inst, manifest):
    """(samples, rate, note, cents) of an instrument's recording, or None (missing)"""
    src = inst["src"]
    if src[0] == "cc0":
        m = manifest.get(inst["name"])
        if not m or not (CC0 / m["file"]).exists():
            return None
        sr, x = read_wav(CC0 / m["file"])
        return x, sr, m["note"], m.get("cents", 0)
    if src[0] == "asset":
        p = CC0 / src[1]
        if not p.exists():
            return None
        sr, x = read_wav(p)
        return x, sr, src[2], src[3] if len(src) > 3 else 0
    if src[0] == "genwav":
        p = GENWAV / (src[1] + ".wav")
        if not p.exists():                              # (made by gen_samples.py, which build.py runs first)
            raise SystemExit(f"gen_brr: {p} is missing: run tools/gen_samples.py first")
        sr, x = read_wav(p)
        return x, sr, 60, 0
    return None


def build(inst, manifest, drum):
    """(BRR bytes, loop byte offset or None, root16, info)"""
    if inst["src"][0] == "synth":
        x, loop, root, rate = synth(inst["src"][1], inst)
        b, rms = brr_encode(x, loop)
        return b, (loop // 16 * 9 if loop is not None else None), root, f"synth {inst['src'][1]}", rms
    rec = recording(inst, manifest)
    if rec is None:                                     # stand-in: a plucked or held tone
        stand = dict(inst, src=("synth", "sine"))
        b, lp, root, info, rms = build(stand, manifest, drum)
        return b, lp, root, "stand-in (no recording: tools/fetch_snes_cc0.py)", rms
    x, sr, note, cents = rec
    x = x[max(0, sio.onset(x) - int(0.002 * sr)):]
    kind, rate0 = inst["kind"], inst["rate"]
    if kind == "oneshot":
        rate = rate0
        n = min(len(x), int(inst["length"] * sr) + 64)
        y = resample(x[:n], sr, rate)[:int(inst["length"] * rate)]
        y = y[:len(y) // 16 * 16]
        fade = len(y) // 6
        y = [v * min(1.0, (len(y) - i) / fade) for i, v in enumerate(y)]
        y = norm(y, PEAK * inst["gain"]) + [0.0] * 16    # a silent last block (the chip mutes the end block)
        root = round(16 * (60 + 12 * math.log2(FS / rate)))   # key 60 plays it at its own rate
        b, rms = brr_encode(y, None)
        return b, None, root, f"{rate} Hz one-shot", rms
    if drum:                                            # a cymbal: no pitch, a loop of its tail
        rate, length = rate0, max(64, int(round(inst["loop"] * rate0 / 16)) * 16)
        period, root = 64, round(16 * (60 + 12 * math.log2(FS / rate)))
    else:
        f0 = note_hz(note) * 2 ** (cents / 1200)         # (measured once, by fetch_snes_cc0.py: a build measures
        #                                                 nothing, so it is the same with or without numpy)
        rate, length, k = plan_loop(f0, rate0, inst["loop"])
        period = rate / f0
        root = round(16 * (69 + 12 * math.log2(f0 * FS / rate / 440.0)))
    att16 = int(math.ceil(inst["att"] * rate / 16)) * 16
    need = att16 + length * 2 + int(0.3 * rate) + 64
    y = resample(x[:int(need * sr / rate) + 64], sr, rate)
    s = place_loop(y, att16, length, int(0.25 * rate), period)
    xfade = length // 2 if drum else max(16, min(length // 2, int(3 * period)))
    z = make_loop(y, s, att16, length, xfade, kind == "decay")
    z = norm(z, PEAK * inst["gain"])
    b, rms = brr_encode(z, att16)
    info = f"{rate:.0f} Hz, loop {length} ({length / rate * 1000:.0f} ms)"
    if not drum:
        info += f", {k} cycles, root {root / 16:.2f}, reaches {root / 16 + 24:.0f}"
    return b, att16 // 16 * 9, root, info, rms


def cached(inst, manifest, drum):
    h = hashlib.sha256(json.dumps([VERSION, inst, drum], sort_keys=True, default=str).encode())
    rec_path = None
    src = inst["src"]
    if src[0] == "cc0" and inst["name"] in manifest:
        rec_path = CC0 / manifest[inst["name"]]["file"]
    elif src[0] == "asset":
        rec_path = CC0 / src[1]
    elif src[0] == "genwav":
        rec_path = GENWAV / (src[1] + ".wav")
    if rec_path is not None:
        h.update(rec_path.read_bytes() if rec_path.exists() else b"missing")
        if src[0] == "cc0":
            h.update(json.dumps(manifest[inst["name"]], sort_keys=True).encode())
    f = CACHE / (h.hexdigest()[:24] + ".json")
    if f.exists():
        d = json.loads(f.read_text())
        return bytes.fromhex(d["brr"]), d["loop"], d["root"], d["info"], d["rms"]
    b, lp, root, info, rms = build(inst, manifest, drum)
    CACHE.mkdir(parents=True, exist_ok=True)
    f.write_text(json.dumps({"brr": b.hex(), "loop": lp, "root": root, "info": info, "rms": rms}))
    return b, lp, root, info, rms


# ------------------------------------------------------- your own samples ---
def local_samples():
    """(name, BRR bytes, loop offset or None, root16) of assets/snes-local/*.spc and *.brr"""
    out = []
    if not LOCAL.exists():
        return out
    for p in sorted(LOCAL.glob("*.brr")):
        d = p.read_bytes()
        d = d[2:] if len(d) % 9 == 2 else d             # (a 2-byte loop header, as some tools write)
        d = d[:len(d) // 9 * 9]
        if not d:
            continue
        meta = {}
        txt = p.with_suffix(".txt")
        if txt.exists():
            meta = dict(re.findall(r"(\w+)\s*=\s*(-?\d+)", txt.read_text()))
        last = len(d) // 9 - 1
        looping = d[9 * last] & 2
        lb = int(meta.get("loop", 0)) if looping else None
        root = int(meta["root"]) * 16 if "root" in meta else measure_root(d, lb)
        up = re.sub(r"[^A-Z0-9 .]", "", p.stem.upper())
        out.append((up.replace(" ", "")[:5] or "BRR", d, None if lb is None else lb * 9, root, up[:12] or "BRR"))
    used = set()
    for p in sorted(LOCAL.glob("*.spc")):
        out += spc_samples(p, used)
    return out


def spc_ranges(spec):
    """"1,3-5,12" -> {1, 3, 4, 5, 12}"""
    out = set()
    for part in re.findall(r"\d+(?:-\d+)?", spec):
        a, _, b = part.partition("-")
        out.update(range(int(a), int(b or a) + 1))
    return out


def spc_prefix(path, used):
    """two letters for an .spc's samples: its name's first two, or another pair of its letters when an .spc before
    it took those. With the entry's three digits: the 5 characters INST shows (Felucca shows no more of a value)"""
    w = re.sub(r"[^A-Z0-9]", "", path.stem.upper()) or "SP"
    tries = [w[0] + c for c in w[1:]] + [a + c for a in w for c in w] + [f"S{d}" for d in range(10)]
    pre = next(t for t in tries if t not in used)
    used.add(pre)
    return pre


def spc_samples(path, used):
    """the BRR samples of an .spc's sound RAM, by its directory (DSP register DIR). INST names them by two letters of
    the file's name and the directory entry (ZE011: entry 11; a name stays as entries are kept or dropped), their
    presets by the file's name and the entry (ZELDA 11). Left out:
    entries that are not BRR, repeat another, start inside another sample (a game's pointer into one), peak
    below -18 dB (leftover memory: a game's samples sit near full scale) or are a click (a one-shot of 3 blocks or less). NAME.txt beside it (optional):
      keep=0,2,5-9     only these entries      skip=3,14     not these      root11=64     entry 11's root (MIDI)"""
    d = path.read_bytes()
    if len(d) < 0x10180 or not d.startswith(b"SNES-SPC700 Sound File Data"):
        print(f"brr: {path.name}: not an SPC file")
        return []
    ram, regs = d[0x100:0x10100], d[0x10100:0x10180]
    base = regs[0x5D] * 0x100
    opts = path.with_suffix(".txt").read_text() if path.with_suffix(".txt").exists() else ""
    keep = spc_ranges(m.group(1)) if (m := re.search(r"keep\s*=\s*([\d,\s-]+)", opts)) else None
    skip = spc_ranges(m.group(1)) if (m := re.search(r"skip\s*=\s*([\d,\s-]+)", opts)) else set()
    roots = {int(a): int(b) for a, b in re.findall(r"root(\d+)\s*=\s*(\d+)", opts)}
    ents, seen = [], set()
    for i in range(256):
        e = base + 4 * i
        if e + 4 > 0x10000:
            break
        start, loop = ram[e] | ram[e + 1] << 8, ram[e + 2] | ram[e + 3] << 8
        if (start, loop) in seen or not start:
            continue
        a, ok = start, False
        while a + 9 <= 0x10000 and a - start < 0x8000:
            h = ram[a]
            if (h >> 4) > 12:
                break
            a += 9
            if h & 1:
                ok = True
                break
        if not ok or a - start < 18:
            continue
        data = bytes(ram[start:a])
        looping = data[-9] & 2
        if looping and not (start <= loop < a and (loop - start) % 9 == 0):
            continue
        seen.add((start, loop))
        ents.append(dict(i=i, start=start, end=a, data=data, lb=(loop - start) // 9 if looping else None))
    out, report = [], []
    stem = spc_prefix(path, used)
    long = re.sub(r"[^A-Z0-9]", "", path.stem.upper())[:8] or "SPC"
    for en in ents:
        i, data, lb = en["i"], en["data"], en["lb"]
        inside = [o["i"] for o in ents if o is not en and o["start"] <= en["start"] and en["end"] <= o["end"]
                  and (o["start"], o["end"]) != (en["start"], en["end"])]
        x = brr_decode(data, lb, 8000)
        peak = max((abs(v) for v in x), default=0.0)
        why = None
        if keep is not None and i not in keep:
            why = "not in keep="
        elif i in skip:
            why = "skip="
        elif inside:
            why = f"inside entry {inside[0]}"
        elif peak < 0.125:                              # games store samples near full scale (BRR keeps detail
            why = f"too quiet to be one ({20 * math.log10(peak + 1e-9):.0f} dB)"   # relative to the peak; volume
            #                                                is the chip's): below -18 dB it is stale memory
        elif lb is None and len(data) <= 27:
            why = "a click"
        name = f"{stem}{i:03d}"
        if why:
            report.append(f"   -  {name}  {len(data):5} B  left out: {why}")
            continue
        root = roots[i] * 16 if i in roots else measure_root(data, lb)
        out.append((name, data, None if lb is None else lb * 9, root, f"{long} {i}"))
        report.append(f"   +  {name}  {len(data):5} B  {'loops' if lb is not None else 'one-shot':8}  root {root / 16:6.2f}"
                      + ("  (root given)" if i in roots else ""))
    print(f"brr: {path.name}: directory at 0x{base:04X}, {len(ents)} samples, {len(out)} kept, "
          f"{sum(len(o[1]) for o in out)} B")
    for ln in report:
        print("brr:" + ln)
    return out


def harmonic_hz(x, sr, start=0.15):
    """the fundamental of a recording: the frequency whose first five harmonics hold the most of its spectrum
    (YIN takes a strong 2nd harmonic for the note; this does not). Needs numpy; None without it or with no
    clear pitch (silence, too short)"""
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


def measure_root(data, loop_block):
    """the note a BRR sample plays at pitch 0x1000 (what the chip decodes, measured); 60 when it has none"""
    x = brr_decode(data, loop_block, 24000)
    f = harmonic_hz(x, FS, 0.05) if len(x) > 4000 else None
    if f is None and len(x) > 4000:                     # (no numpy: YIN)
        try:
            f = sio.yin(x[len(x) // 4:len(x) // 4 + 3000], FS)
        except Exception:
            f = None
    return round(16 * (69 + 12 * math.log2(f / 440.0))) if f and 20 < f < 8000 else 60 * 16


# ------------------------------------------------------------------ main ---
SLOT_DATA = 0x14000 - 512                   # a user slot's data (eng_sample.c SMP_USER_SIZE - SMP_USER_DATA)
SLOT_HDR = 480                              # its header (smp_user_hdr_t)
MAX_PARTS = 4                               # the user slots
NSEL_MAX = 127                              # INST values (a preset keeps INST in a signed byte)
SLOT_MAGIC = 0x52524246                     # "FBRR" (eng_snes.c SNES_SLOT_MAGIC)


def load_manifest():
    man_p = CC0 / "manifest.json"
    return json.loads(man_p.read_text(encoding="utf-8")) if man_p.exists() else {}


def preset_line(name, e, p):
    env = ", ".join(str(v) for v in p["env"])
    echo = p["echo"]
    return (f'    {{"{name}", {{{", ".join(str(v) for v in [e, p["tune"], p["sr"], p["noise"], *echo])}}}, '
            f'{{{env}}}, 0, {p["mono"]}, FX({", ".join(str(v) for v in p["fx"])}), PAT({p["pat"]})}},')


def standard_sel():
    """the bank's standard entries, in INST order after the built-in waves: (name, instrument | None, kit | None)"""
    fw = set(bank.FIRMWARE)
    return [(i["name"], i, None) for i in bank.INSTRUMENTS if i["name"] not in fw] + \
        [(k, None, k) for k in bank.KITS]


def firmware_header(out):
    """the built-in waves (SNES_BRR), their names and roots, and every preset: the waves', then the standard bank's
    (their INST values as the bank lays them out)"""
    manifest = load_manifest()
    fw = [next(i for i in bank.INSTRUMENTS if i["name"] == n) for n in bank.FIRMWARE]
    data, rows = bytearray(), []
    for inst in fw:
        b, lp, root, _, _ = cached(inst, manifest, False)
        rows.append((inst["name"], len(data), len(data) + (lp or 0), root))
        data.extend(b)
    nfw = len(fw)
    presets = [preset_line(i["name"], k, i["preset"]) for k, i in enumerate(fw)]
    for k, (name, inst, kit) in enumerate(standard_sel()):
        presets.append(preset_line(name, nfw + k, inst["preset"] if inst else bank.KIT_PRESET))
    lines = ["/* generated by tools/gen_brr.py: the SNES engine's built-in waves and presets (eng_snes.c); the other",
             " * instruments are the bank's, in the user slots (gen_brr.py --bank) */",
             "#ifndef FELUCCA_BRR_H", "#define FELUCCA_BRR_H",
             "typedef struct { uint32_t start, loop; int16_t root16; } snes_fw_t;   /* byte offsets in SNES_BRR; "
             "root: the note of pitch 0x1000, 1/16 st */",
             f"#define SNES_NFW {nfw}u              /* INST 0 .. SNES_NFW - 1; the bank's after them */",
             f"#define SNES_NSTD {len(standard_sel())}u            /* the standard bank's entries (the presets know them) */",
             f"#define SNES_BRR_LEN {len(data)}u",
             "#define SNES_FW_NAMES_INIT " + ", ".join(f'"{r[0]}"' for r in rows),
             "static const snes_fw_t SNES_FW[SNES_NFW] = {"]
    lines += [f"    {{{r[1]}u, {r[2]}u, {r[3]}}},   /* {r[0]} */" for r in rows]
    names = [i["name"] for i in fw] + [n for n, _, _ in standard_sel()]
    kinds = {n: k for k, ns in bank.KIND.items() for n in ns}
    if set(kinds) != set(names):
        raise SystemExit(f"gen_brr: snes_bank.KIND and the presets differ: {sorted(set(kinds) ^ set(names))}")
    lines.append("/* the presets in the PRESETS browser (ui.c BANK[]), by kind */")
    for k in bank.KIND:
        lines.append(f"#define SNES_BANK_{k} " + ", ".join(f'{{BK_{ {"ORGN": "ORGAN", "PLCK": "PLUCK"}.get(k, k)}, ENGI_SNES, "{n}"}}'
                                                       for n in names if kinds[n] == k))
    lines += ["};", "#ifdef SNES_PRESETS",
              "/* {INST, TUNE, SR, NOISE, ECHO, DELAY, FDBK, FIR}, {ATK, DEC, SUS, REL}: the waves', then the bank's */",
              "static const preset_t SNES_PRESET_TABLE[] = {"] + presets + ["};", "#endif",
                                                                           "static const uint8_t SNES_BRR[SNES_BRR_LEN] = {"]
    for i in range(0, len(data), 18):
        lines.append("    " + " ".join(f"0x{v:02X}," for v in data[i:i + 18]))
    lines += ["};", "#endif", ""]
    Path(out).write_text("\n".join(lines), encoding="utf-8", newline="\n")   # (the same bytes on every OS)
    print(f"brr: firmware: {nfw} waves, {len(data)} B, {len(presets)} presets -> {out}")


def ensure_genwav():
    """the kits' synthesized drums (build/genwav: tools/gen_waves.py, which gen_samples.py runs in a build)"""
    if not all((GENWAV / (d["src"][1] + ".wav")).exists() for d in bank.DRUMS if d["src"][0] == "genwav"):
        import subprocess
        subprocess.run([sys.executable, str(Path(__file__).with_name("gen_waves.py")), str(GENWAV)], check=True)


def build_bank(prefix, own_only=False):
    """the bank for the user slots: prefix.N.hdr (480 B) and prefix.N.bin (the data) per part N. Part 0's data
    starts with the directory (eng_snes.c snes_dir_t): the INST entries, the samples, the kits' notes"""
    import zlib
    manifest = load_manifest()
    insts, kitsel, smps, kits, info = [], [], [], [], []   # sel: (name, kind, idx); smp: (name, BRR, loop, root16)
    index = {}

    def add_smp(name, b, lp, root, what):
        index[name] = len(smps)
        smps.append((name, b, lp, root))
        info.append(f"{name:8} {len(b):6} B  {what}")

    std = [] if own_only else standard_sel()
    for name, inst, _ in std:
        if inst:
            b, lp, root, what, _ = cached(inst, manifest, False)
            add_smp(name, b, lp, root, what)
            insts.append((name, 0, index[name]))
    if any(kit for _, _, kit in std):
        ensure_genwav()
        used = {p for _, _, kit in std if kit for p, _ in bank.KITS[kit].values()}
        for d in bank.DRUMS:
            if d["name"] in used:
                b, lp, root, what, _ = cached(d, manifest, True)
                add_smp(d["name"], b, lp, root, what)
        for _, _, kit in std:
            if kit:
                kitsel.append((kit, 1, len(kits)))
                kits.append(bank.KITS[kit])
    sels = insts + kitsel                          # (as standard_sel lays them out: the presets' INST values)
    for name, b, lp, root, long in local_samples():
        add_smp(name, b, lp, root, f"your own ({long}), root {root / 16:.2f}")
        sels.append((name, 0, index[name]))
    if not sels:
        raise SystemExit("gen_brr: an empty bank (--own-only and nothing in assets/snes-local)")
    nfw = len(bank.FIRMWARE)
    if nfw + len(sels) > NSEL_MAX:
        raise SystemExit(f"gen_brr: {nfw + len(sels)} INST values, {NSEL_MAX} at most: leave some of your own "
                         f"out (NAME.txt beside an .spc: keep= / skip=)")
    # the directory, then the samples, each within one part
    dir_len = 20 + 12 * len(sels) + 12 * len(smps) + 256 * len(kits)
    parts, place = [bytearray(dir_len)], []
    for name, b, lp, root in smps:
        if len(b) > SLOT_DATA:
            raise SystemExit(f"gen_brr: {name} ({len(b)} B) is larger than a slot")
        if len(parts[-1]) + len(b) > SLOT_DATA:
            parts.append(bytearray())
        place.append((len(parts) - 1, len(parts[-1])))
        parts[-1].extend(b)
    if len(parts) > MAX_PARTS:
        raise SystemExit(f"gen_brr: the bank needs {len(parts)} slots, {MAX_PARTS} at most: leave some out")
    d = parts[0]
    sel_off, smp_off = 20, 20 + 12 * len(sels)          # (after snes_dir_t, 20 B)
    kit_off = smp_off + 12 * len(smps)
    struct.pack_into("<4H3I", d, 0, len(sels), len(smps), len(kits), 0, sel_off, smp_off, kit_off)
    for k, (name, kind, idx) in enumerate(sels):
        nm = re.sub(r"[^\x20-\x7E]", "", name)[:7].encode().ljust(8, b"\0")
        struct.pack_into("<8sBB2x", d, sel_off + 12 * k, nm, kind, idx)
    for k, ((name, b, lp, root), (p, off)) in enumerate(zip(smps, place)):
        struct.pack_into("<BxhII", d, smp_off + 12 * k, p, root, off, off + (lp or 0))
    for k, mp in enumerate(kits):
        for n in range(128):
            if n in mp:
                struct.pack_into("<Bb", d, kit_off + 256 * k + 2 * n, index[mp[n][0]] + 1, mp[n][1])
    bid = zlib.crc32(b"".join(bytes(x) for x in parts)) or 1
    out = []
    for p, data in enumerate(parts):
        nm = f"SNES {p + 1}/{len(parts)}".encode()[:8].ljust(8, b"\0")
        hdr = struct.pack("<IHBB8sIIII", SLOT_MAGIC, 1, p, len(parts), nm, len(data), zlib.crc32(bytes(data)), bid, 0)
        Path(f"{prefix}.{p}.hdr").write_bytes(hdr.ljust(SLOT_HDR, b"\0"))
        Path(f"{prefix}.{p}.bin").write_bytes(bytes(data))
        out.append((f"{prefix}.{p}", len(data)))
    for ln in info:
        print("brr:", ln)
    print(f"brr: bank: {len(sels)} INST values ({len(sels) - len(kits)} instruments, {len(kits)} kits) after the "
          f"{nfw} built in, {len(smps)} samples, {sum(len(x) for x in parts)} B in {len(parts)} slot(s):")
    for path, n in out:
        print(f"brr:   {path}.hdr / .bin  {n} B")
    return out


def main():
    args = sys.argv[1:]
    if len(args) == 1 and not args[0].startswith("-"):
        firmware_header(args[0])
    elif args and args[0] == "--bank" and (len(args) == 2 or (len(args) == 3 and args[2] == "--own-only")):
        build_bank(args[1], len(args) == 3)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
