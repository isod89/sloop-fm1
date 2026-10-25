# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ugotworms
"""The SNES engine's instrument bank (firmware/src/eng_snes.c): what tools/fetch_snes_cc0.py fetches and
tools/gen_brr.py builds. An SNES game kept its whole set of instruments in the sound chip's 64 KiB: short
attacks, short loops, low sample rates. This bank is built the same way, from CC0 recordings (Versilian
Studios' VSCO-2 CE and VCSL), Felucca's generated drums (tools/gen_waves.py) and a few synthesized waves.

Each instrument:
  name    its preset's name, and INST's value (the screen shows a value's first 5 characters)
  kind    "loop": a sustained note, an attack then a loop of whole cycles
          "decay": a struck / plucked note, its attack then a loop of its tail (the envelope's SR decays it)
          "oneshot": a hit played to its end (drums)
  src     ("cc0", repo, folder, file regex with (?P<n>NOTE)): fetch_snes_cc0.py picks the note
          ("cc0", repo, folder, file regex, note): that file, its note given
          ("asset", path under assets/snes-cc0, note, cents): a file already in the tree (from-samples-cc0/:
          two of Felucca's CC0 recordings), its pitch as measured
          ("genwav", name): build/genwav/<name>.wav (tools/gen_waves.py, Felucca's own drums)
          ("synth", what): made by gen_brr.py
  rate    the sample rate it is stored at (the chip plays 32 kHz at pitch 0x1000)
  top     the highest note it should reach: the chip's pitch goes 2 octaves above the root at most, so
          the recorded note is chosen (and the root follows the rate) to reach it
  att     seconds of attack before the loop; loop: seconds the loop should last at least
  named   semitones a file's note name is below its sound: VSCO-2 CE and VCSL mostly name octaves one below the
          usual ("C5" sounds C6), a few instruments do not (0); fetch_snes_cc0.py measures each recording anyway
  preset  {INST is the instrument} TUNE, SR, NOISE, ECHO, DELAY, FDBK, FIR; env ATK DEC SUS REL; mono; fx sends
"""

VSCO, VCSL = "VSCO-2-CE", "VCSL"
NOTE = r"(?P<n>[A-G]#?-?\d)"


def I(name, kind, src, rate=24000, top=96, att=0.08, loop=0.035, gain=1.0, length=0.6, preset=None, named=12):
    return dict(name=name, kind=kind, src=src, rate=rate, top=top, att=att, loop=loop, gain=gain, length=length,
                preset=preset or {}, named=named)


# preset helpers: env (ATK DEC SUS REL), SR, echo (ECHO DELAY FDBK FIR), mono, sends (DIST CHOR DLY REV), pattern
def P(env, sr=0, echo=(30, 6, 20, 1), mono=0, fx=(0, 0, 0, 30), pat=0, tune=0, noise=0):
    return dict(env=env, sr=sr, echo=echo, mono=mono, fx=fx, pat=pat, tune=tune, noise=noise)


SUSTAIN = P((10, 40, 120, 40))
PAD = P((60, 30, 120, 70), echo=(45, 10, 35, 1), fx=(0, 30, 0, 40), pat=5)
STRUCK = P((0, 0, 127, 50), sr=10, fx=(0, 10, 0, 30), pat=6)
MALLET = P((0, 0, 127, 40), sr=14, echo=(35, 8, 25, 1), fx=(0, 0, 0, 30), pat=7)
BASS = P((0, 30, 110, 20), echo=(0, 4, 0, 1), mono=1, fx=(0, 0, 0, 10), pat=2, tune=-12)
LEAD = P((5, 40, 110, 30), echo=(40, 9, 22, 1), mono=1, fx=(0, 0, 0, 20), pat=4)

