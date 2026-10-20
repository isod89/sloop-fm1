/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (sloop-lab, github.com/andrerezemini/sloop-lab) */
/* (after 2.7.1) A hold in the middle of a job. On the FM-1, fm1_cpu1_wait gives core 1 4 ms, then holds it wherever
 * it is (hal/fm1_cpu1.h), and core 0 renders the job's units itself (fx.c mix_block's late path): what core 1 had half
 * written must not outlive that block, nor make the re-render read or write out of bounds. The host double
 * (tests/cpu1_host.c, CORES=1) stops a core-1 job at a named CPU1_HOLD_POINT (cpu1.h), the nth time it passes it.
 * Each scene runs without a hold (the reference) and with a hold at each of the first passes of its site; every site
 * must be reached (else the scene tests nothing), and then:
 *   fade_preset, fade_engine  a preset, then an engine, loaded under part 3's sounding notes (core 1's part): mix_part
 *              and track_render swap t->p with the old sound. units_split keeps a fading part on core 0, so these sites
 *              must never be passed on core 1; the reference must show both fades (xf_on), and the parts' values end
 *              as the reference's
 *   fm6_cache  an FM6 cache rebuilt on part 3: its state at the end is the reference's
 *   grain_index, grain_zones  GRAIN's SRC changed on part 3: the zone index is the reference's, every sounding
 *              voice's zone is the one gr_find gives
 *   grain_spawn  a grain held half set (RAND 127: reverse grains too): no grain of a voice reads outside its window
 *   symp_coef  PHYS SYMP's coefficients rebuilt: the reference's
 *   slicer_rec the drums' SLICER STUT held as its recording fills: nothing written past it
 *   unit_done, unit_begin  core 1 held after a unit it finished (a wrong read seen at the wait), or as one begins:
 *              part 3 and the drums through the SLICER, part 3's LFO 2 free running. The late path renders only the
 *              units it did not finish (fx.c cpu1_units_done), so the SLICER step clocks and the LFO phases end as the
 *              reference's (rendered twice they ran a block ahead until the next PLAY; a unit marked done before it
 *              rendered would be skipped), and a hold after its last unit leaves the output itself the reference's
 *   lofi_end, env_end  a voice of part 3 held as it ends (LOFI's STEP amplitude, the ADSR's release): it is never left
 *              active at stage 0 (sounding at full through STOP on LOFI); after the notes stop, every voice ends
 * The out-of-bounds checks bite under ASan (tests/cpu1_modes.sh builds this test with it too). Voices' own phases
 * after a hold in the middle of a unit are not compared: the late path renders that unit again from where it was
 * (one block of glitch, as on the device). */
#include <stddef.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

#define BLOCKS 900u
#define EVENT 300u
#define NHOLDS 4u                                         /* the first passes of a site tried, one run each */
enum { S_FADE, S_FM6, S_GRAIN, S_SPAWN, S_SYMP, S_SLICER, S_LATE, S_LOFI, S_ENV, NSCENE };
static const char *const SCENE_NAME[NSCENE] = {"fade", "fm6", "grain", "spawn", "symp", "slicer", "late", "lofi", "env"};
static const char *const SCENE_SITES[NSCENE][2] = {
    {"fade_preset", "fade_engine"}, {"fm6_cache", 0}, {"grain_index", "grain_zones"}, {"grain_spawn", 0},
    {"symp_coef", 0}, {"slicer_rec", 0}, {"unit_done", "unit_begin"}, {"lofi_end", 0}, {"env_end", 0}};

static void steps(track_t *t, uint32_t base, uint32_t every)   /* a 3-note chord on every `every`-th step */
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t n[3] = {(uint8_t)(base + i % 3u), (uint8_t)(base + 4u + i % 3u), (uint8_t)(base + 7u + i % 3u)};
        int on = i % every == 0u;
        put_step(t, i, on ? 3u : 0u, n, on ? ST_NOTE : ST_REST, 0u);
    }
}

