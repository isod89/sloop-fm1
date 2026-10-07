/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: four slots in RAM (the song sections A..D). With FELUCCA_FLASH every save also goes to
 * flash through storage.c, and an empty RAM slot is filled from flash on load. The working project is
 * also kept in flash by itself (autosave, when the transport is stopped and nothing sounds) and comes
 * back at power-on: SLOOP starts where you left it.
 *
 * Eight tracks, in two halves of one flash sector each: project_t (the globals, tracks 1..4: format 4
 * exactly, so the four-track SLOOP still loads them) and project_b_t (tracks 5..8, "FUN5", carrying the
 * sum of the half it belongs with). The first halves of the slots stay in .noinit (they survive resets
 * and UBOOT entry), the second ones live in .pool (refilled from flash at boot). A save writes the second
 * half first: cut off before the first is written, the other copy of the second half (st_load_other) is
 * the one that matches the first. A first half with no matching second one (a four-track project) loads
 * tracks 5..8 with their power-on sounds, empty (proj_b_default).
 *
 * Formats: 4 ("FUN4", written, SLOOP 2.0): today's P_COUNT / G_COUNT, 10-byte steps (levels and
 * ratchets; the drum track: 16 lanes). Read and converted: 3 ("FUN3", SLOOP 1.x: 8-byte steps, the
 * drum track's notes become its lanes, the swings x 0.8 for the MPC scale), 2 ("FUN2") and 1 ("FUN1"),
 * which held PROJ_NP_V2 parameters per track, mapped by count as user presets are (the first
 * PROJ_NP_V2 - 8 are P_LEVEL.. in order, the last 8 P_E0..P_E7; the parameters added since take their
 * defaults). Their engine bytes are kept: formats 1 and 2 had engines 0..7 (ANALOG .. WHEEL), and the
 * engines added since were appended, no index moved; the drum track's byte (it has no engine) becomes 0.
 *
 * Built on the host too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP), drums.c (the lanes), the engines and trk_def_engine (ui.c). */
