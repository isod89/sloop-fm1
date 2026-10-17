/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro pads (firmware/hal/smk_input.h): the step grid in reading order (top-left = step 1),
 * notes low at the bottom (bottom-left = the lowest); on the PLAY layer DRUM KIT (a pad a drum lane, as on the drum
 * track) or STEPS (a pad a step), through the outer-key queue (no note key), its release too. Built -O2 (the IRQ lock's asm is left out). */
#define BOARD_SMK37 1
#include "smk_input.h"
#include <stdio.h>
static int fails;
static void check(int ok, const char *what) { printf("padgrid: %-60s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static uint32_t hit(uint32_t pad)               /* the note key a pad presses (bit) */
{
    uint32_t n;
    smk__padq(pad, 100);
    n = fm1_in.pnotes;
    smk__padq(pad, 0);
    return n;
}

int main(void)
{
    smk_pad_grid = 0;
    check(hit(8) == 1u << 0, "notes: bottom-left pad = the lowest note (F3)");
    check(hit(0) == 1u << SMK_WHITE_NOTE[8], "notes: top-left pad = white key 9");
    check(SMK_PAD_OF_KEY(0) == 8u, "notes: white key 0 lit on the bottom-left pad");
    smk_pad_grid = 1;
    check(hit(0) == 1u << 0, "grid: top-left pad = step 1");
    check(hit(7) == 1u << SMK_WHITE_NOTE[7], "grid: top-right pad = step 8");
    check(hit(8) == 1u << SMK_WHITE_NOTE[8], "grid: bottom-left pad = step 9");
    check(SMK_PAD_OF_KEY(0) == 0u && SMK_PAD_OF_KEY(15) == 15u, "grid: step w lit on pad w");
    smk__padq(8, 100);                          /* a pad down as the mode changes: its release ends its own key */
    smk_pad_grid = 0;
    smk__padq(8, 0);
    check(fm1_in.pnotes == 0u, "a mode change while held: no stuck key");

    /* the pads on the PLAY layer, DRUM KIT (the default) and STEPS (KNOB BANK + FADER BANK) */
    {
        uint32_t p, ok = 1;
        int32_t e;
        while (smk_extkey_take() >= 0)
            ;
        smk_pad_drums = 1;
        for (p = 0; p < SMK_NPAD; p++) {
            smk__padq(p, 90);
            e = smk_extkey_take();
            ok &= e == (int32_t)((SMK_EXT_PAD + SMK_WHITE_OF_PAD(p)) << 8 | 90u) && fm1_in.pnotes == 0u;
            smk__padq(p, 0);
            e = smk_extkey_take();
            ok &= e == (int32_t)((SMK_EXT_PAD + SMK_WHITE_OF_PAD(p)) << 8);
        }
        check(ok, "DRUM KIT: each pad its lane (bottom-left kick), velocity, release; no note");
        check(SMK_EXT_PAD > 36u && (SMK_EXT_STEP + 15u) << 8 < 0x10000u, "DRUM KIT / STEPS: codes apart from the outer keys'");
        smk_pad_drums = 0;
        smk_pad_steps = 1;
        for (ok = 1, p = 0; p < SMK_NPAD; p++) {
            smk__padq(p, 90);
            e = smk_extkey_take();
            ok &= e == (int32_t)((SMK_EXT_STEP + p) << 8 | 90u) && fm1_in.pnotes == 0u;
            smk__padq(p, 0);
            ok &= smk_extkey_take() == (int32_t)((SMK_EXT_STEP + p) << 8);
        }
        check(ok, "STEPS: each pad its step in reading order (top-left = 1), press, release; no note");
        smk__padq(8, 100);                      /* held as the state changes: its release goes where its press went */
        smk_extkey_take();
        smk_pad_steps = 0;
        smk_pad_drums = 1;
        smk__padq(8, 0);
        check(smk_extkey_take() == (int32_t)((SMK_EXT_STEP + 8u) << 8), "STEPS -> DRUM KIT while a pad is held: its step's release");
        smk__padq(8, 100);
        smk_extkey_take();
        smk_pad_drums = 0;
        smk__padq(8, 0);
        check(smk_extkey_take() == (int32_t)(SMK_EXT_PAD << 8) && fm1_in.pnotes == 0u, "a layer while a drum pad is held: its drum ends");
        smk__padq(8, 100);                      /* a note held (a layer) as DRUM KIT comes back */
        smk_pad_drums = 1;
        smk__padq(8, 0);
        check(fm1_in.pnotes == 0u && smk_extkey_take() < 0, "DRUM KIT back while a note is held: the note ends, no drum");
        smk_pad_grid = 1;
        check(hit(0) == 1u << 0 && smk_extkey_take() < 0, "a grid (SEQ / FX / GLO / SAVE held, DRUMS grid): pads stay the grid");
        smk_pad_grid = 0;
        smk_pad_piano = 1;
        check(hit(0) == 1u << 7 && smk_extkey_take() < 0, "SCALE held (piano): the pads stay the piano");
        smk_pad_piano = 0;
        smk_pad_pick = 1u << 3;
        smk__padq(3, 100);
        check(smk_pick_hits == 1u << 3 && smk_extkey_take() < 0, "the sound picker up: the pad picks");
        smk__padq(3, 0);
        smk_pad_pick = 0;
        smk_pad_drums = 0;
    }
    printf("padgrid: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
