/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37 menu KEY VELOCITY (firmware/src/smk_vel.h): each mode -> the velocity a note gets and the drum level it
 * plays, a release stays a release, the bits in the settings word (older data: DYNAMIC). */
#include "core.h"
#include "smk_vel.h"
#include <stdio.h>
static int fails;
static void check(int ok, const char *what) { printf("smk_vel: %-66s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(void)
{
    uint32_t m, v, ok;
    check(VEL_N == 6u && vel_mode == VEL_DYNAMIC, "six modes, DYNAMIC first (the default)");
    for (m = 0, ok = 1; m < VEL_N; m++)
        ok &= vel_apply(m, 0) == 0u;
    check(ok, "every mode: a release (0) stays a release");
    for (m = 0, ok = 1; m < VEL_N; m++)
        for (v = 1; v <= 127u; v++)
            ok &= vel_apply(m, v) >= 1u && vel_apply(m, v) <= 127u;
    check(ok, "every mode: 1..127 in, 1..127 out");
    for (v = 1, ok = 1; v <= 127u; v++)
        ok &= vel_apply(VEL_DYNAMIC, v) == v;
    check(ok, "DYNAMIC: as played");
    for (v = 1, ok = 1; v <= 127u; v++)
        ok &= vel_apply(VEL_SOFT, v) >= v && (v == 1u || vel_apply(VEL_SOFT, v) >= vel_apply(VEL_SOFT, v - 1u));
    check(ok && vel_apply(VEL_SOFT, 64) == 96u && vel_apply(VEL_SOFT, 32) == 56u && vel_apply(VEL_SOFT, 127) == 127u,
          "SOFT: louder for a light touch (32 -> 56, 64 -> 96), rising, 127 stays");
    for (v = 1, ok = 1; v <= 127u; v++)
        ok &= vel_apply(VEL_HARD, v) <= v && (v == 1u || vel_apply(VEL_HARD, v) >= vel_apply(VEL_HARD, v - 1u));
    check(ok && vel_apply(VEL_HARD, 64) == 32u && vel_apply(VEL_HARD, 100) == 78u && vel_apply(VEL_HARD, 127) == 127u &&
          vel_apply(VEL_HARD, 1) == 1u, "HARD: loud needs a hard hit (64 -> 32, 100 -> 78), rising, never 0");
    for (v = 1, ok = 1; v <= 127u; v++)
        ok &= vel_apply(VEL_FIX64, v) == 64u && vel_apply(VEL_FIX100, v) == 100u && vel_apply(VEL_FIX127, v) == 127u;
    check(ok, "FIXED 64 / 100 / 127: every note at that velocity");
    check(vel_apply(VEL_DYNAMIC, 200) == 127u, "out of range in: 127");

    /* the drum track: the level a key or pad plays (OCT- / OCT+ held win, seq.c) */
    check(vel_drum_lvl(vel_apply(VEL_FIX64, 127)) == LV_SOFT, "FIXED 64: drums soft");
    check(vel_drum_lvl(vel_apply(VEL_FIX100, 10)) == LV_NORM, "FIXED 100: drums normal (as the FM-1's keys)");
    check(vel_drum_lvl(vel_apply(VEL_FIX127, 10)) == LV_HARD, "FIXED 127: drums hard");
    check(vel_drum_lvl(vel_apply(VEL_DYNAMIC, 20)) == LV_GHOST && vel_drum_lvl(vel_apply(VEL_DYNAMIC, 60)) == LV_SOFT &&
          vel_drum_lvl(vel_apply(VEL_DYNAMIC, 100)) == LV_NORM && vel_drum_lvl(vel_apply(VEL_DYNAMIC, 120)) == LV_HARD,
          "DYNAMIC: ghost < 40, soft < 80, normal, hard > 112 (as before)");
    check(vel_drum_lvl(vel_apply(VEL_SOFT, 30)) == LV_SOFT && vel_drum_lvl(vel_apply(VEL_HARD, 60)) == LV_GHOST,
          "SOFT: a light hit (30) soft, not ghost; HARD: a medium hit (60) ghost");

    /* the settings word (panel.c lights_word, bits 28..30) */
    for (m = 0, ok = 1; m < VEL_N; m++)
        ok &= vel_of_word(vel_word(m) | 0x0FFFFFFFu) == m && (vel_word(m) & 0x8FFFFFFFu) == 0u;
    check(ok, "each mode round trips in bits 28..30, the other settings untouched");
    check(vel_of_word(0x0C3FFFFFu) == VEL_DYNAMIC, "older data (Snoop37 2.6, SLOOP: bits 28..30 clear): DYNAMIC");
    check(vel_of_word(6u << 28) == VEL_DYNAMIC && vel_of_word(7u << 28) == VEL_DYNAMIC, "a value out of range: DYNAMIC");

    printf("smk_vel: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
