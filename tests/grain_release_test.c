/* SPDX-License-Identifier: GPL-3.0-only */
/* GRAIN: a note played in the release of the note before sounds as loud as alone. A grain starts only while the
 * part's grains decode at most GR_LOAD_MAX source samples an output sample; the released note kept starting grains
 * through its long release, so the new held note's grains were refused for the load: SCAN PAD 86 then 88 (290 ms
 * after the release) came out 29 dB down, SLOOP's PNO FREEZE 88 then 90 35 dB down, for the whole note.
 * For each preset, note n (C5 .. F#7, every 2) for 1 s, released, 290 ms later n + 2 for 1 s: from 0.29 s on it holds
 * at least a third of the grains it holds alone (starved: none; the rms is printed). Exit status: failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#if CLIPPER_SYNTH
#define TAPL mix_l
#define TAPR mix_r
#else
#define TAPL acc[0].l
#define TAPR acc[0].r
#endif
static int32_t o[CTL * 2];
static void reset(uint32_t pi)
{
    uint32_t i, b, k;
    for (k = 0; k < NPART; k++) {
        trk_all_off(&trk[k]);
        for (i = 0; i < NVOICE; i++) trk[k].v[i].active = 0, trk[k].v[i].stage = 0, trk[k].v[i].env = 0, trk[k].v[i].env_out = 0;
    }
    for (b = 0; b < 400; b++) mix_block(o, CTL);
    for (i = 0; i < P_E0; i++) trk[0].p[i] = TP[i].def;
    host_preset(&trk[0], ENGI_GRAIN, pi);
    trk[0].p[P_AMODE] = 0, trk[0].p[P_DLY] = trk[0].p[P_REV] = 0, trk[0].p[P_VOICE] = V_POLY;
    for (b = 0; b < 64; b++) mix_block(o, CTL);
}
/* note held for blocks: the rms of blocks from .. on, *ng the mean count of its grains there (the held voices') */
static double hold(uint32_t note, uint32_t blocks, uint32_t from, double *ng)
{
    gr_part_t *P = (gr_part_t *)eng_arena_of(&trk[0], ENGI_GRAIN);
    uint32_t b, i;
    double s2 = 0, g = 0;
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        if (b < from)
            continue;
        for (i = 0; i < CTL; i++) {
            double x = (TAPL[i] + TAPR[i]) / 2.0;
            s2 += x * x;
        }
        for (i = 0; i < GR_NG; i++)
            g += P->g[i].owner && trk[0].v[(P->g[i].owner - 1u) % NVOICE].gate;
    }
    trk_note_off(&trk[0], note);
    if (ng)
        *ng = g / (blocks - from);
    return sqrt(s2 / ((blocks - from) * CTL));
}
int main(void)
{
    uint32_t pi, b, gap = 400, n1, n2;
    int fails = 0;
    host_tracks_init();
    for (pi = 0; pi < ENGINES[ENGI_GRAIN]->npresets; pi++) {
        uint32_t bad = 0;
        double worst = 1e9;
        for (n1 = 72; n1 <= 100; n1 += 2) {
            double alone, after, g0, g1;
            n2 = n1 + 2;
            reset(pi);
            alone = hold(n2, 1400, 400, &g0);
            reset(pi);
            hold(n1, 1400, 1300, 0);                  /* 1 s */
            for (b = 0; b < gap; b++)                 /* released, 290 ms */
                mix_block(o, CTL);
            after = hold(n2, 1400, 400, &g1);
            if (g1 < g0 / 3) {                        /* (the level alone varies +-6 dB from cloud to cloud) */
                bad++;
                printf("grain: %-14s note %u: %.1f grains, rms %6.0f alone; 290 ms after note %u's release %.1f grains, rms %6.0f (%+.1f dB)\n",
                       ENGINES[ENGI_GRAIN]->presets[pi].name, n2, g0, alone, n1, g1, after, 20 * log10((after + 1) / (alone + 1)));
            }
            if (g1 / (g0 + 1e-9) < worst)
                worst = g1 / (g0 + 1e-9);
        }
        printf("grain: %-14s 15 pairs C5 .. F#7: %s (the fewest grains: %.0f %% of alone)\n", ENGINES[ENGI_GRAIN]->presets[pi].name, bad ? "FAIL" : "ok", worst * 100);
        fails += bad != 0;
    }
    printf("grain: %d failed\n", fails);
    return fails;
}