#define PROJ_MAGIC 0x46554E34u                 /* "FUN4": four tracks, P_COUNT parameters each, 10-byte steps */
#define PROJ_MAGIC_V3 0x46554E33u              /* "FUN3": SLOOP 1.x; read only */
#define PROJ_MAGIC_V2 0x46554E32u              /* "FUN2": four tracks, PROJ_NP_V2 parameters; read only */
#define PROJ_MAGIC_V1 0x46554E31u              /* "FUN1": one instrument; loads into track 1 */
#define PROJ_NP_V3 57u                         /* P_COUNT of format 3 (P_E0 was 49) */
#define PROJ_NG_V3 27u                         /* G_COUNT of formats 1..3 */
#define PROJ_NP_V2 53u                         /* P_COUNT of formats 1 and 2 (P_E0 was 45) */
#define PROJ_NG_V2 27u                         /* G_COUNT of formats 1 and 2 */
typedef struct {                               /* one track; the drum track ignores engine / preset */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    union {
        step_t step[NSTEP];
        dstep_t dstep[NSTEP];                  /* (the drum track: 16 lanes, the same size) */
    };
} proj_trk_t;
typedef struct {                               /* the first half: format 4 */
    uint32_t magic, size;
    int16_t g[G_COUNT];
    uint8_t sel, rsv[3];                       /* the selected track (0..7) */
    proj_trk_t t[NTRK_V4];                     /* tracks 1..4 */
    uint32_t sum;
} project_t;
#define PROJ_MAGIC_B 0x46554E35u               /* "FUN5": tracks 5..8, the second half */
#define NTRK_B (NTRK - NTRK_V4)
typedef struct {
    uint32_t magic, size;
    uint32_t asum;                             /* the sum of the first half it goes with */
    proj_trk_t t[NTRK_B];                      /* tracks 5..8 */
    uint32_t sum;
} project_b_t;
_Static_assert(NTRK_V4 == TRK_DRUM + 1u, "the drum track is in the first half (as format 4 had it)");
typedef struct { uint8_t note[4], n, time, flags, vel; } step8_t;   /* the steps of formats 1..3 */
typedef struct {                               /* a track of format 3, read only */
    int16_t p[PROJ_NP_V3];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v3_t;
typedef struct {                               /* format 3 (SLOOP 1.x), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V3];
    uint8_t sel, rsv[3];
    proj_trk_v3_t t[NTRK_V4];
    uint32_t sum;
} project_v3_t;
typedef struct {                               /* a track of formats 1 and 2, read only */
    int16_t p[PROJ_NP_V2];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v2_t;
typedef struct {                               /* format 2 (until 0.9), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, rsv[3];
    proj_trk_v2_t t[NTRK_V4];
    uint32_t sum;
} project_v2_t;
typedef struct {                               /* format 1 (until 0.5 beta), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    proj_trk_v2_t t;
    uint32_t sum;
} project_v1_t;
_Static_assert(sizeof(project_v2_t) == 2552u && sizeof(project_v1_t) == 688u && sizeof(project_v3_t) == 2584u,
               "formats 1 / 2 / 3 as they were stored");
project_t proj_slot[4] __attribute__((section(".noinit")));
static project_b_t proj_slot_b[4] __attribute__((section(".pool")));

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q && q->sum == proj_sum(q); }
static uint32_t projb_sum(const project_b_t *b) { return proj_hash(b, sizeof *b - 4u); }
static int projb_ok(const project_b_t *b, const project_t *a)   /* a valid second half, the one of a */
{
    return b->magic == PROJ_MAGIC_B && b->size == sizeof *b && b->sum == projb_sum(b) && b->asum == a->sum;
}

/* a track as at power-on, no steps: its engine's defaults, preset 0xFF (project_apply loads its preset) */
static void proj_trk_default(proj_trk_t *d, uint32_t i)
{
    uint32_t k, e = trk_def_engine(i);
    for (k = 0; k < P_COUNT; k++)
        d->p[k] = k >= P_E0 ? ENGINES[e]->edit[k - P_E0].def : TP[k].def;
    d->engine = (uint8_t)e;
    d->preset = 0xFF;
    memset(d->step, 0, sizeof d->step);
    if (i != TRK_DRUM)
        for (k = 0; k < NSTEP; k++)
            d->step[k].time = ST_REST;
}

/* the second half of a four-track project a: tracks 5..8 as at power-on */
static void proj_b_default(project_b_t *b, const project_t *a)
{
    uint32_t i;
    memset(b, 0, sizeof *b);
    b->magic = PROJ_MAGIC_B;
    b->size = sizeof *b;
    for (i = 0; i < NTRK_B; i++)
        proj_trk_default(&b->t[i], NTRK_V4 + i);
    b->asum = a->sum;
    b->sum = projb_sum(b);
}

/* ---- old formats -> format 4 */
/* an old step into a synth step (no level, no ratchet) */
static void step_from8(step_t *d, const step8_t *s)
{
    memcpy(d->note, s->note, 4);
    d->n = s->n;
    d->time = s->time;
    d->flags = s->flags;
    d->vel = s->vel;
    d->lvl = d->rat = 0;
}
/* an old drum step (GM notes) into the drum track's lanes; its velocity / accent -> their level */
static void dstep_from8(dstep_t *d, const step8_t *s)
{
    uint32_t i, lvl = (s->flags & SF_ACCENT) || s->vel > 115u ? LV_HARD : !s->vel ? LV_NORM : vel_lvl(s->vel);
    memset(d, 0, sizeof *d);
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++)
        dstep_set(d, lane_of_note(s->note[i] & 127u), lvl, 0);
}
static int16_t swing_from_v3(int32_t v) { return (int16_t)clamp((v * 4 + 2) / 5, 0, 100); }   /* /250 -> /200 */

/* the globals of formats 1..3 (G_* unchanged since; any added later: their defaults) */
static void proj_g_from_old(int16_t *g, const int16_t *g2)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        g[i] = i < PROJ_NG_V3 ? g2[i] : GP[i].def;
    g[G_SWING] = swing_from_v3(g[G_SWING]);
}

/* a track of format 3 -> today's (by id up to P_SLDEPTH; P_E0.. moved) */
static void proj_trk_from_v3(proj_trk_t *d, const proj_trk_v3_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V3 - 8u;
    for (k = 0; k < P_E0; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->p[P_E0 + k] = s->p[nc + k];
    d->p[P_SSWING] = swing_from_v3(d->p[P_SSWING]);
    d->p[P_ASWING] = swing_from_v3(d->p[P_ASWING]);
    d->engine = drum ? 0u : s->engine;
    d->preset = drum ? 0u : s->preset;
    for (k = 0; k < NSTEP; k++) {
        if (drum)
            dstep_from8(&d->dstep[k], &s->step[k]);
        else
            step_from8(&d->step[k], &s->step[k]);
    }
}

/* a track of formats 1 and 2 -> format 3 (mapped by count, see the top) */
static void proj_trk_v2_to_v3(proj_trk_v3_t *d, const proj_trk_v2_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V2 - 8u;
    for (k = 0; k < PROJ_NP_V3 - 8u; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->p[PROJ_NP_V3 - 8u + k] = s->p[nc + k];
    d->engine = drum ? 0u : s->engine;          /* (indices 0..7 as they were) */
    d->preset = drum ? 0u : s->preset;
    memcpy(d->step, s->step, sizeof d->step);
}

/* a format 3 project -> slot q as format 4 */
static void proj_from_v3_ok(project_t *q, const project_v3_t *v3)
{
    uint32_t i;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_old(q->g, v3->g);
    q->sel = v3->sel;
    for (i = 0; i < NTRK_V4; i++)
        proj_trk_from_v3(&q->t[i], &v3->t[i], i == TRK_DRUM);
    q->sum = proj_sum(q);
}
static int proj_from_v3(project_t *q, const project_v3_t *v3, int n)
{
    if (n != (int)sizeof *v3 || v3->magic != PROJ_MAGIC_V3 || v3->size != sizeof *v3 ||
        v3->sum != proj_hash(v3, sizeof *v3 - 4u))
        return 0;
    proj_from_v3_ok(q, v3);
    return 1;
}

static project_v3_t proj_v3_tmp;               /* (formats 1, 2: through format 3) */
/* a format 2 project (n bytes in *v2) -> slot q as format 4 */
static int proj_from_v2(project_t *q, const project_v2_t *v2, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v2 || v2->magic != PROJ_MAGIC_V2 || v2->size != sizeof *v2 ||
        v2->sum != proj_hash(v2, sizeof *v2 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v2->g, sizeof v3->g);
    v3->sel = v2->sel;
    for (i = 0; i < NTRK_V4; i++)
        proj_trk_v2_to_v3(&v3->t[i], &v2->t[i], i == TRK_DRUM);
    proj_from_v3_ok(q, v3);
    return 1;
}

/* a format 1 project (n bytes in *v1) -> slot q as format 4: the instrument becomes track 1,
 * tracks 2..4 start empty (their sounds as at power-on) */
static int proj_from_v1(project_t *q, const project_v1_t *v1, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v1 || v1->magic != PROJ_MAGIC_V1 || v1->size != sizeof *v1 ||
        v1->sum != proj_hash(v1, sizeof *v1 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v1->g, sizeof v3->g);
    proj_trk_v2_to_v3(&v3->t[0], &v1->t, 0);
    proj_from_v3_ok(q, v3);
    for (i = 1; i < NTRK_V4; i++)              /* the other tracks: their defaults, no steps */
        proj_trk_default(&q->t[i], i);
    q->sum = proj_sum(q);
    return 1;
}

/* n bytes of a stored project (any format) -> slot q as format 4; 0 = not a project */
static int proj_import(project_t *q, const void *b, int n)
{
    if (n == (int)sizeof *q && proj_ok((const project_t *)b)) {
        memcpy(q, b, sizeof *q);
        return 1;
    }
    return proj_from_v3(q, (const project_v3_t *)b, n) || proj_from_v2(q, (const project_v2_t *)b, n) ||
           proj_from_v1(q, (const project_v1_t *)b, n);
}

/* ---- the working project <-> a project (its two halves) */
static void proj_trk_capture(proj_trk_t *d, const track_t *t)
{
    memcpy(d->p, t->p, sizeof t->p);
    d->engine = t->eng_req;
    d->preset = t->preset;
    memcpy(d->step, t->step, sizeof t->step);
}
static void proj_capture(project_t *p, project_b_t *b)   /* what is playing now, as a project */
{
    uint32_t i;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++)
        p->g[i] = song.g[i];
    p->sel = song.sel;
    for (i = 0; i < NTRK_V4; i++)
        proj_trk_capture(&p->t[i], &trk[i]);
    p->sum = proj_sum(p);
    memset(b, 0, sizeof *b);
    b->magic = PROJ_MAGIC_B;
    b->size = sizeof *b;
    for (i = 0; i < NTRK_B; i++)
        proj_trk_capture(&b->t[i], &trk[NTRK_V4 + i]);
    b->asum = p->sum;
    b->sum = projb_sum(b);
}

/* a project's tracks (and its globals, all: a load; or only the drum level / reverb: a song
 * section) into the working one, every value back inside its range. The audio ISR must not run
 * meanwhile (the song sections: called from it; a load: IRQ off) */
static void proj_apply(const project_t *p, const project_b_t *b, int all)
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        if (all ? i != G_SLOT && i != G_LOAD && i != G_SAVE && i != G_SYNC : i == G_DRLVL || i == G_DRREV)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = k < NTRK_V4 ? &p->t[k] : &b->t[k - NTRK_V4];
        uint32_t e = trk_is_part(k) ? s->engine % NENGINES : 0u;
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range */
            const param_desc_t *d = k == TRK_DRUM && i == P_E0 ? &DRUM_KIT_DESC :   /* the drum kit */
                                    i >= P_E0 && i <= P_E7 ? &ENGINES[e]->edit[i - P_E0] : &TP[i];
            t->p[i] = (int16_t)clamp(s->p[i], d->min, d->max);
        }
        t->preset = (uint8_t)(ENGINES[e]->npresets ? (s->preset == 0xFFu ? 0u : s->preset) % ENGINES[e]->npresets : 0u);
        memcpy(t->step, s->step, sizeof t->step);
        if (k != TRK_DRUM)
            for (i = 0; i < NSTEP; i++) {
                step_t *st = &t->step[i];
                uint32_t j;
                if (st->n > 4u)
                    st->n = 4;
                if (st->time > ST_REST)
                    st->time = ST_REST;
                for (j = 0; j < 4u; j++)
                    st->note[j] &= 127u;
            }
    }
}

