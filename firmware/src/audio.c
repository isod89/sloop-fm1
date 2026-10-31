/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the audio ISR: each half buffer is
 * rendered in blocks of CTL samples by mix_block (fx.c: events -> each synth part
 * -> dist -> level / pan -> sends -> drums -> buses -> master), then scaled to 24-bit stereo. */
/* registers: hal/fm1_audio.h */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define OUT_SHIFT 7               /* Q15 -> 24-bit, -6 dBFS ceiling */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested;
    volatile uint32_t in_audio;           /* TIMER5 reads it to decide what may nest in the render */
    uint32_t late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
static volatile uint32_t t5_nested_ticks;              /* TIMER4 ticks TIMER5 spent nested in this ISR (main.c) */
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];   /* left, right (the visualiser): the mix at full level */
static uint32_t scope_w;

/* 2.8.2, GLO > SYSTEM > CLK OUT (G_ACLK): the audio sync click. While on, one channel is given over to a short, loud click
 * on every clock tick (seq.c aclk_block sets aclk_hi, the samples of the click left to play) and the other sums the stereo
 * mix to mono (L+R)/2, so nothing panned is lost and the click never bleeds into the music. SIDE (G_ACLKCH) picks the
 * channel: RIGHT (a Volca's SYNC IN over a splitter) or LEFT (a Pocket Operator reads sync on its left channel). The click
 * is a SQUARE burst, not a flat gate: the output is AC-coupled, so a DC level would pass as one brief spike a Pocket Operator
 * cannot latch onto; a square (toggled every 8 samples, ~2.8 kHz) is genuinely AC and passes intact as a strong tick. It sits
 * at ~-3 dBFS (ACLK_LEVEL), so it crosstalks (analog, shared codec/jack) into the mono channel a few dB below a full-scale
 * click yet keeps a solid margin over the sync input's threshold (at -6 dB the PO dropped ticks). It is written raw, after the
 * master volume and the limiter (fx.c mix_block): the same level whatever the knob does, back to ground (0) between ticks so
 * there is no DC hum. USB tap untouched. */
#define ACLK_LEVEL 0x5A0000                             /* ~-3 dBFS (~0.70 of codec max 0x7FFF00): a few dB under a full-scale
                                                         * click, strong margin for the PO. Clear of the ROM band the build's
                                                         * static check rejects ([0xFFC00000,0xFFD00000)): -0x5A0000 = 0xFFA60000,
                                                         * well below it. (Exactly -6 dB, 0x3FFF80, is barred: -0x3FFF80 = 0xFFC00080.) */
static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
    uint32_t i;
    int aclk_on, aclk_left;
    mix_block(out, n);
#if FELUCCA_UAC
    uac_tap(usb_full_now ? usb_out : out, n);           /* the USB audio input: the master output, or at full level */
#endif
    aclk_on = song.g[G_ACLK] != 0;                      /* one channel is the sync click, the other a mono sum */
    aclk_left = song.g[G_ACLKCH] != 0;                  /* SIDE: 0 the click on RIGHT (Volca), 1 on LEFT (Pocket Operator) */
    for (i = 0; i < n; i++) {
        if (i & 1u)
        {
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];   /* (fx.c: MASTER all the way up) */
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
        if (aclk_on) {
            int32_t mono = (out[2u * i] + out[2u * i + 1u]) << (OUT_SHIFT - 1u);   /* (L+R)/2: each side is knee-bounded
                                                                                   * (fx.c), so the sum never overflows */
            int32_t pulse = aclk_hi ? ((aclk_hi & 8u) ? ACLK_LEVEL : -ACLK_LEVEL) : 0;   /* a ~-3 dBFS square burst */
            out[2u * i] = aclk_left ? pulse : mono;
            out[2u * i + 1u] = aclk_left ? mono : pulse;
            if (aclk_hi)
                aclk_hi--;
        } else {
            out[2u * i] <<= OUT_SHIFT;
            out[2u * i + 1u] <<= OUT_SHIFT;
        }
    }
}

/* overload: shed_voice (voice.c) */
static volatile uint8_t shed_req;
static uint8_t shed_over;                      /* bit k: the half k halves ago was over 85 % */

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    felucca_dbg.in_audio = 1;                   /* first: TIMER5 nests from here on (main.c) */
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, us;
        int32_t *o = &abuf[half * HALF_WORDS];
        if (shed_req) {
            shed_req = 0;
            shed_voice();
        }
#if FELUCCA_UAC
        uac_render_start();
        usb_full_now = (uint8_t)(usb_full && uac.feed);  /* menu USB AUDIO = FULL, while the computer records */
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL)
            audio_block(o + 2u * b, CTL);
        fm1_audio_ack_half();
        audio_halves++;
#if FELUCCA_CPU1
        cpu1_load_half(HALF_FRAMES * 1000000u / FS);     /* core 1's load and core 0's wait for it (fx.c) */
#endif
        {   /* the deadline sees the render and TIMER5 nested in it (the USB audio stream); the load
             * figures the render alone (after Felucca 1.0) */
            uint32_t all = fm1_ticks() - t0;
            us = (all - t5_nested_ticks) / FM1_TICKS_PER_US;
            shed_over = (uint8_t)(shed_over << 1 | (all / FM1_TICKS_PER_US * 100u > (HALF_FRAMES * 1000000u / FS) * 85u));
        }
        if (us > audio_max_us)
            audio_max_us = us;
        if ((shed_over & 3u) == 3u)
            shed_req = 1;                               /* two in a row */
        song.cpu_q8 = (song.cpu_q8 * 15u + (us * 256u) / (HALF_FRAMES * 1000000u / FS)) / 16u;
        if (fm1_audio_free_half() != half)
            felucca_dbg.late++;                         /* the DMA moved on while we rendered */
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)                   /* hal/fm1_audio.h */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
