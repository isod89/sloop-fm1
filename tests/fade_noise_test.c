/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP Pro #4: "3.5mm & USB audio output has distorted noise while notes fading out". The mix floored each part to
 * 16 LSB of the output (fx.c mix_part_run: x >> 4 at the mute / duck gain, always applied at rest since
 * mulq15(32767, 32767) is 32766, and again at the PAN gain; b >> 2 at LEVEL), the MASTER to 4 LSB (mix >> 2), USB AUDIO =
 * FULL to 4 LSB, DIST's drive to 4 x its gain: an error of ~5.5 LSB rms whatever the level, all of it on the note's
 * harmonics (distortion, not hiss), so a note fading out turned into a buzz (THD+N -8 dB at -60 dBFS). Now rounded
 * (dsp.c MULQ12R): the error is the 16-bit output's own rounding.
 * Each case holds a note that decays slowly (SUS 0), renders the output as the codec (and USB) gets it and, over
 * windows of 8192 samples, fits the note (its harmonics, amplitudes moving as a quadratic) by least squares: what is
 * left is the error. Checked between -70 and -30 dBFS (808 SLIDE: -40): its rms under 1 LSB (an ideal 16-bit rounding:
 * 0.29; it was 5 to 6.5), and the THD+N of the window nearest -60 dBFS under -28 dB (it was -12 to -19). Cases: a
 * clean sine (no drive, no FX) at MASTER 4096 and at 2048 (the power-on level), the same through USB AUDIO = FULL
 * (MASTER ignored), and 808 SLIDE (sine, DRV, DIST 30) at 4096.
 * Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NBLK 13000u                                     /* ~9.4 s: the sine from -6 to below -80 dBFS */
