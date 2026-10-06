/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the MIDI input parsers: the running-status parser in
 * firmware/src/midi_uart.c (um_byte) and the USB-MIDI SysEx path of firmware/src/usb.c
 * (sysex_byte frame assembly, ota_wire_send packetising). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"

static uint32_t now_ms;
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }

static int check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void feed(const uint8_t *p, uint32_t n)
{
    while (n--)
        sysex_byte(*p++);
}

/* the queued SysEx packets back to bytes, as the host's USB-MIDI driver does */
static uint32_t drain(uint8_t *out, int *cin_ok)
{
    uint32_t n = 0;
    *cin_ok = 1;
    while (so_r != so_w) {
        uint32_t pkt = sx_out_q[so_r++ % SXQ], cin = pkt & 15u, k;
        uint32_t nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : cin == 5u ? 1u : 0u;
        if (!nb || (cin == 4u) != (so_r != so_w))      /* 4 except the last, which ends (5/6/7) */
            *cin_ok = 0;
        for (k = 0; k < nb; k++)
            out[n++] = (uint8_t)(pkt >> (8u * (k + 1u)));
    }
    return n;
}

static int test_usb_sysex(void)
{
    static uint8_t msg[4200], got[700];
    uint32_t n, i;
    int bad = 0, enc_ok = 1, rt_ok = 1, cin_ok;
    usb.config = 1;
    for (n = 2; n <= 40u; n++) {                       /* every length mod 3, through the ring */
        msg[0] = 0xF0;
        for (i = 1; i + 1u < n; i++)
            msg[i] = (uint8_t)((i * 37u + n) & 0x7Fu);
        msg[n - 1] = 0xF7;
        if (ota_wire_send(msg, n) || drain(got, &cin_ok) != n || memcmp(got, msg, n) || !cin_ok || sx_busy)
            enc_ok = 0;
        feed(got, n);
        if (n > 2u && (!sx_ready || sx_frame_len != n - 2u || memcmp(sx_frame, msg + 1, n - 2u)))
            rt_ok = 0;
        ota_frame_done();
    }
    bad += check("usb: ota_wire_send packets (CIN 4.. then 5/6/7)", enc_ok);
    bad += check("usb: received frame == the bytes between F0 and F7", rt_ok);

    feed((const uint8_t *)"\xF0\x01\x02\xF7", 4);
    feed((const uint8_t *)"\xF0\x03\x04\x05\xF7", 5);
    bad += check("usb: a frame while one is pending is dropped",
                 sx_ready && sx_frame_len == 2u && sx_frame[0] == 1 && sx_frame[1] == 2);
    ota_frame_done();
    feed((const uint8_t *)"\xF0\x11\x12", 3);           /* cut off by the next F0 */
    feed((const uint8_t *)"\xF0\x21\xF7", 3);
    bad += check("usb: a truncated frame is replaced by the next one",
                 sx_ready && sx_frame_len == 1u && sx_frame[0] == 0x21);
    ota_frame_done();
    msg[0] = 0xF0;
    memset(msg + 1, 0x55, sizeof msg - 2u);
    msg[sizeof msg - 1u] = 0xF7;
    feed(msg, sizeof msg);
    bad += check("usb: an oversized frame (4200 B, more than a DX7 bank dump) is not taken", !sx_ready);
    feed((const uint8_t *)"\xF0\x22\x24\x35\x7D\xF7", 6);
    bad += check("usb: soft key -> uboot_req, not a frame", usb.uboot_req && !sx_ready);
    feed((const uint8_t *)"\xF0\x22\x24\x35\x7F\xF7", 6);
    bad += check("usb: upgrade key -> ota_req, not a frame", usb.ota_req && !sx_ready);
    feed((const uint8_t *)"\x22\x24\xF7", 3);           /* no F0: ignored */
    bad += check("usb: bytes outside F0..F7 are ignored", !sx_ready);

    so_w = so_r + SXQ;                                  /* ring full ... */
    usb.config = 0;                                     /* ... and the host gone */
    bad += check("usb: send with a full ring and no host fails at once",
                 ota_wire_send(msg, 9) == -1 && !sx_busy && now_ms == 0);
    usb.config = 1;
    bad += check("usb: send with a full ring times out (200 ms)",
                 ota_wire_send(msg, 9) == -1 && !sx_busy && now_ms > 200u && now_ms < 210u);
    so_r = so_w;
    return bad;
}

