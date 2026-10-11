/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project formats (firmware/src/project.c, -DPROJ_HOST part). Format 5 ("FUN5",
 * SLOOP 2.4: format 4 + per-step nudges, parameter locks and fill conditions) is written; format 4 ("FUN4", SLOOP 2.0:
 * 10-byte steps with levels and ratchets, the drum track's 16 lanes, P_CHORD), format 3 ("FUN3", SLOOP
 * 1.x), format 2 ("FUN2", 53 parameters per track) and format 1 ("FUN1"), built byte for byte as the
 * firmware stored them, convert: every old value at its parameter, the parameters added since at their
 * defaults, the swings onto the MPC scale (x 0.8), synth steps as they were, the drum track's notes onto
 * its lanes (accent: hard), globals, selection, the engine bytes (kept; the drum track's 0), no nudge, no
 * lock, no fill condition; damaged ones are refused. Run by tests/run_tests.sh (needs build/gen). */
#define main hostsim_main
#include <stddef.h>
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i)       /* ui.c TRK_DEF: ANALOG, DIGITAL, LOFI */
{
    static const uint8_t E[NPART] = {0, 1, 3};
    return i < NPART ? E[i] : 0u;
}
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* the value parameter k (old id) of track t had in the old project */
static int16_t oldv(uint32_t t, uint32_t k) { return (int16_t)(t * 100u + k * 3u + 1u); }

static const uint8_t OLD_ENG[NTRK] = {7, 0, 6, 8};   /* WHEEL, ANALOG, TRIO; the drum track: 8 (none) */
static void fill_old_steps(step8_t *st, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < NSTEP; k++) {
        step8_t *s = &st[k];
        s->note[0] = (uint8_t)(36u + (k + t) % 40u);
        s->note[1] = (uint8_t)(38u + k % 5u);
        s->n = (uint8_t)(k % 3u);
        s->time = (uint8_t)(k % 3u);
        s->flags = (uint8_t)(k & 3u);
        s->vel = (uint8_t)(64u + t);
    }
}
static void fill_v2_track(proj_trk_v2_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V2; k++)
        d->p[k] = oldv(t, k);
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}
static void fill_v3_track(proj_trk_v3_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V3; k++)
        d->p[k] = oldv(t, k);
    d->p[P_SSWING] = 50;                           /* (swings: within 0..100) */
    d->p[P_ASWING] = 100;
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}

/* the steps of a converted track against the old ones: synth as they were, drums onto lanes */
static int steps_ok(const proj_trk_t *n, const step8_t *o, int drum)
{
    uint32_t k, i;
    for (k = 0; k < NSTEP; k++) {
        if (drum) {
            const dstep_t *d = &n->dstep[k];
            uint32_t want = 0;
            if (o[k].time == ST_NOTE)
                for (i = 0; i < o[k].n; i++)
                    want |= 1u << lane_of_note(o[k].note[i]);
            if (dstep_mask(d) != want)
                return 0;
            for (i = 0; i < DRUM_LANES; i++)
                if ((want >> i) & 1u && dstep_lvl(d, i) != ((o[k].flags & SF_ACCENT) ? LV_HARD : vel_lvl(o[k].vel)))
                    return 0;
        } else {
            const step_t *s = &n->step[k];
            if (memcmp(s->note, o[k].note, 4) || s->n != o[k].n || s->time != o[k].time || s->flags != o[k].flags ||
                s->vel != o[k].vel || s->lvl || s->rat)
                return 0;
        }
    }
    return 1;
}

