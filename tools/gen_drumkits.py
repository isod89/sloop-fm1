#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Synthesised drum kits for firmware/src/drum_synth.c -> felucca_drumkits.h

Each kit has 16 sounds (lanes), one per white key of the drum track:
KICK SNARE CLAP CHH OHH TOMLO TOMHI CRASH RIDE SHAKER CONGA RIM COWBELL CLAVE KICK2 SNARE2.
A sound is written in musical units (Hz, ms, octaves, dB) and converted to the table indices
the firmware uses (MIDI note + 1/16, DECAY_K and CUTOFF_HZ indices). The level of every sound
(LEVELS, tools/drumkit_levels.json) is measured, not guessed: tests/drum_level.c renders each
sound, and every kit comes out as loud as the others, each lane at its place in the mix.

  tools/gen_drumkits.py OUT.h
"""
import copy
import json
import math
import sys
from pathlib import Path

LANES = ["KICK", "SNARE", "CLAP", "CHH", "OHH", "TOMLO", "TOMHI", "CRASH", "RIDE", "SHAKER",
         "CONGA", "RIM", "COWBELL", "CLAVE", "KICK2", "SNARE2"]
WAVE = {None: 0, "sine": 1, "tri": 2, "square": 3, "fm": 4, "bell": 5}
SRC = {None: 0, "white": 1, "metal": 2, "cym": 3, "chip": 4, "clap": 0x11}
FMODE = {None: 0, "lp": 1, "bp": 2, "hp": 3}
LEVELS_FILE = Path(__file__).with_name("drumkit_levels.json")


def note16(hz):                    # MIDI note and 1/16 semitones
    n = 69 + 12 * math.log2(hz / 440.0)
    n16 = max(0, min(127 * 16 + 15, int(round(n * 16))))
    return n16 >> 4, n16 & 15


def note(hz):
    return max(0, min(127, int(round(69 + 12 * math.log2(hz / 440.0)))))


def dec(ms):                       # DECAY_K: 5 ms .. 4 s, index 0..127
    ms = max(5.0, min(4000.0, ms))
    return int(round(127 * math.log(ms / 5.0) / math.log(800)))


def cut(hz):                       # CUTOFF_HZ: 30 Hz .. 16 kHz
    hz = max(30.0, min(16000.0, hz))
    return int(round(127 * math.log(hz / 30.0) / math.log(16000 / 30)))


STEPS_PER_OCT = 127 / math.log2(16000 / 30)


def S(wave=None, hz=100, bend=0, bt=20, hold=0, decay=100, tlev=110, t2=0.0, t2lev=0, click=0,
      src=None, nlev=0, nhold=0, ndec=50, filt=None, fenv=0.0, hpf=None, chip=None, drive=0, level=0.0):
    """wave/hz: the tone; bend semitones falling over bt ms; hold ms at full, then decay;
    t2: the second partial's ratio (0 = none) and level; click: the attack burst 0..127;
    src: the noise, its level, hold and decay; filt: (mode, Hz, resonance 0..1[, "all"]) on the
    noise (or everything); fenv: octaves the cutoff starts above it, falling with the pitch;
    hpf: Hz of the noise high-pass; chip: Hz of the CHIP noise clock; drive 0..127; level dB"""
    return dict(wave=wave, hz=hz, bend=bend, bt=bt, hold=hold, decay=decay, tlev=tlev, t2=t2,
                t2lev=t2lev, click=click, src=src, nlev=nlev, nhold=nhold, ndec=ndec, filt=filt,
                fenv=fenv, hpf=hpf, chip=chip, drive=drive, level=level)


def enc(s, trim=0.0):
    p, f = note16(s["hz"])
    flt, fcut = 0, 127
    if s["filt"]:
        mode, hz, res = s["filt"][:3]
        flt = FMODE[mode] | (4 if "all" in s["filt"][3:] else 0) | (int(round(res * 31)) << 3)
        fcut = cut(hz)
    lev = int(round(128 + 4 * (s["level"] + trim)))
    v = [WAVE[s["wave"]], SRC[s["src"]], p, f, int(s["bend"]), dec(s["bt"]),
         min(255, int(round(s["hold"] / 2))), dec(s["decay"]), int(s["tlev"]) if s["wave"] else 0,
         int(round(s["t2"] * 32)) if s["t2"] else 0, int(s["t2lev"]), int(s["click"]),
         int(s["nlev"]) if s["src"] else 0, min(255, int(round(s["nhold"] / 2))), dec(s["ndec"]),
         flt, fcut, int(round(s["fenv"] * STEPS_PER_OCT)),
         0 if s["hpf"] is None else cut(s["hpf"]), note(s["chip"]) if s["chip"] else 0,
         int(s["drive"]), max(0, min(255, lev))]
    return v


def kit(base, **mods):
    k = copy.deepcopy(base)
    for lane, ch in mods.items():
        if isinstance(ch, dict) and "wave" in ch:
            k[lane] = ch
        else:
            k[lane].update(ch)
    return k


# ---- base kits ---------------------------------------------------------------------------
K808 = dict(
    KICK=S("sine", 50, 12, 32, 8, 820, 124, click=26, drive=8),
    KICK2=S("sine", 42, 7, 60, 70, 1700, 124, click=14, drive=34),                  # the long boom
    SNARE=S("sine", 185, 3, 14, 0, 120, 96, 1.78, 92, 22, "white", 100, 0, 170, ("hp", 1700, .12)),
    SNARE2=S("tri", 250, 4, 10, 0, 70, 84, 1.5, 60, 30, "white", 96, 0, 120, ("bp", 5200, .3)),
    CLAP=S(None, src="clap", nlev=122, ndec=230, filt=("bp", 1150, .38), hpf=600),
    CHH=S(None, 205, src="metal", nlev=118, ndec=42, filt=("bp", 9200, .42), hpf=6500, click=10),
    OHH=S(None, 205, src="metal", nlev=114, nhold=20, ndec=380, filt=("bp", 9000, .4), hpf=6200),
    TOMLO=S("sine", 98, 6, 60, 10, 420, 118, 1.5, 40, 16, "white", 18, 0, 30, ("lp", 2000, .1)),
    TOMHI=S("sine", 147, 6, 60, 10, 360, 116, 1.5, 40, 16, "white", 18, 0, 30, ("lp", 2600, .1)),
    CRASH=S(None, 205, src="cym", nlev=108, nhold=10, ndec=1800, filt=("hp", 4600, .22)),
    RIDE=S(None, 262, src="metal", nlev=104, ndec=1300, filt=("bp", 5200, .6), hpf=3000, click=12),
    SHAKER=S(None, src="white", nlev=96, nhold=12, ndec=55, filt=("bp", 7200, .3)),
    CONGA=S("sine", 310, 3, 25, 4, 230, 116, 1.5, 30, 24),
    RIM=S("tri", 1700, 0, 5, 0, 22, 92, 1.47, 70, 40, "white", 40, 0, 6, ("bp", 2600, .4, "all")),
    COWBELL=S("bell", 540, 0, 5, 6, 290, 108, filt=("bp", 2600, .5, "all")),
    CLAVE=S("sine", 2500, 0, 5, 0, 48, 108, click=12),
)
K909 = kit(K808,
    KICK=S("sine", 52, 30, 24, 14, 340, 124, click=64, drive=36, src="white", nlev=40, ndec=6,
           filt=("lp", 5000, .1)),
    KICK2=S("sine", 57, 36, 18, 8, 220, 124, click=84, drive=72),
    SNARE=S("tri", 190, 7, 15, 0, 110, 92, 1.6, 70, 30, "white", 116, 0, 220, ("hp", 1000, .1),
            drive=22),
    SNARE2=S("tri", 230, 6, 10, 0, 70, 86, 1.6, 60, 40, "white", 110, 0, 140, ("bp", 6200, .3)),
    CLAP=dict(nlev=124, ndec=290, filt=("bp", 1300, .42), hpf=700),
    CHH=S(None, 230, src="cym", nlev=114, ndec=55, filt=("hp", 8200, .25), hpf=7000, click=12),
    OHH=S(None, 230, src="cym", nlev=110, nhold=20, ndec=460, filt=("hp", 7800, .25), hpf=6800),
    TOMLO=S("sine", 110, 10, 40, 10, 320, 118, 1.5, 36, 22, "white", 26, 0, 30, ("lp", 3000, .1)),
    TOMHI=S("sine", 165, 10, 40, 10, 280, 116, 1.5, 36, 22, "white", 26, 0, 30, ("lp", 3600, .1)),
    CRASH=S(None, 250, src="cym", nlev=110, nhold=10, ndec=1900, filt=("hp", 5000, .2)),
    RIDE=S(None, 300, src="cym", nlev=108, ndec=1500, filt=("bp", 6200, .45), hpf=4000, click=14),
    RIM=S("tri", 1900, 2, 4, 0, 18, 96, 1.47, 60, 48, "white", 60, 0, 6, ("bp", 3000, .4, "all")),
)
K606 = kit(K808,
    KICK=S("sine", 60, 18, 26, 6, 200, 124, click=34, drive=10),
    KICK2=S("sine", 52, 14, 30, 10, 320, 124, click=24, drive=20),
    SNARE=S("tri", 220, 3, 10, 0, 70, 80, 1.7, 50, 24, "white", 104, 0, 120, ("hp", 2500, .15)),
    CHH=S(None, 260, src="metal", nlev=116, ndec=30, filt=("bp", 10000, .45), hpf=8500, click=8),
    OHH=S(None, 260, src="metal", nlev=110, nhold=10, ndec=210, filt=("bp", 9800, .45), hpf=8000),
    TOMLO=S("sine", 150, 4, 30, 6, 190, 114, click=12),
    TOMHI=S("sine", 220, 4, 30, 6, 170, 112, click=12),
    CRASH=S(None, 300, src="metal", nlev=104, ndec=900, filt=("hp", 6000, .3)),
)
KCR78 = kit(K808,
    KICK=S("sine", 65, 8, 20, 4, 170, 120, click=10, filt=("lp", 1800, .2, "all")),
    KICK2=S("sine", 58, 6, 20, 4, 240, 120, click=8, filt=("lp", 1400, .2, "all")),
    SNARE=S("sine", 240, 2, 10, 0, 60, 74, 1.6, 50, 10, "white", 76, 0, 95, ("bp", 3200, .3)),
    SNARE2=S("sine", 300, 2, 8, 0, 40, 70, 1.5, 40, 12, "white", 70, 0, 60, ("bp", 4200, .3)),
    CLAP=S(None, src="white", nlev=92, nhold=8, ndec=140, filt=("bp", 6200, .35), hpf=4000),  # tambourine
    CHH=S(None, 280, src="metal", nlev=106, ndec=25, filt=("bp", 10500, .5), hpf=9000),
    OHH=S(None, 280, src="metal", nlev=100, nhold=6, ndec=160, filt=("bp", 10000, .5), hpf=8500),
    TOMLO=S("sine", 180, 2, 20, 4, 170, 112, click=8),
    TOMHI=S("sine", 260, 2, 20, 4, 150, 110, click=8),
    SHAKER=S(None, src="white", nlev=92, nhold=6, ndec=30, filt=("bp", 8000, .35)),
    CONGA=S("sine", 400, 2, 15, 4, 130, 112, click=14),
    RIM=S("sine", 1600, 0, 5, 0, 30, 100, 1.5, 50, 30),
    COWBELL=S("bell", 800, 0, 5, 4, 160, 104, filt=("bp", 3000, .5, "all")),
)
K707 = kit(K808,                                                # the 80s machines
    KICK=S("sine", 58, 20, 22, 10, 260, 124, click=56, drive=22, src="white", nlev=40, ndec=6,
           filt=("lp", 4500, .1)),
    KICK2=S("sine", 50, 16, 26, 14, 380, 124, click=40, drive=30),
    SNARE=S("tri", 200, 6, 15, 6, 130, 88, 1.62, 70, 36, "white", 112, 10, 270, ("hp", 900, .15),
            drive=30),
    SNARE2=S("tri", 220, 6, 12, 0, 90, 84, 1.62, 60, 40, "white", 112, 0, 200, ("bp", 4800, .3)),
    CLAP=dict(nlev=124, ndec=320, filt=("bp", 1400, .45)),
    CHH=S(None, 240, src="cym", nlev=106, ndec=40, filt=("hp", 8000, .3), hpf=7000, click=10),
    OHH=S(None, 240, src="cym", nlev=102, nhold=20, ndec=320, filt=("hp", 7500, .3), hpf=6800),
    TOMLO=S("sine", 90, 12, 80, 14, 520, 120, 1.5, 40, 24, "white", 22, 0, 40, ("lp", 2500, .1)),
    TOMHI=S("sine", 140, 12, 80, 14, 440, 118, 1.5, 40, 24, "white", 22, 0, 40, ("lp", 3000, .1)),
)
KCHIP = dict(
    KICK=S("square", 70, 30, 40, 4, 130, 104),
    KICK2=S("tri", 60, 36, 50, 10, 220, 116),
    SNARE=S("square", 220, 12, 20, 0, 40, 60, src="chip", nlev=106, nhold=6, ndec=130, chip=1700),
    SNARE2=S(None, src="chip", nlev=108, nhold=4, ndec=90, chip=3500),
    CLAP=S(None, src="chip", nlev=108, nhold=10, ndec=110, chip=2800),
    CHH=S(None, src="chip", nlev=96, ndec=30, chip=11000),
    OHH=S(None, src="chip", nlev=92, nhold=10, ndec=220, chip=10000),
    TOMLO=S("square", 110, 14, 80, 4, 160, 100),
    TOMHI=S("square", 165, 14, 80, 4, 140, 100),
    CRASH=S(None, src="chip", nlev=96, nhold=20, ndec=900, chip=4200),
    RIDE=S("square", 1500, 0, 5, 0, 300, 50, src="chip", nlev=60, ndec=200, chip=12500),
    SHAKER=S(None, src="chip", nlev=84, ndec=45, chip=13000),
    CONGA=S("tri", 330, 5, 30, 4, 120, 110),
    RIM=S("square", 1200, 0, 5, 0, 25, 90),
    COWBELL=S("square", 700, 0, 5, 4, 150, 90),
    CLAVE=S("square", 2000, 0, 5, 0, 35, 90),
)
KFM = dict(
    KICK=S("fm", 55, 40, 30, 8, 220, 120, click=30, drive=30),
    KICK2=S("fm", 48, 30, 40, 20, 420, 120, drive=50),
    SNARE=S("fm", 200, 18, 20, 0, 90, 90, src="white", nlev=90, ndec=140, filt=("bp", 4000, .3)),
    SNARE2=S("fm", 330, 12, 10, 0, 60, 90, src="white", nlev=80, ndec=90, filt=("hp", 3000, .3)),
    CLAP=S(None, src="clap", nlev=118, ndec=180, filt=("bp", 1500, .4), hpf=1000),
    CHH=S("fm", 4200, 0, 5, 0, 30, 70, src="white", nlev=70, ndec=30, filt=("hp", 8000, .2)),
    OHH=S("fm", 4200, 0, 5, 0, 220, 64, src="white", nlev=64, ndec=220, filt=("hp", 7000, .2)),
    TOMLO=S("fm", 120, 30, 120, 6, 260, 110),
    TOMHI=S("fm", 200, 30, 120, 6, 220, 108),
    CRASH=S("fm", 900, 0, 5, 0, 1200, 50, src="cym", nlev=90, ndec=1300, filt=("hp", 4000, .2)),
    RIDE=S("fm", 1300, 0, 5, 0, 900, 60, src="metal", nlev=50, ndec=700, filt=("bp", 6000, .5)),
    SHAKER=S(None, src="white", nlev=80, nhold=8, ndec=50, filt=("bp", 7000, .3)),
    CONGA=S("fm", 330, 12, 40, 4, 160, 108),
    RIM=S("fm", 1500, 4, 5, 0, 25, 96, click=30),
    COWBELL=S("fm", 600, 0, 5, 4, 200, 96),
    CLAVE=S("fm", 2300, 0, 5, 0, 40, 100),
)

KITS = [
    # -- the machines
    ("808", "HIP HOP", 0, K808),
    ("909", "HOUSE", 0, K909),
    ("606", "ACID", 0, K606),
    ("80S", "80S POP", 0, K707),
    ("VINTAGE", "RHYTHM BOX", 0, KCR78),
    # -- hip-hop
    ("TRAP", "TRAP", 0, kit(K808,
        KICK=S("sine", 52, 18, 26, 10, 520, 124, click=50, drive=30),
        KICK2=S("sine", 46, 12, 40, 80, 1900, 124, click=20, drive=48),            # the 808, distorted
        SNARE=S("tri", 210, 6, 10, 0, 90, 80, 1.7, 60, 40, "white", 116, 6, 150, ("hp", 2300, .2), drive=26),
        CLAP=dict(nlev=126, ndec=170, filt=("bp", 1500, .4), hpf=1100),
        CHH=dict(ndec=26, filt=("bp", 10500, .45), hpf=9000),
        OHH=dict(ndec=260))),
    ("DRILL", "UK DRILL", 0, kit(K808,
        KICK=S("sine", 50, 14, 30, 8, 380, 124, click=44, drive=26),
        KICK2=S("sine", 44, 12, 220, 40, 1900, 124, click=16, drive=44),            # the sliding 808
        SNARE=S("tri", 270, 8, 8, 0, 60, 84, 1.6, 60, 46, "white", 108, 4, 110, ("hp", 2600, .25), drive=30),
        SNARE2=S("tri", 330, 6, 6, 0, 40, 80, 1.5, 50, 50, "white", 100, 0, 80, ("bp", 5500, .4)),
        CHH=dict(ndec=28, filt=("bp", 9800, .5), hpf=8500),
        RIM=dict(hz=2000, decay=30, tlev=104))),
    ("BOOMBAP", "HIP HOP", 0x13, kit(K909,
        KICK=S("sine", 56, 14, 22, 12, 280, 124, click=40, drive=70, src="white", nlev=70, ndec=10,
               filt=("lp", 3000, .15)),
        KICK2=S("sine", 50, 10, 26, 16, 360, 124, click=30, drive=80, filt=("lp", 1800, .1, "all")),
        SNARE=S("tri", 175, 5, 15, 8, 140, 96, 1.7, 70, 34, "white", 112, 10, 200, ("bp", 3200, .25),
                drive=50),
        SNARE2=S("tri", 200, 5, 12, 0, 90, 90, 1.7, 60, 30, "white", 104, 0, 140, ("bp", 2600, .3), drive=40),
        CHH=dict(ndec=40, filt=("bp", 7000, .35), hpf=6000),
        OHH=dict(ndec=260, filt=("bp", 6800, .35), hpf=5500))),
    ("LO-FI", "LO-FI", 0x34, kit(K808,
        KICK=S("sine", 54, 10, 30, 10, 420, 120, click=18, drive=24, filt=("lp", 1300, .15, "all")),
        KICK2=S("sine", 48, 8, 30, 14, 560, 120, click=10, drive=30, filt=("lp", 900, .15, "all")),
        SNARE=dict(filt=("bp", 2400, .25), ndec=160, click=10),
        SNARE2=dict(filt=("bp", 1800, .3), ndec=120),
        CLAP=dict(filt=("bp", 1000, .3)),
        CHH=dict(filt=("bp", 5200, .3), hpf=3800, nlev=96), OHH=dict(filt=("bp", 5000, .3), hpf=3600, nlev=92),
        CRASH=dict(filt=("bp", 4000, .2), nlev=88), RIDE=dict(filt=("bp", 4200, .5), hpf=2800, nlev=80))),
    ("PHONK", "PHONK", 0x02, kit(K808,
        KICK=S("sine", 52, 20, 24, 10, 480, 124, click=60, drive=90),
        KICK2=S("sine", 45, 12, 50, 60, 1600, 124, click=30, drive=110),           # blown-out 808
        SNARE=S("tri", 230, 8, 10, 0, 80, 84, 1.6, 60, 44, "white", 116, 6, 140, ("hp", 2000, .25), drive=60),
        CLAP=dict(nlev=126, ndec=150, hpf=1200),
        CHH=dict(ndec=30), OHH=dict(ndec=220),
        COWBELL=S("bell", 560, 0, 5, 30, 520, 116, filt=("bp", 2400, .55, "all"), drive=40))),   # the melodic cowbell
    # -- dance
    ("HOUSE", "HOUSE", 0, kit(K909,
        KICK=dict(decay=380, drive=30, hold=18),
        OHH=dict(ndec=320, nlev=112), CLAP=dict(nlev=126, ndec=340),
        SHAKER=dict(ndec=70, nlev=98))),
    ("D.HOUSE", "DEEP HOUSE", 0, kit(K909,
        KICK=S("sine", 50, 22, 26, 20, 460, 124, click=36, drive=18, filt=("lp", 3000, .1, "all")),
        SNARE=dict(ndec=180, filt=("bp", 2200, .3), click=16, tlev=80),
        CLAP=dict(nlev=118, ndec=420, filt=("bp", 1100, .4)),
        CHH=dict(ndec=48, filt=("bp", 7500, .45), hpf=6200, nlev=100),
        OHH=dict(ndec=300, filt=("bp", 7200, .45), hpf=6000, nlev=100),
        SHAKER=dict(ndec=80, nlev=100, filt=("bp", 6000, .3)),
        RIM=dict(filt=("bp", 2200, .5, "all")))),
    ("TECHNO", "TECHNO", 0, kit(K909,
        KICK=S("sine", 50, 28, 22, 18, 420, 124, click=70, drive=96, src="white", nlev=50, ndec=8,
               filt=("lp", 4000, .2)),
        KICK2=S("sine", 46, 18, 30, 30, 900, 124, click=40, drive=110, filt=("lp", 900, .3, "all")),   # rumble
        CHH=dict(ndec=34, filt=("hp", 9500, .3), hpf=9000), OHH=dict(ndec=280, filt=("hp", 9000, .3)),
        RIDE=dict(nlev=100, ndec=900), RIM=dict(drive=40),
        CLAP=dict(ndec=210, filt=("bp", 1900, .45)))),
    ("MINIMAL", "MINIMAL", 0, kit(K808,
        KICK=S("sine", 58, 18, 12, 6, 160, 124, click=44, drive=12),
        KICK2=S("sine", 52, 10, 14, 8, 240, 124, click=30),
        SNARE=S("tri", 1100, 0, 5, 0, 18, 90, 1.5, 50, 40, "white", 60, 0, 25, ("hp", 3000, .3)),
        CLAP=dict(ndec=90), CHH=dict(ndec=18, filt=("hp", 10000, .3), hpf=9500), OHH=dict(ndec=120),
        CONGA=dict(decay=90, hz=420), RIM=dict(decay=12))),
    ("ELECTRO", "ELECTRO", 0, kit(K808,
        KICK=S("sine", 52, 24, 70, 10, 520, 124, click=40, drive=24),
        SNARE=dict(ndec=120, filt=("hp", 2000, .2), click=36),
        TOMLO=S("fm", 130, 30, 120, 8, 240, 112), TOMHI=S("fm", 220, 30, 120, 8, 200, 110),
        CHH=K606["CHH"], OHH=K606["OHH"])),
    ("DISCO", "DISCO", 0, kit(K909,
        KICK=dict(decay=260, drive=20),
        OHH=dict(ndec=520, nlev=114),
        TOMLO=S("sine", 120, 24, 300, 10, 600, 118, 1.5, 30), TOMHI=S("sine", 180, 24, 300, 10, 520, 116, 1.5, 30))),
    ("GARAGE", "UK GARAGE", 0, kit(K909,
        KICK=S("sine", 54, 26, 20, 10, 260, 124, click=60, drive=40),
        SNARE=S("tri", 240, 8, 10, 0, 80, 88, 1.6, 60, 50, "white", 112, 4, 120, ("hp", 2200, .25), drive=24),
        SNARE2=S("tri", 280, 6, 8, 0, 50, 84, 1.5, 50, 50, "white", 100, 0, 80, ("bp", 6000, .4)),
        CHH=dict(ndec=36, filt=("hp", 9500, .3)), SHAKER=dict(ndec=60, nlev=100),
        RIM=dict(hz=2100, decay=24))),
    ("JUNGLE", "DRUM & BASS", 0, kit(K909,
        KICK=S("sine", 58, 16, 18, 6, 200, 124, click=56, drive=50),
        SNARE=S("tri", 260, 9, 10, 4, 100, 94, 1.6, 70, 50, "white", 114, 6, 170, ("hp", 1500, .25), drive=40),
        SNARE2=S("tri", 300, 7, 8, 0, 60, 90, 1.6, 60, 50, "white", 106, 0, 110, ("bp", 5000, .35), drive=30),
        CHH=dict(ndec=30), RIDE=dict(nlev=100))),
    ("DUBSTEP", "BASS MUSIC", 0, kit(K909,
        KICK=S("sine", 48, 26, 24, 16, 420, 124, click=70, drive=64),
        SNARE=S("tri", 170, 8, 20, 10, 170, 98, 1.7, 70, 50, "white", 122, 30, 420, ("hp", 600, .2), drive=64),
        CLAP=dict(ndec=380), OHH=dict(ndec=300))),
    # -- the world
    ("DEMBOW", "REGGAETON", 0, kit(K808,
        KICK=dict(decay=520, drive=20, click=40),
        SNARE=S("tri", 230, 5, 10, 0, 80, 84, 1.7, 60, 36, "white", 100, 4, 130, ("hp", 1800, .2), drive=20),
        CLAP=dict(ndec=160), RIM=dict(tlev=104), SHAKER=dict(ndec=70, nlev=98))),
    ("AMAPIANO", "AMAPIANO", 0, kit(K808,
        KICK=S("sine", 54, 14, 20, 8, 300, 124, click=40, drive=16),
        KICK2=S("sine", 55, 24, 120, 20, 700, 124, 2.0, 50, 30, drive=56,             # the log drum
                filt=("lp", 1400, .5, "all"), fenv=1.5),
        SHAKER=S(None, src="white", nlev=104, nhold=16, ndec=70, filt=("bp", 6500, .35)),
        CLAP=dict(ndec=260), CONGA=dict(decay=260),
        COWBELL=dict(hz=900, decay=160))),
    ("AFRO", "AFROBEAT", 0, kit(K808,
        KICK=S("sine", 60, 10, 30, 6, 260, 120, click=20, filt=("lp", 2000, .1, "all")),
        SNARE=S("tri", 330, 3, 10, 0, 60, 92, 1.5, 50, 30, "white", 70, 0, 60, ("bp", 5000, .3)),
        CONGA=dict(decay=260, bend=2), TOMLO=S("sine", 160, 2, 20, 6, 260, 114, click=20),
        TOMHI=S("sine", 230, 2, 20, 6, 220, 112, click=20), SHAKER=dict(ndec=80, nlev=100),
        COWBELL=dict(hz=900, decay=180))),
    ("LATIN", "LATIN", 0, kit(KCR78,
        KICK=S("sine", 62, 6, 20, 6, 230, 116, click=16, filt=("lp", 1800, .1, "all")),
        TOMLO=S("sine", 200, 3, 15, 4, 200, 114, 1.6, 40, 40, "white", 26, 0, 15, ("bp", 4000, .3)),   # timbales
        TOMHI=S("sine", 300, 3, 15, 4, 180, 114, 1.6, 40, 40, "white", 26, 0, 15, ("bp", 4500, .3)),
        CONGA=dict(hz=330, decay=240, click=30), COWBELL=dict(hz=650, decay=240),
        CLAVE=dict(hz=2400, decay=55))),
    ("TRIBAL", "TRIBAL", 0, kit(K808,
        KICK=S("sine", 52, 14, 60, 10, 600, 124, click=30, src="white", nlev=30, ndec=10, filt=("lp", 1500, .2)),
        SNARE=S("sine", 160, 6, 30, 6, 260, 112, 1.6, 50, 30, "white", 40, 0, 60, ("lp", 3000, .2)),
        TOMLO=S("sine", 75, 8, 90, 10, 600, 120, 1.5, 40, 30, "white", 26, 0, 30, ("lp", 1500, .2)),
        TOMHI=S("sine", 115, 8, 90, 10, 520, 118, 1.5, 40, 30, "white", 26, 0, 30, ("lp", 2000, .2)),
        CHH=S(None, src="white", nlev=90, ndec=40, filt=("bp", 6000, .3), hpf=5000),
        OHH=S(None, src="white", nlev=92, nhold=10, ndec=160, filt=("bp", 5000, .3), hpf=4000),
        CONGA=dict(decay=320))),
    # -- synths and games
    ("SYNTHWV", "SYNTHWAVE", 0, kit(K707,
        SNARE=dict(ndec=360, drive=50, nlev=120, hold=10),
        TOMLO=S("sine", 85, 18, 160, 14, 600, 120, 1.5, 40, 24, "white", 24, 0, 50, ("lp", 2500, .1)),
        TOMHI=S("sine", 130, 18, 160, 14, 520, 118, 1.5, 40, 24, "white", 24, 0, 50, ("lp", 3000, .1)))),
    ("CHIP", "CHIPTUNE", 0x04, KCHIP),
    ("ARCADE", "VIDEO GAME", 0x02, KFM),
    ("GLITCH", "GLITCH", 0x25, kit(KFM,
        KICK=dict(decay=120, bt=12), SNARE=S(None, src="chip", nlev=112, ndec=70, chip=1300),
        CHH=S(None, src="chip", nlev=100, ndec=15, chip=13000), OHH=S(None, src="chip", nlev=96, ndec=90, chip=11500),
        CLAP=dict(ndec=80), TOMLO=dict(bt=40, decay=120), TOMHI=dict(bt=40, decay=100))),
    ("INDUSTR", "INDUSTRIAL", 0x02, kit(K909,
        KICK=dict(drive=127, decay=380, click=90), SNARE=dict(drive=110, ndec=260),
        CLAP=dict(nlev=127, ndec=280), CHH=dict(src="metal", ndec=60), OHH=dict(src="metal", ndec=500),
        TOMLO=S("fm", 90, 24, 200, 10, 400, 118, drive=100), TOMHI=S("fm", 140, 24, 200, 10, 340, 116, drive=100),
        CRASH=dict(src="metal", ndec=2200, nlev=118))),
    ("HYPER", "HYPERPOP", 0, kit(K909,
        KICK=S("sine", 54, 40, 20, 10, 380, 124, click=90, drive=110),
        SNARE=S("tri", 300, 10, 10, 0, 90, 100, 1.6, 60, 60, "white", 122, 6, 180, ("hp", 2000, .3), drive=80),
        CLAP=dict(nlev=127, ndec=240, hpf=1300),
        CHH=dict(filt=("hp", 10000, .3), ndec=30), OHH=dict(filt=("hp", 9500, .3)))),
    ("AMBIENT", "AMBIENT", 0, kit(K808,
        KICK=S("sine", 48, 8, 40, 10, 900, 104, filt=("lp", 700, .1, "all")),
        SNARE=dict(filt=("lp", 3500, .2), ndec=420, nlev=70, tlev=60, click=0),
        CLAP=dict(filt=("bp", 900, .3), ndec=500, nlev=90),
        CHH=dict(nlev=72, ndec=70, filt=("bp", 7000, .3), hpf=6000), OHH=dict(nlev=72, ndec=900),
        CRASH=dict(nlev=84, ndec=3500), RIDE=dict(nlev=64, ndec=2500, filt=("bp", 6000, .5)))),
    ("JAZZ", "JAZZ", 0, kit(K808,
        KICK=S("sine", 62, 8, 20, 4, 230, 100, click=24, src="white", nlev=30, ndec=8, filt=("lp", 1200, .1)),
        KICK2=S("sine", 70, 6, 16, 4, 160, 96, click=30, src="white", nlev=30, ndec=6, filt=("lp", 1400, .1)),
        SNARE=S("tri", 210, 2, 10, 0, 60, 40, src="white", nlev=92, nhold=30, ndec=320,     # brushes
                filt=("bp", 3500, .2)),
        SNARE2=S("tri", 220, 3, 10, 0, 90, 80, 1.6, 50, 40, "white", 100, 0, 150, ("bp", 2800, .25)),
        CHH=S(None, 300, src="cym", nlev=82, ndec=60, filt=("bp", 8000, .3), hpf=5000),
        OHH=S(None, 300, src="cym", nlev=82, nhold=10, ndec=500, filt=("bp", 7500, .3), hpf=4500),
        RIDE=S(None, 320, src="cym", nlev=90, ndec=2400, filt=("bp", 6500, .35), hpf=3500, click=16),
        TOMLO=S("sine", 110, 3, 30, 6, 400, 106, 1.5, 40, 20, "white", 16, 0, 40, ("lp", 2000, .1)),
        TOMHI=S("sine", 160, 3, 30, 6, 340, 104, 1.5, 40, 20, "white", 16, 0, 40, ("lp", 2500, .1)))),
]


# (2.7) the kicks heard on a small speaker: a 50 Hz sine is felt on a big system, but the FM-1's speaker,
# a phone or earbuds play nothing below ~100 Hz, so only the click was left ("a crackle, very quiet"). The
# sine and triangle kicks get the drive for harmonics up there (100 .. 300 Hz), a pitch sweep at least
# 18 semitones over 34 ms for the attack's body, a softer click; level_drumkits.py lets them peak as the
# sampled kits do and 2.5 dB louder. The 808 BOOM under it all is the same note
PUNCH_DRIVE, PUNCH_BEND, PUNCH_BT, PUNCH_CLICK = 36, 18, 34, 0.6


def punch(k):
    """the kick lanes of kit k, as heard on a small speaker (above)"""
    for lane in ("KICK", "KICK2"):
        d = k[lane]
        if d["wave"] not in ("sine", "tri"):
            continue
        d["drive"] = max(d["drive"], PUNCH_DRIVE)
        d["bend"] = max(d["bend"], PUNCH_BEND)
        d["bt"] = max(d["bt"], PUNCH_BT)
        d["click"] = int(d["click"] * PUNCH_CLICK)
    return k


def main(path):
    levels = json.loads(LEVELS_FILE.read_text()) if LEVELS_FILE.exists() else {}
    L = ["/* generated by tools/gen_drumkits.py */", "#pragma once",
         f"#define DS_NKITS {len(KITS)}u", "static const dkit_t DS_KITS[DS_NKITS] = {"]
    for name, style, crush, k in KITS:
        k = punch(copy.deepcopy(k))
        assert len(name) <= 8 and len(style) <= 12, name
        L.append(f'    {{"{name}", "{style}", 0x{crush:02X}, {{')
        for lane in LANES:
            v = enc(k[lane], levels.get(name, {}).get(lane, 0.0))
            assert len(v) == 22 and all(0 <= x <= 255 for x in v), (name, lane, v)
            assert -128 <= v[17] <= 127, (name, lane)
            L.append("        {" + ", ".join(str(x) for x in v) + f"}},   /* {lane} */")
        L.append("    }},")
    L.append("};")
    L.append("#define DS_KIT_NAME_LIST " + ", ".join(f'"{k[0]}"' for k in KITS))
    L.append("#define DS_KIT_STYLE_LIST " + ", ".join(f'"{k[1]}"' for k in KITS))
    Path(path).write_text("\n".join(L) + "\n")
    print(f"drum kits: {len(KITS)} synthesised -> {path}")


if __name__ == "__main__":
    main(sys.argv[1])