INSTRUMENTS = [
    # --- synthesized, the chip's own kind of sound (32 kHz single cycles and short loops)
    I("SQUARE", "loop", ("synth", "square"), preset=LEAD),
    I("PULSE", "loop", ("synth", "pulse25"), preset=LEAD),
    I("SAW", "loop", ("synth", "saw"), preset=P((0, 60, 80, 0), mono=1, echo=(0, 4, 0, 1), pat=2, tune=-12)),
    I("SINE", "loop", ("synth", "sine"), preset=P((40, 30, 110, 40), echo=(45, 6, 30, 1), pat=3)),
    I("SYN STR", "loop", ("synth", "strings"), preset=PAD),
    I("SYNBASS", "loop", ("synth", "synbass"), top=72, preset=BASS),
    I("CHOIR", "loop", ("synth", "choir"), top=84, preset=P((70, 30, 120, 70), echo=(50, 12, 40, 1), fx=(0, 30, 0, 50), pat=5)),
    I("ORGAN", "loop", ("synth", "organ"), preset=P((0, 0, 127, 15), echo=(25, 6, 15, 1), fx=(0, 40, 0, 30), pat=6)),
    # --- keys and mallets
    I("PIANO", "decay", ("asset", "from-samples-cc0/PIANO.wav", 72, 2), rate=22050, att=0.12, preset=STRUCK),
    I("E.PIANO", "decay", ("cc0", VCSL, "Electrophones/TX81Z/FM Piano", r"FMPiano_" + NOTE + r"_vl2\.wav"), rate=22050,
      att=0.10, named=0, preset=P((0, 0, 127, 50), sr=9, echo=(35, 8, 25, 1), fx=(0, 40, 0, 30), pat=6)),
    I("HARPSI", "decay", ("cc0", VCSL, "Chordophones/Zithers/Harpsichord, French/Sustains",
                          r"Harpsi2_Normal_" + NOTE + r"_rr1_Main\.wav"), rate=22050, preset=STRUCK),
    I("GLOCK", "decay", ("cc0", VSCO, "Percussion/Glock", r"glock_medium_" + NOTE + r"\.wav"), rate=26000, top=108,
      att=0.05, preset=MALLET),
    I("VIBES", "decay", ("cc0", VCSL, "Idiophones/Struck Idiophones/Vibraphone/Soft Mallets",
                         r"Vibes_soft_" + NOTE + r"_v2_rr1_Main\.wav"), rate=22050,
      preset=P((0, 0, 127, 50), sr=8, echo=(35, 8, 25, 1), fx=(0, 20, 0, 40), pat=7)),
    I("MARIMBA", "decay", ("cc0", VSCO, "Percussion/Marimba", r"Marimba_hit_Outrigger_" + NOTE + r"_loud_01\.wav"),
      rate=20000, top=100, att=0.05, preset=P((0, 0, 127, 30), sr=17, echo=(30, 6, 20, 1), pat=3)),
    I("XYLO", "decay", ("cc0", VSCO, "Percussion/Xylo", r"Xylo_Medium_" + NOTE + r"_ff_01_far\.wav"), rate=22050, top=108,
      att=0.04, preset=P((0, 0, 127, 25), sr=19, echo=(30, 5, 20, 1), pat=3)),
    I("BELLS", "decay", ("cc0", VCSL, "Idiophones/Struck Idiophones/Tubular Bells 1", r"chimes_" + NOTE + r"_ff_rr\d\.wav"),
      rate=20000, top=84, att=0.10, loop=0.06,
      preset=P((0, 0, 127, 70), sr=6, echo=(45, 10, 30, 1), fx=(0, 0, 0, 50), pat=7)),
    I("KALIMBA", "decay", ("cc0", VCSL, "Idiophones/Plucked Idiophones/Kalimba, Tanzania",
                           r"MBira3_pluck_Main_" + NOTE + r"_k\d+_50_100_rr2\.wav"), rate=22050, att=0.05,
      preset=P((0, 0, 127, 40), sr=15, echo=(40, 7, 30, 1), pat=3)),
    # --- strings (ensembles: longer loops, so the section's chorus is not a buzz)
    I("STRINGS", "loop", ("cc0", VSCO, "Strings/Violin Section/susVib", r"VlnEns_susVib_" + NOTE + r"_v2\.wav"),
      top=98, att=0.10, loop=0.10, preset=PAD),
    I("LO STR", "loop", ("cc0", VSCO, "Strings/Cello Section/susvib", r"susvib_" + NOTE + r"_v3_1\.wav"), rate=18000,
      top=72, att=0.10, loop=0.10, preset=PAD),
    I("VIOLIN", "loop", ("cc0", VSCO, "Strings/Solo Violin/Arco Vib", r"LLVln_ArcoVib_" + NOTE + r"_f\.wav"), rate=26000,
      top=100, loop=0.08, named=0, preset=P((25, 30, 120, 40), echo=(40, 8, 25, 1), mono=1, fx=(0, 0, 0, 40), pat=4)),
    I("PIZZ", "decay", ("cc0", VSCO, "Strings/Violin Section/Pizz", r"VlnEns_Pizz_" + NOTE + r"_v2_rr1\.wav"), rate=20000,
      top=90, att=0.06, preset=P((0, 0, 127, 25), sr=19, echo=(30, 6, 20, 1), pat=3)),
    I("TREMOLO", "loop", ("cc0", VSCO, "Strings/Violin Section/Trem", r"VlnEns_Trem_" + NOTE + r"_v2\.wav"), rate=18000,
      top=92, loop=0.12, preset=PAD),
    I("HARP", "decay", ("cc0", VSCO, "Strings/Harp", r"KSHarp_" + NOTE + r"_mf\.wav"), rate=22050,
      named=0, preset=P((0, 0, 127, 60), sr=9, echo=(40, 9, 30, 1), fx=(0, 0, 0, 40), pat=13)),
    I("BASS", "decay", ("cc0", VSCO, "Strings/Solo Contrabass/Pizz", r"BKCtbss_Pizz_" + NOTE + r"_v1_rr1\.wav"), rate=14000,
      top=60, preset=P((0, 0, 127, 20), sr=12, echo=(0, 4, 0, 1), mono=1, fx=(0, 0, 0, 10), pat=2)),
    # --- brass
    I("TRUMPET", "loop", ("cc0", VSCO, "Brass/Trumpet/sus", r"Sum_SHTrumpet_sus_" + NOTE + r"_v3_rr1\.wav"),
      preset=P((8, 40, 115, 30), echo=(35, 8, 25, 1), mono=1, fx=(0, 0, 0, 30), pat=4)),
    I("HORN", "loop", ("cc0", VSCO, "Brass/F Horn/sus", r"MOHorn_sus_" + NOTE + r"_v2_1\.wav"), rate=18000, top=80, att=0.10,
      preset=P((15, 40, 120, 40), echo=(40, 10, 30, 1), fx=(0, 0, 0, 40), pat=5)),
    I("TROMBNE", "loop", ("cc0", VSCO, "Brass/Tenor Trombone/sus", r"tenortbn_sus_" + NOTE + r"_v2_1\.wav"), rate=18000,
      top=76, preset=P((8, 40, 115, 30), echo=(30, 8, 20, 1), fx=(0, 0, 0, 30), pat=8)),
    I("TUBA", "loop", ("cc0", VSCO, "Brass/Tuba/sus", r"Tuba3_sus_" + NOTE + r"_v2_rr1_Mid\.wav"), rate=14000, top=64,
      preset=BASS),
    # --- woodwinds
    I("FLUTE", "loop", ("cc0", VSCO, "Woodwinds/Flute/susNV", r"LDFlute_susNV_" + NOTE + r"_v1_1\.wav"), rate=26000, top=98,
      preset=P((20, 30, 115, 40), echo=(45, 7, 30, 1), mono=1, fx=(0, 0, 0, 40), pat=3)),
    I("OBOE", "loop", ("cc0", VSCO, "Woodwinds/Oboe/Sus", r"Oboe_Sus_" + NOTE + r"_v3_Main\.wav"), top=92,
      preset=P((10, 30, 115, 35), echo=(40, 8, 25, 1), mono=1, fx=(0, 0, 0, 35), pat=3)),
    I("CLARNET", "loop", ("cc0", VSCO, "Woodwinds/Clarinet/susLong", r"DCClar_susLong_" + NOTE + r"_v2_rr1_sum\.wav"),
      rate=22050, top=92, preset=P((10, 30, 115, 35), echo=(40, 8, 25, 1), mono=1, fx=(0, 0, 0, 35), pat=3)),
    I("BASSOON", "loop", ("cc0", VSCO, "Woodwinds/Bassoon/sus", r"PSBassoon_" + NOTE + r"_v2_1\.wav"), rate=18000, top=72,
      preset=P((10, 30, 115, 30), echo=(25, 6, 15, 1), mono=1, fx=(0, 0, 0, 20), pat=2)),
    I("RECORDR", "loop", ("cc0", VCSL, "Aerophones/Edge-blown Aerophones/Baroque Alto Recorder/Sustain",
                          r"AltRecorder_Sus_" + NOTE + r"_rr1_Main\.wav"), att=0.06, preset=LEAD),
    I("OCARINA", "loop", ("cc0", VCSL, "Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains/Sus",
                          r"StdOcarina_Sus_" + NOTE + r"\.wav"), att=0.06,
      preset=P((15, 30, 115, 40), echo=(45, 9, 30, 1), mono=1, fx=(0, 0, 0, 40), pat=3)),
    I("HARMNCA", "loop", ("cc0", VCSL, "Aerophones/Free Aerophones/Harmonica-Hohner-Super64/Sustains/Normal",
                          r"Hohner-Super64_Normal _" + NOTE + r"\.wav"), rate=22050, preset=LEAD),
    I("SAX", "loop", ("asset", "from-samples-cc0/SAX.wav", 70, -1), rate=22050, preset=LEAD),
    # --- plucked, percussion
    I("GUITAR", "decay", ("cc0", VCSL, "Chordophones/Composite Chordophones/Strumstick/Finger",
                          r"Strumstick_Finger_Str\d_Main_" + NOTE + r"_vl2_rr1\.wav"), rate=22050, top=88, att=0.10,
      preset=P((0, 0, 127, 40), sr=11, echo=(35, 8, 25, 1), fx=(0, 20, 0, 30), pat=13)),
    I("TIMPANI", "decay", ("cc0", VSCO, "Percussion/Timpani", r"Timpani2_Hit_v3_rr1_Sum\.wav", 47), rate=14000, top=60,
      att=0.18, loop=0.06, preset=P((0, 0, 127, 60), sr=9, echo=(40, 10, 30, 1), fx=(0, 0, 0, 40), pat=8)),
    I("ORCHHIT", "oneshot", ("synth", "orchhit"), rate=16000, length=0.32,
      preset=P((0, 0, 127, 50), echo=(45, 8, 30, 1), fx=(0, 0, 0, 40), pat=1)),
]

