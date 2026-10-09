/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table: order = PRESETS browsing order (and the engine numbers of the editor protocol). */
#include "dsp.c"
/* SLOOP 2.5: the big per-part state of GRAIN, FM6 and PHYS shares one arena per synth part (a part plays one
 * engine at a time: on a switch the old engine fades out, its voices end, then the new one starts, voice.c
 * engine_block). The first engine to ask for it after another one gets it cleared, as at power-on. */
static void *eng_arena_of(const track_t *t, uint32_t eng);
static int eng_arena_held(const track_t *t, uint32_t eng);   /* is eng's state in it now (no claim) */
static inline int32_t voice_amp(int32_t s, const vmod_t *m, uint32_t i) { return mulq15(mulq15(s, amp_at(m, i)), VOICE_FS); }
static inline int32_t soft_knee(int32_t y, int32_t k)   /* linear up to k, then only the peaks saturate */
{
    int32_t a = y < 0 ? -y : y;
    if (a <= k)
        return y;
    a = k + (softclip((a - k) * 2) >> 1);
    return y < 0 ? -a : a;
}
/* (pitch_inc: felucca_tables.h, SLOOP 2.5: from the top octave) */
#include "eng_analog.c"
#include "eng_digital.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_drawbar.c"
#include "eng_grain.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0); SLOOP 2.4 */
#include "eng_phys.c"           /* PHYS: physical models, from Felucca 1.0 (DaisySP, Rings: MIT); SLOOP 2.5 */
#include "eng_noise.c"          /* NOISE: from Felucca 1.0; SLOOP 2.5 */
#if FELUCCA_SNES
#include "eng_snes.c"           /* SNES: the S-DSP, ported from snes_spc (LGPL-2.1-or-later) */
#endif

typedef union {
    gr_part_t grain;
    fm6_note_t fm6[FM6_POLY];
    uint32_t phys[PHYS_ARENA / 4u];      /* eng_phys.c: two SYMP slots or three small ones */
#if FELUCCA_SNES
    int16_t snes[SNES_ELINE];            /* eng_snes.c: the echo line */
#endif
} eng_arena_t;
#if FELUCCA_SNES
_Static_assert(sizeof(int16_t[SNES_ELINE]) <= sizeof(gr_part_t), "the SNES echo line within GRAIN's state: no more pool");
#endif
static eng_arena_t eng_arena[NPART] __attribute__((section(".pool")));
static uint8_t eng_arena_own[NPART];            /* the engine + 1 whose state is in it, 0 = none */
#ifdef ARENA_STATS
static uint32_t eng_arena_claims, eng_arena_bad;   /* host tests: claims, accesses by an engine the part does not play */
#endif
static void *eng_arena_of(const track_t *t, uint32_t eng)
{
    uint32_t p = (uint32_t)(t - trk);
    if (p >= NPART)
        return 0;
#ifdef ARENA_STATS
    if (t->engine != eng)
        eng_arena_bad++;
#endif
    if (eng_arena_own[p] != eng + 1u) {
        memset(&eng_arena[p], 0, sizeof eng_arena[p]);
        eng_arena_own[p] = (uint8_t)(eng + 1u);
#ifdef ARENA_STATS
        eng_arena_claims++;
#endif
    }
    return &eng_arena[p];
}
static int eng_arena_held(const track_t *t, uint32_t eng)
{
    uint32_t p = (uint32_t)(t - trk);
    return p < NPART && eng_arena_own[p] == eng + 1u;
}
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif

static const engine_t *const ENGINES[NENGINES] = {&ENG_ANALOG, &ENG_DIGITAL, &ENG_PHASE, &ENG_LOFI, &ENG_SAMPLE,
                                                    &ENG_FORMANT, &ENG_TRIO, &ENG_DRAWBAR, &ENG_GRAIN,
                                                    &ENG_FM6,    /* 9 (ENGI_FM6): always; SLICE after it (core.h) */
                                                    &ENG_PHYS, &ENG_NOISE,
#if FELUCCA_SNES
                                                    &ENG_SNES,   /* 12 (ENGI_SNES) */
#endif
#if FELUCCA_SLICE
                                                    &ENG_SLICE,
#endif
};
_Static_assert(ENGI_FM6 == 9u, "ENGINES[ENGI_FM6] is FM6");
_Static_assert(ENGI_GRAIN == 8u && ENGI_PHYS == 10u && ENGI_NOISE == 11u && ENGI_SNES == 12u &&
               ENGI_SLICE == 12u + FELUCCA_SNES, "ENGINES[] order");

/* every factory sound as loud as the others: a level trim per preset, 1/2 dB, measured on a phrase
 * that fits the sound (tools/level_presets.py writes preset_trim.h); a track keeps it in P_ED_FX */
#include "preset_trim.h"
static int16_t preset_trim(uint32_t e, uint32_t pi)
{
#if !FELUCCA_SNES
    if (e >= 12u)                                   /* (the table has SNES's row: 12, SLICE 13) */
        e++;
#endif
    return e < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[e][pi] : 0;
}
