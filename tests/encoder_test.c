/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the knob decoder (firmware/hal/fm1_input.h fm1__frame): an FM-1 detent is one full
 * quadrature cycle and the knob rests in one state. Clicks are played as the scan sees them, one
 * sample per frame (~1.1 ms): slow and fast turns, pauses mid-click (short and long), contact
 * bounce, back-and-forth, two-state jumps. Each click must give exactly one step. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_input.h"

static const uint8_t CW[4] = {0, 1, 3, 2};          /* + (clockwise): 00 -> 01 -> 11 -> 10 -> 00 */
static uint32_t pos;                                /* quadrature phase, 0..3 along CW */
static int fails;
static int32_t take(uint32_t e) { int32_t s = fm1_in.enc_steps[e]; fm1_in.enc_steps[e] = 0; return s; }

static void put(uint32_t e, uint32_t st)            /* encoder e reads state st (A << 1 | B) */
{
    const uint8_t *m = FM1_ENC[e];
    fm1_in.raw[m[0]] = (uint8_t)((fm1_in.raw[m[0]] & ~(1u << m[1])) | ((st >> 1) & 1u) << m[1]);
    fm1_in.raw[m[2]] = (uint8_t)((fm1_in.raw[m[2]] & ~(1u << m[3])) | (st & 1u) << m[3]);
}
static void frames(uint32_t e, uint32_t n) { while (n--) { put(e, CW[pos & 3u]); fm1__frame(); } }
static void move(uint32_t e, int dir, uint32_t hold)   /* one transition, then hold frames */
{
    pos = (pos + (dir > 0 ? 1u : 3u)) & 3u;
    frames(e, hold);
}
static void click(uint32_t e, int dir, uint32_t hold) { int i; for (i = 0; i < 4; i++) move(e, dir, hold); }
static void check(const char *what, int ok) { printf("encoder: %-58s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static void reset(void)
{
    uint32_t i;
    memset((void *)&fm1_in, 0, sizeof fm1_in);
    for (i = 0; i < FM1_NENC; i++)
        fm1_in.enc_prev[i] = fm1_in.enc_last[i] = 0xFF;
    pos = 0;
    frames(0, 50);                                  /* power-on: resting on a detent */
}

int main(void)
{
    uint32_t e = 0, k;
    int32_t s;
    reset();
    for (k = 0; k < 10u; k++) click(e, 1, 6), frames(e, 30);
    check("10 slow clicks clockwise: +10", take(e) == 10);
    for (k = 0; k < 10u; k++) click(e, -1, 6), frames(e, 30);
    check("10 slow clicks anticlockwise: -10", take(e) == -10);
    for (k = 0; k < 24u; k++) click(e, 1, 2);
    frames(e, 30);
    check("24 fast clicks (2 frames a transition): +24", take(e) == 24);
    for (k = 0; k < 24u; k++) {                     /* 2.7.2: a fast turn: the mid states one frame each */
        move(e, 1, 1); move(e, 1, 1); move(e, 1, 1); move(e, 1, 6);
    }
    frames(e, 30);
    check("2.7.2: 24 fast clicks, mid states 1 frame each (~1.1 ms): +24", take(e) == 24);
    for (k = 0; k < 24u; k++) {
        move(e, -1, 1); move(e, -1, 1); move(e, -1, 1); move(e, -1, 6);
    }
    frames(e, 30);
    check("2.7.2: the same anticlockwise: -24", take(e) == -24);
    for (k = 0; k < 5u; k++) {                      /* a slow turn that stops mid-click (50 ms) */
        move(e, 1, 6); move(e, 1, 45); move(e, 1, 6); move(e, 1, 30);
    }
    s = take(e);
    check("5 clicks with 50 ms pauses mid-click: +5 (0.9: the knob went dead)", s == 5);
    for (k = 0; k < 5u; k++) {
        move(e, 1, 6); move(e, 1, 6); frames(e, 200); move(e, 1, 6); move(e, 1, 30);
    }
    s = take(e);
    check("5 clicks held half-way 0.2 s: +5 (0.9: counted twice)", s == 5);
    for (k = 0; k < 5u; k++) {                      /* contact bounce on each transition */
        move(e, 1, 1); move(e, -1, 1); move(e, 1, 4);
        move(e, 1, 4); move(e, 1, 1); move(e, -1, 1); move(e, 1, 4); move(e, 1, 30);
    }
    check("5 clicks with contact bounce: +5", take(e) == 5);
    move(e, 1, 6); move(e, 1, 6); move(e, -1, 6); move(e, -1, 30);
    check("half a click and back: nothing", take(e) == 0);
    for (k = 0; k < 6u; k++) {                      /* a fast flick: two transitions in one sample */
        pos = (pos + 2u) & 3u; frames(e, 1);
        pos = (pos + 2u) & 3u; frames(e, 1);
    }
    frames(e, 30);
    s = take(e);
    check("6 flicks seen as two-state jumps: no wrong direction", s >= 0 && s <= 6);
    click(e, 1, 6); frames(e, 30); take(e);
    move(e, 1, 6); move(e, 1, 1000);                /* parked ~1 s mid-click (held at power-on) */
    for (k = 0; k < 4u; k++) click(e, 1, 6), frames(e, 30);
    check("parked 1 s off the detent, then 4 clicks: 4 steps", take(e) == 4);
    {   /* 2.7: a button held over MIDI (fm1_remote_btn) is read as closed: a press after the debounce, held, then let go */
        uint32_t i, down, held, up;
        fm1_in.buttons = fm1_in.pressed = fm1_in.released = 0;
        fm1_remote_btn = 1u << 10;
        for (i = 0; i < FM1_DEB_PRESS; i++)
            fm1__key(10, 0);
        down = (fm1_in.buttons >> 10) & 1u && (fm1_in.pressed >> 10) & 1u;
        for (i = 0; i < 50u; i++)
            fm1__key(10, 0);
        held = (fm1_in.buttons >> 10) & 1u;
        fm1_remote_btn = 0;
        for (i = 0; i < FM1_DEB_RELEASE; i++)
            fm1__key(10, 0);
        up = !((fm1_in.buttons >> 10) & 1u) && (fm1_in.released >> 10) & 1u;
        check("2.7: a button held over MIDI: pressed, held, released (as a real one)", down && held && up);
    }
    printf("%s\n", fails ? "ENCODER TEST FAILED" : "encoder test passed");
    return fails;
}