#ifndef PROJ_HOST
#if FELUCCA_ARRANGER
#include "arranger_scene.c"
#endif
static uint8_t sec_dirty, song_dirty;           /* live sections / the song: in RAM, not yet in flash */
#if FELUCCA_FLASH
/* slot from flash into RAM (format 4, or an old one converted) */
static union {
    project_t v4;
    project_v3_t v3;
    project_v2_t v2;
    project_v1_t v1;
    project_b_t b;
    struct { project_t a; project_b_t b; } ab;   /* (editor.c: the backup snapshot of the working project) */
} proj_tmp;

/* the second half of first half a from flash object obj: its current copy, else the other one (a save
 * cut off between the halves); else tracks 5..8 as at power-on. 1: found */
static int proj_fetch_b(uint32_t obj, project_b_t *b, const project_t *a)
{
    int n = st_load(obj, &proj_tmp.b, sizeof proj_tmp.b);
    if (n != (int)sizeof *b || !projb_ok(&proj_tmp.b, a))
        n = st_load_other(obj, &proj_tmp.b, sizeof proj_tmp.b);
    if (n == (int)sizeof *b && projb_ok(&proj_tmp.b, a)) {
        memcpy(b, &proj_tmp.b, sizeof *b);
        return 1;
    }
    proj_b_default(b, a);
    return 0;
}
static void proj_fetch(uint32_t slot)
{
    project_t *q = &proj_slot[slot & 3u];
    int n = st_load(OBJ_PROJECT0 + (slot & 3u), &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n)) {
        q->magic = 0;
        return;
    }
    proj_fetch_b(OBJ_PROJB0 + (slot & 3u), &proj_slot_b[slot & 3u], q);
}
/* both halves into flash, the second first (see the top); 0: ok */
static int proj_store(uint32_t obja, uint32_t objb, const project_t *a, const project_b_t *b)
{
    return st_save(objb, b, sizeof *b) || st_save(obja, a, sizeof *a);
}
#endif