/* the RX DMA: bytes land in the ring in order, from where it last stopped */
static uint32_t dma_w;
static void dma_put(const uint8_t *p, uint32_t n)
{
    while (n--) {
        um_ring[dma_w] = *p++;
        dma_w = (dma_w + 1u) & (UM_RING - 1u);
    }
}

static int ring_clean(void)
{
    uint32_t i;
    for (i = 0; i < UM_RING; i++)
        if (um_ring[i] != UM_EMPTY)
            return 0;
    return 1;
}

/* the ring read by content (the Salt fix): no byte left behind, across the wrap, line noise skipped */
static int test_uart_ring(void)
{
    /* an MPC Sample pad: note-on, running-status poly aftertouch, note-off, then silence */
    static const uint8_t pad[] = {0x90, 0x3A, 0x11, 0xA0, 0x3A, 0x75, 0x3A, 0x7F, 0x3A, 0x06, 0x80, 0x3A, 0x00};
    uint32_t i, k, w0, ok;
    int bad = 0;
    memset(&um, 0, sizeof um);
    for (i = 0; i < UM_RING; i++)
        um_ring[i] = UM_EMPTY;
    mi_r = mi_w;
    dma_w = 0;
    um_drain();
    bad += check("uart ring: nothing landed, nothing read", mi_w == mi_r && um.rd == 0 && um.bytes == 0);

    dma_put(pad, sizeof pad);
    um_drain();
    bad += check("uart ring: a burst's last message (note-off) at once",
                 mi_w - mi_r == 5u && midi_in_q[(mi_w - 1u) % MQ] == 0x003A8008u && ring_clean());
    mi_r = mi_w;

    ok = 1;                                           /* byte by byte, many times round the ring */
    for (k = 0; k < 40u; k++) {
        w0 = mi_w;
        for (i = 0; i < sizeof pad; i++) {
            dma_put(pad + i, 1);
            um_drain();
            if (i == 2u)
                ok &= mi_w == w0 + 1u;                /* the note-on with its third byte */
        }
        ok &= mi_w == w0 + 5u && midi_in_q[(mi_w - 1u) % MQ] == 0x003A8008u && um.rd == dma_w;
        mi_r = mi_w;
    }
    bad += check("uart ring: byte-by-byte arrival across the wrap", ok && ring_clean());

    dma_put((const uint8_t[]){0x90, 0x3C, 0x40, UM_EMPTY}, 4);   /* line noise as the last byte */
    um_drain();
    ok = mi_w - mi_r == 1u && um.rd != dma_w;        /* the stray byte waits for its successor */
    dma_put((const uint8_t[]){0x80, 0x3C, 0x00}, 3);
    um_drain();
    ok &= mi_w - mi_r == 2u && midi_in_q[(mi_w - 1u) % MQ] == 0x003C8008u && um.rd == dma_w;
    bad += check("uart ring: a received FD is skipped, not taken for empty", ok && ring_clean());
    mi_r = mi_w;
    return bad;
}