/* track t converted from format 2 / 1 has the old values where they belong */
static int track_ok_v2(const proj_trk_t *n, const proj_trk_v2_t *o, uint32_t t)
{
    uint32_t k;
    int ok = (t == TRK_DRUM ? n->engine == 0 && n->preset == 0 : n->engine == o->engine && n->preset == o->preset) &&
             steps_ok(n, o->step, t == TRK_DRUM);
    for (k = 0; k <= P_DETUNE; k++)
        if (k != P_SSWING && k != P_ASWING)
            ok &= n->p[k] == oldv(t, k);
    ok &= n->p[P_SLCR] == 0 && n->p[P_SLPAT] == TP[P_SLPAT].def && n->p[P_SLRATE] == TP[P_SLRATE].def &&
          n->p[P_SLDEPTH] == TP[P_SLDEPTH].def && n->p[P_CHORD] == 0;
    for (k = 0; k < 8u; k++)
        ok &= pt_get(n, P_E0 + k) == oldv(t, 45u + k);
    for (k = P_ITYPE; k < P_E0; k++)
        ok &= pt_get(n, k) == TP[k].def;
    return ok;
}

int main(void)
{
    static project_v3_t v3;
    static project_v2_t v2;
    static project_v1_t v1;
    static project_v4_t v4;
    static project_t q, q2;
    static project_v5_t v5;
    static union {
        project_t v6;
        project_v5_t v5;
        project_v4_t v4;
        project_v3_t v3;
        project_v2_t v2;
        project_v1_t v1;
    } buf;
    uint32_t i, t;
    int bad = 0, ok;

    bad += check("layout: P_CHORD, P_TFLT, P_STRUM, P_VLEAD, then the ten of 2.6 just before P_E0 (63), P_COUNT = format 5's + 10",
                 P_CHORD + 1 == P_TFLT && P_TFLT + 1 == P_STRUM && P_STRUM + 1 == P_VLEAD && P_VLEAD + 1 == P_ITYPE &&
                 P_LPOL + 1 == P_E0 && P_E0 == 63 && P_COUNT == PROJ_NP_V5 + 10u && PROJ_NP_V5 == PROJ_NP_V4 + 3u &&
                 PROJ_NP_V4 == PROJ_NP_V3 + 1u && P_SLDEPTH + 1 == P_CHORD);
    bad += check("format 6 fits one flash object (3816 of 3840 bytes); 4 slots fit .noinit (with panel, settings, dbg, bootguard)",
                 sizeof(project_t) <= 3840u && 4u * sizeof(project_t) <= 0x3D50u - 256u && sizeof(project_t) == 3816u);
    {   /* every 2.6 parameter's range fits its byte */
        uint32_t k, fits = 1;
        for (k = P_ITYPE; k < P_E0; k++) fits &= TP[k].min >= 0 && TP[k].max <= 255;
        bad += check("format 6: the 2.6 parameters fit a byte each (0..255)", fits);
    }

    /* format 3 (SLOOP 1.x) */
    memset(&v3, 0, sizeof v3);
    v3.magic = PROJ_MAGIC_V3;
    v3.size = sizeof v3;
    for (i = 0; i < PROJ_NG_V3; i++)
        v3.g[i] = (int16_t)(300 + i);
    v3.g[G_SWING] = 50;
    v3.sel = 3;
    for (t = 0; t < NTRK; t++)
        fill_v3_track(&v3.t[t], t);
    v3.sum = proj_hash(&v3, sizeof v3 - 4u);
    memcpy(&buf, &v3, sizeof v3);
    ok = proj_import(&q, &buf, (int)sizeof v3);
    bad += check("FUN3 -> FUN6: converted, valid format 6 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 3 && q.g[G_SWING] == 40;
    for (i = 0; i < PROJ_NG_V3; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(300 + i);
    for (i = PROJ_NG_V3; i < PROJ_NG; i++)
        ok &= q.g[i] == GP[i].def;
    ok &= q.drdly == 0;
    bad += check("FUN3 -> FUN6: globals (swing 50 -> 40: the MPC scale), the new ones default", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++) {
        const proj_trk_t *n = &q.t[t];
        uint32_t k;
        ok &= (t == TRK_DRUM ? n->engine == 0 : n->engine == OLD_ENG[t]) && steps_ok(n, v3.t[t].step, t == TRK_DRUM);
        for (k = 0; k < PROJ_NP_V3 - 8u; k++)
            if (k != P_SSWING && k != P_ASWING)
                ok &= n->p[k] == oldv(t, k);
        ok &= n->p[P_SSWING] == 40 && n->p[P_ASWING] == 80 && n->p[P_CHORD] == 0;
        for (k = 0; k < 8u; k++)
            ok &= pt_get(n, P_E0 + k) == oldv(t, PROJ_NP_V3 - 8u + k);
        for (k = 0; k < NLOCK; k++)
            ok &= n->lock[k].step == LOCK_FREE;
        for (k = 0; k < NSTEP; k++)
            ok &= n->mf[k] == 0;
        for (k = P_ITYPE; k < P_E0; k++)
            ok &= pt_get(n, k) == TP[k].def;
    }
    bad += check("FUN3 -> FUN6: parameters (P_E0.. moved), steps, drum notes -> lanes, no lock, no fill", ok);

    /* format 2, as written before the SLICER */
    memset(&v2, 0, sizeof v2);
    v2.magic = PROJ_MAGIC_V2;
    v2.size = sizeof v2;
    for (i = 0; i < PROJ_NG_V2; i++)
        v2.g[i] = (int16_t)(500 + i);
    v2.sel = 2;
    for (t = 0; t < NTRK; t++)
        fill_v2_track(&v2.t[t], t);
    v2.sum = proj_hash(&v2, sizeof v2 - 4u);
    bad += check("FUN2 image is 2552 bytes (as stored)", sizeof v2 == 2552u);
    memcpy(&buf, &v2, sizeof v2);
    ok = proj_import(&q, &buf, (int)sizeof v2);
    bad += check("FUN2 -> FUN6: converted, valid format 6 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 2;
    for (i = 0; i < PROJ_NG_V2; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(500 + i);
    bad += check("FUN2 -> FUN6: globals and selected track", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++)
        ok &= track_ok_v2(&q.t[t], &v2.t[t], t);
    bad += check("FUN2 -> FUN6: every parameter mapped, SLICER OFF, CHORD OFF (4 tracks)", ok);
    bad += check("FUN2 -> FUN6: engine bytes kept (WHEEL 7, ANALOG 0, TRIO 6), drum 0",
                 q.t[0].engine == 7 && q.t[1].engine == 0 && q.t[2].engine == 6 && q.t[3].engine == 0 &&
                 str_eq(ENGINES[7]->name, "WHEEL") && str_eq(ENGINES[6]->name, "TRIO") && NENGINES > 8);

    /* a FUN4 image (SLOOP 2.0 .. 2.3, as stored) -> FUN5: the same, zero nudges, every lock free */
    memset(&v4, 0, sizeof v4);
    v4.magic = PROJ_MAGIC_V4;
    v4.size = sizeof v4;
    memcpy(v4.g, q.g, sizeof v4.g);
    v4.sel = 1;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NP_V4; i++)               /* (format 4's ids: P_E0 was P_TFLT) */
            v4.t[t].p[i] = i < P_TFLT ? q.t[t].p[i] : pt_get(&q.t[t], P_E0 + i - P_TFLT);
        v4.t[t].engine = q.t[t].engine;
        v4.t[t].preset = q.t[t].preset;
        memcpy(v4.t[t].step, q.t[t].step, sizeof v4.t[t].step);
    }
    v4.t[1].engine = 8;
    v4.t[0].step[3].lvl = 0x9C;
    v4.t[0].step[3].rat = 0x27;
    dstep_set(&v4.t[TRK_DRUM].dstep[5], 13, LV_GHOST, 2);
    v4.sum = proj_hash(&v4, sizeof v4 - 4u);
    bad += check("FUN4 image is 3112 bytes (as stored)", sizeof v4 == 3112u);
    memcpy(&buf, &v4, sizeof v4);
    ok = proj_import(&q2, &buf, (int)sizeof v4) && proj_ok(&q2) && q2.magic == PROJ_MAGIC && q2.sel == 1 && q2.t[1].engine == 8 &&
         q2.t[0].step[3].lvl == 0x9C && q2.t[0].step[3].rat == 0x27 && dstep_lvl(&q2.t[TRK_DRUM].dstep[5], 13) == LV_GHOST &&
         dstep_rat(&q2.t[TRK_DRUM].dstep[5], 13) == 2u && !memcmp(q2.g, v4.g, sizeof q2.g);
    for (t = 0; t < NTRK; t++) {
        ok &= !memcmp(q2.t[t].p, v4.t[t].p, P_TFLT * 2u) && !memcmp(q2.t[t].e, v4.t[t].p + P_TFLT, 16u) &&
              q2.t[t].p[P_TFLT] == 0 && q2.t[t].p[P_STRUM] == 0 && q2.t[t].p[P_VLEAD] == 0 &&
              !memcmp(q2.t[t].step, v4.t[t].step, sizeof q2.t[t].step);
        for (i = 0; i < NLOCK; i++)
            ok &= q2.t[t].lock[i].step == LOCK_FREE;
        for (i = 0; i < NSTEP; i++)
            ok &= q2.t[t].mf[i] == 0;
        for (i = P_ITYPE; i < P_E0; i++)
            ok &= pt_get(&q2.t[t], i) == TP[i].def;
    }
    bad += check("FUN4 -> FUN6: as stored (levels, ratchets, lanes, engine 8; P_E0.. moved, FILTER off), zero nudges, locks free, no fill condition", ok);
    v4.t[2].step[1].vel ^= 1u;
    memcpy(&buf, &v4, sizeof v4);
    bad += check("FUN4 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v4));
    v4.t[2].step[1].vel ^= 1u;

    /* a FUN5 image (SLOOP 2.4 .. 2.5, as stored) -> FUN6: by id, the 2.6 ten at their defaults, the nudges and
     * conditions into a byte a step, a lock on P_E0.. moved with it (format 5's 53.. -> 63..) */
    memset(&v5, 0, sizeof v5);
    v5.magic = PROJ_MAGIC_V5;
    v5.size = sizeof v5;
    memcpy(v5.g, q.g, sizeof v5.g);
    v5.sel = 2;
    v5.drdly = 55;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NP_V5; i++)
            v5.t[t].p[i] = i < PROJ_NP_V5 - 8u ? (int16_t)(t * 10 + i) : (int16_t)(100 + t * 10 + i);
        v5.t[t].engine = (uint8_t)t;
        v5.t[t].preset = (uint8_t)(3 + t);
        memcpy(v5.t[t].step, q.t[t].step, sizeof v5.t[t].step);
        for (i = 0; i < NLOCK; i++)
            v5.t[t].lock[i].step = LOCK_FREE;
    }
    v5.t[0].micro[3] = -32, v5.t[0].micro[4] = 31, v5.t[TRK_DRUM].micro[9] = -7;
    v5.t[0].fill[0] = 0x09;                        /* steps 1 and 2: FILL ONLY, NO FILL */
    v5.t[TRK_DRUM].fill[15] = 0x40;                /* step 64: FILL ONLY */
    v5.t[1].lock[0].step = 3, v5.t[1].lock[0].param = 53 + 1, v5.t[1].lock[0].val = 9;   /* format 5's P_E1 */
    v5.t[1].lock[1].step = 4, v5.t[1].lock[1].param = P_DIST, v5.t[1].lock[1].val = 100;
    v5.sum = proj_hash(&v5, sizeof v5 - 4u);
    memcpy(&buf, &v5, sizeof v5);
    ok = proj_import(&q2, &buf, (int)sizeof v5) && proj_ok(&q2) && q2.magic == PROJ_MAGIC && q2.sel == 2 && q2.drdly == 55 &&
         !memcmp(q2.g, v5.g, sizeof q2.g);
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < PROJ_NP_V5 - 8u; i++)
            ok &= pt_get(&q2.t[t], i) == (int16_t)(t * 10 + i);
        for (i = 0; i < 8u; i++)
            ok &= pt_get(&q2.t[t], P_E0 + i) == (int16_t)(100 + t * 10 + PROJ_NP_V5 - 8u + i);
        for (i = P_ITYPE; i < P_E0; i++)
            ok &= pt_get(&q2.t[t], i) == TP[i].def;
        ok &= q2.t[t].engine == t && q2.t[t].preset == 3 + t && !memcmp(q2.t[t].step, v5.t[t].step, sizeof v5.t[t].step);
    }
    ok &= pt_micro(&q2.t[0], 3) == -32 && pt_micro(&q2.t[0], 4) == 31 && pt_micro(&q2.t[TRK_DRUM], 9) == -7 &&
          pt_micro(&q2.t[0], 5) == 0 && pt_fill(&q2.t[0], 0) == FC_FILL && pt_fill(&q2.t[0], 1) == FC_NOFILL &&
          pt_fill(&q2.t[0], 2) == FC_NORM && pt_fill(&q2.t[TRK_DRUM], 63) == FC_FILL &&
          q2.t[1].lock[0].param == P_E1 && q2.t[1].lock[0].val == 9 && q2.t[1].lock[1].param == P_DIST &&
          q2.t[1].lock[2].step == LOCK_FREE;
    bad += check("FUN5 -> FUN6: by id, the 2.6 parameters default, nudges and conditions kept, a lock on E2 moved with it", ok);
    v5.t[3].micro[0] ^= 1;
    memcpy(&buf, &v5, sizeof v5);
    bad += check("FUN5 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v5));

    /* a FUN6 round trip: stored as is (nudges, locks, the 2.6 parameters, an engine added since: 8) */
    q.t[1].engine = 8;
    q.t[0].step[3].lvl = 0x9C;
    q.t[0].step[3].rat = 0x27;
    dstep_set(&q.t[TRK_DRUM].dstep[5], 13, LV_GHOST, 2);
    pt_set_mf(&q.t[0], 3, -32, FC_FILL);
    pt_set_mf(&q.t[0], 4, 31, FC_NOFILL);
    pt_set_mf(&q.t[TRK_DRUM], 9, -7, 0);
    q.t[0].lock[0].step = 3, q.t[0].lock[0].param = P_E0, q.t[0].lock[0].val = 2;
    q.t[0].lock[1].step = 3, q.t[0].lock[1].param = P_DIST, q.t[0].lock[1].val = 100;
    q.t[2].lock[23].step = 63, q.t[2].lock[23].param = P_LEVEL, q.t[2].lock[23].val = 50;
    q.t[2].p[P_TFLT] = -30;                        /* a track FILTER (2.4) */
    pt_set(&q.t[1], P_ITYPE, 4), pt_set(&q.t[1], P_IMIX, 90), pt_set(&q.t[TRK_DRUM], P_ITYPE, 2);
    pt_set(&q.t[0], P_LSYNC, 5), pt_set(&q.t[0], P_ESYNC, 1), pt_set(&q.t[0], P_FTYPE, 2);
    q.sum = proj_sum(&q);
    memcpy(&buf, &q, sizeof q);
    bad += check("FUN6 -> FUN6: as stored (levels, ratchets, lanes, nudges, locks, fill conditions, INSERT, SYNC, engine 8)",
                 proj_import(&q2, &buf, (int)sizeof q) && !memcmp(&q, &q2, sizeof q) && q2.t[1].engine == 8 &&
                 pt_micro(&q2.t[0], 3) == -32 && pt_fill(&q2.t[0], 3) == FC_FILL && pt_micro(&q2.t[0], 4) == 31 &&
                 q2.t[0].lock[1].val == 100 && q2.t[2].lock[23].step == 63 && q2.t[2].p[P_TFLT] == -30 &&
                 pt_get(&q2.t[1], P_ITYPE) == 4 && pt_get(&q2.t[1], P_IMIX) == 90 && pt_get(&q2.t[TRK_DRUM], P_ITYPE) == 2 &&
                 pt_get(&q2.t[0], P_LSYNC) == 5 && pt_get(&q2.t[0], P_ESYNC) == 1 && pt_get(&q2.t[0], P_FTYPE) == 2);

    /* damaged / wrong size */
    v2.t[1].p[3]++;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v2));
    v2.t[1].p[3]--;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a wrong length: refused", !proj_import(&q2, &buf, (int)sizeof v2 - 2));
    memcpy(&buf, &q, sizeof q);
    buf.v4.magic = PROJ_MAGIC_V3;
    bad += check("FUN6 size with a FUN3 magic: refused", !proj_import(&q2, &buf, (int)sizeof q));
    memcpy(&buf, &q, sizeof q);
    buf.v6.magic = PROJ_MAGIC_V5;
    bad += check("FUN6 size with a FUN5 magic: refused", !proj_import(&q2, &buf, (int)sizeof q));
    memcpy(&buf, &v3, sizeof v3);
    buf.v3.t[2].step[7].vel ^= 1u;
    bad += check("FUN3 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v3));

    /* format 1: one instrument -> track 1, the others their defaults */
    memset(&v1, 0, sizeof v1);
    v1.magic = PROJ_MAGIC_V1;
    v1.size = sizeof v1;
    for (i = 0; i < PROJ_NG_V2; i++)
        v1.g[i] = (int16_t)(700 + i);
    fill_v2_track(&v1.t, 0);
    v1.sum = proj_hash(&v1, sizeof v1 - 4u);
    memcpy(&buf, &v1, sizeof v1);
    ok = proj_import(&q, &buf, (int)sizeof v1) && proj_ok(&q) && track_ok_v2(&q.t[0], &v1.t, 0) && q.g[5] == 705;
    for (t = 1; t < NTRK; t++)
        ok &= q.t[t].preset == 0xFF && q.t[t].p[P_SLCR] == 0 && q.t[t].p[P_LEVEL] == TP[P_LEVEL].def &&
              pt_get(&q.t[t], P_E0) == ENGINES[trk_def_engine(t)]->edit[0].def && q.t[t].lock[0].step == LOCK_FREE &&
              (t == TRK_DRUM ? dstep_mask(&q.t[t].dstep[0]) == 0u : q.t[t].step[0].time == ST_REST);
    bad += check("FUN1 -> FUN6: track 1 mapped, tracks 2..4 defaults", ok);

    /* capture / apply: the working project round trip (nudges and locks too) */
    host_tracks_init();
    for (t = 0; t < NTRK; t++)
        trk[t].p[P_SLEN] = (int16_t)(5 + t);
    trk[1].step[2].n = 2, trk[1].step[2].note[0] = 60, trk[1].step[2].note[1] = 64, trk[1].step[2].time = ST_NOTE;
    trk[1].step[2].lvl = 0x0D;
    dstep_set(&TDRUM->dstep[9], 4, LV_SOFT, 1);
    song.g[G_DUST] = 33;
    song.g[G_DRDLY] = 77;
    trk[1].micro[2] = -20;
    TDRUM->micro[9] = 12;
    lock_set(&trk[1], 2, P_ED_FLT, -30);
    lock_set(&trk[1], 2, P_E1, 5);
    lock_set(TDRUM, 9, P_DIST, 64);
    step_fill_set(&trk[1], 2, FC_FILL);
    step_fill_set(TDRUM, 9, FC_NOFILL);
    step_fill_set(TDRUM, 63, FC_FILL);
    trk[2].p[P_ITYPE] = 7, trk[2].p[P_IA] = 11, trk[2].p[P_IMIX] = 77, TDRUM->p[P_ITYPE] = 4;   /* 2.6 */
    trk[0].p[P_LSYNC] = 6, trk[0].p[P_LTRIG] = 1, trk[0].p[P_LPOL] = 1, trk[0].p[P_ESYNC] = 1, trk[0].p[P_FTYPE] = 1;
    proj_capture(&q);
    host_tracks_init();
    song.g[G_DRDLY] = 0;
    proj_apply(&q, 1);
    ok = q.drdly == 77 && song.g[G_DRDLY] == 77 && trk[2].p[P_SLEN] == 7 && trk[1].step[2].n == 2 && trk[1].step[2].lvl == 0x0D && song.g[G_DUST] == 33 &&
         dstep_has(&TDRUM->dstep[9], 4) && dstep_lvl(&TDRUM->dstep[9], 4) == LV_SOFT && dstep_rat(&TDRUM->dstep[9], 4) == 1u &&
         trk[1].micro[2] == -20 && TDRUM->micro[9] == 12 && trk[1].micro[3] == 0 &&
         lock_find(&trk[1], 2, P_ED_FLT, 0) >= 0 && trk[1].lock[lock_find(&trk[1], 2, P_ED_FLT, 0)].val == -30 &&
         lock_find(&trk[1], 2, P_E1, 0) >= 0 && lock_find(TDRUM, 9, P_DIST, 0) >= 0 && lock_find(TDRUM, 9, P_E0, 0) < 0 &&
         step_fill(&trk[1], 2) == FC_FILL && step_fill(&trk[1], 3) == FC_NORM && step_fill(TDRUM, 9) == FC_NOFILL &&
         step_fill(TDRUM, 63) == FC_FILL && step_fill(TDRUM, 8) == FC_NORM &&
         trk[2].p[P_ITYPE] == 7 && trk[2].p[P_IA] == 11 && trk[2].p[P_IMIX] == 77 && TDRUM->p[P_ITYPE] == 4 &&
         trk[0].p[P_LSYNC] == 6 && trk[0].p[P_LTRIG] == 1 && trk[0].p[P_LPOL] == 1 && trk[0].p[P_ESYNC] == 1 &&
         trk[0].p[P_FTYPE] == 1;
    bad += check("the working project: capture -> apply round trip (levels, lanes, DUST, drum DLY, nudges, locks, fill conditions, the 2.6 parameters)", ok);
    q2 = q;
    q2.drdly = 0;                                  /* a 2.4 project: the byte was 0 */
    proj_apply(&q2, 0);                            /* (as a song section) */
    ok = song.g[G_DRDLY] == 0;
    q2.drdly = 200;
    proj_apply(&q2, 1);
    ok &= song.g[G_DRDLY] == 127;
    bad += check("2.5 drum DLY: kept in the byte after sel, a 2.4 project: 0, a section sets it, 200 -> 127",
                 ok && G_DRDLY == PROJ_NG && offsetof(project_t, drdly) == 8u + 2u * PROJ_NG + 1u);
    {   /* 2.6: GLO > TEXTURE in the project (and in a section); a project before it: OFF; the drum channel (a setting of
         * the FM-1 now, GLO > MIDI) not taken from a project */
        song.g[G_TXSRC] = 5, song.g[G_TXLVL] = 101, song.g[G_TXTONE] = -37, song.g[G_TXMOVE] = 90;
        song.g[G_TXRATE] = 3, song.g[G_TXWHEN] = 1, song.g[G_TXDUCK] = 1, song.g[G_DRCH] = 4;
        proj_capture(&q2);
        song.g[G_TXSRC] = 0, song.g[G_TXLVL] = 0, song.g[G_TXTONE] = 0, song.g[G_TXMOVE] = 0;
        song.g[G_TXRATE] = 0, song.g[G_TXWHEN] = 0, song.g[G_TXDUCK] = 0;
        proj_apply(&q2, 1);
        ok = song.g[G_TXSRC] == 5 && song.g[G_TXLVL] == 101 && song.g[G_TXTONE] == -37 && song.g[G_TXMOVE] == 90 &&
             song.g[G_TXRATE] == 3 && song.g[G_TXWHEN] == 1 && song.g[G_TXDUCK] == 1 && song.g[G_DRCH] == 4;
        song.g[G_DRCH] = 9;
        song.g[G_TXSRC] = 0;
        proj_apply(&q2, 0);                        /* a section: its texture too */
        ok &= song.g[G_TXSRC] == 5 && song.g[G_DRCH] == 9;
        q2.rsv[0] = 0;                             /* a project before 2.6 (converted): no texture */
        proj_apply(&q2, 1);
        ok &= song.g[G_TXSRC] == 0 && song.g[G_TXLVL] == GP[G_TXLVL].def && song.g[G_TXMOVE] == GP[G_TXMOVE].def && song.g[G_DRCH] == 9;
        bad += check("2.6 TEXTURE: kept in a project (SRC LEVEL TONE MOVE RATE WHEN DUCK), a section sets it, an older one: OFF; "
                     "the drum channel not taken from a project", ok);
    }
    {   /* what format 6 packs in fewer bits holds every value: the byte parameters (P_ITYPE .. P_LPOL: 0..255), TEXTURE's
         * SRC (4 bits), WHEN and DUCK (1), RATE (a byte), LEVEL and TONE + 64 (7 bits each) */
        uint32_t k;
        int ok = 1;
        for (k = PROJ_NC6; k < P_E0; k++)
            ok &= TP[k].min >= 0 && TP[k].max <= 255;
        ok &= GP[G_TXSRC].min >= 0 && GP[G_TXSRC].max <= 15 && GP[G_TXWHEN].min >= 0 && GP[G_TXWHEN].max <= 1 &&
              GP[G_TXDUCK].min >= 0 && GP[G_TXDUCK].max <= 1 && GP[G_TXRATE].min >= 0 && GP[G_TXRATE].max <= 255 &&
              GP[G_TXLVL].min >= 0 && GP[G_TXLVL].max <= 127 && GP[G_TXTONE].min >= -64 && GP[G_TXTONE].max <= 63;
        bad += check("format 6: the byte parameters and TEXTURE's packed fields hold every value of their range", ok);
    }
    /* a damaged image: a nudge out of range, a lock on a parameter that cannot lock, on a step past the end,
     * with a value past the range: clamped, freed, freed, clamped */
    q.t[1].lock[5].step = 4, q.t[1].lock[5].param = P_SLEN, q.t[1].lock[5].val = 8;
    q.t[1].lock[6].step = 64, q.t[1].lock[6].param = P_E0, q.t[1].lock[6].val = 1;
    q.t[1].lock[7].step = 4, q.t[1].lock[7].param = P_LEVEL, q.t[1].lock[7].val = 999;
    q.t[1].lock[8].step = 4, q.t[1].lock[8].param = 200, q.t[1].lock[8].val = 1;
    pt_set_mf(&q.t[1], 4, 0, FC_FILL), pt_set_mf(&q.t[1], 5, 0, FC_NOFILL);   /* steps 5, 6: fill only, no fill; */
    pt_set_mf(&q.t[1], 6, 0, 3), pt_set_mf(&q.t[1], 7, 0, 3);                    /* 7, 8: 3 (-> normal) */
    pt_set(&q.t[1], P_ITYPE, 200);                 /* an INSERT type past the list: clamped */
    proj_apply(&q, 1);
    ok = trk[1].p[P_ITYPE] == TP[P_ITYPE].max && trk[1].lock[5].step == LOCK_FREE &&
         trk[1].lock[6].step == LOCK_FREE && trk[1].lock[8].step == LOCK_FREE && trk[1].lock[7].step == 4 &&
         trk[1].lock[7].val == 127 && lock_find(&trk[1], 2, P_ED_FLT, 0) >= 0 &&
         step_fill(&trk[1], 4) == FC_FILL && step_fill(&trk[1], 5) == FC_NOFILL && trk[1].fill[1] == 0x09;
    bad += check("apply: INSERT 200 -> the last type, a lock on LEN / step 64 / param 200 is freed, LEVEL 999 -> 127, condition 3 -> normal", ok);

    q2 = q;
    bad += check("2.6 SAVE > PROJECT > ERASE: an object of no bytes is no project (an empty slot, older SLOOP too)",
                 !proj_import(&q2, &q, 0));
    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}
