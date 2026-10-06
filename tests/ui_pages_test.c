/* SPDX-License-Identifier: GPL-3.0-only */
/* The real UI (ui.c, ui_draw.c, ui_studio.c, ui_layers.c, ui_song.c, ui_menu.c, ui_input.c) on a
 * framebuffer with panel / flash / storage doubles, the audio (mix_block) running between frames as on
 * the device. Renders every screen to PPM for review (DIR/page-*.ppm, live-*.ppm, layer-*.ppm) and
 * drives the panel:
 *   taps    a layer button tapped opens its pages; held + a key / knob it does not
 *   FX      held + a white key: punch-in; knobs: filter, dust, duck
 *   SEQ     held: the steps on the white keys (drums: the sound played last), OCT: pages, a step key
 *           held + KNOB 2 / 3: level / ratchet
 *   EDIT    held + a key: erase; OCT- / OCT+: undo / redo; KNOB 1 shift, 2 length x2
 *   ARP     held + a key: a roll; KNOB 1 the rate
 *   SCL     held + a key: the key of the song
 *   GLO     held + keys: mute, solo, tap tempo; knobs: levels
 *   REC     press: arm / record at once; held: the clear ring, to the end: cleared (undo brings it back)
 *   SAVE    tapped: the song page; held: the SONG layer (sections A..D: play / store, SONG REC)
 *   DRUMS   the grid: sound / step / hit / level knobs, GRID <-> KIT
 * then 20000 frames of random use: every draw stays on the screen. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; assert(x+w<=240 && y+h<=240); for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; assert(x+w<=240&&y+h<=240); for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static void fm1_wdt_feed(void) {}
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static uint32_t saves, loads;
static int project_used(uint32_t i) { return i < 2; }
static void project_save(uint32_t i) { (void)i; saves++; ui_message("SAVED"); }
static void project_load(uint32_t i) { (void)i; loads++; }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return 3; }
static void arrangement_apply(uint32_t s) { (void)s; }
static void song_backup(void) {}
static void song_restore(void) {}
static uint32_t sec_stores, sec_loads;
static void section_store(uint32_t s) { sec_stores++; live_sec = (int8_t)s; }
static void section_load(uint32_t s) { sec_loads++; live_sec = (int8_t)s; }
static int up_used(uint32_t k) { return k < 2; }
static int up_load(uint32_t k) { (void)k; return 0; }
static uint32_t up_count(void) { return 2; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { str_cpy(b, k ? "MY PAD" : "MY LEAD", 13); }
static void up_slot_label(char *b, uint32_t k) { fmt_int(b, (int32_t)k + 1); }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
static void settings_save(void) {}
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/splash.c"
static const char *outdir;
static void ppm(const char *name) {
    char path[512]; snprintf(path,sizeof path,"%s/%s.ppm",outdir,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++) { uint16_t p=swap16(screen[i]); uint8_t rgb[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31}; fwrite(rgb,1,3,f); }
    fclose(f);
}
/* one UI frame (~16 ms): the audio between (as the ISR does), then input, LEDs, draw */
static void frame(void)
{
    uint32_t q;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++) mix_block(o, CTL);
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void frames(uint32_t n) { while (n--) frame(); }
static uint32_t BT(uint32_t b) { return 1u << panel.btn[b]; }
static void press(uint32_t b) { edges_btn |= BT(b); fm1_in.buttons |= BT(b); frame(); }
static void release(uint32_t b) { fm1_in.buttons &= ~BT(b); frame(); }
static void tap(uint32_t b) { press(b); release(b); }
static void key(uint32_t k) { fm1_in.notes |= 1u << k; frame(); fm1_in.notes &= ~(1u << k); frame(); }
static int fails;
static void check(int ok, const char *what) { printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    {   /* the preset list by kind (ui.c BANK): every factory preset of every engine once, every name found */
        uint32_t e, k, n, hits;
        bank_resolve();
        for (n = 0; n < NBANK; n++)
            if (bank_pi[n] == 0xFF) {
                printf("ui: BANK %s: no such preset in engine %u\n", BANK[n].name, BANK[n].e);
                fails++;
            }
        for (e = 0; e < NENGINES; e++)
            for (k = 0; k < ENGINES[e]->npresets; k++) {
                for (hits = 0, n = 0; n < NBANK; n++)
                    hits += BANK[n].e == e && bank_pi[n] == k;
                if (hits != 1) {
                    printf("ui: preset %s (engine %u) is %u times in BANK\n", ENGINES[e]->presets[k].name, e, hits);
                    fails++;
                }
            }
    }
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    sloop_splash(); ppm("page-splash");
    ui.menu = 2; ui.force = 1; frame(); ppm("page-about"); ui.menu = 0;
    go_home(); ui.force = 1; frame(); ppm("page-tracks");
    open_family(FAM_ENV); ui.force = 1; ui.hot_col = 1; ui.hot_t = 30; frame(); ppm("page-env");
    open_family(FAM_EDIT); ui.force = 1; frame(); ppm("page-edit");
    open_family(FAM_FX); ui.force = 1; frame(); ppm("page-fx");
    open_family(FAM_SEQ); ui.force = 1; frame(); ppm("page-step");
    open_family(FAM_GLO); ui.force = 1; frame(); ppm("page-global");
    open_family(FAM_GLO); ui.force = 1; frame(); ppm("page-master");
    open_family(FAM_SCL); ui.force = 1; frame(); ppm("page-scale");

    /* ---- taps open pages, holds are layers */
    go_home(); ui.force = 1; frame();
    { uint8_t was = song.sel; song.sel = 0; check(keys_guide() == 0u, "synth track, no layer: no landmarks (a piano)"); song.sel = was; }
    tap(B_SEQ); check(cur_fam() == FAM_SEQ, "SEQ tapped: the SEQ pages");
    go_home(); frame();
    tap(B_FX); check(cur_fam() == FAM_FX, "FX tapped: the FX pages");
    go_home(); frame();
    press(B_FX); frames(12); check(ui.layer == LY_FX && punch.hold, "FX held: the punch layer shows");
    check(keys_guide() == (1u << key_of_white(0) | 1u << key_of_white(4) | 1u << key_of_white(8) | 1u << key_of_white(12)),
          "FX held: keys 1, 5, 9, 13 lit dim (the rows of the grid)");
    ppm("layer-punch");
    fm1_in.notes = 1u << 4; frame(); check(punch.req == 2, "FX + the 3rd white key: punch effect 3");
    ppm("layer-punch-on");
    fm1_in.notes = 0; frame(); check(punch.req == -1, "key up: the mix comes back");
    encs[panel.enc[EN_K2]] = 10; frame(); check(song.g[G_DUST] > 0, "FX + KNOB 2: DUST");
    encs[panel.enc[EN_K1]] = -10; frame(); check(song.g[G_FILT] < 0, "FX + KNOB 1: the filter (low-pass)");
    release(B_FX); check(cur_page()->scope == SC_TRK && ui.layer == LY_PLAY, "FX used then let go: no FX page");
    song.g[G_DUST] = 0; song.g[G_FILT] = 0;

    /* ---- a layer locked open: held + HOME tapped; any other button (not PLAY, REC, OCT) lets it go */
    go_home(); frame();
    press(B_FX); frames(3); tap(B_HOME); release(B_FX); frames(3);
    check(ly_lock == LY_FX && ui.layer == LY_FX && punch.hold, "FX held + HOME: locked open, FX let go");
    check(cur_page()->scope == SC_TRK, "FX + HOME: no FX page, no HOME jump");
    fm1_in.notes = 1u << 4; frame(); check(punch.req == 2, "locked FX + the 3rd white key: punch effect 3 (no hands on FX)");
    fm1_in.notes = 0; frame(); check(punch.req == -1, "locked FX, key up: the mix comes back");
    encs[panel.enc[EN_K2]] = 6; frame(); check(song.g[G_DUST] > 0, "locked FX + KNOB 2: DUST");
    ui.force = 1; frame(); ppm("layer-locked");
    { uint8_t was = song.playing; tap(B_PLAY); frames(2);
      check(ly_lock == LY_FX && song.playing != was, "locked: PLAY plays and keeps the lock"); tap(B_PLAY); frames(2); }
    tap(B_ENV); frames(2);
    check(ly_lock == LY_PLAY && ui.layer == LY_PLAY && !punch.hold && cur_page()->scope == SC_TRK,
          "locked, ENV pressed: unlocked, and only that (no ENV page)");
    press(B_FX); frames(3); tap(B_HOME); release(B_FX); frames(3);
    tap(B_HOME); frames(2);
    check(ly_lock == LY_PLAY && ui.layer == LY_PLAY && cur_page()->scope == SC_TRK, "locked, HOME tapped: unlocked");
    press(B_FX); frames(3); tap(B_HOME); release(B_FX); frames(3);
    tap(B_FX); frames(2);
    check(ly_lock == LY_PLAY && ui.layer == LY_PLAY && cur_fam() != FAM_FX, "locked, FX tapped: unlocked, no FX page");
    go_home(); frame();
    press(B_SEQ); frames(3); tap(B_HOME); release(B_SEQ); frames(3);
    press(B_FX); frames(12);
    check(ly_lock == LY_PLAY && ui.layer == LY_FX, "locked SEQ, FX held: unlocked, the FX layer");
    release(B_FX); frames(2); check(ui.layer == LY_PLAY, "and FX let go: back to playing");
    press(B_FX); frames(3); tap(B_HOME); release(B_FX); frames(3);
    press(B_HOME); frames(50); release(B_HOME); frames(2);
    check(ui.menu && ly_lock == LY_PLAY, "locked, HOME held: the menu, unlocked");
    ui.menu = 0; ui.force = 1; frames(2);
    song.g[G_DUST] = 0; song.g[G_FILT] = 0;

    /* ---- SEQ layer: drum steps on the white keys */
    song.sel = TRK_DRUM; go_home(); frame();
    key(4);                                          /* A3: the snare, played: the layer's sound */
    check(pen_lane == 2, "a drum key played: the SEQ layer's sound (snare)");
    press(B_SEQ); frames(10);
    key(0); key(7); key(14);                          /* steps 1, 5, 9 */
    check(dstep_has(&TDRUM->dstep[0], 2) && dstep_has(&TDRUM->dstep[4], 2) && dstep_has(&TDRUM->dstep[8], 2) &&
          !dstep_has(&TDRUM->dstep[1], 2), "SEQ + white keys 1, 5, 9: snare steps");
    ppm("layer-steps");
    fm1_in.notes = 1u << 7; frame();                  /* step 5 held + KNOB 2 / 3: level, ratchet */
    encs[panel.enc[EN_K2]] = 1; frame();
    encs[panel.enc[EN_K3]] = 2; frame();
    fm1_in.notes = 0; frame();
    check(dstep_lvl(&TDRUM->dstep[4], 2) == LV_HARD && dstep_rat(&TDRUM->dstep[4], 2) == 2u,
          "step 5 held + KNOB 2 / 3: hard, x3");
    key(0); check(!dstep_has(&TDRUM->dstep[0], 2), "step 1 again: off");
    release(B_SEQ); check(ui.layer == LY_PLAY && !on_drum_page(), "SEQ used then let go: no page change");

    /* ---- EDIT: undo / redo, erase, length */
    press(B_EDIT); frames(10);
    edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
    check(!dstep_has(&TDRUM->dstep[4], 2) && !dstep_has(&TDRUM->dstep[8], 2), "EDIT + OCT-: undo (the SEQ hold's steps gone)");
    edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
    check(!dstep_has(&TDRUM->dstep[0], 2) && dstep_lvl(&TDRUM->dstep[4], 2) == LV_HARD, "EDIT + OCT+: redo (back, as left)");
    ppm("layer-erase");
    key(4);                                           /* stopped: every snare goes */
    check(!dstep_has(&TDRUM->dstep[4], 2) && !dstep_has(&TDRUM->dstep[8], 2), "EDIT + snare (stopped): every snare erased");
    encs[panel.enc[EN_K2]] = 1; frame();
    check(TDRUM->p[P_SLEN] == 32, "EDIT + KNOB 2: length x2");
    release(B_EDIT);
    edges_btn |= BT(B_EDIT); fm1_in.buttons |= BT(B_EDIT); frame();      /* undo the length too */
    edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
    release(B_EDIT);
    check(TDRUM->p[P_SLEN] == 16, "EDIT + OCT-: the length back to 16");

    /* ---- ARP layer: a roll, rate knob */
    press(B_ARP); frames(10);
    encs[panel.enc[EN_K1]] = 1; frame();
    check(song.g[G_ROLL] == 2, "ARP + KNOB 1: the roll rate (1/32)");
    fm1_in.notes = 1u << 7; frames(3); check(roll[0].on, "ARP + a key: it rolls");
    ppm("layer-roll");
    fm1_in.notes = 0; frame(); check(!roll[0].on, "key up: the roll ends");
    release(B_ARP);

    /* ---- SCL: the key of the song; GLO: mute, solo, tap */
    song.sel = 0; go_home(); frame();
    press(B_SCL); frames(10);
    key(9);                                           /* D4 */
    check(trk[0].p[P_ROOT] == 2 && trk[1].p[P_ROOT] == 2 && trk[2].p[P_ROOT] == 2, "SCL + D: every part in D");
    encs[panel.enc[EN_K1]] = 2; frame();
    check(trk[0].p[P_CHORD] == 2, "SCL + KNOB 1: chords (7TH) on the track");
    ppm("layer-key");
    release(B_SCL);
    press(B_GLO); frames(10);
    key(0); check(trk[0].p[P_MUTE] == 1, "GLO + key 1: track 1 muted");
    key(9); check(song.solo == 2u, "GLO + key 6: track 2 soloed");
    ppm("layer-mix");
    for (i = 0; i < 4u; i++) { fm1_in.notes = 1u << 26; frame(); fm1_in.notes = 0; frames(36); }   /* ~0.6 s apart */
    check(song.g[G_BPM] >= 95 && song.g[G_BPM] <= 105, "GLO + the last key, tapped at ~0.6 s: ~100 BPM");
    key(0); key(9);
    check(!trk[0].p[P_MUTE] && !song.solo, "again: unmuted, no solo");
    release(B_GLO);
    trk[0].p[P_CHORD] = 0;

    /* ---- REC: press arms; held: the ring; to the end: cleared */
    go_home(); frame();
    song.playing = 0; song.rec = 0; rec_wait = 0;
    tap(B_REC); check(rec_wait == 1, "REC tapped (stopped): armed");
    ui.force = 1; frame(); ppm("live-rec-ready");
    tap(B_REC); check(rec_wait == 0, "REC again: cancelled");
    trk[0].step[3].n = 1, trk[0].step[3].note[0] = 60, trk[0].step[3].time = ST_NOTE;
    press(B_REC); frames(50);                         /* 0.8 s: the ring */
    check(ui.hold_kind == 1u && rec_wait == 0, "REC held: the press undone, the clear ring");
    ppm("hold-clear");
    frames(90);                                       /* to the end */
    check(!trk[0].step[3].n && ui.hold_kind == 0, "REC held to the end: track 1 cleared");
    release(B_REC);
    press(B_EDIT); frames(10);
    edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
    release(B_EDIT);
    check(trk[0].step[3].n == 1, "EDIT + OCT-: the cleared track back");
    press(B_REC); frames(60); release(B_REC);         /* let go before the end: nothing */
    check(trk[0].step[3].n == 1 && !rec_wait, "REC let go before the end: nothing cleared");

    /* ---- SAVE: tap = the song page; held = the SONG layer (live sections, SONG REC) */
    go_home(); frame();
    song.playing = 0; live_sec = -1; live_req = -1; srec = 0; arrangement_enabled = 0;
    press(B_SAVE); frames(15); check(ui.layer == LY_SONG, "SAVE held: the song layer");
    key(key_of_white(4)); check(sec_stores == 0 && sec_armed == 1u, "store over a used A: asks again");
    key(key_of_white(4)); check(sec_stores == 1 && live_sec == 0, "again: the loop stored in A");
    key(key_of_white(6)); check(sec_stores == 2 && live_sec == 2, "an empty C: stored at once");
    key(key_of_white(1)); check(sec_loads == 1 && live_sec == 1, "stopped: B loaded as the loop");
    key(key_of_white(3)); check(sec_loads == 1, "an empty D: not played");
    song.playing = 1; clk_beat = 1; clk_pos = 0;
    key(key_of_white(0)); check(live_req == 0, "playing: A asked for the next bar");
    live_req = -1; song.playing = 0;
    key(key_of_white(13)); check(srec == 1u && !arrangement_enabled, "SONG REC armed (loop mode)");
    ui.force = 1; frame(); ppm("layer-song");
    key(key_of_white(13)); check(srec == 0u, "SONG REC again: off");
    key(key_of_white(12)); check(arrangement_enabled == 1u, "loop / song: song mode");
    key(key_of_white(12)); check(arrangement_enabled == 0u, "again: loop mode");
    release(B_SAVE);
    check(!on_song_page() && saves == 0, "SAVE held and let go: no song page, no save");
    tap(B_SAVE); check(on_song_page(), "SAVE tapped on TRACKS: the song page");
    ui.force = 1; frame(); ppm("page-song");

    /* ---- the drum screen */
    song.sel = TRK_DRUM; studio_open(SC_DRUM); ui.force = 1; frame();
    check(on_drum_page(), "the drum screen");
    encs[panel.enc[EN_K3]] = 1; frame(); check(dstep_has(&TDRUM->dstep[0], 0), "DRUMS: KNOB 3 adds the kick on step 1");
    encs[panel.enc[EN_K4]] = -1; frame(); check(dstep_lvl(&TDRUM->dstep[0], 0) == LV_SOFT, "DRUMS: KNOB 4 softer");
    encs[panel.enc[EN_K1]] = 100; encs[panel.enc[EN_K2]] = 100; frame();
    check(drum_lane == 15 && drum_cursor == 15, "DRUMS: KNOB 1 / 2 bounded (16 sounds, 16 steps)");
    {   /* a scene to look at */
        static const uint8_t BEAT[16] = {0x11, 0x10, 0x10, 0x10, 0x14, 0x10, 0x10, 0x21, 0x11, 0x10, 0x01, 0x10, 0x1C, 0x10, 0x10, 0x20};
        uint32_t j;
        for (j = 0; j < 16u; j++) {
            memset(&TDRUM->dstep[j], 0, sizeof(dstep_t));
            if (BEAT[j] & 1u) dstep_set(&TDRUM->dstep[j], 0, LV_HARD, 0);
            if (BEAT[j] & 4u) dstep_set(&TDRUM->dstep[j], 2, LV_NORM, 0);
            if (BEAT[j] & 8u) dstep_set(&TDRUM->dstep[j], 3, LV_GHOST, 0);
            if (BEAT[j] & 0x10u) dstep_set(&TDRUM->dstep[j], 4, j % 4u ? LV_SOFT : LV_NORM, j == 14u ? 2u : 0u);
            if (BEAT[j] & 0x20u) dstep_set(&TDRUM->dstep[j], 5, LV_NORM, 0);
        }
        for (j = 0; j < 16u; j += 3u) { trk[0].step[j].n = 1; trk[0].step[j].note[0] = 36; trk[0].step[j].time = ST_NOTE; }
        for (j = 0; j < 16u; j += 4u) { trk[1].step[j].n = 3; trk[1].step[j].time = ST_NOTE; }
        drum_lane = 4; drum_cursor = 6; drum_page = 0; ui.force = 1; ui.msg_t = 0;
        drums.hits = 1u | 1u << 4; frame(); ppm("live-grid");
        drum_page = 1; ui.force = 1; drums.hits = 1u | 1u << 4; frame(); ppm("live-kit");
        drum_page = 0; song.sel = 1; go_home(); ui.force = 1;
        transport_req = 1; frames(30); song.rec = 2u; frame(); ui.force = 1; frame(); ppm("live-tracks");
        song.rec = 0; transport_req = 2; frames(2);
        song.sel = TRK_DRUM;
    }
    {   /* the free take screens */
        static track_t keep[NTRK];
        memcpy(keep, trk, sizeof keep);
        for (i = 0; i < NTRK; i++) steps_clear(&trk[i]);
        rec_tempo = 0; rec_count = 0;
        rec_wait = 1; ui.force = 1; frame(); ppm("live-rec-free");
        /* the REC screen's dials (2.3): KNOB 1 MODE, KNOB 2 LENGTH, KNOB 3 START */
        encs[panel.enc[EN_K1]] = 1; frame();
        check(rec_tempo == 1u && rec_wait, "REC screen, empty project: KNOB 1 -> MODE tempo");
        ui.force = 1; frame(); ppm("live-rec-tempo");
        trk[song.sel].p[P_SLEN] = 16;
        encs[panel.enc[EN_K2]] = 1; frame();
        encs[panel.enc[EN_K2]] = 1; frame();
        check(trk[song.sel].p[P_SLEN] == 64, "REC screen: KNOB 2 -> LENGTH 1, 2, 4 bars");
        encs[panel.enc[EN_K2]] = -1; frame();
        check(trk[song.sel].p[P_SLEN] == 32, "REC screen: KNOB 2 back -> 2 bars");
        encs[panel.enc[EN_K3]] = 1; frame();
        check(rec_count == 1u, "REC screen: KNOB 3 -> START count");
        ui.force = 1; frame(); ppm("live-rec-count");
        check((lights_word() >> 9 & 3u) == 3u, "REC screen: MODE and START saved with the settings");
        ci_on = 1; ci_beat = 1; ui.force = 1; frame(); ppm("live-rec-countin");
        encs[panel.enc[EN_K1]] = -1; frame();
        check(rec_tempo == 1u, "REC screen: the count-in running, the dials wait");
        ci_on = 0; ci_beat = 0;
        trk[1].step[0].time = ST_NOTE; trk[1].step[0].n = 1; trk[1].step[0].note[0] = 60;
        encs[panel.enc[EN_K1]] = -1; frame();
        check(rec_tempo == 1u, "REC screen, a project with notes: no MODE (always the tempo set)");
        ui.force = 1; frame(); ppm("live-rec-notes");
        steps_clear(&trk[1]);
        encs[panel.enc[EN_K3]] = -1; frame();
        encs[panel.enc[EN_K1]] = -1; frame();
        check(rec_tempo == 0u && rec_count == 0u, "REC screen: KNOB 3 / 1 back -> note, free");
        rec_count = 1; encs[panel.enc[EN_K3]] = -1; frame();
        check(rec_count == 1u, "REC screen, MODE free: no START (the take starts on the first note)");
        rec_count = 0;
        trk[song.sel].p[P_SLEN] = 16;
        rec_wait = 0; ft_on = 1; ft_t = (uint32_t)(5.4 * FS / CTL); ui.force = 1; ui_draw(); ppm("live-free-take");
        ft_on = 0; ft_t = 0; memcpy(trk, keep, sizeof keep);
        rec_wait = 0; frame();
    }
    for (i = 0; i < DRUM_KITS; i++) { TDRUM->p[P_E0] = (int16_t)i; ui.force = 1; drum_page = 1; frame(); }
    drum_page = 0;

    {   /* menu NOTES (PR #11 by @renebohne): sounding synth voices light their keys */
        song.sel = 0; go_home(); ui.force = 1; frame();
        trk[0].p[P_CHORD] = 0; trk[0].p[P_QUANT] = 0; trk[0].p[P_ROOT] = 0; trk[0].p[P_TRANS] = 0;
        song.octave = 0;
        lights_notes = 0;
        trk[0].v[0].active = 1; trk[0].v[0].gate = 1; trk[0].v[0].stage = 1; trk[0].v[0].note = 60;
        frame();
        check((keys_lit() & (1u << 7)) == 0, "NOTES off: a sounding synth voice lights no key");
        ui.menu = 1; ui.menu_sel = MI_NOTES; ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 1; frame();
        check(lights_notes == 1u, "menu NOTES: KNOB 1 right: ON");
        ui.force = 1; frame(); ppm("page-menu-notes");
        ui.menu = 0; ui.force = 1; go_home(); frame();
        check((keys_lit() & (1u << 7)) != 0, "NOTES on: a sounding synth voice lights key 7 (C4)");
        {   /* every screen and layer: lit where the keys are notes, a glow under the tiles */
            static const struct { uint32_t ly; int lit; const char *name; } L[] = {
                {LY_ERASE, 1, "EDIT erase"}, {LY_ROLL, 1, "ARP roll"}, {LY_SCALE, 1, "SCL key"}, {LY_SONG, 1, "SAVE song"},
                {LY_FX, 0, "FX punch"}, {LY_STEP, 0, "SEQ steps"}, {LY_MIX, 0, "GLO mix"}};
            uint32_t k, ok = 1, ly0 = ui.layer;
            char what[96];
            for (k = 0; k < sizeof L / sizeof L[0]; k++) {
                ui.layer = (uint8_t)L[k].ly;
                if (L[k].lit)
                    ok = (keys_lit() >> 7 & 1u) != 0;
                else
                    ok = (keys_notes_dim() >> 7 & 1u) != 0 && (keys_lit() >> 7 & 1u) == 0;
                snprintf(what, sizeof what, "NOTES on, %s layer: the sounding C4 %s", L[k].name, L[k].lit ? "lit" : "glows under the tiles");
                check(ok, what);
            }
            ui.layer = LY_SCALE;
            check((keys_notes_dim() & scale_keys(0)) == scale_keys(0), "NOTES on, SCL: the scale glows");
            lights_notes = 0;
            ui.layer = LY_ERASE;
            check((keys_lit() >> 7 & 1u) == 0 && keys_notes_dim() == 0u, "NOTES off, EDIT erase: as before (no note lights)");
            ui.layer = LY_SCALE;
            check(keys_notes_dim() == 0u, "NOTES off, SCL: as before");
            lights_notes = 1;
            ui.layer = (uint8_t)ly0;
        }
        song.octave = -1;
        frame();
        check((keys_lit() & (1u << 19)) != 0 && (keys_lit() & (1u << 7)) == 0, "NOTES on: the octave moves the lit key");
        song.octave = 0;
        trk[0].v[0].gate = 0; trk[0].v[0].stage = 3;
        frame();
        check((keys_lit() & (1u << 7)) == 0, "NOTES on: the release turns the key off");
        trk[0].v[0].active = 0;
        check((lights_word() >> 8 & 1u) == 1u, "NOTES: saved with the light settings");
        ui.menu = 1; ui.menu_sel = MI_USB; ui.force = 1; frame(); ppm("menu-usb");
        usb_full = 0;
        encs[panel.enc[EN_K1]] = 1; frame();
        check(usb_full == 1u && (lights_word() >> 11 & 1u) == 1u, "menu USB AUDIO: KNOB 1 -> FULL, saved with the settings");
        tap(B_OCTUP);
        check(usb_full == 0u && ui.menu == 1, "menu USB AUDIO: OCT+ toggles back to MASTER");
        ui.menu = 0; ui.force = 1; go_home(); frame();
        ui.menu = 1; ui.menu_sel = MI_NOTES; ui.force = 1; frame();
        tap(B_OCTUP);
        check(lights_notes == 0u && ui.menu == 1, "menu NOTES: OCT+ toggles it off");
        ui.menu = 0; ui.force = 1; go_home(); frame();
    }

    {   /* menu LIGHTS / KEYS: the backlight for playing in the dark */
        uint32_t m, nbits;
        go_home(); ui.force = 1; frame();
        check(lights_lvl == LIGHTS_OFF && fm1_led_bg_ns == 0u && lights_keys_mask() == 0u, "LIGHTS: off by default");
        ui.menu = 1; ui.menu_sel = MI_LIGHTS; ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 1; frame();
        check(lights_lvl == LIGHTS_LOW && fm1_led_bg_ns == 500u, "LIGHTS: KNOB 1 right: LOW (a 0.5 us pulse a frame)");
        encs[panel.enc[EN_K1]] = 1; frame();
        check(lights_lvl == LIGHTS_MID && fm1_led_bg_ns == 1000u, "LIGHTS: MID (1 us)");
        encs[panel.enc[EN_K1]] = 1; frame(); encs[panel.enc[EN_K1]] = 1; frame();
        check(lights_lvl == LIGHTS_HIGH && fm1_led_bg_ns == 2000u, "LIGHTS: HIGH (2 us, under the 4 us glow), and it stops there");
        ui.force = 1; frame(); ppm("page-menu-lights");
        encs[panel.enc[EN_K1]] = -1; frame();
        check(lights_lvl == LIGHTS_MID, "LIGHTS: KNOB 1 left: dimmer");
        tap(B_OCTUP);
        check(lights_lvl == LIGHTS_HIGH && ui.menu == 1, "LIGHTS: OCT+ steps round, the menu stays");
        tap(B_OCTUP);
        check(lights_lvl == LIGHTS_OFF && fm1_led_bg_ns == 0u, "LIGHTS: ... back to OFF");
        ui.menu_sel = MI_KEYS; ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 1; frame();
        m = lights_keys_mask();
        check(lights_keys == KEYS_C && lights_lvl == LIGHTS_LOW && m == (1u << 7 | 1u << 19),
              "KEYS: C KEYS (C4, C5), LIGHTS raised to LOW");
        encs[panel.enc[EN_K1]] = 1; frame(); encs[panel.enc[EN_K1]] = 1; frame();
        for (m = lights_keys_mask(), nbits = 0; m; m &= m - 1u)
            nbits++;
        check(lights_keys == KEYS_WHITE && nbits == 16u && !(lights_keys_mask() & (1u << 1)),
              "KEYS: WHITE KEYS (16 of 27, F#3 dark), and it stops there");
        lights_lvl = LIGHTS_OFF; frame();
        check(lights_keys_mask() == 0u && fm1_led_bg_ns == 0u, "KEYS: nothing lit while LIGHTS is OFF");
        lights_keys = KEYS_OFF;
        ui.menu = 0; ui.force = 1; go_home(); frame();
    }

    {   /* FLOYD engine: 7 EDIT pages, per-operator ADSR, levels, ratios */
        song.sel = 0;
        uint32_t floyd_idx = 0;
        for (i = 0; i < NENGINES; i++) if (ENGINES[i] == &ENG_FLOYD) floyd_idx = i;
        trk[0].eng_req = (uint8_t)floyd_idx;
        trk[0].engine = (uint8_t)floyd_idx;
        ui.home = 1; open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-1-algo");
        check(strcmp(cur_page()->title, "ALGO") == 0, "FLOYD page 1: ALGO");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-2-op1");
        check(strcmp(cur_page()->title, "OP1") == 0, "FLOYD page 2: OP1");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-3-op2");
        check(strcmp(cur_page()->title, "OP2") == 0, "FLOYD page 3: OP2");
        encs[panel.enc[EN_K1]] = 10; frame();
        check(floyd_state[0].atk[1] > 5, "FLOYD OP2 ATK knob turned");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-4-op3");
        check(strcmp(cur_page()->title, "OP3") == 0, "FLOYD page 4: OP3");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-5-op4");
        check(strcmp(cur_page()->title, "OP4") == 0, "FLOYD page 5: OP4");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-6-level");
        check(strcmp(cur_page()->title, "LEVEL") == 0, "FLOYD page 6: LEVEL");
        open_family(FAM_EDIT); ui.force = 1; frame(); ppm("floyd-7-ratio");
        check(strcmp(cur_page()->title, "RATIO") == 0, "FLOYD page 7: RATIO");

        /* Live note tracer screenshot */
        trk[0].v[0].active = 1;
        trk[0].v[0].gate = 1;
        trk[0].v[0].stage = 2;
        trk[0].v[0].env_out = 18000;
        trk[0].v[0].s[0] = 1 << 23; /* OP2 env level */
        trk[0].v[0].s[3] = 2;        /* OP2 decay stage */
        open_family(FAM_EDIT); open_family(FAM_EDIT); open_family(FAM_EDIT); /* OP2 page */
        ui.force = 1; frame(); ppm("floyd-live-tracer");
        trk[0].v[0].active = 0; trk[0].v[0].gate = 0;

        open_family(FAM_EDIT); open_family(FAM_EDIT); open_family(FAM_EDIT); open_family(FAM_EDIT); open_family(FAM_EDIT);
        check(strcmp(cur_page()->title, "ALGO") == 0, "FLOYD cycles back to ALGO (7 pages total)");

        /* Non-Floyd engine: standard 4 pages */
        trk[0].eng_req = 0; /* ANALOG */
        trk[0].engine = 0;
        ui.home = 1; open_family(FAM_EDIT); frame();
        check(strcmp(cur_page()->title, "EDIT 1") == 0, "ANALOG page 1: EDIT 1");
        open_family(FAM_EDIT); frame();
        check(strcmp(cur_page()->title, "EDIT 2") == 0, "ANALOG page 2: EDIT 2");
        open_family(FAM_EDIT); frame();
        check(strcmp(cur_page()->title, "VOICE") == 0, "ANALOG page 3: VOICE");
        open_family(FAM_EDIT); frame();
        check(strcmp(cur_page()->title, "VOICE 2") == 0, "ANALOG page 4: VOICE 2");
        open_family(FAM_EDIT); frame();
        check(strcmp(cur_page()->title, "EDIT 1") == 0, "ANALOG cycles back to EDIT 1 (4 pages total)");
    }

    {   /* fuzz: 20000 frames of random buttons (held or tapped), knobs and keys, with the audio running
         * between frames; every draw stays on the screen (lcd_blit / lcd_fill assert it) */
        uint32_t f, seed = 777, held = 0;
        #define R(n) ((seed = seed * 1664525u + 1013904223u) >> 8) % (n)
        song.sel = 0; go_home(); ui.force = 1;
        for (f = 0; f < 20000u; f++) {
            if (R(6) == 0) {
                uint32_t bt = R(NB);
                edges_btn |= 1u << panel.btn[bt];
                if (bt != B_HOME && R(3) == 0) held ^= 1u << panel.btn[bt];
            }
            if (R(30) == 0) held = 0;
            if (R(700) == 0) ly_lock = (uint8_t)R(LY_COUNT);              /* (a layer locked open, at random) */
            fm1_in.buttons = held & ~(1u << panel.btn[B_HOME]);
            if (R(4) == 0) encs[R(7)] += (int32_t)R(5) - 2;
            fm1_in.notes = R(10) == 0 ? (1u << R(27)) : (R(3) ? fm1_in.notes : 0);
            frame();
            if (ui.menu) { ui.menu = 0; ui.force = 1; }               /* (the menu is tested above) */
        }
        fm1_in.buttons = 0; fm1_in.notes = 0; frames(4);
        #undef R
    }
    printf("ui: %s\n", fails ? "FAILED" : "pages, layers (punch, steps, erase, roll, key, mix), layer lock, song layer, REC hold, drums, REC, 20000-frame fuzz PASS");
    return fails;
}
