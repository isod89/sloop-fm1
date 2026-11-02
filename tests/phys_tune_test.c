/* SPDX-License-Identifier: GPL-3.0-only */
/* PHYS in tune: every pitched factory preset plays its notes at the equal-tempered pitch. The STRING and SYMP models
 * were sharp: STR PLUCK +20 cents above E5 and +56 below E2, PLUCK BASS +71, BANJO +86, HARPSICH +104 at E1 .. E2
 * (the loop's damping filter was compensated by twice its lag, the dispersion all-pass by a rule of thumb that grew
 * with the delay; phys_dsp.c px_string_block).
 * Pitched: the STRING and SYMP models (plucked strings, the sympathetic ones), and MODAL with a harmonic body
 * (STRC 32 .. 38: stiffness 0) struck. Not here: MODAL's stiff bodies (bars, bells, blocks: their fundamental is set
 * off on purpose, Rings' NthHarmonicCompensation), its bowed presets (noise through the same resonators: in tune as
 * the struck ones, their phase too noisy to measure) and MEMB (drums).
 * For each preset, notes 28 .. 100 every 4 (velocity 100, the preset's sound without vibrato): the fundamental from
 * the phase advance of its partial between two windows 0.25 s apart (exact on a synthetic tone: SELF=cents tests the
 * estimator); fails beyond +-5 cents (env TOL). The strings with a curved bridge (STRING below STRC 31, SYMP with BUZZ:
 * KOTO, SITAR, TANPURA) bend their pitch as loud as they ring (the bridge shortens the string on the way up, longer on
 * the way down: Rings' CURVED_BRIDGE, by design: SITAR -16 at E5): +-20 cents for them (BENDTOL); their loop is in tune (+-1 cent
 * with the bridge straight). ONLY=name measures one preset. Exit status: failed checks. */
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
#define W 16384u                         /* window, samples (0.37 s) */
#define D 11025u                         /* the second window this much later (0.25 s) */
#define NX (W + D)
static double x[NX];
/* the Hann-windowed DFT of x[o .. o + W) at f Hz: re, im */
static void dft(uint32_t o, double f, double *re, double *im)
{
    uint32_t i;
    double r = 0, m = 0, w = 2 * M_PI * f / FS, c = cos(w), s = sin(w), cr = 1, ci = 0;
    for (i = 0; i < W; i++) {
        double h = (0.5 - 0.5 * cos(2 * M_PI * i / W)) * x[o + i], t;
        r += h * cr, m -= h * ci;
        t = cr * c - ci * s, ci = ci * c + cr * s, cr = t;   /* (rotate: e^(j w i)) */
    }
    *re = r, *im = m;
}
/* the fundamental near fe: the strongest of +-80 cents (2-cent steps), then the phase advance over D samples */
static double f0_of(double fe)
{
    double best = 0, bf = fe, k, r0, i0, r1, i1, dp;
    for (k = -80; k <= 80; k += 2) {
        double ff = fe * pow(2, k / 1200.0), v;
        dft(0, ff, &r0, &i0);
        v = r0 * r0 + i0 * i0;
        if (v > best)
            best = v, bf = ff;
    }
    dft(0, bf, &r0, &i0);
    dft(D, bf, &r1, &i1);
    dp = atan2(i1, r1) - atan2(i0, r0) - 2 * M_PI * bf * D / FS;   /* = 2 pi (f - bf) D / FS, mod 2 pi */
    dp -= 2 * M_PI * floor(dp / (2 * M_PI) + 0.5);
    return bf + dp * FS / (2 * M_PI * D);
}
static int pitched(const preset_t *pr)
{
    if (pr->e[0] == PM_STRING || pr->e[0] == PM_SYMP)
        return 1;
    return pr->e[0] == PM_MODAL && pr->e[1] >= 32 && pr->e[1] <= 38 && !pr->e[6];   /* harmonic, struck */
}
static int bent(const preset_t *pr)                      /* a curved bridge (px_string_block: STRUCTURE < 0.24) */
{
    return (pr->e[0] == PM_STRING && pr->e[1] * 516 < 15729) || (pr->e[0] == PM_SYMP && 16384 - pr->e[6] * 129 < 15729);
}
int main(void)
{
    static int32_t o[CTL * 2];
    const engine_t *en = ENGINES[ENGI_PHYS];
    double tol = getenv("TOL") ? atof(getenv("TOL")) : 5.0, btol = getenv("BENDTOL") ? atof(getenv("BENDTOL")) : 20.0;
    uint32_t pi, n, i, b, k, fails = 0;
    host_tracks_init();
    if (getenv("SELF")) {                                  /* the estimator on a decaying 3-partial tone */
        for (n = 28; n <= 100; n += 4) {
            double fe = 440 * pow(2, ((int)n - 69) / 12.0), ft = fe * pow(2, atof(getenv("SELF")) / 1200);
            for (i = 0; i < NX; i++) {
                double t = (i + 4480.0) / FS;
                x[i] = exp(-t * 3) * (8000 * sin(2 * M_PI * ft * t) + 6000 * sin(2 * M_PI * 2.01 * ft * t + 1) +
                                      3000 * sin(2 * M_PI * 3 * ft * t));
            }
            printf(" %u:%+.2f", n, 1200 * log2(f0_of(fe) / ft));
        }
        printf("\n");
        return 0;
    }
    for (pi = 0; pi < en->npresets; pi++) {
        const preset_t *pr = &en->presets[pi];
        double worst = 0, t = bent(pr) ? btol : tol;
        uint32_t bad = 0, wn = 0;
        if (!pitched(pr) || (getenv("ONLY") && !strstr(pr->name, getenv("ONLY"))))
            continue;
        printf("phys_tune: %-12s", pr->name);
        for (n = 28; n <= 100; n += 4) {
            double fe = 440 * pow(2, ((int)n - 69) / 12.0), c;
            uint32_t any = 1;
            for (k = 0; k < NPART; k++)
                trk_all_off(&trk[k]);
            for (b = 0; b < 6000u && any; b++) {           /* the last note rung out */
                mix_block(o, CTL);
                for (any = 0, i = 0; i < NVOICE; i++)
                    any |= trk[0].v[i].active;
            }
            for (i = 0; i < P_E0; i++)                     /* the preset as ui.c apply_preset_to loads it (dry) */
                trk[0].p[i] = TP[i].def;
            host_preset(&trk[0], ENGI_PHYS, pi);
            trk[0].p[P_AMODE] = 0, trk[0].p[P_DLY] = trk[0].p[P_REV] = 0, trk[0].p[P_VOICE] = V_POLY;
            trk[0].p[P_LD_PIT] = 0;                        /* (no vibrato) */
            for (b = 0; b < 16; b++)
                mix_block(o, CTL);
            trk_note_on(&trk[0], n, 100);
            for (b = 0; b < 140; b++)                      /* (0.1 s: the attack) */
                mix_block(o, CTL);
            for (b = 0; b < NX / CTL; b++) {
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++)
                    x[b * CTL + i] = (TAPL[i] + TAPR[i]) / 2.0;
            }
            trk_note_off(&trk[0], n);
            c = 1200 * log2(f0_of(fe) / fe);
            printf(" %3u:%+5.1f", n, c);
            if (fabs(c) > t)
                bad++;
            if (fabs(c) > fabs(worst))
                worst = c, wn = n;
        }
        printf("  %s (worst %+.1f cents at %u%s)\n", bad ? "FAIL" : "ok", worst, wn, bent(pr) ? "; curved bridge: +-20" : "");
        fails += bad != 0;
    }
    printf("phys_tune: %u failed\n", fails);
    return (int)fails;
}