#define WIN 8192
static int32_t cap[NBLK * CTL];
static double incq[NBLK];                               /* the note's phase step a block (2^32 a turn) */
static int fails;
static void check(int ok, const char *what) { printf("fade: %-96s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

/* least squares fit of H harmonics of w (each with a constant, linear and quadratic amplitude) and a line; returns the
 * rms of what is left, *sig the fit's rms */
static double fit_left(const double *y, double w, int H, double *sig)
{
    enum { KMAX = 6 * 16 + 2 };
    static double A[KMAX][KMAX], bv[KMAX], phi[KMAX], c[KMAX];
    int K = 6 * H + 2, i, j, k, n;
    double e2 = 0, s2 = 0;
    memset(A, 0, sizeof A), memset(bv, 0, sizeof bv);
    for (n = 0; n < WIN; n++) {
        double t = (n - WIN / 2.0) / WIN;
        for (i = 1, k = 0; i <= H; i++) {
            double s = sin(i * w * n), cs = cos(i * w * n);
            phi[k++] = s, phi[k++] = cs, phi[k++] = s * t, phi[k++] = cs * t, phi[k++] = s * t * t, phi[k++] = cs * t * t;
        }
        phi[k++] = 1, phi[k++] = t;
        for (i = 0; i < K; i++) {
            bv[i] += phi[i] * y[n];
            for (j = 0; j <= i; j++)
                A[i][j] += phi[i] * phi[j];
        }
    }
    for (i = 0; i < K; i++)                             /* Cholesky, then the two triangular solves */
        for (j = 0; j <= i; j++) {
            double s = A[i][j];
            for (k = 0; k < j; k++)
                s -= A[i][k] * A[j][k];
            A[i][j] = i == j ? sqrt(s > 1e-30 ? s : 1e-30) : s / A[j][j];
        }
    for (i = 0; i < K; i++) {
        double s = bv[i];
        for (k = 0; k < i; k++)
            s -= A[i][k] * c[k];
        c[i] = s / A[i][i];
    }
    for (i = K - 1; i >= 0; i--) {
        double s = c[i];
        for (k = i + 1; k < K; k++)
            s -= A[k][i] * c[k];
        c[i] = s / A[i][i];
    }
    for (n = 0; n < WIN; n++) {
        double t = (n - WIN / 2.0) / WIN, f = 0;
        for (i = 1, k = 0; i <= H; i++, k += 6) {
            double s = sin(i * w * n), cs = cos(i * w * n);
            f += (c[k] + c[k + 2] * t + c[k + 4] * t * t) * s + (c[k + 1] + c[k + 3] * t + c[k + 5] * t * t) * cs;
        }
        e2 += (y[n] - f - c[K - 2] - c[K - 1] * t) * (y[n] - f - c[K - 2] - c[K - 1] * t);
        s2 += f * f;
    }
    *sig = sqrt(s2 / WIN);
    return sqrt(e2 / WIN);
}

/* sine: the analog engine's SIN, no drive, no FX; else 808 SLIDE as it comes (its DIST), dry (no sends). usb: capture
 * USB AUDIO = FULL's stream instead of the codec's */
static void run(const char *name, int sine, int32_t master, int usb, int H, int top)
{
    static int32_t o[2 * CTL];
    static double y[WIN];
    track_t *t = &trk[0];
    uint32_t b, i, n;
    double worst = 0, wlev = 0, at60 = 1e9, thdn60 = 0, d60 = 1e9;
    char s[240];
    host_tracks_init();
    for (i = 0; i < NVOICE; i++)                        /* (the case before: its note, held, gone) */
        memset(&t->v[i], 0, sizeof t->v[i]);
    t->nmono = 0, t->mono_note = 0;
    song.master_q12 = (uint32_t)master;
    host_preset(t, 0, 3);                               /* ANALOG 808 SLIDE */
    if (sine) {
        t->p[P_E0] = 3, t->p[P_E1] = 0, t->p[P_E2] = 0, t->p[P_E3] = 0, t->p[P_E4] = 127, t->p[P_E5] = 0;
        t->p[P_E6] = 0, t->p[P_DIST] = 0, t->p[P_ED_PIT] = 0;
        t->p[P_ATK] = 0, t->p[P_DEC] = 118, t->p[P_SUS] = 0, t->p[P_REL] = 60;
    }
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    usb_full_now = (uint8_t)usb;
    for (b = 0; b < 4; b++)
        mix_block(o, CTL);
    trk_note_on(t, 60, 100);
    for (b = 0; b < NBLK; b++) {
        uint32_t ph = t->v[0].ph[0];
        mix_block(o, CTL);
        incq[b] = (double)(uint32_t)(t->v[0].ph[0] - ph) / CTL;
        for (i = 0; i < CTL; i++)
            cap[b * CTL + i] = usb ? usb_out[2 * i] : o[2 * i];
    }
    usb_full_now = 0;
    for (n = 0; n + WIN <= NBLK * CTL; n += WIN) {
        double rms = 0, lev, left, sig;
        for (i = 0; i < WIN; i++)
            rms += (double)cap[n + i] * cap[n + i];
        lev = 20 * log10(sqrt(rms / WIN) * sqrt(2.0) / 32767 + 1e-12);   /* dBFS of a sine as loud */
        if (lev > top || lev < -70)
            continue;
        for (i = 0; i < WIN; i++)
            y[i] = cap[n + i];
        left = fit_left(y, 2 * M_PI * incq[(n + WIN / 2) / CTL] / 4294967296.0, H, &sig);
        if (left > worst)
            worst = left, wlev = lev;
        if (fabs(lev + 60) < d60)
            d60 = fabs(lev + 60), at60 = lev, thdn60 = 20 * log10(left / sig);
    }
    snprintf(s, sizeof s, "%s: error while it fades (-70..%d dBFS) at most %.2f LSB rms (at %.0f dBFS), bound 1",
             name, top, worst, wlev);
    check(worst > 0 && worst < 1.0, s);
    snprintf(s, sizeof s, "%s: THD+N at %.0f dBFS %.1f dB, bound -28", name, at60, thdn60);
    check(d60 < 5 && thdn60 < -28, s);
}

int main(void)
{
    run("sine, MASTER 4096", 1, 4096, 0, 1, -30);
    run("sine, MASTER 2048 (power-on)", 1, 2048, 0, 1, -30);
    run("sine, USB AUDIO = FULL", 1, 2048, 1, 1, -30);
    run("808 SLIDE (DRV, DIST 30), MASTER 4096", 0, 4096, 0, 15, -40);   /* (from -40: its drive's own grit, -58 dB under
                                                         * the note, follows the note: eng_analog.c, not the fade) */
    printf("fade: %d failed\n", fails);
    return fails;
}
