/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The SMK-37's wheels (main.c smk_wheels; voice.c applies them to the selected synth track). No hardware here: the
 * host tests (tests/smk_wheel_test.c) run it.
 * Pitch wheel: raw ADC reading -> 1/4096 semitone, +bend_up / -bend_dn semitones (HOME menu > MIDI > BEND UP, BEND
 * DOWN). Taught by
 * HARDWARE CALIBRATION (smk_wcal_t: the rest, full down, full up): the full travel gives the full range, the dead band
 * fits the wheel's noise, and at power-on a wheel resting within the dead band of the taught rest is the centre (a
 * wheel held at power-on: the taught rest). Not taught: the first reading is the centre, the ends start SMK_PW_SPAN0
 * away, the dead band is SMK_PW_DEAD. Either way the ends widen to what the wheel reaches, and while it rests in the
 * dead band the centre follows a slow drift, at most one step per SMK_PW_DRIFT_MS (in time, not in calls: the main
 * loop calls this thousands of times a second, and a centre that moves that fast is dragged along by a slow touch of
 * the wheel; the spring then brings the wheel back to its real rest, outside the dragged dead band, and a bend stays
 * on whatever track is selected).
 * Mod wheel: 0..127, up (a lower reading) = more; taught: from the bottom to full up. */
#pragma once
#include <stdint.h>

#define SMK_PW_DEAD 12            /* raw counts around the centre: no bend (not taught) */
#define SMK_PW_SPAN0 80           /* the range assumed until the wheel goes further (not taught) */
#define SMK_PW_DRIFT_MS 250u      /* the centre follows a resting wheel by one count per this */
#define SMK_MW_LO 485             /* the mod wheel's range assumed until it goes further (not taught) */
#define SMK_MW_HI 615
#define SMK_MW_DEAD 12

/* HARDWARE CALIBRATION's wheels (ui_input.c wheel_setup), kept in the settings (project.c, storage.c OBJ_WCAL);
 * older data, none, or a part out of range: that wheel calibrates itself as before */
#define SMK_WCAL_MAGIC 0x5743u    /* "WC" */
#define SMK_WCAL_EDGE 3           /* the taught ends move in by this: the full travel reaches the full range */
#define SMK_WCAL_MIN 30           /* the least travel (beyond the dead band) that counts as taught */
typedef struct {
    uint16_t magic, pdead;        /* pdead: the pitch wheel's dead band (counts) */
    int16_t plo, pc, phi;         /* pitch: full down, rest, full up (raw) */
    int16_t mrest, mtop;          /* mod: the bottom (its rest) and full up (raw: up reads lower) */
    uint16_t mdead;               /* mod: the dead band above the bottom */
} smk_wcal_t;
_Static_assert(sizeof(smk_wcal_t) == 16u, "the wheels' calibration: 16 bytes in flash");
static smk_wcal_t wcal __attribute__((unused));
static volatile uint8_t wcal_gen; /* changed by a calibration: smk_wheels starts over with it */

static int smk_wcal_pitch_ok(const smk_wcal_t *c)
{
    return c->magic == SMK_WCAL_MAGIC && c->pdead >= 2u && c->pdead <= 40u && c->plo >= 0 && c->phi <= 1023 &&
           c->pc - c->plo >= (int32_t)c->pdead + SMK_WCAL_MIN && c->phi - c->pc >= (int32_t)c->pdead + SMK_WCAL_MIN;
}
static int smk_wcal_mod_ok(const smk_wcal_t *c)
{
    return c->magic == SMK_WCAL_MAGIC && c->mdead >= 2u && c->mdead <= 40u && c->mtop >= 0 && c->mrest <= 1023 &&
           c->mrest - c->mtop >= (int32_t)c->mdead + SMK_WCAL_MIN;
}
static int32_t smk_wcal_dead(int32_t noise)          /* a dead band for the noise seen at rest */
{
    int32_t d = 2 * noise + 6;
    return d < 8 ? 8 : d > 24 ? 24 : d;
}
/* the pitch wheel taught: the readings at rest (lowest, highest), the lowest at full down, the highest at full up;
 * 1: taken, 0: too little travel (c unchanged) */
static int smk_wcal_pitch(smk_wcal_t *c, int32_t rlo, int32_t rhi, int32_t down, int32_t up)
{
    smk_wcal_t n = *c;
    n.magic = SMK_WCAL_MAGIC;
    n.pdead = (uint16_t)smk_wcal_dead(rhi - rlo);
    n.pc = (int16_t)((rlo + rhi) / 2);
    n.plo = (int16_t)(down + SMK_WCAL_EDGE);
    n.phi = (int16_t)(up - SMK_WCAL_EDGE);
    if (!smk_wcal_pitch_ok(&n))
        return 0;
    *c = n;
    return 1;
}
/* the mod wheel taught: the readings at the bottom (lowest, highest), the lowest at full up */
static int smk_wcal_mod(smk_wcal_t *c, int32_t rlo, int32_t rhi, int32_t up)
{
    smk_wcal_t n = *c;
    n.magic = SMK_WCAL_MAGIC;
    n.mdead = (uint16_t)smk_wcal_dead(rhi - rlo);
    n.mrest = (int16_t)((rlo + rhi) / 2);
    n.mtop = (int16_t)(up + SMK_WCAL_EDGE);
    if (!smk_wcal_mod_ok(&n))
        return 0;
    *c = n;
    return 1;
}

