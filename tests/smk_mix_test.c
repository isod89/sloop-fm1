/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 HOME menu > AUDIO > LIMITER (firmware/src/fx.c, SMK_MIX): held chords through the whole mix (every factory
 * preset, velocity 100) at the MASTER knob's power-on position (724) and at its top.
 *   SOFT (the default): a held 4- or 6-voice chord of the usual sounds (the power-on sounds of tracks 1..3 among them)
 *   leaves the limiter alone; fewer chords of all the presets reach it than in CLASSIC; at the top a hot chord is still
 *   caught; nothing clips. CLASSIC: SLOOP's knob law and threshold, as before.
 *   cc -O2 -DSMK_MIX=1 -Ibuild/gen -Ifirmware/src tests/smk_mix_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(int ok, const char *what) { printf("smk_mix: %-78s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

static const uint8_t CHORD[6] = {48, 60, 64, 67, 72, 55};
typedef struct { int32_t g, out; } res_t;           /* the least limiter gain (Q15), the largest output sample */
static res_t chord(uint32_t e, uint32_t pi, uint32_t nv, uint32_t knob, uint32_t classic)
{
    int32_t o[2 * CTL];
    uint32_t b, i;
    res_t r = {32768, 0};
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    memset(trk[0].v, 0, sizeof trk[0].v);
    song.sel = 0;
    lim_classic = (uint8_t)classic;
    song.master_q12 = smk_master_q12(knob, classic);
    memset(&lim_dac, 0, sizeof lim_dac);
    lim_dac.env = LIM_TH, lim_dac.g = 32768, master_cur = -1;
    for (b = 0; b < FS / CTL / 4u; b++)
        mix_block(o, CTL);
    for (i = 0; i < nv; i++)
        trk_note_on(&trk[0], CHORD[i], 100);
    for (b = 0; b < FS * 3u / 2u / CTL; b++) {          /* held 1.5 s: the attack and the body */
        mix_block(o, CTL);
        r.g = lim_dac.g < r.g ? lim_dac.g : r.g;
        for (i = 0; i < 2u * CTL; i++)
            r.out = abs(o[i]) > r.out ? abs(o[i]) : r.out;
    }
    for (i = 0; i < nv; i++)
        trk_note_off(&trk[0], CHORD[i]);
    return r;
}
static int find(const char *name, uint32_t *e, uint32_t *pi)
{
    for (*e = 0; *e < NENGINES; (*e)++)
        for (*pi = 0; *pi < ENGINES[*e]->npresets; (*pi)++)
            if (!strcmp(ENGINES[*e]->presets[*pi].name, name))
                return 1;
    return 0;
}

int main(void)
{
    /* the usual held sounds; the power-on ones first (ui.c TRK_DEF: 808 BOOM, RHODES, LOFI FLUTE) */
    static const char *const USUAL[] = {"808 BOOM", "RHODES", "LOFI FLUTE", "SUPERSAW", "SINE PAD", "ATMOS PAD", "CLAV",
                                        "CZ PAD", "SOUL ORGAN", "ENSEMBLE", "CHOIR AAH", "VOX PAD", "TINE EP", "DRAWBARS"};
    uint32_t e, pi, i, k, n = 0, lim_soft[2] = {0, 0}, lim_cl[2] = {0, 0}, ok, clip = 0;
    char b[128];
    res_t r;

    /* the knob laws: CLASSIC as the FM-1's pot; SOFT the same at the top, quieter below */
    for (ok = 1, k = 0; k < 1024u; k++)
        ok &= smk_master_q12(k, 1) == (k * k) >> 8;
    check(ok, "CLASSIC: the MASTER knob as before ((k * k) >> 8)");
    lim_classic = 1;
    check(LIM_TH == 18000, "CLASSIC: the threshold as before (18000)");
    lim_classic = 0;
    check(LIM_TH == LIM_T_SOFT && LIM_T_SOFT > 32767 && knee(LIM_T_SOFT) < 32767, "SOFT: the threshold past full scale, under it after the knee");
    snprintf(b, sizeof b, "SOFT: the knob's top as CLASSIC's (%u / %u), %.1f dB at power-on (724)", (unsigned)smk_master_q12(1023, 0),
             (unsigned)smk_master_q12(1023, 1), 20.0 * log10((double)smk_master_q12(724, 0) / smk_master_q12(724, 1)));
    check(smk_master_q12(1023, 0) * 100u >= smk_master_q12(1023, 1) * 99u && smk_master_q12(724, 0) < smk_master_q12(724, 1), b);
    for (ok = 1, k = 1; k < 1024u; k++)
        ok &= smk_master_q12(k, 0) >= smk_master_q12(k - 1u, 0);
    check(ok, "SOFT: the knob only ever turns up");

    /* the usual sounds, 4 and 6 voices, power-on knob: SOFT leaves them alone */
    for (ok = 1, i = 0; i < sizeof USUAL / sizeof USUAL[0]; i++) {
        if (!find(USUAL[i], &e, &pi)) {
            printf("smk_mix: no preset %s\n", USUAL[i]);
            ok = 0;
            continue;
        }
        for (k = 4; k <= 6u; k += 2u) {
            res_t c = chord(e, pi, k, 724, 1);
            r = chord(e, pi, k, 724, 0);
            if (r.g < 32768) {
                printf("smk_mix:   %s, %u voices: SOFT %.1f dB (CLASSIC %.1f dB)\n", USUAL[i], k, 20.0 * log10(r.g / 32768.0),
                       20.0 * log10(c.g / 32768.0));
                ok = 0;
            }
        }
    }
    check(ok, "SOFT, the knob at power-on: a held 4 / 6 voice chord of the usual sounds, no gain reduction");

    /* every preset: how many chords reach the limiter, SOFT against CLASSIC; nothing clips */
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ENGINES[e]->npresets; pi++, n++)
            for (k = 0; k < 2u; k++) {
                r = chord(e, pi, k ? 6u : 4u, 724, 0);
                lim_soft[k] += r.g < 32768;
                clip += r.out > 32767;
                r = chord(e, pi, k ? 6u : 4u, 724, 1);
                lim_cl[k] += r.g < 32768;
                clip += r.out > 32767;
            }
    snprintf(b, sizeof b, "every preset, knob at power-on: chords limited SOFT 4v %u / 6v %u, CLASSIC %u / %u (of %u)",
             (unsigned)lim_soft[0], (unsigned)lim_soft[1], (unsigned)lim_cl[0], (unsigned)lim_cl[1], (unsigned)n);
    check(lim_soft[0] * 4u <= lim_cl[0] && lim_soft[1] * 3u <= lim_cl[1], b);

    /* the knob at the top: a hot 6-voice chord is still caught, a single peak too; never past full scale */
    if (find("BRASS SECT", &e, &pi)) {
        r = chord(e, pi, 6, 1023, 0);
        snprintf(b, sizeof b, "SOFT, knob at the top, BRASS SECT x6: caught (%.1f dB), largest sample %d", 20.0 * log10(r.g / 32768.0), (int)r.out);
        check(r.g < 32768 && r.out <= 32767, b);
    } else {
        check(0, "BRASS SECT");
    }
    {
        int32_t l[CTL], rr[CTL], pk = 0;
        uint32_t blk;
        memset(&lim_dac, 0, sizeof lim_dac);
        lim_dac.env = LIM_TH, lim_dac.g = 32768;
        for (blk = 0; blk < 8u; blk++) {
            for (i = 0; i < CTL; i++)
                l[i] = rr[i] = blk == 2u && i == CTL / 2u ? 200000 : 1000;
            lim_block(&lim_dac, l, rr, CTL);
            for (i = 0; i < CTL; i++)
                pk = abs(l[i]) > pk ? abs(l[i]) : pk;
        }
        snprintf(b, sizeof b, "SOFT: a single peak 6x full scale is caught (out %d <= %d), the knee keeps it under full scale", (int)pk, LIM_T_SOFT);
        check(pk <= LIM_T_SOFT && knee(pk) <= 32767, b);
    }
    check(!clip, "no output sample past full scale (every render above)");
    printf("smk_mix: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
