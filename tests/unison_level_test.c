/* SPDX-License-Identifier: GPL-3.0-only */
/* VOICE = UNISON about as loud as POLY, on every engine. Unison starts its extra voices at random phases (voice.c
 * unison_spread) and at a lower level (the velocity: about the same sum). In phase they stack: 8 voices at 0.36 are
 * +9 dB over one. unison_spread moved ph[0] only, and most engines keep more oscillators (ph[1], ph[2], operators, a
 * sample's read position): their voices stacked in phase (ANALOG WARM PAD +12.9 dB, PHASE CZ LEAD +10.6, SAMPLE +8.4
 * on average). Each engine now spreads all of its oscillators by the same time shift (their relative phases, the
 * timbre, as they were); SAMPLE starts a voice up to four periods later (at most 256 source samples).
 * For every preset of every engine, notes 36 and 60 (velocity 100, chorus, delay, reverb off): the rms over
 * 0.05 .. 0.55 s in UNISON (the preset's DETUNE) against POLY. Fails when a render is more than +8 dB over POLY or an
 * engine's mean over +5 dB. Exit status: failed checks. (The same file as Clipper's, which also plays UNI4.) */
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
static double play(uint32_t e, uint32_t p, uint32_t mode, uint32_t note)
{
    uint32_t i, b, k, any = 1;
    double s2 = 0;
    for (k = 0; k < NPART; k++)
        trk_all_off(&trk[k]);
    for (b = 0; b < 6000u && any; b++) {             /* the last note rung out */
        mix_block(o, CTL);
        for (any = 0, i = 0; i < NVOICE; i++)
            any |= trk[0].v[i].active;
    }
    for (i = 0; i < P_E0; i++)                       /* the preset as ui.c apply_preset_to loads it */
        trk[0].p[i] = TP[i].def;
    host_preset(&trk[0], e, p);
    trk[0].p[P_AMODE] = 0, trk[0].p[P_DLY] = trk[0].p[P_REV] = 0, trk[0].p[P_CHOR] = 0;
    trk[0].p[P_VOICE] = (int16_t)mode;
    for (b = 0; b < 16; b++)
        mix_block(o, CTL);
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < 760; b++) {
        mix_block(o, CTL);
        if (b >= 69)
            for (i = 0; i < CTL; i++) {
                double x = (TAPL[i] + TAPR[i]) / 2.0;
                s2 += x * x;
            }
    }
    trk_note_off(&trk[0], note);
    return sqrt(s2 / ((760 - 69) * CTL));
}
#if CLIPPER_SYNTH
#define NM 2u
static const uint32_t MODE[NM] = { V_UNISON, V_PUNI4 };
static const char *const MN[NM] = { "UNISON", "UNI4" };
#else
#define NM 1u
static const uint32_t MODE[NM] = { V_UNISON };
static const char *const MN[NM] = { "UNISON" };
#endif
/* engine e: in each unison mode, the mean unison / POLY ratio (dB) of its presets at notes 36 and 60 */
static void engine(uint32_t e, int *fails)
{
    const engine_t *en = ENGINES[e];
    uint32_t p, k, m, n = 0;
    double sum[NM][2], worst[NM], mean;
    const char *wn[NM];
    for (m = 0; m < NM; m++)
        sum[m][0] = sum[m][1] = 0, worst[m] = -99, wn[m] = "";
    for (p = 0; p < en->npresets; p++, n++)
        for (k = 0; k < 2u; k++) {
            uint32_t note = k ? 60u : 36u;
            double r0 = play(e, p, V_POLY, note);
            for (m = 0; m < NM; m++) {
                double d = 20 * log10((play(e, p, MODE[m], note) + 1) / (r0 + 1));
                if (d > 8.0) {
                    printf("unison: %-7s %-6s %-14s note %u: %+.1f dB over POLY  FAIL\n", en->name, MN[m],
                           en->presets[p].name, note, d);
                    ++*fails;
                }
                if (d > worst[m])
                    worst[m] = d, wn[m] = en->presets[p].name;
                sum[m][k] += d;
            }
        }
    for (m = 0; n && m < NM; m++) {
        mean = (sum[m][0] + sum[m][1]) / (2 * n);
        printf("unison: %-7s %-6s mean %+5.1f dB over POLY (note 36 %+5.1f, 60 %+5.1f; %3u presets; the most %+5.1f, %s)%s\n",
               en->name, MN[m], mean, sum[m][0] / n, sum[m][1] / n, n, worst[m], wn[m], mean > 5.0 ? "  FAIL" : "");
        if (mean > 5.0)
            ++*fails;
    }
}
int main(void)
{
    int fails = 0;
    uint32_t e;
    host_tracks_init();
    for (e = 0; e < NENGINES; e++)
        engine(e, &fails);
    printf("unison: %d failed\n", fails);
    return fails;
}
