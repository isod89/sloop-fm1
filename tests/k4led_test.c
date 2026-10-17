/* SMK-37 Pro: KNOB BANK lit on a PRESETS detent; which pad modes are a grid (UI harness) */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
static uint8_t smk_fbank, smk_fbank_new;           /* (ui_input.c, SMK build) */
#include "../firmware/src/ui_pick.c"
#include "../firmware/src/ui_fbank.c"
#include "../firmware/src/ui_padgrid.c"

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    go_home(); ui.force = 1; frame();
    song.sel = 0;

    ui.layer = LY_STEP;
    ui.layer = LY_PLAY;
    fb_set(smk_fbank);
    fb_close();
    pick_open_tmp();
    check((int32_t)(fm1_ms - pick_k4_until) < 0, "a PRESETS detent: KNOB BANK lit");
    fm1_ms += 160u;
    check((int32_t)(fm1_ms - pick_k4_until) >= 0, "~150 ms later: off");
    pick_close();
    ui.menu = 1;
    ui.menu = 0;
    open_family(FAM_ENV);
    go_home();
    check(!padgrid_on(), "playing notes: not a grid (low notes at the bottom)");
    ui.layer = LY_STEP; check(padgrid_on(), "SEQ held (steps): a grid");
    ui.layer = LY_FX; check(padgrid_on(), "FX held (effects): a grid");
    ui.layer = LY_MIX; check(padgrid_on(), "GLO held (mix tiles): a grid");
    ui.layer = LY_SONG; check(padgrid_on(), "SAVE held (slots): a grid");
    ui.layer = LY_ERASE; check(!padgrid_on(), "EDIT held (erase notes / sounds): not a grid");
    ui.layer = LY_ROLL; check(!padgrid_on(), "ARP held (note repeat): not a grid");
    ui.layer = LY_PLAY; kb_grid = 1; check(padgrid_on(), "the DRUMS grid page: a grid");
    kb_grid = 0;
    printf("k4led: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
