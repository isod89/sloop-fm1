/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the USB audio input in src/usb.c (FELUCCA_UAC):
 *   descriptors  the configuration parsed as a host does: lengths, interface and endpoint counts,
 *                class codes, the UAC1 chain (AC header collection, terminals, AS general, type I
 *                format, the isochronous endpoint), the IADs; with and without CDC (-DT_CDC=0/1), and with CDC
 *                built in but the menu's USB SERIAL OFF (-DT_CDC=2: as a T_CDC=0 build, byte for byte: UAC_DUMP=1)
 *   ring         uac_render_start / uac_tap (the audio ISR) against uac_packet (TIMER5): renders of 256
 *                frames at the I2S rate, as late as the load makes them, packets at 1 kHz: sizes 43..46,
 *                44.1 on average, every frame delivered in order, then an underrun (repeats), an
 *                overrun (drops, the ring never overfills) and a restart.
 * The SIE is never touched (uac_service and usb_poll are not called). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifndef HALF_FRAMES
#error "-DHALF_FRAMES=n (src/core.h; run_tests.sh passes it)"
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC (T_CDC != 0)
#define CDC_SHOWN (T_CDC == 1)            /* the console presented */
#define FELUCCA_UAC 1
#ifndef T_RATE
#define T_RATE 0                          /* 2.7 beta: menu USB RATE (0 AUTO, 1 44.1, 2 48): what this start offers */
#endif
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
#include "../firmware/src/usb.c"
#define RATES (FELUCCA_UAC_48K ? T_RATE : 1)   /* 0: both; 1: 44.1 only (as 2.4: a FELUCCA_UAC_48K=0 build); 2: 48 only */

