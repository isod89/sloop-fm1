/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The pads as a numbered grid read like text (1-8 the top row, 9-16 the bottom): the steps (SEQ held, the
 * DRUMS grid) and the tiles (FX effects, GLO mix, SAVE slots). Playing notes keeps low notes at the bottom. */
static int padgrid_on(void)
{
    return kb_grid || ui.layer == LY_STEP || ui.layer == LY_FX || ui.layer == LY_MIX || ui.layer == LY_SONG;
}