static void scene_setup(uint32_t s)
{
    track_t *t = &trk[NPART - 1u];
    host_tracks_init();
    song.g[G_BPM] = 150;
    host_preset(&trk[0], 0, 4);                          /* ANALOG on core 0 throughout */
    steps(&trk[0], 48, 4);
    switch (s) {
    case S_FADE:
        host_preset(t, 0, 2);                            /* ANALOG, a chord on every step: part 3 sounds */
        t->p[P_REL] = 90;
        steps(t, 55, 1);
        break;
    case S_FM6:
        host_preset(t, ENGI_FM6, 1);
        steps(t, 55, 4);
        break;
    case S_GRAIN:
    case S_SPAWN:
        host_preset(t, ENGI_GRAIN, 0);
        if (s == S_SPAWN)
            t->p[P_E0 + 6] = 127;                        /* RAND: half the grains reverse */
        steps(t, 50, 2);
        break;
    case S_SYMP: {
        uint8_t n[1] = {57};
        uint32_t i;
        host_preset(t, ENGI_PHYS, 0);
        t->p[P_E0] = PM_SYMP;
        for (i = 0; i < 16u; i++)                        /* the same note at every bar: the strings' inputs repeat */
            put_step(t, i, i ? 0u : 1u, n, i ? ST_REST : ST_NOTE, 0u);
        break;
    }
    case S_LOFI:                                         /* voices ending: LOFI's STEP amplitude ends them (its 1 %) */
    case S_ENV:                                          /* ANALOG: the ADSR's release ends them (env_tick) */
        host_preset(t, s == S_LOFI ? 3u : 0u, 0);
        if (s == S_LOFI)
            t->p[P_E0] = 4;                              /* CHIP = STEP (eng_lofi.c CHIP_STEP) */
        t->p[P_REL] = 12;
        steps(t, 55, 1);
        break;
    case S_LATE: {                                       /* part 3 and the drums through the SLICER (GATE), LFO 2 FREE */
        uint32_t i;
        uint8_t n[1] = {36};
        host_preset(t, 0, 2);                            /* ANALOG */
        steps(t, 55, 2);
        t->p[P_SLCR] = SL_GATE;
        t->p[P_SLPAT] = 3;
        t->p[P_SLDEPTH] = 100;
        t->p[P_LTRIG] = 1;                               /* FREE: the phase only ever runs on */
        t->p[P_LRATE] = 70;
        t->p[P_LD_PIT] = 20;
        for (i = 0; i < 16u; i++)
            put_step(TDRUM, i, i % 2u ? 0u : 1u, n, i % 2u ? ST_REST : ST_NOTE, 0u);
        TDRUM->p[P_SLCR] = SL_GATE;
        TDRUM->p[P_SLPAT] = 5;
        TDRUM->p[P_SLDEPTH] = 100;
        break;
    }
    default: {                                           /* the drums on core 1, STUT recording long live steps */
        uint32_t i;
        uint8_t n[1] = {36};
        song.g[G_BPM] = 40;
        for (i = 0; i < 16u; i++)
            put_step(TDRUM, i, 1u, n, ST_NOTE, 0u);
        TDRUM->p[P_SLCR] = SL_STUT;
        TDRUM->p[P_SLPAT] = 1;
        TDRUM->p[P_SLRATE] = 0;
        TDRUM->p[P_SLDEPTH] = 100;
        break;
    }
    }
    transport_req = 1;
}

static uint32_t fades_seen;                              /* bit 0: the preset fade, bit 1: the engine fade (reference) */
static void scene_event(uint32_t s, uint32_t b)
{
    track_t *t = &trk[NPART - 1u];
    uint32_t k;
    if (s == S_FADE) {
        if (b == EVENT) {                                /* a preset under the notes: its fade */
            sound_change(t);
            host_preset_req(t, 0, 9);
        }
        if (b == EVENT + 200u) {                         /* then another engine: its fade */
            sound_change(t);
            host_preset_req(t, 7, 1);                    /* DRAWBAR */
        }
        if (t->xf_on && t->xf)
            fades_seen |= b < EVENT + 200u ? 1u : 2u;
    }
    if (s == S_FM6 && b == EVENT) {
        t->p[P_E0 + 3] = (int16_t)(t->p[P_E0 + 3] + 9);  /* FM6 macros: the cache is rebuilt */
        t->p[P_E0 + 1] = (int16_t)(t->p[P_E0 + 1] + 2);
    }
    if ((s == S_LOFI || s == S_ENV) && b == 2u * EVENT) {
        uint8_t n[1] = {0};
        for (k = 0; k < 16u; k++)                        /* the notes stop: every voice must end */
            put_step(t, k, 0u, n, ST_REST, 0u);
    }
    if ((s == S_GRAIN || s == S_SPAWN) && b == EVENT)
        t->p[P_E0] = (int16_t)((t->p[P_E0] + 1) % SMP_NALL);   /* SRC: the zone index is rebuilt */
    if (s == S_SYMP && b == EVENT)
        for (k = 1; k < 8u; k++)
            t->p[P_E0 + k] = (int16_t)(t->p[P_E0 + k] + 7 > 120 ? 30 : t->p[P_E0 + k] + 7);
}

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 0x100000001B3ull;
    return h;
}

