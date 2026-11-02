/* SPDX-License-Identifier: GPL-3.0-only */
/* SAMPLE and GRAIN follow the keys to the top of the FM-1's keyboard (OCT +3: note 115). Both bounded their decode
 * load with a cap on the playback rate (SAMPLE: 3 octaves over the zone's root; GRAIN: 2 source samples an output
 * sample), so every key past it played the same pitch: Clipper's sets are one 16 kHz zone (VOX's root A3), so SAMPLE
 * CHOIR was flat from A6 (93) and VOX CLOUD from G6 (87); SLOOP's top zones (root C5) went flat from C8 (108).
 * For each preset that follows the keys (C4 -> C5 one octave up): each note two semitones over the one before sounds
 * 1..3 semitones higher (the shift of its log-frequency spectrum, or of its strongest peak, or 4 semitones over the note before it;
 * no two steps in a row wrong), from C4 to TOP (env TOP, default 115). GRAIN: up to
 * where a grain would read more than 8 source samples an output sample (its part's decode bound, GR_LOAD_MAX, with the
 * preset's PTCH and RAND detune), if that comes first: past it a grain's pitch may stop (the cost bound). SAMPLE: up to
 * where a voice would decode more than 8 source samples an output sample (a 44.1 kHz zone 3 octaves up, the bound it
 * always had; SMP_STEP_MAX), if that comes first.
 * Exit status: failed checks. */
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
#define N 32768u                         /* (0.74 s: a cloud of short grains averages out) */
#define LB 8                             /* log-spectrum bins a semitone */
#define LN (128 * LB)                    /* MIDI 8 .. 136 */
static double re_[N], im_[N];
static int fails;
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j = 0, k, m;
    for (i = 1; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (i = 0; i < m / 2; i++) {
                double wr = cos(a * i), wi = sin(a * i);
                double xr = re[k + i + m / 2] * wr - im[k + i + m / 2] * wi, xi = re[k + i + m / 2] * wi + im[k + i + m / 2] * wr;
                re[k + i + m / 2] = re[k + i] - xr; im[k + i + m / 2] = im[k + i] - xi;
                re[k + i] += xr; im[k + i] += xi;
            }
    }
}
/* note k of preset pi: its log spectrum (dB over a -60 dB floor) into ls, returns the rms */
static double render(uint32_t e, uint32_t pi, uint32_t note, double *ls)
{
    static int32_t o[CTL * 2];
    static double x[N], mag[N / 2 + 1];
    uint32_t i, b, any = 1, k;
    double s = 0, s2 = 0, mx = 1e-9;
    for (k = 0; k < NPART; k++)
        trk_all_off(&trk[k]);
    for (b = 0; b < 3000u && any; b++) {         /* the last note's release */
        mix_block(o, CTL);
        for (any = 0, i = 0; i < NVOICE; i++)
            any |= trk[0].v[i].active;
    }
    for (i = 0; i < P_E0; i++)                    /* the preset as ui.c apply_preset_to loads it (dry) */
        trk[0].p[i] = TP[i].def;
    host_preset(&trk[0], e, pi);
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    for (b = 0; b < 16; b++)
        mix_block(o, CTL);
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < 140; b++)                     /* (0.1 s: the attack) */
        mix_block(o, CTL);
    for (b = 0; b < N / CTL; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            x[b * CTL + i] = (TAPL[i] + TAPR[i]) / 2.0;
    }
    trk_note_off(&trk[0], note);
    for (i = 0; i < N; i++)
        s += x[i];
    for (i = 0; i < N; i++) {
        double v = x[i] - s / N;
        s2 += v * v;
        re_[i] = v * (0.5 - 0.5 * cos(2 * M_PI * i / N));
        im_[i] = 0;
    }
    fft(re_, im_, N);
    for (i = 0; i <= N / 2; i++)
        mag[i] = sqrt(re_[i] * re_[i] + im_[i] * im_[i]);
    for (i = 0; i < LN; i++) {                    /* the strongest bin within each 1/8 semitone */
        double f0 = 440.0 * pow(2, ((double)i / LB + 8 - 0.5 / LB - 69) / 12), f1 = f0 * pow(2, 1.0 / (12 * LB));
        double b0 = f0 * N / FS, b1 = f1 * N / FS, v = 0;
        uint32_t a = (uint32_t)b0, c;
        if (b1 >= N / 2) {
            ls[i] = 0;
            continue;
        }
        if (b1 - b0 < 1) {
            double fr = (b0 + b1) / 2 - a;
            v = mag[a] * (1 - fr) + mag[a + 1] * fr;
        } else
            for (c = a; c <= (uint32_t)b1; c++)
                if (mag[c] > v)
                    v = mag[c];
        ls[i] = v;
        if (v > mx)
            mx = v;
    }
    for (i = 0; i < LN; i++) {
        double d = 20 * log10(ls[i] / mx + 1e-9) + 60;
        ls[i] = d > 0 ? d : 0;
    }
    return sqrt(s2 / N);
}
/* GRAIN: source samples an output sample a grain of the note reads at most (PTCH, the RAND detune at its top), Q16 */
static double grain_rate(uint32_t pi, uint32_t note)
{
    const preset_t *pr = &ENGINES[ENGI_GRAIN]->presets[pi];
    uint32_t src = (uint32_t)pr->e[0] % SMP_NALL;
    int32_t zl = gr_find(src, note);
    const smp_zone_t *z;
    double d16;
    if (zl < 0)
        return 0;
    z = gr_zone(src, (uint32_t)zl);
    d16 = note * 16.0 + pr->e[4] * 16 - z->root16 + pr->e[6] * pr->e[6] * 192.0 / (127 * 127);
    return pow(2, d16 / 192) * z->rate / 65536.0;
}
/* SAMPLE: the playback rate of the note (source samples an output sample) and its cap (eng_sample.c sample_render) */
static int sample_capped(uint32_t pi, uint32_t note)
{
    const preset_t *pr = &ENGINES[4]->presets[pi];
    uint32_t si = (uint32_t)pr->e[0] % SMP_NALL, i;
    const smp_zone_t *z = 0;
    double rate, lim;
    if (si >= SMP_NSETS)
        return 0;
    for (i = 0; i < SMP_SETS[si].nz; i++)
        if (note >= SMP_ZONES[SMP_SETS[si].z0 + i].lo && note <= SMP_ZONES[SMP_SETS[si].z0 + i].hi)
            z = &SMP_ZONES[SMP_SETS[si].z0 + i];
    if (!z)
        z = &SMP_ZONES[SMP_SETS[si].z0];
    rate = pow(2, (note * 16.0 + pr->e[1] * 16 - z->root16) / 192) * z->rate / 65536.0;
    lim = 8.0 * z->rate / 65536.0;
    return rate > (lim > (double)(SMP_STEP_MAX >> 16) ? lim : (double)(SMP_STEP_MAX >> 16));
}
/* how far b's spectrum lies over a's, in semitones (-4 .. +14) */
static double shift(const double *a, const double *b)
{
    int s, i, best = 0;
    double bv = -1;
    for (s = -4 * LB; s <= 14 * LB; s++) {
        double ab = 0, aa = 0, bb = 0;
        for (i = 0; i < LN; i++) {
            int j = i + s;
            double y = j >= 0 && j < LN ? b[j] : 0;
            ab += a[i] * y, aa += a[i] * a[i], bb += y * y;
        }
        if (aa > 0 && bb > 0 && ab / sqrt(aa * bb) > bv)
            bv = ab / sqrt(aa * bb), best = s;
    }
    return (double)best / LB;
}
/* the same from the strongest peak alone (a second opinion: either one right is enough) */
static double peak_shift(const double *a, const double *b)
{
    int i, ia = 0, ib = 0;
    for (i = 1; i < LN; i++) {
        if (a[i] > a[ia])
            ia = i;
        if (b[i] > b[ib])
            ib = i;
    }
    return (double)(ib - ia) / LB;
}
int main(void)
{
    static const uint32_t EN[2] = {4, ENGI_GRAIN};   /* SAMPLE, GRAIN */
    static double l0[LN], l1[LN], lp[LN];       /* notes n, n + 2, n - 2 */
    uint32_t top = getenv("TOP") ? (uint32_t)atoi(getenv("TOP")) : 115u, k, pi, n;
    host_tracks_init();
    for (k = 0; k < 2u; k++) {
        const engine_t *en = ENGINES[EN[k]];
        for (pi = 0; pi < en->npresets; pi++) {
            double r60 = render(EN[k], pi, 60, l0), r72 = render(EN[k], pi, 72, l1), sh = shift(l0, l1), r;
            uint32_t bad = 0, first = 0, tested = 0, run = 0, runs = 0;   /* runs: two wrong steps in a row (one alone:
                                                         * a grain cloud's spectrum can fool the estimate once) */
            if (r60 < 30 || r72 < 30 || sh < 11 || sh > 13) {
                printf("pitch: %-6s %-14s C4 -> C5: %+5.2f semitones: does not follow the keys (a kit), not tested\n",
                       en->name, en->presets[pi].name, sh);
                continue;
            }
            r = render(EN[k], pi, 60, l0);
            memcpy(lp, l0, sizeof lp);
            for (n = 60; n + 2u <= top; n += 2u) {
                double r1, d;
                if (EN[k] == ENGI_GRAIN && grain_rate(pi, n + 2u) > 8.0) {
                    printf("pitch: %-6s %-14s from note %u: past the grain decode bound, not tested\n", en->name, en->presets[pi].name, n + 2u);
                    break;
                }
                if (EN[k] == 4u && sample_capped(pi, n + 2u)) {
                    printf("pitch: %-6s %-14s from note %u: past the decode bound, not tested\n", en->name, en->presets[pi].name, n + 2u);
                    break;
                }
                r1 = render(EN[k], pi, n + 2u, l1);
                if (r > r60 / 30 && r1 > r60 / 30) {
                    d = shift(l0, l1);
                    tested++;
                    if (d < 1.0 || d > 3.0)
                        d = peak_shift(l0, l1);
                    if ((d < 1.0 || d > 3.0) && n > 60u) {   /* (a noisy cloud: n - 2 -> n + 2 is 4 semitones) */
                        double d4 = shift(lp, l1), p4 = peak_shift(lp, l1);
                        if ((d4 >= 2.5 && d4 <= 5.5) || (p4 >= 2.5 && p4 <= 5.5))
                            d = 2.0;
                    }
                    if (d < 1.0 || d > 3.0) {
                        if (!bad++)
                            first = n;
                        if (bad <= 4)
                            printf("pitch: %-6s %-14s note %3u -> %3u: %+5.2f semitones\n", en->name, en->presets[pi].name, n, n + 2u, d);
                        runs += ++run >= 2u;
                    } else {
                        run = 0;
                    }
                }
                memcpy(lp, l0, sizeof lp);
                memcpy(l0, l1, sizeof l0);
                r = r1;
            }
            printf("pitch: %-6s %-14s C4..%u, %2u steps of 2 semitones: %s", en->name, en->presets[pi].name, n < top ? n : top, tested,
                   runs ? "FAIL" : "ok");
            if (bad)
                printf(" (%u wrong, %u in a row, the first from note %u)", bad, runs, first);
            printf("\n");
            fails += runs != 0;
        }
    }
    printf("pitch: %d failed\n", fails);
    return fails;
}
