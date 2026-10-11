/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNCH-IN FX (punch.c): each of the 27 effects changes the mix while held, stays bounded,
 * and the mix is exactly the dry one again once its key is up (after the 64-sample fade).
 * The FX-held keyboard picks effects with every key (white 1-16, black 17-27) and never plays or
 * records a note. The black keys' effects (2.6) each do what their name says (punch_new_fx).
 * argv[1]: a WAV demo (a beat, then every effect for one beat). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <math.h>
#define BLOCKS (FS * 3u / CTL)                        /* 3 s at 120 BPM */
static int32_t got[CTL * 2u];

static void beat_setup(void)
{
    static const uint8_t B[16][2] = {{36, 42}, {42}, {42}, {42}, {38, 42}, {42}, {42}, {36, 46},
                                     {36, 42}, {42}, {36}, {42}, {38, 42}, {42}, {42}, {46}};
    uint32_t j, q;
    host_tracks_init();
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    TDRUM->p[P_E0] = 0;                               /* the sampled kit: no random noise */
    song.g[G_BPM] = 120;
    for (j = 0; j < NSTEP; j++) {
        step_t *s = &TDRUM->step[j];
        memset(s, 0, sizeof *s);
        s->time = ST_REST;
        if (j < 16u)
            for (q = 0; q < 2u && B[j][q]; q++) {
                s->note[s->n++] = B[j][q];
                s->time = ST_NOTE;
                s->vel = 100;
            }
    }
    TDRUM->p[P_SLEN] = 16;
    punch.req = -1;
    punch.cur = -1;
    punch.g = 0;
    punch.hold = 0;
    clk_beat = 0;
    clk_pos = 0;
    transport_req = 1;
}

/* a deterministic "mix": a decaying 110 Hz tone every quarter + noise bursts every 1/16 */
static void test_mix(uint32_t b, int32_t *l, int32_t *r)
{
    static uint32_t seed = 1;
    uint32_t i;
    if (b == 0) seed = 1;
    for (i = 0; i < CTL; i++) {
        uint32_t t = b * CTL + i, q = t % (FS / 2u), s16 = t % (FS / 8u);
        int32_t tone = (sine_i((uint32_t)t * 10712093u) * (int32_t)(FS / 2u - q) / (int32_t)(FS / 2u)) >> 1;
        int32_t nz = s16 < 2000u ? (int32_t)((seed = seed * 1664525u + 1013904223u) >> 18) - 8192 : 0;
        l[i] = (tone + nz) * 3;
        r[i] = (tone - nz) * 3;
    }
}