# each preset's place in the PRESETS browser (firmware/src/ui.c BANK[]: its kinds)
KIND = {"BASS": ["SAW", "SYNBASS", "BASS", "TUBA", "BASSOON"],
        "KEYS": ["PIANO", "E.PIANO", "HARPSI"],
        "ORGN": ["ORGAN"],
        "PAD": ["SYN STR", "CHOIR", "STRINGS", "LO STR", "TREMOLO"],
        "LEAD": ["SQUARE", "PULSE", "SINE", "VIOLIN", "TRUMPET", "FLUTE", "OBOE", "CLARNET", "RECORDR", "OCARINA",
                 "HARMNCA", "SAX"],
        "PLCK": ["GLOCK", "VIBES", "MARIMBA", "XYLO", "BELLS", "KALIMBA", "PIZZ", "HARP", "GUITAR"],
        "STAB": ["HORN", "TROMBNE", "ORCHHIT"],
        "FX": ["TIMPANI", "KIT", "E.KIT"]}

# built into the firmware (INST 0 ..): single cycles of a few dozen bytes; every other instrument, the kits and your
# own samples make the bank in the user slots (gen_brr.py --bank), after them in INST
FIRMWARE = ["SQUARE", "PULSE", "SAW", "SINE", "SYNBASS", "ORGAN"]