/* the second half of slot (whose first half is valid): as it is, else from flash, else made */
static project_b_t *proj_b_of(uint32_t slot)
{
    project_b_t *b = &proj_slot_b[slot & 3u];
    const project_t *a = &proj_slot[slot & 3u];
    if (!projb_ok(b, a)) {
#if FELUCCA_FLASH
        if (flash_ok)
            proj_fetch_b(OBJ_PROJB0 + (slot & 3u), b, a);
        else
#endif
            proj_b_default(b, a);
    }
    return b;
}

static void project_save(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
    project_b_t *b = &proj_slot_b[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
#endif
    proj_capture(p, b);
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(proj_store(OBJ_PROJECT0 + (slot & 3u), OBJ_PROJB0 + (slot & 3u), p, b) ? "SAVE ERROR" : "SAVED");
        return;
    }
#endif
    ui_message("SAVED (RAM)");
}

/* a project into the working one: the transport stops, everything sounding is released */
static void project_apply(const project_t *p, const project_b_t *b)
{
    uint32_t k;
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    proj_apply(p, b, 1);
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    for (k = 0; k < NTRK; k++) {                        /* tracks as at power-on (format 1: 2, 3; format 4: 5..8) */
        const proj_trk_t *s = k < NTRK_V4 ? &p->t[k] : &b->t[k - NTRK_V4];
        if (trk_is_part(k) && s->preset == 0xFFu) {
            apply_preset_to(&trk[k], TRK_DEF[k][1]);
            steps_clear(&trk[k]);
        }
    }
    sync_reload = 1;
    ui.force = 1;
}

