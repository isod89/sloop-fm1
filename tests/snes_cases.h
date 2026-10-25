/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ugotworms */
/* The SNES engine's voice against snes_spc: the scenarios both sides play (tests/snes_ref.cpp on the
 * original S-DSP, tests/snes_test.c on eng_snes.c). One voice keyed on at sample 0; at koff_at either
 * KOFF (rel_gain 0) or the driver's release (ADSR off, GAIN = rel_gain); n samples. echo: the voice
 * into the echo (EON) at EVOL evol, EFB efb, EDL edl, these FIR coefficients (C0 .. C7). */
typedef struct {
    const char *what;
    const char *inst;                    /* the instrument's name: built in or the bank's (snes_bankfile.h) */
    uint16_t pitch;
    uint8_t adsr0, adsr1, gain;
    uint8_t non, nrate;                  /* noise instead of the sample, at this rate */
    int32_t koff_at;                     /* -1: never */
    uint8_t rel_gain;
    int32_t n;
    uint8_t echo;
    int8_t evol, efb;
    uint8_t edl;
    int8_t fir[8];
} snes_case_t;

#define FIR_FLAT {127, 0, 0, 0, 0, 0, 0, 0}
#define FIR_LOW {0x0C, 0x21, 0x2B, 0x2B, 0x13, -0x02, -0x0D, -0x07}
#define FIR_HIGH {0x58, -0x41, -0x25, -0x10, -0x02, 0x07, 0x0C, 0x0C}
#define FIR_BAND {0x34, 0x33, 0x00, -0x27, -0x1B, 0x01, -0x04, -0x15}
static const snes_case_t SNES_CASES[] = {
    {"SQUARE native, instant attack, held, KOFF", "SQUARE", 0x1000, 0x8F, 0xE0, 0, 0, 0, 4000, 0, 6000},
    {"SAW an octave down, AR 10 DR 7 SL 2 SR 5, KOFF", "SAW", 0x0800, 0xFA, 0x45, 0, 0, 0, 20000, 0, 24000},
    {"SINE at the top (0x3FFF), slow attack, GAIN exp release", "SINE", 0x3FFF, 0x83, 0xB4, 0, 0, 0, 30000, 0xB2, 40000},
    {"SYN STR odd pitch, GAIN linear-decrease release", "SYN STR", 0x1234, 0xC5, 0x7F, 0, 0, 0, 25000, 0x94, 32000},
    {"ORCHHIT native, plays to its end (no loop)", "ORCHHIT", 0x1000, 0x8F, 0xE0, 0, 0, 0, -1, 0, 30000},
    {"CHOIR lower, DR 5 SL 1 SR 10, KOFF", "CHOIR", 0x0A00, 0xDF, 0x2A, 0, 0, 0, 10000, 0, 14000},
    {"noise at rate 29, ADSR, KOFF", "SQUARE", 0x1000, 0xFF, 0x1C, 0, 1, 29, 5000, 0, 7000},
    {"noise at rate 12, slow", "SQUARE", 0x1000, 0x8C, 0xE0, 0, 1, 12, 9000, 0, 11000},
    {"GAIN direct 0x7F, KOFF", "SQUARE", 0x1000, 0x00, 0x00, 0x7F, 0, 0, 3000, 0, 4000},
    {"GAIN linear increase rate 10", "SAW", 0x0C00, 0x00, 0x00, 0xCA, 0, 0, 6000, 0, 8000},
    {"GAIN bent increase rate 12", "SINE", 0x1800, 0x00, 0x00, 0xEC, 0, 0, 6000, 0xA8, 9000},
    {"echo: SQUARE, EDL 4, EFB 0x50, low-pass FIR", "SQUARE", 0x1000, 0x8F, 0xE0, 0, 0, 0, 3000, 0, 16000,
     1, 0x60, 0x50, 4, FIR_LOW},
    {"echo: CHOIR, EDL 12, EFB -0x40, high-pass FIR", "CHOIR", 0x1000, 0x8F, 0xE0, 0, 0, 0, -1, 0, 30000,
     1, 0x7F, -0x40, 12, FIR_HIGH},
    {"echo: noise, EDL 1, EFB 0x70, band-pass FIR", "SQUARE", 0x1000, 0xFF, 0x1C, 0, 1, 29, 4000, 0, 12000,
     1, 0x50, 0x70, 1, FIR_BAND},
    {"echo: SAW, EDL 0 (one sample), EFB 0x40", "SAW", 0x1000, 0x8F, 0xE0, 0, 0, 0, 3000, 0, 6000,
     1, 0x40, 0x40, 0, FIR_FLAT},
    {"echo: SINE, EDL 7, EFB 0x7F (runs away, clamps)", "SINE", 0x1000, 0x8F, 0xE0, 0, 0, 0, 2000, 0, 20000,
     1, 0x7F, 0x7F, 7, FIR_FLAT},
};
#define SNES_NCASES (sizeof SNES_CASES / sizeof SNES_CASES[0])