/* ---- the black keys' effects (2.6): each does what its name says ---- */
#define NS (FS * 8u)                                  /* up to 8 s */
static int32_t ol[NS], or_[NS];
typedef int32_t (*gen_f)(uint32_t t, int ch);
static double sweep_hz;                               /* the generators' tone */
static int32_t g_sine(uint32_t t, int ch) { (void)ch; return (int32_t)(100000.0 * sin(2 * M_PI * sweep_hz * t / FS)); }
static int32_t g_left(uint32_t t, int ch) { return ch ? 0 : g_sine(t, 0); }
static int32_t g_burst(uint32_t t, int ch)            /* a chord for 0.5 s, then nothing */
{
    double x = 0;
    if (t >= FS / 2u) return 0;
    x += sin(2 * M_PI * 220 * t / FS) + sin(2 * M_PI * 277.2 * t / FS + ch) + sin(2 * M_PI * 329.6 * t / FS) + 0.5 * sin(2 * M_PI * 880 * t / FS);
    return (int32_t)(10000.0 * x);
}
/* fx held from sample `on` to `off`, through the buses as mix_block runs them (FREEZE needs the reverb) */
static void run_fx(int32_t fx, gen_f g, uint32_t ns, uint32_t on, uint32_t off, int playing)
{
    uint32_t b, i;
    beat_setup();
    song.playing = (uint8_t)playing;
    song.g[G_RSIZE] = 64, song.g[G_RDAMP] = 40;
    clk_pos = 0;
    for (b = 0; b < ns / CTL; b++) {
        int32_t l[CTL], r[CTL], sc[CTL] = {0}, sd[CTL] = {0}, sr[CTL] = {0}, wl[CTL], wr[CTL];
        uint32_t t0 = b * CTL;
        for (i = 0; i < CTL; i++) l[i] = g(t0 + i, 0), r[i] = g(t0 + i, 1);
        punch.req = (int8_t)(t0 >= on && t0 < off ? fx : -1);
        punch_freeze_feed(sr, l, r, CTL);
        fx_buses(sc, sd, sr, wl, wr, CTL);
        for (i = 0; i < CTL; i++) l[i] += wl[i], r[i] += wr[i];
        punch_process(l, r, CTL);
        clk_pos = (clk_pos + CTL * (uint32_t)song.g[G_BPM]) % BEAT_U;
        for (i = 0; i < CTL; i++) {
            assert(l[i] > -(1 << 22) && l[i] < (1 << 22) && r[i] > -(1 << 22) && r[i] < (1 << 22));
            ol[t0 + i] = l[i], or_[t0 + i] = r[i];
        }
    }
    punch.req = -1;
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < b; i++) e += (double)x[i] * x[i];
    return sqrt(e / (double)(b - a));
}
static double tone_pow(const int32_t *x, uint32_t a, uint32_t b, double hz)   /* Goertzel */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0, s0;
    uint32_t i;
    for (i = a; i < b; i++) s0 = x[i] + c * s1 - s2, s2 = s1, s1 = s0;
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
/* the held effect's level on a tone, from 0.3 s into the hold (the filters settled) */
static double kill_ratio(int32_t fx, double hz)
{
    sweep_hz = hz;
    run_fx(fx, g_sine, FS * 2u, FS / 2u, FS * 3u / 2u, 0);
    return rms(ol, FS * 4u / 5u, FS * 7u / 5u) / rms(ol, FS / 10u, FS * 2u / 5u);
}
static void punch_new_fx(void)
{
    uint32_t beat = FS / 2u, i, t;
    double a, b2;
    /* the kills: 24 dB/oct at 250 Hz and 2.5 kHz */
    assert(kill_ratio(PX_KLOW, 60) < 0.05 && fabs(kill_ratio(PX_KLOW, 1000) - 1) < 0.12);
    assert(kill_ratio(PX_KHIGH, 9000) < 0.05 && fabs(kill_ratio(PX_KHIGH, 500) - 1) < 0.12);
    assert(kill_ratio(PX_KMID, 900) < 0.08);
    assert(fabs(kill_ratio(PX_KMID, 50) - 1) < 0.12 && fabs(kill_ratio(PX_KMID, 10000) - 1) < 0.12);
    printf("kills: low 60 Hz %.3f, high 9 kHz %.3f, mid 900 Hz %.3f (1 kHz through KILL LOW %.2f)\n",
           kill_ratio(PX_KLOW, 60), kill_ratio(PX_KHIGH, 9000), kill_ratio(PX_KMID, 900), kill_ratio(PX_KLOW, 1000));
    /* OCT UP: 220 Hz in, 440 Hz out */
    sweep_hz = 220;
    run_fx(PX_OCT, g_sine, FS * 2u, FS / 2u, FS * 3u / 2u, 0);
    a = tone_pow(ol, FS * 3u / 5u, FS * 7u / 5u, 440), b2 = tone_pow(ol, FS * 3u / 5u, FS * 7u / 5u, 220);
    printf("oct up: 440 Hz / 220 Hz power %.1f, level %.2f\n", a / b2, rms(ol, FS * 3u / 5u, FS * 7u / 5u) / rms(ol, 0, FS / 2u));
    assert(a > 30 * b2 && fabs(rms(ol, FS * 3u / 5u, FS * 7u / 5u) / rms(ol, 0, FS / 2u) - 1) < 0.25);
    /* MONO: a left-only tone comes out on both sides */
    sweep_hz = 440;
    run_fx(PX_MONO, g_left, FS, FS / 4u, FS * 3u / 4u, 0);
    for (t = FS / 4u + 128u; t < FS * 3u / 4u; t++) assert(abs(ol[t] - or_[t]) <= 64);   /* (the wet gain tops at 4095 / 4096) */
    assert(rms(or_, FS / 4u + 128u, FS * 3u / 4u) > 0.4 * rms(ol, 0, FS / 4u));
    /* AUTO-PAN: once a beat the mix goes left, then right */
    run_fx(PX_PAN, g_sine, FS * 2u, FS / 2u, FS * 3u / 2u, 0);
    {
        uint32_t lw = 0, rw = 0, q = beat / 8u;
        for (t = FS / 2u; t + q < FS * 3u / 2u; t += q) {
            double el = rms(ol, t, t + q), er = rms(or_, t, t + q);
            lw += el > 4 * er, rw += er > 4 * el;
        }
        printf("auto-pan: %u eighths of a beat hard left, %u hard right (of 16)\n", lw, rw);
        assert(lw >= 3 && rw >= 3);
        assert(fabs(rms(ol, FS / 2u, FS * 3u / 2u) / rms(ol, 0, FS / 2u) - 1) < 0.2);   /* as loud overall */
    }
    /* FADE OUT: a quarter at one beat (squared), nothing at two */
    run_fx(PX_FADE, g_sine, FS * 3u, FS / 2u, FS * 5u / 2u, 0);
    a = rms(ol, FS / 2u + beat - 400u, FS / 2u + beat + 400u) / rms(ol, 0, FS / 2u);
    printf("fade out: %.3f at a beat, %.4f at two\n", a, rms(ol, FS / 2u + 2u * beat, FS / 2u + 2u * beat + 2000u) / rms(ol, 0, FS / 2u));
    assert(a > 0.18 && a < 0.32 && rms(ol, FS / 2u + 2u * beat + 64u, FS * 5u / 2u) < 0.001 * rms(ol, 0, FS / 2u));   /* (-72 dB: the wet gain tops at 4095 / 4096) */
    /* REWIND: the past backwards and faster, then silence until the key is up */
    sweep_hz = 330;
    run_fx(PX_RWD, g_sine, FS * 2u, FS / 2u, FS * 3u / 2u, 0);
    for (t = FS / 2u + 64u, i = 0; t < FS * 3u / 2u; t++)
        if (abs(ol[t]) > 100000 / 2000) i = t;           /* (the dry under it: -72 dB) */
    printf("rewind: sound for %.2f s, then silence\n", (double)(i - FS / 2u) / FS);
    assert(i > FS / 2u + FS / 4u && i < FS / 2u + FS / 2u);
    assert(tone_pow(ol, FS / 2u + FS / 5u, FS / 2u + FS * 3u / 10u, 330 * 2.5) > tone_pow(ol, FS / 2u + FS / 5u, FS / 2u + FS * 3u / 10u, 330));   /* pitched up going back */
    /* LOOP 8T: a third of a beat, over and over */
    sweep_hz = 97;
    run_fx(PX_LOOP8T, g_burst, FS * 2u, FS / 4u, FS * 3u / 2u, 0);
    for (t = FS * 3u / 5u; t < FS * 3u / 5u + beat; t++) assert(ol[t] == ol[t + beat / 3u]);   /* (the chord has stopped) */
    assert(rms(ol, FS * 3u / 5u, FS * 3u / 5u + beat) > 0.3 * rms(ol, FS / 10u, FS / 5u));
    /* FLANGER: stereo, bounded, different */
    run_fx(PX_FLANGE, g_burst, FS, FS / 10u, FS * 9u / 10u, 0);
    for (t = FS / 5u, i = 0; t < FS * 2u / 5u; t++) i += ol[t] != or_[t];
    assert(i > FS / 10u);
    /* FREEZE: the chord stops at 0.5 s; held from 0.4 s, it rings on, steady (it beats a little), then decays
     * once let go (at 5 s) */
    run_fx(PX_FREEZE, g_burst, FS * 8u, FS * 2u / 5u, FS * 5u, 0);
    {
        double dry = rms(ol, FS / 10u, FS * 2u / 5u), h1 = rms(ol, FS, FS * 3u), h2 = rms(ol, FS * 3u, FS * 5u),
               tail = rms(ol, FS * 7u, FS * 8u), lr = rms(or_, FS, FS * 3u);
        printf("freeze: held %.2f then %.2f of the dry level (right %.2f), %.4f 2 s after release\n", h1 / dry, h2 / dry, lr / dry, tail / dry);
        assert(h1 > 0.4 * dry && h1 < 1.6 * dry && lr > 0.4 * dry && fabs(h2 / h1 - 1) < 0.08 && tail < 0.05 * dry);
    }
    assert(!rev_frz);
    {   /* (2.6) a FREEZE held long: the reverb's level stays put, no DC creeping in (a loop gain of 1 adds up any
         * rounding that leans one way: it went -33 a second, a thump at release) */
        static int32_t c0[CTL], d0[CTL], rv[CTL], w0[CTL], w1[CTL];
        uint32_t bk, k, seed = 1;
        double m[3] = {0, 0, 0};
        for (bk = 0; bk < 31u * FS / CTL; bk++) {
            for (k = 0; k < CTL; k++)
                c0[k] = d0[k] = 0, rv[k] = bk < 300u ? (int32_t)((seed = seed * 1664525u + 1013904223u) >> 16) - 32768 : 0;
            rev_frz = bk >= 300u;
            fx_buses(c0, d0, rv, w0, w1, CTL);
            if (bk >= FS / CTL)
                for (k = 0; k < CTL; k++)
                    m[(bk - FS / CTL) / (10u * FS / CTL) % 3u] += rev_l[k] + rev_r[k];
        }
        rev_frz = 0;
        for (k = 0; k < 3u; k++) m[k] /= 2.0 * 10 * FS;
        printf("freeze held 30 s: the reverb's mean %.0f, %.0f, %.0f (10 s each)\n", m[0], m[1], m[2]);
        assert(fabs(m[2] - m[0]) < 60 && fabs(m[2]) < 600);
    }
    {   /* (2.6) a quick switch (an effect starts on a block's first sample right after another): it starts with its
         * own set-up, not the last one's (a kill replayed its old block over a silent input) */
        static int32_t L[CTL], R[CTL];
        uint32_t bk, k;
        int32_t mx = 0;
        beat_setup();
        for (bk = 0; bk < 50u; bk++) {
            for (k = 0; k < CTL; k++) L[k] = R[k] = (int32_t)(150000.0 * sin(2 * M_PI * 80 * (bk * CTL + k) / FS));
            punch.req = PX_KLOW;
            punch_process(L, R, CTL);
        }
        punch.req = PX_MONO;
        for (bk = 0; bk < 2u; bk++) {
            for (k = 0; k < CTL; k++) L[k] = R[k] = (int32_t)(150000.0 * sin(2 * M_PI * 80 * (bk * CTL + k) / FS));
            punch_process(L, R, CTL);
        }
        punch.req = PX_KMID;
        for (bk = 0; bk < 10u; bk++) {
            for (k = 0; k < CTL; k++) L[k] = R[k] = 0;
            punch_process(L, R, CTL);
            for (k = 0; k < CTL; k++) mx = abs(L[k]) > mx ? abs(L[k]) : mx;
        }
        printf("quick switch KILL LOW -> MONO -> KILL MID over silence: peak %d\n", mx);
        assert(mx == 0);
        punch.req = -1;
        for (bk = 0; bk < 4u; bk++) punch_process(L, R, CTL);
    }
    song.playing = 0;
    printf("black keys: KILL LOW / MID / HIGH, REWIND, OCT UP, FLANGER, AUTO-PAN, FREEZE, LOOP 8T, MONO, FADE OUT do what they say PASS\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    uint32_t fx, i, b, on = FS / CTL, off = on + FS / 2u / CTL;   /* held from 1 s to 1.5 s */
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0;
    for (fx = 0; fx < PUNCH_NFX; fx++) {
        uint64_t diff = 0;
        beat_setup();
        song.playing = 1;
        clk_pos = 0;
        for (b = 0; b < BLOCKS; b++) {
            int32_t l[CTL], r[CTL], l0[CTL], r0[CTL];
            test_mix(b, l, r);
            memcpy(l0, l, sizeof l), memcpy(r0, r, sizeof r);
            if (b == on) punch.req = (int8_t)fx;
            if (b == off) punch.req = -1;
            punch_process(l, r, CTL);
            clk_pos = (clk_pos + CTL * (uint32_t)song.g[G_BPM]) % BEAT_U;   /* (the transport clock) */
            for (i = 0; i < CTL; i++) {
                assert(l[i] > -(1 << 22) && l[i] < (1 << 22) && r[i] > -(1 << 22) && r[i] < (1 << 22));
                if (b < on || b >= off + 3u)
                    assert(l[i] == l0[i] && r[i] == r0[i]);   /* dry before, dry again after */
                else
                    diff += (uint64_t)(l[i] > l0[i] ? l[i] - l0[i] : l0[i] - l[i]);
            }
        }
        if (diff <= 2000u * (off - on))
            printf("fx %u (%s): diff %llu\n", fx, PUNCH_NAME[fx], (unsigned long long)diff);
        assert(diff > 2000u * (off - on));            /* it did something */
        assert(punch.cur == -1);
        song.playing = 0;
    }
    {   /* the keyboard while FX is held: white keys pick effects; nothing sounds, nothing records */
        uint32_t k;
        beat_setup();
        song.sel = 0;
        song.rec = 1u;
        mix_block(got, CTL);
        ly_bit[LY_FX] = 1u << 2;                      /* FX held (seq.c layer_now) */
        fm1_in.buttons = ly_bit[LY_FX];
        uint32_t seen = 0;
        for (k = 0; k < 27u; k++) {
            int32_t w = punch_fx_of_key(k);
            fm1_in.notes = 1u << k;
            events_block(CTL);
            assert(w >= 0 && w < (int32_t)PUNCH_NFX && punch.req == w);
            assert(punch_key_of_fx((uint32_t)w) == k);   /* (the key that lights) */
            assert(punch_key(k) < 0 ? w >= 16 : w == punch_key(k));
            seen |= 1u << w;
            assert(!trk[0].step[0].n && !trk[0].v[0].active);
            fm1_in.notes = 0;
            events_block(CTL);
            assert(punch.req == -1);
        }
        assert(seen == (1u << PUNCH_NFX) - 1u);         /* every effect on a key of its own */
        assert(punch_key(0) == 0 && punch_key(26) == 15);
        assert(punch_fx_of_key(1) == PX_KLOW && punch_fx_of_key(25) == PX_FADE);
        fm1_in.buttons = 0;
        song.rec = 0;
        transport_req = 2;
        events_block(CTL);
    }
    punch_new_fx();
    if (f) {   /* demo: one beat dry, then each effect for one beat with a beat dry between */
        uint32_t beat = FS / 2u / CTL, b, total = beat * (1u + 2u * PUNCH_NFX);
        static int32_t blk[CTL * 2u];
        wav_hdr(f, total * CTL);
        beat_setup();
        TDRUM->p[P_E0] = DRUM_SAMPLED + 1u;           /* 909 */
        for (b = 0; b < total; b++) {
            uint32_t q;
            int32_t e = b < beat ? -1 : (int32_t)((b - beat) / beat);
            punch.req = (int8_t)(e >= 0 && e % 2 == 0 ? e / 2 : -1);
            mix_block(blk, CTL);
            for (q = 0; q < CTL; q++) wav_put(f, blk[2 * q], blk[2 * q + 1]);
        }
        fclose(f);
    }
    printf("punch-in FX: %u effects change the mix while held, bounded, exact dry mix after release; "
           "FX-held keys pick effects, play and record nothing PASS\n", PUNCH_NFX);
    return 0;
}
