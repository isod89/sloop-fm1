/* SPDX-License-Identifier: GPL-3.0-only */
/* NOISE in LFSR mode: its register clock follows the keys past the sample rate. A Clipper report (clipper-fm1 #10):
 * NZ ARCADE (TONE +84 semitones) played the same pitch on the whole upper half of the keyboard: the clock stopped at
 * one register step a sample. Now a sample takes as many steps as clocks passed: the register's period (127 steps, the
 * short register of NZ ARCADE) gives a fundamental of clock / 127 that goes on rising. For each note: the spectrum at
 * that fundamental stands out of the rest, and no two notes give the same samples. Exit status: failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define NB 1200u                                      /* blocks a note (~0.9 s) */
static int32_t cap[24][NB * CTL];
static int fails;
static void ok(int c, const char *what)
{
    printf("noise: %-96s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}
/* the magnitude of the DFT of x[0..n) at f Hz (Hann window), and its median over 200 Hz .. 10 kHz */
static double mag_at(const int32_t *x, uint32_t n, double f)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), a = 2 * M_PI * f * i / FS;
        re += x[i] * w * cos(a);
        im -= x[i] * w * sin(a);
    }
    return sqrt(re * re + im * im);
}
int main(void)
{
    uint32_t e, pi = 0, k, b, nn = 0, same = 0, weak = 0;
    static int32_t o[CTL * 2];
    const engine_t *en = ENGINES[ENGI_NOISE];
    host_tracks_init();
    for (k = 0; k < en->npresets; k++)
        if (!strcmp(en->presets[k].name, "NZ ARCADE"))
            pi = k;
    ok(!strcmp(en->presets[pi].name, "NZ ARCADE"), "NZ ARCADE found");
    e = ENGI_NOISE;
    host_preset(&trk[0], e, pi);
    trk[0].p[P_VOICE] = V_POLY;
    for (k = 36; k <= 102; k += 3, nn++) {
        trk_all_off(&trk[0]);
        for (b = 0; b < 400u; b++)
            mix_block(o, CTL);
        trk_note_on(&trk[0], k, 100);
        for (b = 0; b < NB; b++) {
            uint32_t i;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++)
                cap[nn][b * CTL + i] = o[2 * i];
        }
        trk_note_off(&trk[0], k);
    }
    for (k = 0; k < nn; k++) {
        uint32_t note = 36 + 3 * k, n = 16384, a = 4000, j;
        double clock = 440.0 * pow(2.0, (note + en->presets[pi].e[7] - 69) / 12.0), f0 = clock / 127.0, m, med;
        static double s[64];
        if (k + 1 < nn && !memcmp(&cap[k][a], &cap[k + 1][a], 4096u * 4u))
            same++;
        if (f0 > 10000)
            continue;
        m = 0;
        for (j = 0; j < 7; j++) {                       /* the strongest bin around f0 (+-3 bins) */
            double v = mag_at(&cap[k][a], n, f0 + (j - 3.0) * FS / n);
            if (v > m)
                m = v;
        }
        for (j = 0; j < 64; j++)                        /* a sample of the spectrum: 200 Hz .. 10 kHz */
            s[j] = mag_at(&cap[k][a], n, 200.0 + j * 153.1);
        for (j = 1; j < 64; j++) {                      /* (sorted: the median) */
            double v = s[j];
            uint32_t q = j;
            while (q && s[q - 1] > v) { s[q] = s[q - 1]; q--; }
            s[q] = v;
        }
        med = s[32];
        printf("noise: NZ ARCADE note %3u: clock %8.0f Hz, fundamental %6.0f Hz, %5.1f dB over the median\n", note, clock, f0,
               20 * log10(m / (med + 1e-9)));
        if (20 * log10(m / (med + 1e-9)) < 6)
            weak++;
    }
    ok(same == 0, "NZ ARCADE: no two notes (3 semitones apart, C2..F#7) give the same samples");
    ok(weak == 0, "NZ ARCADE: each note's fundamental (clock / 127) stands 6 dB over the spectrum's median");
    printf("noise: %d failed\n", fails);
    return fails;
}