static void project_load(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE LOAD"); return; }
#endif
#if FELUCCA_FLASH
    if (flash_ok && !proj_ok(p))
        proj_fetch(slot);
#endif
    if (!proj_ok(p)) {
        ui_message("EMPTY SLOT");
        return;
    }
    project_apply(p, proj_b_of(slot));
    ui_message("LOADED");
}

/* ---- the working project, kept in flash by itself: saved when it changed, the transport is stopped,
 * nothing sounds and the panel was not touched for AUTOSAVE_IDLE (a flash erase stops the audio for
 * ~50 ms: never while something plays); loaded at power-on (autosave_resume) */
#define AUTOSAVE_IDLE 2500u                    /* ms without input */
#define AUTOSAVE_GAP 20000u                    /* ms between two saves at least */
static project_t autosave_buf __attribute__((section(".pool")));
static project_b_t autosave_b;                  /* (RAM: the pool keeps its 8 KiB spare) */
static uint32_t autosave_hash, autosave_ms, autosave_checked;

static int audio_quiet(void)
{
    uint32_t p, i;
    for (p = 0; p < NTRK; p++)
        for (i = 0; trk_is_part(p) && i < NVOICE; i++)
            if (trk[p].v[i].active)
                return 0;
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active)
            return 0;
    return 1;
}

static void autosave_tick(void)                /* main loop */
{
#if FELUCCA_FLASH
    uint32_t h, now = fm1_ms;
    if (!flash_ok || song.playing || transport_req || rec_wait || ft_on || ui.menu ||
        now - ui_input_ms < AUTOSAVE_IDLE || now - autosave_ms < AUTOSAVE_GAP || now - autosave_checked < 1000u)
        return;
    autosave_checked = now;
    proj_capture(&autosave_buf, &autosave_b);
    h = autosave_b.sum;                            /* (it covers the first half too: asum) */
    if (h == autosave_hash || !audio_quiet())
        return;
    if (proj_store(OBJ_AUTOSAVE, OBJ_AUTOSAVE_B, &autosave_buf, &autosave_b) == 0)
        autosave_hash = h;
    autosave_ms = fm1_ms;
#endif
}

static void autosave_resume(void)              /* power-on: the project as it was left (felucca_init) */
{
#if FELUCCA_FLASH
    project_t *q = &autosave_buf;
    project_b_t *b = &autosave_b;
    int n;
    if (!flash_ok)
        return;
    n = st_load(OBJ_AUTOSAVE, &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n))
        return;
    if (!proj_fetch_b(OBJ_AUTOSAVE_B, b, q)) {    /* a four-track project: tracks 5..8 as felucca_init set them */
        for (n = 0; n < (int)NTRK_B; n++)
            proj_trk_capture(&b->t[n], &trk[NTRK_V4 + (uint32_t)n]);
        b->sum = projb_sum(b);
    }
    autosave_hash = b->sum;
    proj_apply(q, b, 1);
    song.sel = (uint8_t)(q->sel < NTRK ? q->sel : 0u);
    for (n = 0; n < (int)NTRK; n++)
        if (trk_is_part((uint32_t)n))
            trk[n].engine = trk[n].eng_req;    /* (nothing sounds yet: no fade) */
