/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro FADER BANK screen (firmware/src/ui_fbank.c) on the UI harness: banks wrap, what each fader sets,
 * closes by itself; renders DIR/fbank-a.ppm, fbank-b.ppm. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
static uint8_t smk_fbank, smk_fbank_new;           /* (ui_input.c, SMK build) */
static char knob_digit(uint32_t role) { return role == EN_SELECT ? '2' : '5'; }   /* (panel.c, SMK build: KNOB LAYOUT FM-1) */
#include "../firmware/src/ui_fbank.c"

static void fb_shot(const char *name)
{
    ui.frame++;
    fb_draw();
    ppm(name);
}

int main(int argc, char **argv)
{
    uint32_t i;
    int16_t *vp;
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    go_home(); ui.force = 1; frame();

    fb_set(smk_fbank + 1u);
    check(fb_shown() && smk_fbank == 1 && smk_fbank_new, "FADER BANK: the next bank, its screen up");
    fb_set(smk_fbank + 3u);
    check(smk_fbank == 0, "SELECT back: bank A");
    fb_set(3u + 1u);
    check(smk_fbank == 0, "D -> A wraps");
    check(fb_param(0, TRK_DRUM, &vp) == &GP[G_DRLVL] && vp == &song.g[G_DRLVL], "bank A, fader 4: the drums' level");
    check(fb_param(1, TRK_DRUM, &vp) == &TP[P_TFLT] && vp == &TDRUM->p[P_TFLT], "bank B, fader 4: the drum track's filter");
    check(fb_param(2, 0, &vp) == &TP[P_REV] && vp == &trk[0].p[P_REV], "bank C, fader 1: track 1's reverb");
    ui.force = 1; fb_shot("fbank-a");
    trk[1].p[P_TFLT] = -30; trk[2].p[P_TFLT] = 20;
    fb_set(1); ui.force = 1; fb_shot("fbank-b");
    ui.layer = LY_STEP;
    check(!fb_shown(), "hidden while a layer is held");
    ui.layer = LY_PLAY;
    fm1_ms += 60000u;
    check(fb_shown(), "stays up until FADER BANK / PATCH");
    fb_set(3); fb_set(smk_fbank + 1u);
    check(smk_fbank == 0, "K2 turns the bank round (D -> A)");
    fb_close();
    check(!fb_shown(), "closed");
    printf("fbank: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