/* HOME menu > MIDI > BEND UP, BEND DOWN: the pitch wheel's range on the internal sounds, semitones up and down (0: that
 * way does not bend). Kept in the SMK-37 settings (project.c smkset_t; earlier SMK-37 builds: two spare bytes of the
 * MIDI part, read once), as bytes: UP's byte is
 * 2.6.4's BEND RANGE (0 = the default 7, BEND_NONE = 0); DOWN's: 0 = as UP, so 2.6.4's range (and older data: 7)
 * comes back as both */
#define BEND_N 8u
#define BEND_DEF 7u
#define BEND_NONE 0x80u                              /* (the byte of 0 semitones) */
static const uint8_t BEND_ST[BEND_N] = {0, 1, 2, 3, 4, 5, 7, 12};
static uint8_t bend_up = BEND_DEF, bend_dn = BEND_DEF;
static uint32_t bend_idx(uint32_t st)                /* its place in BEND_ST, BEND_N if none */
{
    uint32_t i;
    for (i = 0; i < BEND_N && BEND_ST[i] != st; i++)
        ;
    return i;
}
static uint32_t bend_of_byte(uint32_t b, uint32_t none)   /* a byte -> semitones; 0 or a bad one: none */
{
    if (b == BEND_NONE)
        return 0u;
    return b && bend_idx(b) < BEND_N ? b : none;
}
static uint32_t bend_up_of_byte(uint32_t b) { return bend_of_byte(b, BEND_DEF); }
static uint32_t bend_dn_of_byte(uint32_t b, uint32_t up) { return bend_of_byte(b, up); }
static uint32_t bend_up_byte(uint32_t st) { return st == BEND_DEF || bend_idx(st) >= BEND_N ? 0u : st ? st : BEND_NONE; }
static uint32_t bend_dn_byte(uint32_t st, uint32_t up) { return st == up || bend_idx(st) >= BEND_N ? 0u : st ? st : BEND_NONE; }
static uint32_t bend_step(uint32_t st, int32_t s)    /* a knob turn s (stops at the ends), 0: the next, round */
{
    uint32_t i = bend_idx(st);
    if (i >= BEND_N)
        return BEND_DEF;
    if (s > 0)
        return BEND_ST[i + 1u < BEND_N ? i + 1u : i];
    if (s < 0)
        return BEND_ST[i ? i - 1u : 0u];
    return BEND_ST[(i + 1u) % BEND_N];
}

typedef struct {
    int32_t pc, plo, phi, dead;   /* centre, lowest and highest reading, dead band (pc 0: not started yet) */
    uint32_t drift_ms;            /* when the centre last moved (or the wheel left the dead band) */
} smk_pw_t;

/* the bend for reading p, up to +st_up / -st_dn semitones (1/4096 semitone) */
static int32_t smk_pw_bend(smk_pw_t *w, const smk_wcal_t *c, int32_t p, uint32_t now_ms, uint32_t st_up, uint32_t st_dn)
{
    int32_t d, b, up, dn, full;
    if (!w->pc) {
        if (smk_wcal_pitch_ok(c)) {               /* taught: re-centred if it rests now, else the taught rest */
            d = p - c->pc;
            w->dead = c->pdead;
            w->pc = d > -w->dead && d < w->dead ? p : c->pc;
            w->plo = c->plo;
            w->phi = c->phi;
        } else {
            w->dead = SMK_PW_DEAD;
            w->pc = p;
            w->plo = p - SMK_PW_SPAN0;
            w->phi = p + SMK_PW_SPAN0;
        }
        if (!w->pc)
            w->pc = 1;
        w->drift_ms = now_ms;
    }
    if (p < w->plo)
        w->plo = p;
    if (p > w->phi)
        w->phi = p;
    d = p - w->pc;
    if (d > -w->dead && d < w->dead) {
        if (now_ms - w->drift_ms >= SMK_PW_DRIFT_MS) {   /* resting: the centre follows a slow drift */
            w->pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            w->drift_ms = now_ms;
        }
        return 0;
    }
    w->drift_ms = now_ms;                                /* (bent: the drift waits until it rests again) */
    up = w->phi - w->pc - w->dead;
    dn = w->pc - w->plo - w->dead;
    full = (int32_t)(d > 0 ? (st_up > 12u ? 12u : st_up) : (st_dn > 12u ? 12u : st_dn)) * 4096;   /* (0: no bend) */
    b = d > 0 ? (d - w->dead) * full / (up > 1 ? up : 1) : (d + w->dead) * full / (dn > 1 ? dn : 1);
    return b < -full ? -full : b > full ? full : b;
}

typedef struct {
    int32_t lo, hi, dead;         /* full up, the bottom (raw), dead band (hi 0: not started yet) */
} smk_mw_t;

/* the mod wheel for reading m: 0..127 */
static int32_t smk_mw_val(smk_mw_t *w, const smk_wcal_t *c, int32_t m)
{
    int32_t v, span;
    if (!w->hi) {
        if (smk_wcal_mod_ok(c)) {
            w->lo = c->mtop;
            w->hi = c->mrest;
            w->dead = c->mdead;
        } else {
            w->lo = m < SMK_MW_LO ? m : SMK_MW_LO;
            w->hi = m > SMK_MW_HI ? m : SMK_MW_HI;
            w->dead = SMK_MW_DEAD;
        }
    }
    if (m < w->lo)
        w->lo = m;
    if (m > w->hi)
        w->hi = m;
    span = w->hi - w->lo - w->dead;
    v = w->hi - m <= w->dead ? 0 : (w->hi - m - w->dead) * 127 / (span > 1 ? span : 1);
    return v < 0 ? 0 : v > 127 ? 127 : v;
}