#endif
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
#if FELUCCA_ARRANGER
    arr_config_t arrangement;
#endif
    uint32_t lights;                               /* SLOOP 2.3: the backlight (panel.c lights_word); appended,
                                                    * so 2.2 still reads its part (st_load cuts at its size) */
} persist_t;
#define PERSIST_SIZE_V22 __builtin_offsetof(persist_t, lights)   /* the settings as 2.2 wrote them (no lights) */
_Static_assert(sizeof(persist_t) == PERSIST_SIZE_V22 + 4u, "lights: the last word, no padding before it");
#if FELUCCA_ARRANGER
#define PERSIST_MAGIC 0x50455233u                  /* "PER3": includes the song order */
#else
#define PERSIST_MAGIC 0x50455232u
#endif
#if FELUCCA_FLASH
static persist_t persist_saved;
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_ARRANGER
    arr_defaults(&arrangement);
#endif
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
    {
        uint32_t k;
        for (k = 0; k < SMP_USER_SLOTS; k++)
            smp_user_scan(k);
    }
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (n == (int)PERSIST_SIZE_V22 && p.magic == PERSIST_MAGIC)
            p.lights = 0;                          /* from 2.2: backlight off */
        if (((n == (int)sizeof p || n == (int)PERSIST_SIZE_V22) && p.magic == PERSIST_MAGIC)
#if FELUCCA_ARRANGER
            || (n == (int)(16u + sizeof(panel_t)) && p.magic == 0x50455232u)
#endif
            ) {
            settings.magic = SETTINGS_MAGIC;
            settings.palette = p.palette;
            settings.lowcut = p.lowcut;
            settings.zoom = p.zoom;
            if (p.panel.magic == PANEL_MAGIC)
                panel = p.panel;
            if (p.magic == PERSIST_MAGIC)
                lights_from_word(p.lights);
            else
                p.lights = 0;
#if FELUCCA_ARRANGER
            if (p.magic == PERSIST_MAGIC && arr_valid(&p.arrangement, 15u))
                arrangement = p.arrangement;
            else
                p.arrangement = arrangement;
#endif
            persist_saved = p;
        } else if (n == (int)(8u + sizeof(panel_t)) && p.magic == 0x50455231u) {   /* "PER1": palette, panel */
            const uint32_t *w = (const uint32_t *)&p;
            panel_t old;
            memcpy(&old, w + 2, sizeof old);
            settings.magic = SETTINGS_MAGIC;
            settings.palette = w[1];
            settings.lowcut = 0;
            settings.zoom = 0;
            if (old.magic == PANEL_MAGIC)
                panel = old;
        }
    }
    {   /* projects: fill empty RAM slots from flash, so the slot list is right after power-on. A slot
         * still valid in RAM (a warm reset: an update, UPDATE MODE, a crash) may never have reached
         * flash (a live section stored while playing): marked to be written when quiet */
        uint32_t i;
        for (i = 0; i < 4u; i++)
            if (!proj_ok(&proj_slot[i])) {
                proj_fetch(i);
            } else {                               /* (the second halves never survive: .pool) */
                int n = st_load(OBJ_PROJECT0 + i, &proj_tmp, sizeof proj_tmp);
                if (n != (int)sizeof proj_slot[i] || memcmp(&proj_tmp.v4, &proj_slot[i], sizeof proj_slot[i]))
                    sec_dirty |= (uint8_t)(1u << i);
                proj_fetch_b(OBJ_PROJB0 + i, &proj_slot_b[i], &proj_slot[i]);
            }
    }
    up_boot();                                     /* user presets */
#endif
}

static int project_used(uint32_t slot) { return proj_ok(&proj_slot[slot & 3u]); }

static void persist_fill(persist_t *p)              /* the settings as they are now */
{
    memset(p, 0, sizeof *p);
    p->magic = PERSIST_MAGIC;
    p->palette = settings.palette;
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom;
    p->panel = panel;
    p->lights = lights_word();
#if FELUCCA_ARRANGER
    p->arrangement = arrangement;
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok)
        return;
    persist_fill(&p);
    if (!memcmp(&p, &persist_saved, sizeof p))
        return;                                    /* unchanged: no erase cycle */
    if (st_save(OBJ_SETTINGS, &p, sizeof p) == 0)
        persist_saved = p;
