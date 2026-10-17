/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SNOOP37 boot jingle: a tiny synth of its own, mixed in the audio ISR (fx.c mix_block, before the buses:
 * dry into the mix, a send into SLOOP's reverb, then the master volume), independent of the project's sounds.
 * A cinematic "deep note": four voices start as a quiet cluster wobbling around G3..D4, glide apart (ease in
 * and out, in cents) into a wide Dmaj9 (D2, F#3, C#4, E4) while swelling, bloom when the dog lands and the
 * title lights (3.0 s), shimmer, and fade out with the picture (4.83..5.2 s). Each roof key's landing rings
 * a small glockenspiel note (D major pentatonic, rising with the rainbow, left to right), and a brighter bell
 * pings when the sun settles into place and the black keys land (4.23 s). mkj_band[] tells the picture how loud each of 12 pitch bands is.
 * Each voice: two oscillators 5 cents apart (left and right), 2-operator FM at a low index (a soft electric
 * piano / pad tone), a gentle low-pass; the bells: FM at 3.5:1, a quickly closing index. Peak around -24 dBFS
 * at the master volume's middle. Silent and free (nothing rendered) when not running; mkj_stop() fades out
 * in 150 ms. */

#define MKJ_NV 4
#define MKJ_LEN 5200u              /* ms, as the splash */
#define MKJ_END 4830               /* everything fades out from here (snoop_splash.c MK_FADE) */
static const uint32_t MKJ_SEMI[12] = {    /* phase increments of C9..B9 at 44.1 kHz */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static const int16_t MKJ_FROM[MKJ_NV] = {5730, 5980, 5640, 6120};   /* cents: the cluster */
static const int16_t MKJ_TO[MKJ_NV] = {3800, 5400, 6100, 6400};     /* D2 F#3 C#4 E4 */
static const int16_t MKJ_LVL[MKJ_NV] = {150, 256, 230, 210};        /* (the bass at about half) */
#define MKJ_K0 2028                /* the first key lands (snoop_splash.c MK_K0 + MK_LAND), ms */
#define MKJ_KD 70                  /* then every 70 ms */
#define MKJ_PING 4230              /* the sun settles, the black keys land: the ping (snoop_splash.c MK_PING) */
static const uint8_t MKJ_BELL[12] = {69, 71, 74, 76, 78, 81, 83, 86, 88, 90, 93, 95};   /* A4 .. B6 */
static volatile uint8_t mkj_band[12];   /* (the picture reads them: one byte each, no lock) */

static struct {
    volatile uint8_t on, stop;
    uint32_t s;                    /* samples since the start */
    int32_t fade;                  /* the stop fade, Q15 */
    uint32_t ph[MKJ_NV + 1][2], bph[3][2];
    int32_t bamp[3];
    int32_t amp[MKJ_NV + 1], lpl, lpr;
} mkj;

static inline int32_t mkj_sin(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINE[i], b = SINE[i + 1u];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

static uint32_t mkj_inc(int32_t cents)           /* cents above MIDI note 0 -> phase increment */
{
    int32_t n = cents / 100, c = cents % 100;
    uint32_t inc = MKJ_SEMI[n % 12] >> (10 - n / 12);
    return inc + (inc >> 12) * (uint32_t)(c * 37 / 16);
}

static int32_t mkj_ramp(uint32_t ms, uint32_t t0, uint32_t d)   /* 0..256 */
{
    return ms <= t0 ? 0 : ms >= t0 + d ? 256 : (int32_t)((ms - t0) * 256u / d);
}

/* a voice's two oscillators into l, r: amplitude a0 -> a1 (Q15 x level) over the block, FM index k */
static void mkj_voice(uint32_t *ph, uint32_t inc0, uint32_t inc1, int32_t a0, int32_t a1, int32_t k, int32_t *l,
                      int32_t *r, uint32_t n)
{
    uint32_t i;
    int32_t da = (a1 - a0) / (int32_t)n;
    for (i = 0; i < n; i++, a0 += da) {
        int32_t m0 = mkj_sin(ph[0]) * k, m1 = mkj_sin(ph[1]) * k;
        l[i] += (mkj_sin(ph[0] + (uint32_t)m0) * a0) >> 15;
        r[i] += (mkj_sin(ph[1] + (uint32_t)m1) * a0) >> 15;
        ph[0] += inc0;
        ph[1] += inc1;
    }
}

/* a bell: FM 3.5:1, index k, amplitude a0 -> a1, panned (0 left .. 256 right) */
static void mkj_bell(uint32_t *ph, uint32_t inc, int32_t a0, int32_t a1, int32_t k, int32_t pan, int32_t *l,
                     int32_t *r, uint32_t n)
{
    uint32_t i, im = inc * 7u / 2u;
    int32_t da = (a1 - a0) / (int32_t)n;
    for (i = 0; i < n; i++, a0 += da) {
        int32_t y = (mkj_sin(ph[0] + (uint32_t)(mkj_sin(ph[1]) * k)) * a0) >> 15;
        l[i] += y * (256 - pan) >> 8;
        r[i] += y * pan >> 8;
        ph[0] += inc;
        ph[1] += im;
    }
}

static void mkj_band_add(int32_t *b, int32_t cents, int32_t a)
{
    int32_t i = (cents - 3700) / 330;
    b[i < 0 ? 0 : i > 11 ? 11 : i] += a;
}

/* the audio ISR, one block (n <= 32): adds into the dry mix and the reverb send */
static void mkj_render(int32_t *mix_l, int32_t *mix_r, int32_t *send, uint32_t n)
{
    int32_t l[32], r[32], bl[32], br[32], band[12], v, g, idx, glide, ms;
    uint32_t i;
    if (!mkj.on || n > 32u)
        return;
    ms = (int32_t)(mkj.s * 10u / 441u);
    mkj.s += n;
    if (mkj.stop) {                                 /* skipped: out in ~150 ms */
        mkj.fade -= (int32_t)(32767u * n / 6615u);
        if (mkj.fade <= 0) {
            mkj.on = 0;
            for (v = 0; v < 12; v++)
                mkj_band[v] = 0;
            return;
        }
    }
    if ((uint32_t)ms >= MKJ_LEN) {
        mkj.on = 0;
        for (v = 0; v < 12; v++)
            mkj_band[v] = 0;
        return;
    }
    /* the level: almost silent, a swell (ease in) while gliding, the bloom, a shimmer, the fade out */
    if (ms < 500)
        g = ms * 2000 / 500;
    else if (ms < 3000) {
        int32_t p = mkj_ramp((uint32_t)ms, 500, 2500);
        g = 2000 + (32767 - 2000) * (p * p >> 8) / 256;
    } else if (ms < MKJ_END)
        g = 32767 - ((mkj_sin((uint32_t)ms * 21474836u) + 32768) >> 4);   /* 5 Hz, -6 % */
    else {
        int32_t q = 256 - mkj_ramp((uint32_t)ms, MKJ_END, MKJ_LEN - MKJ_END);
        g = 32767 * (q * q >> 8) / 256;
    }
    g = g * mkj.fade >> 15;
    {   /* the pitch: ease in and out from 0.5 to 3.0 s */
        int32_t p = mkj_ramp((uint32_t)ms, 500, 2500);
        glide = p * p * (768 - 2 * p) >> 16;        /* 3p^2 - 2p^3, 0..256 */
    }
    idx = 7000 + 14000 * mkj_ramp((uint32_t)ms, 2800, 500) / 256 - 5000 * mkj_ramp((uint32_t)ms, 3300, 1300) / 256;
    for (i = 0; i < n; i++)
        l[i] = r[i] = bl[i] = br[i] = 0;
    for (v = 0; v < 12; v++)
        band[v] = 0;
    for (v = 0; v < MKJ_NV; v++) {
        int32_t c = MKJ_FROM[v] + (MKJ_TO[v] - MKJ_FROM[v]) * glide / 256, a;
        c += (mkj_sin((uint32_t)ms * (2400000u + (uint32_t)v * 900000u) + (uint32_t)v * 0x40000000u) * 38 +
              mkj_sin((uint32_t)ms * (700000u + (uint32_t)v * 300000u)) * 18) / 32768 * (256 - glide) / 256;
        a = (g >> 4) * MKJ_LVL[v] >> 8;             /* 2048 at full: four voices ~ -18 dB below the ceiling */
        mkj_voice(mkj.ph[v], mkj_inc(c - 5), mkj_inc(c + 5), mkj.amp[v], a, v ? idx : idx / 2, l, r, n);
        mkj.amp[v] = a;
        mkj_band_add(band, c, a);
    }
    for (v = 0; v < 3; v++) {                       /* the keys' bells: three voices take turns */
        int32_t k = (ms - MKJ_K0) / MKJ_KD, a = 0, d;
        k -= ((k - v) % 3 + 3) % 3;                 /* this voice's latest key */
        if (ms >= MKJ_K0 && k >= 0 && k < 12) {
            d = ms - (MKJ_K0 + k * MKJ_KD);
            a = d < 3 ? d * 520 / 3 : 520 * 250 / (250 + d * 4);
            a = (a * mkj.fade >> 15) * (ms < MKJ_END ? 256 : 256 - mkj_ramp((uint32_t)ms, MKJ_END, MKJ_LEN - MKJ_END)) >> 8;
            if (d < 1 && !mkj.bamp[v])
                mkj.bph[v][0] = mkj.bph[v][1] = 0;  /* (a strike starts at zero phase) */
            mkj_bell(mkj.bph[v], mkj_inc(MKJ_BELL[k] * 100), mkj.bamp[v], a, d < 40 ? 9000 - d * 120 : 4200,
                     40 + k * 16, bl, br, n);
            mkj_band_add(band, MKJ_BELL[k] * 100, a * 3);
        } else if (mkj.bamp[v])
            mkj_bell(mkj.bph[v], mkj_inc(6900), mkj.bamp[v], 0, 4000, 128, bl, br, n);
        mkj.bamp[v] = a;
    }
    {   /* the ping: E6, a bright bell decaying in ~0.6 s */
        int32_t a = 0;
        if (ms >= MKJ_PING) {
            int32_t d = ms - MKJ_PING;
            a = d < 6 ? d * 1820 / 6 : 1820 * 600 / (600 + d * 3);
            a = a * mkj.fade >> 15;
            if (ms >= MKJ_END)
                a = a * (320 - mkj_ramp((uint32_t)ms, MKJ_END, MKJ_LEN - MKJ_END)) / 320;   /* (rings into the fade) */
        }
        if (a || mkj.amp[MKJ_NV])
            mkj_voice(mkj.ph[MKJ_NV], mkj_inc(8800), mkj_inc(8803), mkj.amp[MKJ_NV], a, 3000, l, r, n);
        mkj.amp[MKJ_NV] = a;
        mkj_band_add(band, 8800, a);
    }
    for (i = 0; i < n; i++) {                       /* a gentle low-pass (~4.5 kHz), into the mix and the reverb */
        mkj.lpl += ((l[i] << 2) - mkj.lpl) * 15 >> 5;
        mkj.lpr += ((r[i] << 2) - mkj.lpr) * 15 >> 5;
        mix_l[i] += ((mkj.lpl >> 2) + bl[i]) >> 1;   /* (>> 1: the whole jingle at half level, -6 dB) */
        mix_r[i] += ((mkj.lpr >> 2) + br[i]) >> 1;
        send[i] += (((mkj.lpl + mkj.lpr) >> 3) + ((bl[i] + br[i]) >> 1)) >> 1;
    }
    for (v = 0; v < 12; v++)
        mkj_band[v] = (uint8_t)(band[v] > 2047 ? 255 : band[v] >> 3);
}

/* the main loop: start (from the splash) and stop (a fade; nothing sounds after it) */
static void mkj_start(void)
{
    uint32_t v;
    fm1_irq_off();
    mkj.s = 0;
    mkj.fade = 32767;
    mkj.stop = 0;
    mkj.lpl = mkj.lpr = 0;
    for (v = 0; v < 3; v++)
        mkj.bamp[v] = 0;
    for (v = 0; v <= MKJ_NV; v++) {
        mkj.amp[v] = 0;
        mkj.ph[v][0] = v * 0x51EB851Fu;
        mkj.ph[v][1] = v * 0x3A5E353Fu + 0x40000000u;
    }
    mkj.on = 1;
    fm1_irq_on();
}

static void mkj_stop(void) { mkj.stop = 1; }
