/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_input.h: the same fm1_in, key ids, LED columns and edge /
 * step API, fed by the emulator panel (emu_keys, emu_enc_take) instead of the scanned matrix.
 * The panel's input is clean, so there is no debounce or quadrature decoding. */
#pragma once
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_irq.h"
#include "emu_bridge.h"

#define FM1_NCOL 11u
#define FM1_NKEY 41u              /* ids: 0..13 buttons, 14..40 note keys */
#define FM1_NENC 7u

/* key id at (physical column, packed row bit), -1 = none (as the real header) */
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
enum { FM1_BTN_OCT_DOWN = 0, FM1_BTN_OCT_UP = 1 };

static volatile struct {
    uint32_t notes;              /* bit n = note key n (0 = F3 .. 26 = G5) */
    uint32_t buttons;            /* bit i = button i (0..13) */
    uint32_t pressed, released;  /* button edges since the last fm1_input_edges() */
    uint32_t notes_pressed;      /* note-key press edges since the last fm1_input_note_edges() */
    uint8_t raw[FM1_NCOL];
    uint8_t cnt[FM1_NKEY];
    uint8_t enc_prev[FM1_NENC], enc_last[FM1_NENC];
    uint8_t enc_rest[FM1_NENC];
    uint8_t enc_still[FM1_NENC];
    int8_t enc_sub[FM1_NENC];
    int16_t enc_steps[FM1_NENC]; /* + = clockwise */
    uint32_t frames;
} fm1_in;
static uint8_t fm1_led[FM1_NCOL];
#define FM1_GLOW_NS 4000u        /* (the real HAL's glow pulse; the panel draws glow and backlight as levels) */
#define FM1__NS_T(ns) (((uint32_t)(ns) * FM1_TICKS_PER_US + 500u) / 1000u)
static uint8_t fm1_led_dim[FM1_NCOL];   /* the glow (landmarks, notes under tiles) */
static uint8_t fm1_led_bg[FM1_NCOL];    /* the backlight (menu LIGHTS) */
static volatile uint16_t fm1_led_bg_ns; /* its pulse (ns), 0 = off */

static void fm1_input_init(void) {}

static void fm1__frame(void)                   /* the panel's state into fm1_in, with edges */
{
    uint64_t k = emu_keys();
    uint32_t b = (uint32_t)(k & 0x3FFFu), n = (uint32_t)(k >> 14) & 0x7FFFFFFu, e;
    fm1_in.notes_pressed |= n & ~fm1_in.notes;
    fm1_in.notes = n;
    fm1_in.pressed |= b & ~fm1_in.buttons;
    fm1_in.released |= fm1_in.buttons & ~b;
    fm1_in.buttons = b;
    for (e = 0; e < FM1_NENC; e++)
        fm1_in.enc_steps[e] = (int16_t)(fm1_in.enc_steps[e] + emu_enc_take(e));
    fm1_in.frames++;
}

static void fm1_input_scan(void) { fm1__frame(); }

static uint8_t fm1__tick_col;
static void fm1_input_tick(void)               /* TIMER5: a frame every FM1_NCOL ticks, as the scan */
{
    if (++fm1__tick_col == FM1_NCOL) {
        fm1__tick_col = 0;
        fm1__frame();
    }
}

static inline uint32_t fm1__lock(void) { emu_irq_off(); return 0; }
static inline void fm1__unlock(uint32_t v) { (void)v; emu_irq_on(); }

static int32_t fm1_enc_take(uint32_t e)
{
    uint32_t k = fm1__lock();
    int32_t s = fm1_in.enc_steps[e];
    fm1_in.enc_steps[e] = 0;
    fm1__unlock(k);
    return s;
}

static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t k = fm1__lock();
    uint32_t p = fm1_in.pressed, r = fm1_in.released;
    if (released)
        *released = r;
    fm1_in.pressed = fm1_in.released = 0;
    fm1__unlock(k);
    if (!p && !r)
        emu_idle();
    return p;
}

static uint32_t fm1_input_note_edges(void)
{
    uint32_t k = fm1__lock();
    uint32_t p = fm1_in.notes_pressed;
    fm1_in.notes_pressed = 0;
    fm1__unlock(k);
    return p;
}

static void fm1_led_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}