# drum pieces: hits played to their end (cymbals: a looped tail the envelope decays), at the low rates and short
# lengths the games used
DRUMS = [
    I("KICK", "oneshot", ("genwav", "D KICK"), rate=12000, length=0.25),
    I("SNARE", "oneshot", ("cc0", VCSL, "Membranophones/Struck Membranophones/Snare Drum, Modern 1",
                           r"Snare2_HitSN_v5_rr1_Mid\.wav", 60), rate=16000, length=0.22),
    I("RIM", "oneshot", ("genwav", "D RIM"), rate=16000, length=0.08),
    I("CLAP", "oneshot", ("cc0", VCSL, "Idiophones/Struck Idiophones/Claps", r"Clap_rr1\.wav", 60), rate=16000, length=0.20),
    I("CHAT", "oneshot", ("cc0", VCSL, "Idiophones/Struck Idiophones/Hi-Hat Cymbal", r"HiHat_HitC_v3_rr1_Mid\.wav", 60),
      rate=26000, length=0.07),
    I("OHAT", "decay", ("cc0", VCSL, "Idiophones/Struck Idiophones/Hi-Hat Cymbal", r"HiHat_HitO_rr1_Mid\.wav", 60),
      rate=22050, att=0.08, loop=0.05),
    I("TOM", "oneshot", ("cc0", VCSL, "Membranophones/Struck Membranophones/Tom 2/Stick", r"TomL_HitS_v3_rr1_Mid\.wav", 60),
      rate=12000, length=0.30),
    I("CRASH", "decay", ("cc0", VSCO, "VSCO 1 Percussion/varMetal/Cymbals/clash", r"crash_hit_ff_loose\.wav", 60),
      rate=22050, att=0.12, loop=0.08),
    I("RIDE", "decay", ("genwav", "D RIDE"), rate=22050, att=0.08, loop=0.06),
    I("TAMB", "oneshot", ("cc0", VCSL, "Idiophones/Struck Idiophones/Tambourine 1", r"Tamb1_Hit_v2_rr1_Mid\.wav", 60),
      rate=22050, length=0.15),
    I("COWBELL", "oneshot", ("cc0", VCSL, "Idiophones/Struck Idiophones/Cowbells", r"Cowbell1_Normal_v3_rr1_Mid\.wav", 60),
      rate=16000, length=0.15),
    I("E.SNARE", "oneshot", ("genwav", "D SNARE"), rate=16000, length=0.18),
    I("E.CHAT", "oneshot", ("genwav", "D CHAT"), rate=26000, length=0.06),
    I("E.OHAT", "decay", ("genwav", "D OHAT"), rate=22050, att=0.06, loop=0.05),
    I("E.CLAP", "oneshot", ("genwav", "D CLAP"), rate=16000, length=0.20),
    I("E.COWBL", "oneshot", ("genwav", "D COWBELL"), rate=16000, length=0.15),
    I("E.TOM", "oneshot", ("genwav", "D TOM LO"), rate=12000, length=0.28),
    I("E.CRASH", "decay", ("genwav", "D CRASH"), rate=22050, att=0.10, loop=0.07),
]