static uint64_t state_sig(uint32_t s)                    /* what a hold could leave wrong for good, vs the reference */
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        h = fnv(h, trk[k].p, sizeof trk[k].p);
        h = fnv(h, &trk[k].engine, 1);
        h = fnv(h, &trk[k].preset, 1);
    }
    if (s == S_FM6)
        h = fnv(h, &fm6_eff[NPART - 1u], sizeof fm6_eff[0]);
    if (s == S_GRAIN) {
        const gr_part_t *P = (const gr_part_t *)eng_arena_of(&trk[NPART - 1u], ENGI_GRAIN);
        h = fnv(h, &P->src, sizeof P->src);
        h = fnv(h, &P->stamp, sizeof P->stamp);
        h = fnv(h, &P->nz, sizeof P->nz);
        h = fnv(h, P->zbase, sizeof P->zbase);
        h = fnv(h, P->zcnt, sizeof P->zcnt);
    }
    if (s == S_LOFI || s == S_ENV)                       /* the notes stopped long before: no voice left */
        for (k = 0; k < NVOICE; k++)
            h = fnv(h, &trk[NPART - 1u].v[k].active, sizeof trk[NPART - 1u].v[k].active);
    if (s == S_LATE)                                     /* the free-running clocks a second render would advance */
        for (k = 0; k < NTRK; k++) {
            h = fnv(h, &sl[k], offsetof(sl_t, rec_on) + 1u);
            if (k < NPART)
                h = fnv(h, &trk[k].lfo_ph, sizeof trk[k].lfo_ph);
        }
    if (s == S_SYMP) {
        track_t *t = &trk[NPART - 1u];
        uint8_t *a = (uint8_t *)eng_arena_of(t, ENGI_PHYS);
        for (k = 0; k < phys_poly(t); k++) {
            const phys_slot_t *S = (const phys_slot_t *)(a + k * PHYS_SLOT_BIG);
            if (S->model == PM_SYMP) {
                h = fnv(h, S->u.y.key, sizeof S->u.y.key);
                h = fnv(h, &S->u.y.cf, sizeof S->u.y.cf);
            }
        }
    }
    return h;
}

static int consistent(uint32_t s)                        /* checks of the held run's own state, block by block */
{
    track_t *t = &trk[NPART - 1u];
    const gr_part_t *P;
    uint32_t i;
    if (s == S_LOFI || s == S_ENV) {                     /* no voice active at stage 0 (a voice no end reaches) */
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && !t->v[i].stage)
                return 0;
        return 1;
    }
    if (s != S_GRAIN && s != S_SPAWN)
        return 1;
    P = (const gr_part_t *)eng_arena_of(t, ENGI_GRAIN);
    if (s == S_GRAIN && P->src)                          /* a valid index: every sounding voice on its zone */
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].s[0] != gr_find(P->src - 1u, t->v[i].note))
                return 0;
    for (i = 0; i < GR_NG; i++) {                        /* a live reverse grain reads inside its window */
        const gr_grain_t *g = &P->g[i];
        if (g->owner && g->rev && (g->pos < g->rlo || g->pos - g->rlo >= GR_RB))
            return 0;
    }
    return 1;
}

typedef struct { uint64_t sig, out; uint32_t holds, passes, fades; int32_t ok; } res_t;