static int fails;
static int check(const char *what, int ok)
{
    printf("%-64s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

/* ------------------------------------------------------------------ descriptors --- */
static void test_descriptors(void)
{
    const uint8_t *c, *dv;
    uint16_t cl, dl;
    uint32_t total, n, off, nif = 0, i;
    int lens_ok = 1, eps_ok = 1, ac_ok = 0, it_ok = 0, ot_ok = 0, asg_ok = 0, fmt_ok = 0, iso_ok = 0, csep_ok = 0;
    int alt0_ok = 0, iad_ok = 1, midi_ok = 0, dup_ok = 1, coll_ok = 0;
    uint8_t seen_if[16] = {0}, if_class[16] = {0}, if_sub[16] = {0};
    int cur_if = -1, cur_alt = -1, cur_neps = 0, got_eps = 0, ac_len_left = -1, ac_total = 0, ac_wt = -1;
    uint8_t coll[8], ncoll = 0, iad_n = 0;
    uint32_t ep_seen[32] = {0};
    char name[96];
#if T_CDC
    usb_cdc_on = CDC_SHOWN;                            /* (main.c sets it from the menu before usb_start) */
#endif
#if FELUCCA_UAC_48K
    usb_rate_mode = T_RATE;                            /* (main.c, from the menu USB RATE, the same way) */
#endif
    get_desc(0x0100, &dv, &dl);
    get_desc(0x0200, &c, &cl);
    total = le16(c + 2);
    n = cl;
    if (getenv("UAC_DUMP")) {                          /* the bytes a host reads (tests: T_CDC=0 and 2 compared) */
        for (i = 0; i < dl; i++) printf("%02X", dv[i]);
        printf("\n");
        for (i = 0; i < cl; i++) printf("%02X", c[i]);
        printf("\n");
        exit(0);
    }

    check("device descriptor: 18 bytes, type 1, EP0 64", dv[0] == 18 && dv[1] == 1 && dv[7] == 64);
    check(RATES == 0 ? "bcdDevice bumped for the audio input at 44.1 / 48 kHz (x.2x)" :
          RATES == 1 ? "bcdDevice of the audio input at 44.1 only (x.1x, as 2.4)" : "bcdDevice of 48 kHz only (x.3x)",
          (dv[12] & 0xF0u) == (RATES == 0 ? 0x20u : RATES == 1 ? 0x10u : 0x30u) && dv[13] == 3);
    check("bcdDevice +0.08: the AC collection's order of 2.7.2 (the audio streaming interface first)", (dv[12] & 0x0Eu) == 0x08u);
    check(CDC_SHOWN ? "device class misc / IAD (EF 02 01)" : "device class 0 (per interface)",
          CDC_SHOWN ? dv[4] == 0xEF && dv[5] == 2 && dv[6] == 1 : dv[4] == 0);
    check("configuration: type 2, wTotalLength = the bytes sent", c[1] == 2 && total == n);

    for (off = 0; off < n;) {
        const uint8_t *d = c + off;
        if (d[0] < 2 || off + d[0] > n) {
            lens_ok = 0;
            break;
        }
        if (ac_len_left > 0 && d[1] == 0x24) {
            ac_total += d[0];
            ac_len_left -= d[0];
        } else if (ac_len_left > 0) {
            ac_len_left = -2;                          /* the CS AC block ended early */
        }
        switch (d[1]) {
        case 0x0B:                                     /* IAD */
            iad_n++;
            if (d[0] != 8 || d[2] + d[3] > 16)
                iad_ok = 0;
            if (iad_n == 1 && !(d[2] == 0 && d[3] == 3 && d[4] == 1))
                iad_ok = 0;                            /* audio: IF 0..2 */
            if (iad_n == 2 && !(d[2] == 3 && d[3] == 2 && d[4] == 2))
                iad_ok = 0;                            /* CDC: IF 3..4 */
            break;
        case 4:
            if (cur_if >= 0 && got_eps != cur_neps)
                eps_ok = 0;
            cur_if = d[2];
            cur_alt = d[3];
            cur_neps = d[4];
            got_eps = 0;
            if (cur_if < 16) {
                if (!seen_if[cur_if])
                    nif++;
                seen_if[cur_if] = 1;
                if_class[cur_if] = d[5];
                if_sub[cur_if] = d[6];
            }
            if (d[5] == 1 && d[6] == 2 && cur_alt == 0 && d[4] == 0)
                alt0_ok = 1;
            break;
        case 5:
            got_eps++;
            if (d[2] == 0x84) {
                iso_ok = d[0] == 9 && d[3] == 0x05 && le16(d + 4) == (RATES == 1 ? 46u : 49u) * 4u && le16(d + 4) <= 1023u &&
                         d[6] == 1 && cur_alt == 1 && if_sub[cur_if] == 2;
            }
            if ((d[2] == 0x01 || d[2] == 0x81) && d[3] == 2 && if_sub[cur_if] == 3)
                midi_ok++;
            if (ep_seen[(d[2] & 15u) + (d[2] >> 7) * 16u]++)
                dup_ok = 0;                            /* each address once (no alt shares one here) */
            break;
        case 0x24:
            if (if_class[cur_if] == 1 && if_sub[cur_if] == 1) {      /* audio control */
                if (d[2] == 1) {
                    ac_ok = d[0] == 8u + d[7] && le16(d + 3) == 0x0100u;
                    ac_len_left = ac_wt = (int)le16(d + 5);
                    ac_total = 0;
                    ac_len_left -= d[0];
                    ac_total += d[0];
                    ncoll = d[7];
                    for (i = 0; i < ncoll && i < sizeof coll; i++)
                        coll[i] = d[8 + i];
                } else if (d[2] == 2) {
                    it_ok = d[0] == 12 && d[3] == 1 && d[7] == 2 && le16(d + 8) == 3u && le16(d + 4) != 0x0101u;
                } else if (d[2] == 3) {
                    ot_ok = d[0] == 9 && d[3] == 2 && le16(d + 4) == 0x0101u && d[7] == 1;
                }
            } else if (if_class[cur_if] == 1 && if_sub[cur_if] == 2) {   /* audio streaming */
                if (d[2] == 1)
                    asg_ok = d[0] == 7 && d[3] == 2 && le16(d + 5) == 1u;
                else if (d[2] == 2)
                    fmt_ok = RATES == 0 ? d[0] == 14 && d[3] == 1 && d[4] == 2 && d[5] == 2 && d[6] == 16 && d[7] == 2 &&
                             (d[8] | d[9] << 8 | d[10] << 16) == 44100 && (d[11] | d[12] << 8 | d[13] << 16) == 48000 :
                             d[0] == 11 && d[3] == 1 && d[4] == 2 && d[5] == 2 && d[6] == 16 && d[7] == 1 &&
                             (d[8] | d[9] << 8 | d[10] << 16) == (RATES == 1 ? 44100 : 48000);
            }
            break;
        case 0x25:
            if (d[2] == 1 && if_sub[cur_if] == 2)
                csep_ok = d[0] == 7 && (d[3] & 1u);
            break;
        }
        off += d[0];
    }
    if (cur_if >= 0 && got_eps != cur_neps)
        eps_ok = 0;
    coll_ok = ncoll == 2 && if_sub[coll[0]] == 2 && if_sub[coll[1]] == 3;   /* (2.7.2: audio streaming first, for
                                                       * macOS up to 15; usb.c, the AC header) */
    for (i = 0; i < ncoll && i < sizeof coll; i++)
        if (coll[i] >= 16 || if_class[coll[i]] != 1 || (if_sub[coll[i]] != 2 && if_sub[coll[i]] != 3))
            coll_ok = 0;

    check("descriptor lengths add up to wTotalLength", lens_ok && off == n);
    check("bNumInterfaces = the interfaces present", c[4] == nif && nif == (CDC_SHOWN ? 5u : 3u));
    check("each interface setting has bNumEndpoints endpoints", eps_ok);
    check("endpoint addresses unique", dup_ok);
    check("AC header: UAC 1.00, length 8 + collection", ac_ok);
    check("AC header wTotalLength = the class-specific AC descriptors", ac_len_left == 0 && ac_total == ac_wt);
    check("AC collection: the MIDI and audio streaming interfaces", coll_ok);
    check("input terminal 1: 2 ch, L R, not USB streaming", it_ok);
    check("output terminal 2: USB streaming, source 1", ot_ok);
    check("AS alt 0 has no endpoint", alt0_ok);
    check("AS general: terminal 2, PCM", asg_ok);
    check(RATES == 0 ? "type I format: 2 ch, 2-byte subframe, 16 bit, 44100 and 48000 Hz (2.5)" :
          RATES == 1 ? "type I format: 2 ch, 16 bit, 44100 Hz only" : "type I format: 2 ch, 16 bit, 48000 Hz only", fmt_ok);
    check(RATES == 1 ? "EP 0x84: isochronous async, 184 B (46 frames), every frame, alt 1" :
          "EP 0x84: isochronous async, 196 B (49 frames), every frame, alt 1", iso_ok);
    check("CS endpoint: sampling frequency control", csep_ok);
    check("MIDI bulk endpoints 0x01 / 0x81 unchanged", midi_ok == 2);
    snprintf(name, sizeof name, "IADs: %s", CDC_SHOWN ? "audio IF 0-2, CDC IF 3-4" : "none");
    check(name, iad_ok && iad_n == (CDC_SHOWN ? 2 : 0));
}

/* --------------------------------------------------------------------- the ring --- */
/* time in ns. The producer renders HALF_FRAMES frames at FS_DEV, a render taking `rend` ns in
 * HALF_FRAMES / 32 blocks of 32 (the samples count up: L = n, R = ~n, so the order can be checked); the consumer
 * takes one packet per ms. */
#define FS_DEV 44117.6
static uint32_t prod_n, cons_n, cons_bad, cons_rep, cons_zero, sizes[64], npk;
static uint32_t last_frame;
static int32_t blk[64];

static int sine_mode;                                   /* 2.5: the producer plays sines (the 48 kHz tests) */
static double sine_f = 1000.0;
static void render_block(void)
{
    uint32_t i;
    for (i = 0; i < 32u; i++, prod_n++) {
        if (sine_mode) {                               /* L: a sine at sine_f, R: half of it, inverted */
            double v = sin(2 * M_PI * sine_f * prod_n / 44100.0);
            blk[2 * i] = (int32_t)lrint(16000.0 * v);
            blk[2 * i + 1] = (int32_t)lrint(-8000.0 * v);
            continue;
        }
        blk[2 * i] = (int16_t)prod_n;
        blk[2 * i + 1] = (int16_t)~prod_n;
    }
    uac_tap(blk, 32);
}

#define CAP_N (48000u * 12u)
static int16_t cap_l[CAP_N], cap_r[CAP_N];
static uint32_t cap_n;
static void take_packet(void)
{
    uint32_t d[UA_MAXF + 2], n = uac_packet(d), i;
    sizes[n]++;
    npk++;
    if (sine_mode) {                                   /* keep what the host records */
        for (i = 0; i < n && cap_n < CAP_N; i++, cap_n++) {
            cap_l[cap_n] = (int16_t)(uint16_t)d[i];
            cap_r[cap_n] = (int16_t)(uint16_t)(d[i] >> 16);
        }
        return;
    }
    for (i = 0; i < n; i++) {
        uint16_t l = (uint16_t)d[i], r = (uint16_t)(d[i] >> 16);
        if (d[i] == 0 && !uac.go) {
            cons_zero++;
            continue;
        }
        if ((uint16_t)~l != r) {
            if (d[i] == 0) {                           /* primed silence ahead of the first render */
                cons_zero++;
                continue;
            }
            cons_bad++;
            continue;
        }
        if (cons_n && l == (uint16_t)last_frame)
            cons_rep++;                                 /* a repeated frame (underrun) */
        else if (cons_n && l != (uint16_t)(last_frame + 1u))
            cons_bad++;
        last_frame = l;
        cons_n++;
    }
}

/* run for `ms`; renders take `rend_ns` (or vary with the load when 0); consumer / producer can be paused */
static uint64_t now_ns, next_half, next_pkt;
static uint32_t blocks_left;
static uint64_t next_block, rend_len;
static uint32_t lcg = 12345;
/* render times: 7 % .. 85 % of a half (the overload shedding starts above 85 %) */
#define REND_MIN ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.07))
#define REND_MAX ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.85))

static void run(uint32_t ms, uint64_t rend_ns, int produce, int consume)
{
    uint64_t end = now_ns + (uint64_t)ms * 1000000u;
    while (now_ns < end) {
        uint64_t t = end;
        if (next_half < t)
            t = next_half;
        if (blocks_left && next_block < t)
            t = next_block;
        if (next_pkt < t)
            t = next_pkt;
        now_ns = t;
        if (now_ns >= end)
            break;
        if (now_ns == next_half) {
            next_half += (uint64_t)((double)HALF_FRAMES / FS_DEV * 1e9);
            if (produce) {
                lcg = lcg * 1103515245u + 12345u;
                rend_len = rend_ns ? rend_ns : REND_MIN + (lcg >> 8) % (REND_MAX - REND_MIN);
                uac_render_start();
                blocks_left = HALF_FRAMES / 32u;
                next_block = now_ns + rend_len / blocks_left;
            }
        } else if (blocks_left && now_ns == next_block) {
            render_block();
            blocks_left--;
            next_block += rend_len / (HALF_FRAMES / 32u);
        } else if (now_ns == next_pkt) {
            lcg = lcg * 1103515245u + 12345u;
            next_pkt = (now_ns / 1000000u + 1u) * 1000000u + (lcg >> 8) % 500000u;   /* in the next frame */
            if (consume)
                take_packet();
        }
    }
}

static void test_ring(void)
{
    uint32_t i, sum, base_n, base_p;
    char s[120];
    usb.config = 1;
    memset(&uac, 0, sizeof uac);
    uac_stream(1);
    next_half = 300000;
    next_pkt = 1000000;
    run(3, 0, 1, 1);                                    /* alt 1 set: no IN token yet */
    check("not fed before the host reads (no overruns while it waits)", uac.overruns == 0 && ua_w == 0);
    uac.flowing = 1;                                    /* (uac_service: the first packet went) */
    run(2000, 0, 1, 1);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    base_n = cons_n;
    base_p = uac.underruns + uac.overruns;
    run(10000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "10 s, random render times: sizes 43..46 (44:%u 45:%u 46:%u 43:%u)", sizes[44], sizes[45],
             sizes[46], sizes[43]);
    check(s, sizes[44] + sizes[45] + sizes[46] + sizes[43] == npk);
    snprintf(s, sizeof s, "  mean packet %.4f frames (I2S %.1f Hz)", (double)sum / npk, FS_DEV);
    check(s, (double)sum / npk > 44.05 && (double)sum / npk < 44.2);
    check("  every frame once, in order; no underrun / overrun", cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p && cons_n - base_n == sum);
    snprintf(s, sizeof s, "  fill at render start stays %u..%u (band %u..%u)", uac.fill_lo, uac.fill_hi, UA_LO, UA_HI);
    check(s, uac.fill_lo >= 46u && uac.fill_hi + HALF_FRAMES <= UA_N);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    for (i = 0; i < 1000u; i++) {                      /* 1000 frames worst case: renders of 85 % */
        run(1, REND_MAX, 1, 1);
    }
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "1000 packets, %.1f ms renders: mean %.3f, no glitch", REND_MAX / 1e6, (double)sum / npk);
    check(s, (double)sum / npk > 44.0 && (double)sum / npk < 44.25 && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);

    base_p = uac.underruns;
    run(30, 0, 0, 1);                                   /* the render stops (a crash would; a stuck ISR) */
    check("producer stopped 30 ms: underruns counted, packets keep their size", uac.underruns > base_p &&
          cons_bad == 0);
    check("  the empty ring repeats the last frame", cons_rep > 0);
    base_p = uac.overruns;
    run(40, 0, 1, 0);                                   /* the consumer stops (flowing still 1) */
    check("consumer stopped 40 ms: overruns counted, the ring never overfills", uac.overruns > base_p &&
          ua_w - ua_r <= UA_N);
    uac_stream(0);
    run(10, 0, 1, 1);
    check("alt 0: nothing fed, nothing taken", ua_w - ua_r <= UA_N && !uac.feed);
    uac_stream(1);
    uac.flowing = 1;
    cons_n = 0;
    cons_bad = cons_rep = 0;
    base_p = uac.underruns + uac.overruns;
    run(3000, 0, 1, 1);
    check("restart: primed again, in order, no glitch", cons_n > 3000u * 44u && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);
}

/* ------------------------------------------------------------ 2.5: 48 kHz --- */
/* a sine of f Hz at fs in y[0..n): the fitted amplitude and the SNR (the residual after a sine + DC fit) */
static void sine_fit(const int16_t *y, uint32_t n, double f, double fs, double *amp, double *snr)
{
    double a11 = 0, a12 = 0, a22 = 0, b1 = 0, b2 = 0, m = 0, c1, c2, det, e = 0, p = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        m += y[i];
    m /= n;
    for (i = 0; i < n; i++) {
        double s = sin(2 * M_PI * f * i / fs), c = cos(2 * M_PI * f * i / fs), v = y[i] - m;
        a11 += s * s; a12 += s * c; a22 += c * c; b1 += s * v; b2 += c * v;
    }
    det = a11 * a22 - a12 * a12;
    c1 = (b1 * a22 - b2 * a12) / det;
    c2 = (b2 * a11 - b1 * a12) / det;
    for (i = 0; i < n; i++) {
        double s = sin(2 * M_PI * f * i / fs), c = cos(2 * M_PI * f * i / fs), fit = c1 * s + c2 * c;
        p += fit * fit;
        e += (y[i] - m - fit) * (y[i] - m - fit);
    }
    *amp = sqrt(c1 * c1 + c2 * c2);
    *snr = 10 * log10(p / (e > 1e-9 ? e : 1e-9));
}

static void test_rate(void)
{
    static const struct { uint32_t hz; uint8_t r48; } T[] = {
        {44100, 0}, {48000, 1}, {44100, 0}, {32000, 0}, {46049, 0}, {46050, 1}, {96000, 1}, {0, 0}};
    uint32_t i;
    int ok = 1;
    memset(&uac, 0, sizeof uac);
    for (i = 0; i < sizeof T / sizeof T[0]; i++) {
        uac.go = 1;
        uac_rate_set(T[i].hz);
        ok &= uac.r48 == T[i].r48;
        if (i && T[i].r48 != T[i - 1].r48)
            ok &= uac.go == 0;                          /* a change re-primes */
    }
    check("SET_CUR: 44100 / 48000, the nearer of the two; a change re-primes", ok);
#if FELUCCA_UAC_48K
    {   /* 2.7 beta, menu USB RATE: whatever the host asks, the one rate described */
        int ok1 = 1, ok2 = 1;
        memset(&uac, 0, sizeof uac);
        usb_rate_mode = 1;
        for (i = 0; T[i].hz; i++)
            uac_rate_set(T[i].hz), ok1 &= uac.r48 == 0;
        usb_rate_mode = 2;
        uac_rate_set(UAC_RATE);                         /* (SET_CONFIGURATION: "a new configuration starts at 44.1") */
        ok2 &= uac.r48 == 1;
        for (i = 0; T[i].hz; i++)
            uac_rate_set(T[i].hz), ok2 &= uac.r48 == 1;
        usb_rate_mode = 0;
        memset(&uac, 0, sizeof uac);
        check("USB RATE 44.1: always 44.1 (the resampler never runs)", ok1);
        check("USB RATE 48: 48 from the configuration on, whatever SET_CUR says", ok2);
    }
#endif
}

#if FELUCCA_UAC_48K
/* the resampler alone: blocks of 32 frames of a sine through uac_tap (48 kHz), the ring drained after each */
static void test_resampler(void)
{
    static const double FQ[] = {100, 1000, 5000, 10000, 12000, 16000};
    static int16_t out_l[60000], out_r[60000];
    uint32_t q, i, b, n;
    char s[120];
    for (q = 0; q < sizeof FQ / sizeof FQ[0]; q++) {
        double amp, snr, ampr, snrr, g;
        memset(&uac, 0, sizeof uac);
        memset(&rs, 0, sizeof rs);
        ua_w = ua_r = 0;
        uac.feed = 1;
        uac.ring48 = uac.r48 = 1;
        n = 0;
        for (b = 0; b < 1500u; b++) {                   /* 48000 input frames: ~52245 out */
            int32_t in[64];
            for (i = 0; i < 32u; i++) {
                double v = sin(2 * M_PI * FQ[q] * (b * 32u + i) / 44100.0);
                in[2 * i] = (int32_t)lrint(29000.0 * v);
                in[2 * i + 1] = (int32_t)lrint(-14500.0 * v);
            }
            uac_tap(in, 32);
            while (ua_r != ua_w && n < 60000u) {
                uint32_t d = ua_ring[ua_r++ & (UA_N - 1u)];
                out_l[n] = (int16_t)(uint16_t)d;
                out_r[n] = (int16_t)(uint16_t)(d >> 16);
                n++;
            }
        }
        sine_fit(out_l + 200, n - 400, FQ[q], 48000.0, &amp, &snr);
        sine_fit(out_r + 200, n - 400, FQ[q], 48000.0, &ampr, &snrr);
        g = 20 * log10(amp / 29000.0);
        snprintf(s, sizeof s, "48 kHz: %5.0f Hz: %u frames out of 48000, gain %+.2f dB, SNR %.1f dB (R %.1f)", FQ[q], n, g, snr, snrr);
        if (FQ[q] <= 1000)
            check(s, n == (160u * 48000u - 1u) / 147u + 1u && fabs(g) < 0.05 && snr > 78.0 && snrr > 72.0);
        else if (FQ[q] <= 10000)
            check(s, g > -0.5 && g < 0.1 && snr > 70.0);
        else if (FQ[q] <= 12000)
            check(s, g > -0.8 && snr > 65.0);
        else
            check(s, g > -1.5 && snr > 55.0);           /* 16 kHz: the top of the passband */
    }
}

/* the stream at 48 kHz: renders at the I2S rate, packets at 1 kHz, what the host records is one clean sine */
static void test_ring48(void)
{
    uint32_t i, sum = 0, base;
    double amp, snr;
    char s[140];
    usb.config = 1;
    memset(&uac, 0, sizeof uac);
    ua_w = ua_r = 0;
    uac_stream(1);
    uac_rate_set(48000);
    uac.flowing = 1;
    sine_mode = 1;
    sine_f = 997.0;
    cap_n = 0;
    run(500, 0, 1, 1);                                  /* (primed, settled) */
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    base = cap_n;
    run(10000, 0, 1, 1);
    for (i = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "48 kHz stream, 10 s: sizes 47..49 (47:%u 48:%u 49:%u), mean %.4f", sizes[47], sizes[48], sizes[49],
             (double)sum / npk);
    check(s, sizes[47] + sizes[48] + sizes[49] == npk && (double)sum / npk > 48.0 && (double)sum / npk < 48.05);
    sine_fit(cap_l + base, cap_n - base, 997.0 * 44100.0 / FS_DEV, 48000.0 * 48000.0 / (FS_DEV * 160.0 / 147.0), &amp, &snr);
    snprintf(s, sizeof s, "  what the host records: one sine, SNR %.1f dB, no underrun / overrun (%u / %u)", snr,
             uac.underruns, uac.overruns);
    check(s, snr > 60.0 && uac.underruns == 0 && uac.overruns == 0 && amp > 15000.0);
    uac_rate_set(44100);                                /* back to 44.1 mid-stream: silence, primed again */
    sine_mode = 0;
    cons_n = 0;
    cons_bad = cons_rep = 0;
    base = uac.underruns + uac.overruns;
    run(3000, 0, 1, 1);
    check("48 -> 44.1 mid-stream: primed again, every frame in order", cons_n > 3000u * 44u && cons_bad == 0 &&
          cons_rep == 0 && uac.underruns + uac.overruns == base);
}

#endif

int main(void)
{
    printf("-- USB audio input, CDC %s\n", T_CDC == 2 ? "built in, USB SERIAL OFF" : T_CDC ? "on" : "off");
    test_descriptors();
#if FELUCCA_UAC_48K
    usb_rate_mode = 0;                                 /* (the rest: AUTO, the rates the host picks) */
#endif
    test_ring();
    test_rate();
#if FELUCCA_UAC_48K
    test_resampler();
    test_ring48();
#endif
    printf(fails ? "UAC TEST FAILED (%d)\n" : "uac: all ok\n", fails);
    return fails != 0;
}