#endif
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_t) <= ST_PAYLOAD_MAX && sizeof(project_b_t) <= ST_PAYLOAD_MAX,
               "a project half does not fit one flash sector");

/* ---- backup restore (editor.c BK_PUT): each object checked as a load checks it, then written through the
 * same A/B commit as a save. rc: 0 ok, 2 not a valid object, 3 stop the song first, 4 flash */
static int panel_valid(const panel_t *q)           /* a permutation of the buttons and of the knobs */
{
    uint32_t i, b = 0, e = 0;
    if (q->magic != PANEL_MAGIC)
        return 0;
    for (i = 0; i < NB; i++) {
        if (q->btn[i] >= 14u || (b >> q->btn[i]) & 1u)
            return 0;
        b |= 1u << q->btn[i];
    }
    for (i = 0; i < NE; i++) {
        if (q->enc[i] >= 7u || (e >> q->enc[i]) & 1u || (q->dir[i] != 1 && q->dir[i] != -1))
            return 0;
        e |= 1u << q->enc[i];
    }
    return 1;
}

static uint32_t settings_restore(const void *raw, uint32_t n)
{
    persist_t p;
    if (n != sizeof p && n != PERSIST_SIZE_V22)
        return 2;
    memset(&p, 0, sizeof p);
    memcpy(&p, raw, n);
    if (p.magic != PERSIST_MAGIC || p.palette >= NPALETTES || p.lowcut > 1u || p.zoom > 1u || !panel_valid(&p.panel))
        return 2;
#if FELUCCA_ARRANGER
    if (!arr_valid(&p.arrangement, 15u))
        return 2;
#endif
    if (!flash_ok || st_save(OBJ_SETTINGS, &p, sizeof p))
        return 4;
    persist_saved = p;
    settings.palette = p.palette;
    settings.lowcut = p.lowcut;
    settings.zoom = p.zoom;
    panel = p.panel;
#if FELUCCA_ARRANGER
    arrangement = p.arrangement;
#endif
    lights_from_word(p.lights);
    song.g[G_SYNC] = (int16_t)lights_sync;
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut != 0);
    ui.force = 1;
    return 0;
}

/* slot 0..3 (n 0: empty), or 4: the working project (loaded now) */
static uint32_t project_restore(uint32_t slot, const void *raw, uint32_t n)
{
    if (song.playing || transport_req)
        return 3;
    if (slot < 4u && !n) {
        if (!flash_ok || st_save(OBJ_PROJECT0 + slot, raw, 0))
            return 4;
        st_save(OBJ_PROJB0 + slot, raw, 0);
        memset(&proj_slot[slot], 0, sizeof proj_slot[slot]);
        memset(&proj_slot_b[slot], 0, sizeof proj_slot_b[slot]);
        sec_dirty &= (uint8_t)~(1u << slot);
        return 0;
    }
    if (!proj_import(&autosave_buf, raw, (int)n))
        return 2;
    if (slot == 4u) {                                   /* tracks 5..8: as at power-on until their half comes */
        proj_b_default(&autosave_b, &autosave_buf);
        project_apply(&autosave_buf, &autosave_b);
        return 0;
    }
    if (!flash_ok || st_save(OBJ_PROJECT0 + slot, &autosave_buf, sizeof autosave_buf))
        return 4;
    memcpy(&proj_slot[slot], &autosave_buf, sizeof proj_slot[slot]);
    proj_fetch_b(OBJ_PROJB0 + slot, &proj_slot_b[slot], &proj_slot[slot]);   /* (restored before it, or not) */
    sec_dirty &= (uint8_t)~(1u << slot);
    return 0;
}

