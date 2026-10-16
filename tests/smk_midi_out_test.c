/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 TRS MIDI OUT (firmware/src/smk_midi_out.h) against a simulated UART: the bytes of each USB-MIDI packet
 * (notes, CC, program change, pitch bend, clock; SysEx and broken packets not sent; DAW mode's MMC whole, smo_raw),
 * whole messages in order through
 * the ring, one byte at a time only once the UART has finished the last, a full ring drops whole messages, TRS MIDI
 * OUT OFF sends nothing, and a UART that never finishes neither hangs the tick nor fills up: the jack goes quiet,
 * and setting it ON again tries again. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static uint32_t ut_busy, ut_len, ut_dead, ut_overrun;   /* the UART: ticks left on the byte in flight */
static uint8_t ut_out[65536];
#define UT_TICKS 4u               /* a byte: 0.32 ms = 3.2 ticks of 0.1 ms */
static uint32_t ut_done(void) { return !ut_dead && !ut_busy; }
static void ut_put(uint32_t b)
{
    ut_overrun += ut_busy != 0u;                         /* a byte written over one still going */
    if (ut_len < sizeof ut_out)
        ut_out[ut_len++] = (uint8_t)b;
    ut_busy = UT_TICKS;
}
static void ut_tick(void) { if (ut_busy && !ut_dead) ut_busy--; }
#define SMO_TX_DONE() ut_done()
#define SMO_TX_PUT(b) ut_put(b)
#include "../firmware/src/smk_midi_out.h"

static int fails;
static void check(int ok, const char *what) { printf("smk_midi_out: %-70s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static void tick(uint32_t n) { while (n--) { ut_tick(); smo_tick(); } }
static void reset(void)
{
    memset(&smo, 0, sizeof smo);
    trs_out = 1;
    ut_busy = ut_len = ut_dead = ut_overrun = 0;
}
#define PKT(cin, a, b, c) ((uint32_t)(cin) | (uint32_t)(a) << 8 | (uint32_t)(b) << 16 | (uint32_t)(c) << 24)
static int bytes_are(const uint8_t *want, uint32_t n) { return ut_len == n && !memcmp(ut_out, want, n); }

int main(void)
{
    uint8_t b[3];
    uint32_t i, n, t;

    check(smo_bytes(PKT(9, 0x90, 60, 100), b) == 3 && b[0] == 0x90 && b[1] == 60 && b[2] == 100, "note on: 90 3C 64");
    check(smo_bytes(PKT(8, 0x83, 60, 0), b) == 3 && b[0] == 0x83, "note off: 83 3C 00 (channel 4)");
    check(smo_bytes(PKT(11, 0xB0, 7, 100), b) == 3, "control change: 3 bytes");
    check(smo_bytes(PKT(12, 0xC2, 5, 0), b) == 2 && b[0] == 0xC2 && b[1] == 5, "program change: 2 bytes");
    check(smo_bytes(PKT(13, 0xD0, 64, 0), b) == 2, "channel pressure: 2 bytes");
    check(smo_bytes(PKT(14, 0xE0, 0, 64), b) == 3, "pitch bend: 3 bytes");
    check(smo_bytes(PKT(15, 0xF8, 0, 0), b) == 1 && b[0] == 0xF8 && smo_bytes(PKT(15, 0xFA, 0, 0), b) == 1 &&
          smo_bytes(PKT(15, 0xFC, 0, 0), b) == 1, "clock, start, stop: 1 byte");
    check(smo_bytes(PKT(2, 0xF3, 1, 0), b) == 2 && smo_bytes(PKT(3, 0xF2, 0, 8), b) == 3, "song select, song position");
    check(!smo_bytes(PKT(4, 0xF0, 0x22, 0x24), b) && !smo_bytes(PKT(7, 0x35, 0x7D, 0xF7), b) &&
          !smo_bytes(PKT(5, 0xF7, 0, 0), b), "SysEx (the editor's): not sent");
    check(!smo_bytes(PKT(9, 0xFF, 60, 100), b) && !smo_bytes(PKT(9, 0x80, 60, 100), b),
          "a status that is not its CIN's (channel OFF: 0x90 | 0xFF): not sent");
    check(!smo_bytes(PKT(9, 0x90, 200, 100), b) && !smo_bytes(PKT(9, 0x90, 60, 0x80), b), "a data byte >= 0x80: not sent");
    check(!smo_bytes(PKT(15, 0xF9, 0, 0), b) && !smo_bytes(PKT(15, 0x90, 0, 0), b) && !smo_bytes(PKT(0, 0x90, 1, 2), b) &&
          !smo_bytes(PKT(1, 0x90, 1, 2), b), "undefined realtime, a stray byte, reserved CINs: not sent");

    reset();
    smo_put(PKT(9, 0x90, 60, 100));
    smo_put(PKT(12, 0xC0, 3, 0));
    smo_put(PKT(8, 0x80, 60, 0));
    tick(200);
    {
        static const uint8_t want[] = {0x90, 60, 100, 0xC0, 3, 0x80, 60, 0};
        check(bytes_are(want, sizeof want), "three messages: their bytes, in order, no running status");
    }
    check(!ut_overrun && smo.sent == 8u, "one byte at a time: never written over one still going");

    reset();                                           /* the pace: a byte as soon as the UART is free */
    for (i = 0; i < 40u; i++)
        smo_put(PKT(9, 0x90, i, 100));
    for (t = 0; ut_len < 120u && t < 100000u; t++)
        tick(1);
    snprintf((char *)ut_out + 60000, 100, "120 bytes in %u ticks (%.0f bytes/s; the line carries 3125)", t, 120.0 * 10000 / t);
    check(t <= 120u * (UT_TICKS + 1u) && !ut_overrun, (char *)ut_out + 60000);

    reset();                                           /* producer and tick interleaved at random */
    srand(7);
    for (i = n = 0; i < 3000u; i++) {
        if (rand() % 3 == 0) {
            smo_put(PKT(9, 0x90, n & 127u, 1u + n % 127u));
            n++;
        }
        tick((uint32_t)(rand() % 12));                  /* (on average slower than the line: nothing dropped) */
    }
    tick(100000);
    for (i = 0, t = 1; i < n; i++)
        t &= ut_out[3 * i] == 0x90 && ut_out[3 * i + 1] == (i & 127u) && ut_out[3 * i + 2] == 1u + i % 127u;
    check(ut_len == 3u * n && t && !ut_overrun && !smo.drops && smo.r == smo.w, "random interleaving: every message whole, in order");

    reset();                                           /* a full ring: whole messages only */
    for (i = 0; i < 100u; i++)
        smo_put(PKT(9, 0x90, i, 100));
    check(smo.w - smo.r == 85u * 3u && smo.drops == 15u, "a full ring (256): 85 messages kept, 15 dropped whole");
    tick(100000);
    check(ut_len == 255u && ut_out[252] == 0x90 && ut_out[253] == 84, "... and those 85 go out whole");

    reset();
    trs_out = 0;
    smo_put(PKT(9, 0x90, 60, 100));
    tick(100);
    check(ut_len == 0 && smo.w == smo.r, "TRS MIDI OUT OFF: nothing queued, nothing sent");
    smo_on(1);
    smo_put(PKT(9, 0x90, 60, 100));
    tick(100);
    check(ut_len == 3, "set ON again: sent");

    reset();                                           /* DAW mode's MMC: a whole SysEx message as it is (smo_raw) */
    {
        static const uint8_t mmc[6] = {0xF0, 0x7F, 0x7F, 0x06, 0x02, 0xF7};
        smo_put(PKT(9, 0x90, 60, 100));
        smo_raw(mmc, 6);
        smo_put(PKT(8, 0x80, 60, 0));
        tick(300);
        {
            static const uint8_t want[] = {0x90, 60, 100, 0xF0, 0x7F, 0x7F, 0x06, 0x02, 0xF7, 0x80, 60, 0};
            check(bytes_are(want, sizeof want), "DAW mode's MMC (smo_raw): F0 7F 7F 06 02 F7 whole, between the notes");
        }
        reset();
        for (i = 0; i < 84u; i++)
            smo_put(PKT(9, 0x90, i, 100));
        smo_raw(mmc, 6);
        check(smo.w - smo.r == 252u && smo.drops == 1u, "... no room for all six: dropped whole");
        reset();
        trs_out = 0;
        smo_raw(mmc, 6);
        check(smo.w == smo.r, "... TRS MIDI OUT OFF: nothing");
    }

    reset();                                           /* no UART: the byte never finishes */
    ut_dead = 1;
    for (i = 0; i < 20u; i++)
        smo_put(PKT(9, 0x90, i, 100));
    tick(SMO_DEAD * (SMO_TIMEOUT + 1u) + 5u);
    check(smo.dead && smo.timeouts == SMO_DEAD && ut_len == SMO_DEAD && smo.r == smo.w,
          "no UART: 4 bytes tried (3 ms each), then quiet, the ring emptied");
    smo_put(PKT(9, 0x90, 1, 100));
    tick(1000);
    check(smo.w == smo.r && ut_len == SMO_DEAD, "... nothing queued any more (no hang, no stall)");
    ut_dead = 0;
    smo_on(1);
    smo_put(PKT(9, 0x90, 1, 100));
    tick(100);
    check(!smo.dead && ut_len == SMO_DEAD + 3u, "the UART back, TRS MIDI OUT set again: sent");

    reset();                                           /* a UART that stalls once: that byte is given up */
    smo_put(PKT(9, 0x90, 60, 100));
    tick(1);
    ut_dead = 1;
    tick(SMO_TIMEOUT + 2u);
    ut_dead = 0;
    tick(200);
    check(!smo.dead && smo.timeouts == 1u && ut_len == 3u, "a single stall: given up after 3 ms, then on as normal");

    printf("smk_midi_out: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
