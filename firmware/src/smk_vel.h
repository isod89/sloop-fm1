/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37: HOME menu > AUDIO > KEY VELOCITY, how hard the keys and pads play. DYNAMIC: as played (the unit's factory
 * curves, smk_input.h); SOFT: a light touch plays louder; HARD: loud needs a hard hit; FIXED 64 / 100 / 127: every
 * note at that velocity, as on the FM-1 (100). Applied where seq.c reads a key's or a pad's velocity, so the notes,
 * their MIDI out and the drum levels (ghost .. hard) follow it. Kept in the SMK-37 settings (project.c smkset_t;
 * earlier SMK-37 builds: the settings word's bits 28..30, read once; none: DYNAMIC). */
#pragma once

enum { VEL_DYNAMIC, VEL_SOFT, VEL_HARD, VEL_FIX64, VEL_FIX100, VEL_FIX127, VEL_N };
static const char *const VEL_NAME[VEL_N] = {"DYNAMIC", "SOFT", "HARD", "FIXED 64", "FIXED 100", "FIXED 127"};
static uint8_t vel_mode;                           /* menu KEY VELOCITY (VEL_*) */

/* a played velocity (1..127; 0 = a release, kept) -> the one the note gets */
static uint32_t vel_apply(uint32_t mode, uint32_t v)
{
    uint32_t q;
    if (!v)
        return 0;
    if (v > 127u)
        v = 127u;
    switch (mode) {
    case VEL_SOFT:                                 /* 127 - (127 - v)^2 / 127: 32 -> 56, 64 -> 96 */
        q = 127u - v;
        return 127u - q * q / 127u;
    case VEL_HARD:                                 /* v^2 / 127: 64 -> 32, 100 -> 78 (at least 1) */
        q = v * v / 127u;
        return q ? q : 1u;
    case VEL_FIX64: return 64u;
    case VEL_FIX100: return 100u;
    case VEL_FIX127: return 127u;
    default: return v;
    }
}

/* its bits in the settings word of earlier SMK-37 builds (bits 28..30; project.c smkset_boot reads them while there
 * is no OBJ_SMKSET); 0 or a value out of range: DYNAMIC */
static uint32_t vel_word(uint32_t mode) { return (mode % VEL_N) << 28; }
static uint32_t vel_of_word(uint32_t w) { return ((w >> 28) & 7u) < VEL_N ? (w >> 28) & 7u : VEL_DYNAMIC; }

/* a velocity -> the drum level it plays (seq.c smk_vel_lvl, unless OCT- / OCT+ is held) */
static uint32_t vel_drum_lvl(uint32_t v)
{
    return v < 40u ? LV_GHOST : v < 80u ? LV_SOFT : v > 112u ? LV_HARD : LV_NORM;
}