/* EP1 OUT (USB-MIDI from the computer): back-pressure instead of drops, malformed events ignored */
static int test_usb_ep1(void)
{
    uint8_t pk[64];
    uint32_t i, w0, n = 0;
    int bad = 0;
    for (i = 0; i < 16u; i++) {                       /* 16 note-ons, channel 1 */
        pk[4 * i] = 0x09; pk[4 * i + 1] = 0x90; pk[4 * i + 2] = (uint8_t)(48 + i); pk[4 * i + 3] = 100;
    }
    mi_r = mi_w;
    mi_w += MQ - 20u;                                 /* the ring nearly full: 20 slots free (< 16 + 8) */
    w0 = mi_w;
    bad += check("usb ep1: ring too full -> packet held, nothing dropped", ep1_take(pk, 64) == 0 && mi_w == w0);
    mi_r = mi_w;                                      /* the sequencer drained it */
    bad += check("usb ep1: room again -> all 16 events taken", ep1_take(pk, 64) == 1 && mi_w == w0 + 16u &&
                 midi_in_q[(mi_w - 1u) % MQ] == 0x643F9009u);
    mi_r = mi_w;
    w0 = mi_w;
    {
        static const uint8_t m[] = {
            0x09, 0x80, 60, 100,                      /* status does not match CIN 9: ignored */
            0x09, 0x90, 0xC0, 100,                    /* data byte with bit 7: ignored */
            0x08, 0x80, 60, 0,                        /* a good note-off */
            0x0F, 0xF8, 0, 0,                         /* clock */
            0x0F, 0xFA, 0, 0,                         /* start */
            0x0F, 0xFE, 0, 0,                         /* active sensing: not queued */
            0x0C, 0xC0, 5, 0,                         /* program change (2 bytes) */
        };
        n = ep1_take(m, sizeof m);
        bad += check("usb ep1: malformed ignored, clock / start / note-off / PC queued",
                     n == 1u && mi_w == w0 + 4u && midi_in_q[w0 % MQ] == 0x003C8008u &&
                     midi_in_q[(w0 + 1u) % MQ] == 0xF80Fu && midi_in_q[(w0 + 2u) % MQ] == 0xFA0Fu &&
                     midi_in_q[(w0 + 3u) % MQ] == 0x0005C00Cu);
    }
    mi_r = mi_w;
    return bad;
}

int main(void)
{
    static const uint8_t in[] = {
        0x90, 60, 100, 62, 101,          /* note on + running status */
        0xF8, 64, 0xFE, 102,             /* realtime inside a message (the clock is queued, cable 1) */
        0xC1, 5, 6,                      /* program change + running */
        0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7, 70, 71,   /* SysEx dropped; cancels running status */
        0xB0, 7, 0x7F, 0xF2, 1, 2, 9, 9, /* CC, song position (dropped), data without status */
        0x80, 60, 0,
    };
    static const uint32_t want[] = {
        0x643C9009u, 0x653E9009u, 0x0000F81Fu, 0x66409009u, 0x0005C10Cu, 0x0006C10Cu, 0x7F07B00Bu, 0x003C8008u,
    };
    uint32_t i, bad = 0, n = sizeof want / sizeof want[0];
    for (i = 0; i < sizeof in; i++)
        um_byte(in[i]);
    if (mi_w != n) {
        printf("got %u packets, want %u\n", mi_w, n);
        bad = 1;
    }
    for (i = 0; i < n && i < mi_w; i++)
        if (midi_in_q[i] != want[i]) {
            printf("pkt %u: %08x want %08x\n", i, midi_in_q[i], want[i]);
            bad = 1;
        }
    bad += (uint32_t)check("uart: running status, realtime, SysEx, system common", !bad);
    {   /* 4-track routing reads the channel from the packet as for USB-MIDI: cable 0, CIN = status >> 4 */
        static const uint8_t chs[] = {0x90, 60, 1, 0x91, 61, 2, 0x92, 62, 3, 0x99, 36, 4, 0x9F, 63, 5, 0x89, 36, 0};
        uint32_t w0 = mi_w, ok = 1;
        for (i = 0; i < sizeof chs; i++)
            um_byte(chs[i]);
        for (i = 0; i < 6u; i++) {
            uint32_t pkt = midi_in_q[(w0 + i) % MQ], st = chs[3u * i];
            ok &= mi_w == w0 + 6u && (pkt & 0xFFu) == (st >> 4) && ((pkt >> 8) & 0xFFu) == st &&
                  ((pkt >> 16) & 0x7Fu) == chs[3u * i + 1u] && (pkt >> 24) == chs[3u * i + 2u];
        }
        bad += (uint32_t)check("uart: channels 1, 2, 3, 10, 16 -> USB-MIDI packets", ok);
    }
    bad += (uint32_t)test_uart_ring();
    bad += (uint32_t)test_usb_ep1();
    bad += (uint32_t)test_usb_sysex();
    printf("%s\n", bad ? "MIDI PARSER TEST FAILED" : "midi parser test passed");
    return (int)bad;
}