# the kits: General MIDI note -> (piece, semitones from its native pitch). Toms: one piece, tuned by key
_TOMS = {41: -5, 43: -3, 45: 0, 47: 3, 48: 5, 50: 8}
KITS = {
    "KIT": {35: ("KICK", -1), 36: ("KICK", 0), 37: ("RIM", 0), 38: ("SNARE", 0), 39: ("CLAP", 0), 40: ("SNARE", 1),
            42: ("CHAT", 0), 44: ("CHAT", -1), 46: ("OHAT", 0), 49: ("CRASH", 0), 51: ("RIDE", 0), 52: ("CRASH", -2),
            53: ("RIDE", 2), 54: ("TAMB", 0), 55: ("CRASH", 3), 56: ("COWBELL", 0), 57: ("CRASH", 1), 59: ("RIDE", -1),
            **{n: ("TOM", d) for n, d in _TOMS.items()}},
    "E.KIT": {35: ("KICK", -2), 36: ("KICK", 0), 37: ("RIM", 0), 38: ("E.SNARE", 0), 39: ("E.CLAP", 0),
              40: ("E.SNARE", 1), 42: ("E.CHAT", 0), 44: ("E.CHAT", -1), 46: ("E.OHAT", 0), 49: ("E.CRASH", 0),
              51: ("RIDE", 0), 52: ("E.CRASH", -2), 53: ("RIDE", 2), 56: ("E.COWBL", 0), 57: ("E.CRASH", 1),
              59: ("RIDE", -1), **{n: ("E.TOM", d) for n, d in _TOMS.items()}},
}
# a kit: its drums play to their end (a key-off does not stop them); SR decays the cymbals' looped tails
KIT_PRESET = P((0, 0, 127, 0), sr=15, echo=(20, 5, 10, 1), fx=(0, 0, 0, 20), pat=12)

# the presets of your own samples (assets/snes-local): a looped one held, a one-shot played to its end
OWN_LOOP = P((0, 20, 120, 30), echo=(30, 6, 20, 1), fx=(0, 0, 0, 30), pat=3)
OWN_HIT = P((0, 0, 127, 40), echo=(25, 5, 15, 1), fx=(0, 0, 0, 20), pat=12)