/* the second half of slot 0..3, or 4: the working project's tracks 5..8 (now) */
static uint32_t project_restore_b(uint32_t slot, const void *raw, uint32_t n)
{
    const project_b_t *b = (const project_b_t *)raw;
    if (song.playing || transport_req)
        return 3;
    if (n != sizeof *b || b->magic != PROJ_MAGIC_B || b->size != sizeof *b || b->sum != projb_sum(b))
        return 2;
    if (slot == 4u) {
        proj_capture(&autosave_buf, &autosave_b);
        memcpy(autosave_b.t, b->t, sizeof autosave_b.t);
        project_apply(&autosave_buf, &autosave_b);
        return 0;
    }
    if (!flash_ok || st_save(OBJ_PROJB0 + slot, b, sizeof *b))
        return 4;
    if (projb_ok(b, &proj_slot[slot]))                  /* (its first half restored before it) */
        memcpy(&proj_slot_b[slot], b, sizeof *b);
    return 0;
}
#endif
#if FELUCCA_ARRANGER
static void arrangement_save(void)
{
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
    settings_save();
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(memcmp(&persist_saved.arrangement, &arrangement, sizeof arrangement) ? "SAVE ERROR" : "SONG SAVED");
        return;
    }
#endif
    ui_message("SONG IN RAM ONLY");
}

/* ---- live sections (SAVE + key, ui_layers.c). A section is a project slot (A..D = 1..4): stored into RAM
 * at once (playing too), written to flash once the transport is stopped and nothing sounds (an erase
 * stops the audio for ~50 ms); a song recorded with SONG REC is saved the same way. */
static void section_store(uint32_t s)
{
    s &= 3u;
    fm1_irq_off();                                      /* (the audio ISR may be applying a section) */
    proj_capture(&proj_slot[s], &proj_slot_b[s]);
    live_sec = (int8_t)s;
    fm1_irq_on();
    sec_dirty |= (uint8_t)(1u << s);
}
static void section_load(uint32_t s)                    /* stopped: the section is the loop now */
{
    s &= 3u;
    project_apply(&proj_slot[s], proj_b_of(s));
    live_sec = (int8_t)s;
}
static void sections_write(void)                        /* the dirty sections and song into flash */
{
    uint32_t i;
#if FELUCCA_FLASH
    if (flash_ok)
        for (i = 0; i < 4u; i++)
            if (((sec_dirty >> i) & 1u) && proj_store(OBJ_PROJECT0 + i, OBJ_PROJB0 + i, &proj_slot[i], &proj_slot_b[i]) == 0)
                sec_dirty &= (uint8_t)~(1u << i);       /* (a failed write stays dirty: tried again later) */
    if (!flash_ok)
#endif
        sec_dirty = 0;
    (void)i;
    if (song_dirty) {
        song_dirty = 0;
        settings_save();
    }
}
/* before an intentional reset (an update, UPDATE MODE, UBOOT from the host): the audio is stopped, so
 * whatever is only in RAM goes to flash now: the live sections, the song, the working project */
static void persist_flush_now(void)
{
    sections_write();
#if FELUCCA_FLASH
    if (flash_ok && !arrangement_clock.running) {     /* (a song playing: the tracks hold a section) */
        proj_capture(&autosave_buf, &autosave_b);
        if (autosave_b.sum != autosave_hash && proj_store(OBJ_AUTOSAVE, OBJ_AUTOSAVE_B, &autosave_buf, &autosave_b) == 0)
            autosave_hash = autosave_b.sum;
    }
#endif
}
static void sections_flush(void)                        /* main loop */
{
    static uint32_t tried;
    if (srec_done) {
        song_dirty = srec_done != 0xFFu;
        if (song_dirty) {
            char b[8];
            fmt_int(b, srec_done);
            ui_say("SONG PARTS ", b);
        } else {
            ui_message("NO SONG");
        }
        srec_done = 0;
    }
    if ((uint32_t)song.g[G_SYNC] != lights_sync) {      /* GLO > SYSTEM > SYNC: kept with the settings */
        lights_sync = (uint8_t)song.g[G_SYNC];
        settings_later = 1;
    }
    if (settings_later) {                               /* the menu closed while playing */
        settings_later = 0;
        song_dirty = 1;                                 /* (settings_save when quiet, with the song) */
    }
    if ((!sec_dirty && !song_dirty) || song.playing || transport_req || !audio_quiet() || fm1_ms - ui_input_ms < 1500u ||
        fm1_ms - tried < 5000u)
        return;
    tried = fm1_ms;                                     /* (a failed write: again in 5 s, not every frame) */
    sections_write();
    if (sec_dirty)
        ui_message("SAVE ERROR: RETRYING");
}
#endif
#endif /* PROJ_HOST */
