/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro pad sound picker (firmware/src/ui_pick.c) on the UI harness: the pages by kind, 16 a page, the
 * current sound's page, a cell loads its sound, < > wrap, the short names, the drum kits; renders
 * DIR/pick-synth.ppm, pick-drums.ppm. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include "../firmware/src/ui_pick.c"

static void pick_shot(const char *name)
{
    ui.frame++;
    pick_draw();
    ppm(name);
}

int main(int argc, char **argv)
{
    uint32_t i, np, sum = 0, of = 0;
    pick_pg_t pg;
    char a[4], b[4];
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    go_home(); ui.force = 1; frame();
    song.sel = 0;

    pick_abbr("PLUGG BASS", a, b);
    check(!strcmp(a, "PLU") && !strcmp(b, "BAS"), "short names: two words, 3 + 3");
    pick_abbr("SUPERSAW", a, b);
    check(!strcmp(a, "SUP") && !strcmp(b, "ERS"), "short names: one word, its first 6 letters");
    pick_abbr("PAD", a, b);
    check(!strcmp(a, "PAD") && !b[0], "short names: a short word, one line");

    np = pick_walk(0, &pg, 0xFFFFu, 0);
    check(pg.first == 0 && pg.len == 16 && pg.kind == 0 && pg.nsub == 2, "page 1: the first 16 basses, BASS 1/2");
    for (i = 0; i < np; i++) {
        pick_walk(i, &pg, 0xFFFFu, 0);
        if (pg.first != sum || pg.len == 0 || pg.len > 16) break;
        sum += pg.len;
    }
    check(i == np && sum == NBANK + up_count(), "the pages cover the list once, in order, 16 at most");
    pick_walk(np - 1u, &pg, 0xFFFFu, 0);
    check(pg.kind == PK_USER, "the last page: the user presets");

    pick_open_tmp();
    pick_walk(0xFFFFu, 0, pick_cur(), &of);
    check(pick_shown() && pick_page == of, "a PRESETS turn: open at the current sound's page");
    pick_turn(-1);
    pick_turn(1);
    check(pick_page == of, "< > step the pages and back");
    pick_page = 0; pick_turn(-1);
    check(pick_page == np - 1u, "< on the first page: the last");
    pick_page = 1;
    pick_select(3);
    pick_info(&pg);
    check(pick_cur() == pg.first + 3u, "a pad loads its sound (as PRESETS)");
    fm1_ms += PICK_MS + 10u;
    check(!pick_shown(), "closes by itself after PICK_MS");
    pick_pin_toggle();
    fm1_ms += PICK_MS + 10u;
    check(pick_shown(), "KNOB BANK: pinned open");
    ui.layer = LY_STEP;
    check(!pick_shown(), "hidden while a layer is held");
    ui.layer = LY_PLAY;
    ui.force = 1; pick_shot("pick-synth");
    pick_pin_toggle();
    check(!pick_shown(), "KNOB BANK again: closed");

    track_select(TRK_DRUM);
    pick_pin_toggle();
    np = pick_info(&pg);
    check(np == (DRUM_USR + 15u) / 16u + (DRUM_KITS - DRUM_USR + 15u) / 16u, "drum track: the kits, 16 a page");
    pick_select(2);
    check(pick_cur() == pg.first + 2u, "drum track: a pad picks its kit");
    ui.force = 1; pick_shot("pick-drums");
    pick_close();
    printf("pick: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