static res_t render(uint32_t s, const char *site, uint32_t nth)
{
    res_t r = {0, 0xcbf29ce484222325ull, 0, 0, 0, 1};
    uint32_t b, i;
    cpu1_host_mode();
    cpu1_units = -1;                                     /* the firmware's split: part 3 and the drums go */
    fades_seen = 0;
    scene_setup(s);
    if (site)
        cpu1_host_hold_site(site, nth ? nth : 0xFFFFFFFFu);   /* nth 0: count the passes, never stop */
    for (b = 0; b < BLOCKS; b++) {
        int32_t o[2 * CTL];
        scene_event(s, b);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            if (o[i] > (1 << 28) || o[i] < -(1 << 28))
                r.ok = 0;
        r.out = fnv(r.out, o, sizeof o);
        if (!consistent(s))
            r.ok = 0;
    }
    r.sig = state_sig(s);
    r.holds = cpu1_host_holds();
    r.passes = cpu1_host_site_passes();
    r.fades = fades_seen;
    return r;
}

static int child(uint32_t s, const char *site, uint32_t nth, res_t *out)   /* a fresh process each (as regress.c) */
{
    int p[2], st = 0;
    pid_t pid;
    size_t got = 0;
    ssize_t n;
    if (pipe(p))
        return -1;
    fflush(stdout);
    if (!(pid = fork())) {
        res_t r = render(s, site, nth);
        close(p[0]);
        if (write(p[1], &r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    close(p[1]);
    while (got < sizeof *out && (n = read(p[0], (char *)out + got, sizeof *out - got)) > 0)
        got += (size_t)n;
    close(p[0]);
    waitpid(pid, &st, 0);
    return got == sizeof *out && WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

int main(void)
{
    uint32_t s, fails = 0;
    setenv("CORES", "1", 1);                             /* the hold is the inline double's (cpu1_host.c) */
    for (s = 0; s < NSCENE; s++) {
        res_t ref;
        uint32_t k, bad = 0, held = 0, whole = 0;
        if (child(s, 0, 0, &ref) || !ref.ok) {
            printf("hold: %-6s the reference render crashed, ran away or is inconsistent\n", SCENE_NAME[s]);
            fails++;
            continue;
        }
        if (s == S_FADE && ref.fades != 3u) {            /* the positive control: both fades happened */
            printf("hold: fade   the reference shows no %s fade: the scene tests nothing\n", ref.fades & 1u ? "engine" : "preset");
            bad++;
        }
        for (k = 0; k < 2u && SCENE_SITES[s][k]; k++) {
            const char *site = SCENE_SITES[s][k];
            res_t c;
            uint32_t nth;
            if (child(s, site, 0, &c)) {                 /* how often a core-1 job passes the site */
                printf("hold: %-6s counting %s: crashed\n", SCENE_NAME[s], site);
                bad++;
                continue;
            }
            if (s == S_FADE) {                           /* a fading part never renders on core 1 */
                if (c.passes) {
                    printf("hold: fade   %s passed %u times on core 1: a fading part must stay on core 0\n", site, c.passes);
                    bad++;
                }
                continue;
            }
            if (!c.passes) {
                printf("hold: %-6s %s is never passed on core 1: the scene tests nothing there\n", SCENE_NAME[s], site);
                bad++;
                continue;
            }
            for (nth = 1; nth <= NHOLDS && nth <= c.passes; nth++) {
                res_t r;
                if (child(s, site, nth, &r) || !r.ok) {
                    printf("hold: %-6s held at %s (pass %u): crashed, ran away or left a state inconsistent\n", SCENE_NAME[s], site, nth);
                    bad++;
                    continue;
                }
                held += r.holds;
                if (r.holds && r.sig != ref.sig) {
                    printf("hold: %-6s held at %s (pass %u): the state differs from the run without a hold\n", SCENE_NAME[s], site, nth);
                    bad++;
                }
                whole += r.holds && r.out == ref.out;
            }
        }
        if (s == S_LATE && !whole) {                     /* a job finished, then held: acc[1] whole, no glitch at all */
            printf("hold: late   no hold after a job's last unit gave the reference's output: a finished job is rendered again\n");
            bad++;
        }
        printf("hold: %-6s %3u holds, %s\n", SCENE_NAME[s], held, bad ? "FAIL" : "same state");
        fails += bad;
    }
    printf("a hold in the middle of a job (fades, FM6, GRAIN, SYMP, SLICER, a finished unit, voice ends): %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
