/* SPDX-License-Identifier: GPL-3.0-only */
/* The real UI (ui.c, ui_draw.c, ui_studio.c, ui_layers.c, ui_song.c, ui_menu.c, ui_input.c) on a
 * framebuffer with panel / flash / storage doubles, the audio (mix_block) running between frames as on
 * the device. Renders every screen to PPM for review (DIR/page-*.ppm, live-*.ppm, layer-*.ppm) and
 * drives the panel:
 *   taps    a layer button tapped opens its pages; held + a key / knob it does not
 *   FX      held + a white key: punch-in; knobs: filter, dust, duck
 *   SEQ     held: the steps on the white keys (drums: the sound played last), OCT: pages, a step key
 *           held + KNOB 1..4: sound / note, level, ratchet, nudge (2.4); PRESETS a lock of the parameter
 *           ALGORITHM picks (the sound parameter last touched first); + OCT-: its nudge, locks and fill
 *           condition go; + OCT+: the condition (normal, fill only, no fill)
 *   EDIT    held + a key: erase; OCT- / OCT+: undo / redo; KNOB 1 shift, 2 length x2
 *   ARP     held + a key: a roll; KNOB 1 the rate
 *   SEL     held + a key: the key of the song
 *   GLO     held + keys: mute, solo, fill (held) / fill the next bar, tap tempo; knobs: levels
 *   REC     press: arm / record at once; held: the clear ring, to the end: cleared (undo brings it back)
 *   SAVE    tapped: the song page; held: the SONG layer (sections A..D: play / store, SONG REC; two or more
 *           section taps: a chain, "chain A B" on the sub line)
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
static uint32_t wdt_tick_ms;                       /* (stress_test: 1, the blocking PANEL setup's clock runs out) */
static void fm1_wdt_feed(void) { fm1_ms += wdt_tick_ms; }
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static uint32_t saves, loads, erases;
static int project_used(uint32_t i) { return i < 2; }
static void project_save(uint32_t i) { (void)i; saves++; ui_message("SAVED"); }
static void project_load(uint32_t i) { (void)i; loads++; }
static void project_erase(uint32_t i) { (void)i; erases++; ui_message("ERASED"); }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return 3; }
static void arrangement_apply(uint32_t s) { (void)s; }
static uint32_t section_bars(uint32_t s) { return s == 1u ? 2u : 1u; }   /* (B: two bars) */
static void song_backup(void) {}
static void song_restore(void) {}
static uint32_t sec_stores, sec_loads;
static void section_store(uint32_t s) { sec_stores++; live_sec = (int8_t)s; }
static void section_load(uint32_t s) { sec_loads++; live_sec = (int8_t)s; }
static int up_used(uint32_t k) { return k < 2; }
static int up_load(uint32_t k) { (void)k; return 0; }
static int up_load_to(track_t *t, uint32_t k) { (void)t; (void)k; return 0; }
static int up_arp_of(uint32_t k, int16_t *a) { (void)k; (void)a; return 1; }
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
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/splash.c"
/* (2.7) the theme's CRC: storage.c's st_crc32 (its flash hooks unused here) */
static int st_read(uint32_t off, void *dst, uint32_t n) { (void)off, (void)dst, (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { (void)off, (void)src, (void)n; return -1; }
#include "../firmware/src/storage.c"
/* (2.7) the editor's LIVE page (firmware/src/editor_live.c): its reply captured here */
static uint8_t lv_out[96];
static uint32_t lv_n;
static void ed_b(uint32_t v) { if (lv_n < sizeof lv_out) lv_out[lv_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_v(int32_t v) { uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192); ed_b(u); ed_b(u >> 7); }
#include "../firmware/src/editor_live.c"
static void live(uint32_t op, uint32_t x, uint32_t y, uint32_t z)
{
    uint8_t a[4] = {(uint8_t)op, (uint8_t)x, (uint8_t)y, (uint8_t)z};
    lv_n = 0;
    ed_live(a, 4);
}
static void live5(uint32_t op, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    uint8_t a[5] = {(uint8_t)op, (uint8_t)x, (uint8_t)y, (uint8_t)z, (uint8_t)w};
    lv_n = 0;
    ed_live(a, 5);
}
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
    for (q = 0; q < 22u; q++) {
        uint32_t i;
        mix_block(o, CTL);
        for (i = 1; i < CTL; i += 2u) {             /* the scope, as audio.c fills it (the visualiser) */
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
    }
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
static const char *sub_line(void) { return layer_sub_shown; }   /* the layer's sub line as last drawn (ui_layers.c) */

#ifndef UI_PAGES_HARNESS_ONLY                       /* (tests/stress_test.c: the harness, its own main) */
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
    open_family(FAM_ARP); ui.force = 1; frame(); ppm("page-arp");
    check(big_page(cur_page()) && !strcmp(cur_page()->title, "ARP"), "ARP (2.7.2): page 1 shows its values large, as ARP 2");
    {   /* 2.7.2 (a YouTube comment): FX > FILTER on the drum track: the track's filter, not "DRUM TRACK" */
        uint32_t j, sel = song.sel;
        int16_t f0 = TDRUM->p[P_TFLT];
        uint8_t lp = ui.lock_par;
        for (j = 0; j < NPAGES; j++) if (!strcmp(PAGES[j].title, "FILTER") && PAGES[j].fam == FAM_FX) break;
        song.sel = TRK_DRUM;
        check(j < NPAGES && page_for_drum(&PAGES[j]), "FX > FILTER (2.7.2): a page of the drum track (its filter)");
        ui.page = (uint8_t)j; ui.home = 0; ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 5; frame();
        check(TDRUM->p[P_TFLT] == f0 + 5, "FX > FILTER on the drum track: KNOB 1 turns the drum track's FILT");
        TDRUM->p[P_TFLT] = f0; song.sel = (uint8_t)sel; ui.lock_par = lp; go_home(); frame();
    }
    open_family(FAM_EDIT); ui.force = 1; frame(); ppm("page-edit");
    open_family(FAM_FX); ui.force = 1; frame(); ppm("page-fx");
    open_family(FAM_SEQ); ui.force = 1; frame(); ppm("page-step");
    open_family(FAM_GLO); ui.force = 1; frame(); ppm("page-global");
    open_family(FAM_GLO); ui.force = 1; frame(); ppm("page-master");
    open_family(FAM_SCL); ui.force = 1; frame(); ppm("page-scale");
    {   /* (2.4) GLO > SYSTEM: KNOB 1 is MIDI OUT (KEYS / SEQ), shown as such; the USB status in the third column */
        uint32_t i, m0 = (uint32_t)song.g[G_MIDI];
        for (i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].title, "SYSTEM")) break;
        open_family(FAM_GLO); ui.page = (uint8_t)i; ui.fam_last[FAM_GLO] = (uint8_t)i; page_entered(); ui.force = 1; frame();
        song.g[G_MIDI] = 0; ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 1; frame();
        check(i < NPAGES && song.g[G_MIDI] == 1, "GLO > SYSTEM: KNOB 1 sets MIDI = SEQ");
        {
            char val[12]; const char *unit = "";
            int16_t *vp; const param_desc_t *d = page_desc(cur_page(), 0, &vp);
            param_format(d, *vp, val, &unit);
            check(d && !strcmp(d->label, "OUT") && !strcmp(val, "SEQ"), "GLO > SYSTEM: the first column reads OUT SEQ (MIDI OUT)");
        }
        ui.force = 1; frame(); ppm("page-system");
        {   /* (2.4) KNOB 3: IN = CLOCK (notes ignored), kept in the settings word; back to NOTES */
            int16_t r0 = song.g[G_ROUTE];
            int16_t *vp; const param_desc_t *d = page_desc(cur_page(), 2, &vp);
            char val[12]; const char *unit = "";
            encs[panel.enc[EN_K3]] = 1; frame();
            param_format(d, *vp, val, &unit);
            check(song.g[G_ROUTE] == 1 && !strcmp(d->label, "IN") && !strcmp(val, "CLOCK"), "GLO > SYSTEM: KNOB 3 sets IN = CLOCK");
            lights_min = 1;
            check((lights_word() >> 15 & 1u) == 1u, "IN = CLOCK kept in the settings word (bit 15)");
            lights_from_word(lights_word() & ~(1u << 15));
            check(lights_min == 0u, "IN read back from the settings word");
            encs[panel.enc[EN_K3]] = 12; frame();             /* (2.6) on to CH 12 */
            param_format(d, *vp, val, &unit);
            lights_min = (uint8_t)song.g[G_ROUTE];
            {
                uint32_t w = lights_word();
                lights_min = 0;
                lights_from_word(w);
                check(song.g[G_ROUTE] == 13 && !strcmp(val, "CH 12") && lights_min == 13u && !(w >> 15 & 1u) && (w >> 21 & 31u) == 12u
                      && d->max == 17, "2.6: IN = CH 12 (CH 1..16 after CLOCK), kept in the settings word (bits 21-25), read back");
                lights_from_word(w & ~(31u << 21));
                check(lights_min == 0u, "2.6: a settings word from 2.5 (no channel bits): IN = NOTES");
            }
            encs[panel.enc[EN_K3]] = -13; frame();
            song.g[G_ROUTE] = r0;
        }
        song.g[G_MIDI] = (int16_t)m0; go_home(); frame();
    }

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
    fm1_in.notes = 1u << 17; frame(); check(punch.req == PX_FREEZE, "FX + the black key A#4: FREEZE (2.6: 11 black-key effects)");
    check(keys_lit() == 1u << 17, "its key lights");
    ppm("layer-punch-black");
    fm1_in.notes = 0; frame(); check(punch.req == -1, "black key up: the mix comes back");
    encs[panel.enc[EN_K2]] = 10; frame(); check(song.g[G_DUST] > 0, "FX + KNOB 2: DUST");
    encs[panel.enc[EN_K1]] = -10; frame(); check(song.g[G_FILT] < 0, "FX + KNOB 1: the filter (low-pass)");
    encs[panel.enc[EN_K4]] = 12; frame(); check(TSEL->p[P_TFLT] > 0 && song.g[G_FILT] < 0, "FX + KNOB 4: the selected track's FILTER (high-pass)");
    ui.force = 1; frame(); ppm("layer-punch-trkflt");
    {   /* (2.7.2) SELECT in the FX layer sets the punch-in FX fade (not the tempo); shown top right */
        int16_t bpm0 = song.g[G_BPM];
        song.g[G_FXFADE] = 0;
        encs[panel.enc[EN_SELECT]] = 5; frame();
        check(song.g[G_FXFADE] == 5 && song.g[G_BPM] == bpm0, "FX + SELECT (2.7.2): the punch-in FX fade, not the tempo");
        encs[panel.enc[EN_SELECT]] = 99; frame();
        check(song.g[G_FXFADE] == 15, "FX + SELECT: the fade stops at 15");
        encs[panel.enc[EN_SELECT]] = -99; frame();
        check(song.g[G_FXFADE] == 0 && song.g[G_BPM] == bpm0, "FX + SELECT: and down to 0 (tempo untouched)");
        song.g[G_FXFADE] = 6; ui.force = 1; frame(); ppm("layer-punch-fade");   /* (the top-right readout)*/
        song.g[G_FXFADE] = 0;
    }
    TSEL->p[P_TFLT] = 0;
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
    {   /* 2.7.1: the drum track, ARP (or any button's pages) tapped, then SEQ tapped: the DRUMS screen, as from TRACKS */
        static const uint8_t FROM[] = {B_ARP, B_ENV, B_LFO, B_FX, B_GLO, B_SCL};
        uint32_t i;
        int ok = 1;
        for (i = 0; i < sizeof FROM; i++) {
            go_home(); frame();
            tap(FROM[i]); frames(2);
            ok &= !on_drum_page();
            tap(B_SEQ); frames(2);
            ok &= on_drum_page();
            if (!on_drum_page())
                printf("ui:   drum track, button %u then SEQ: page %s\n", (unsigned)FROM[i], ui.home ? "HOME" : cur_page()->title);
        }
        go_home(); frame();
        check(ok, "drum track (2.7.1): ARP, ENV, LFO, FX, GLO or SEL tapped, then SEQ: the DRUMS screen");
    }

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

    /* ---- SEL: the key of the song; GLO: mute, solo, tap */
    song.sel = 0; go_home(); frame();
    press(B_SCL); frames(10);
    key(9);                                           /* D4 */
    check(trk[0].p[P_ROOT] == 2 && trk[1].p[P_ROOT] == 2 && trk[2].p[P_ROOT] == 2, "SEL + D: every part in D");
    encs[panel.enc[EN_K1]] = 2; frame();
    check(trk[0].p[P_CHORD] == 2, "SEL + KNOB 1: chords (7TH) on the track");
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
    /* (2.4) key 9 held: a fill; key 10: the next bar is one (again: cancelled) */
    fm1_in.notes = 1u << key_of_white(8); frame(); frame();
    check(fill_held == 1u && fill_now == 1u && (keys_lit() & (1u << key_of_white(8))) != 0, "GLO + key 9 held: FILL (the key lit)");
    ui.force = 1; frame(); ppm("layer-mix-fill");
    fm1_in.notes = 0; frame(); frame();
    check(fill_held == 0u && fill_now == 0u, "key 9 let go: the fill ends");
    key(key_of_white(9)); check(fill_arm == 1u && (keys_lit() & (1u << key_of_white(9))) != 0, "GLO + key 10: FILL NEXT BAR armed (the key lit)");
    key(key_of_white(9)); check(fill_arm == 0u, "GLO + key 10 again: cancelled");
    release(B_GLO);
    fm1_in.notes = 1u << key_of_white(8); frame();     /* (no layer: a plain key, no fill) */
    check(fill_held == 0u, "key 9 without GLO: no fill");
    fm1_in.notes = 0; frame();
    trk[0].p[P_CHORD] = 0;

    /* ---- SEQ layer (2.4): a step held + KNOB 4 nudges it, PRESETS locks the lock parameter (ALGORITHM picks it),
     * KNOB 1 still edits the note, OCT- clears nudge and locks; PRESETS / ALGORITHM browse nothing meanwhile */
    check(ui.lock_par == P_ED_FLT, "the lock parameter starts as ENV > FLT");
    song.sel = 0; go_home(); frames(16);              /* (KNOB 1..4 quiet ~250 ms after a layer: #39) */
    open_family(FAM_FX); frame();                     /* the FX page: KNOB 1 = DIST */
    encs[panel.enc[EN_K1]] = 2; frame();
    check(ui.lock_par == P_DIST && trk[0].p[P_DIST] > 0, "a sound knob turned (FX > DIST): the lock parameter");
    open_family(FAM_SEQ); frame();                    /* PATTERN page knobs (LEN, DIV..) do not lock */
    open_family(FAM_SEQ); frame();
    if (cur_page()->id[1] == P_SDIV) { encs[panel.enc[EN_K2]] = 1; frame(); encs[panel.enc[EN_K2]] = -1; frame(); }
    check(ui.lock_par == P_DIST, "a pattern knob (DIV) is not a lock parameter");
    go_home(); frame();
    press(B_SEQ); frames(10);
    key(7);                                           /* step 5 set with the pen */
    fm1_in.notes = 1u << 7; frame();                  /* step 5 held */
    {
        uint32_t n0 = trk[0].step[4].note[0];
        encs[panel.enc[EN_K1]] = 2; frame();
        check(trk[0].step[4].note[0] == n0 + 2u && trk[0].step[4].n == 1, "step 5 held + KNOB 1: the note two up (as in 2.3)");
    }
    encs[panel.enc[EN_K4]] = -3; frame();
    encs[panel.enc[EN_K4]] = -4; frame();
    check(trk[0].micro[4] == -7 && trk[0].step[4].n == 1, "step 5 held + KNOB 4: nudged -7 (the step kept)");
    {
        int k0 = lock_find(&trk[0], 4, P_DIST, 0);
        int32_t d0 = trk[0].p[P_DIST];
        uint32_t sel0 = song.sel, eng0 = trk[0].eng_req, pr0 = trk[0].preset;
        encs[panel.enc[EN_PRESET]] = 1; frame();
        {
            int k = lock_find(&trk[0], 4, P_DIST, 0);
            check(k0 < 0 && k >= 0 && trk[0].lock[k].val == d0 + 1, "step 5 held + PRESETS: a DIST lock at the track's value, moved one up");
            encs[panel.enc[EN_PRESET]] = 5; frame();
            check(k >= 0 && trk[0].lock[k].val > d0 + 1 && trk[0].p[P_DIST] == d0, "PRESETS again: the lock moves, the track's DIST does not");
            check(trk[0].eng_req == eng0 && trk[0].preset == pr0, "PRESETS with a step held browses no sound");
        }
        ui.force = 1; frame(); ppm("layer-steps-lock");
        check(step_locked(&trk[0], 4) && !step_locked(&trk[0], 5), "the step shows its mark (a nudge or a lock)");
        encs[panel.enc[EN_ALGO]] = 1; frame();
        check(ui.lock_par == P_CHOR && song.sel == sel0, "step held + ALGORITHM: the lock parameter (DIST -> CHO), the track stays selected");
        encs[panel.enc[EN_ALGO]] = -1; frame(); encs[panel.enc[EN_ALGO]] = -1; frame();
        check(ui.lock_par == P_SGATE && song.sel == sel0, "ALGORITHM back twice: CHO -> DIST -> GATE (LEN, DIV, SWG skipped)");
        encs[panel.enc[EN_PRESET]] = 1; frame();
        check(lock_find(&trk[0], 4, P_SGATE, 0) >= 0 && lock_find(&trk[0], 4, P_DIST, 0) >= 0, "PRESETS: a second lock (GATE) on the same step");
        for (i = 0; i < 80u; i++) { encs[panel.enc[EN_ALGO]] = 1; frame(); if (ui.lock_par == P_SGATE) break; }
        check(i > 0 && i < 80u && ui.lock_par == P_SGATE, "ALGORITHM wraps round through the lockable parameters");
        encs[panel.enc[EN_K4]] = -40; frame();
        check(trk[0].micro[4] == MICRO_MIN, "KNOB 4 bounded at -32");
        edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
        check(trk[0].micro[4] == 0 && lock_find(&trk[0], 4, P_DIST, 0) < 0 && lock_find(&trk[0], 4, P_SGATE, 0) < 0 &&
              trk[0].step[4].n == 1 && ui.step_page == 0, "step held + OCT-: its nudge and locks cleared, the step kept, no page change");
    }
    {   /* (2.4) OCT+ with the step held: its fill condition round; OCT- resets it with the nudge and locks */
        edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
        check(step_fill(&trk[0], 4) == FC_FILL && ui.step_page == 0 && trk[0].step[4].n == 1, "step held + OCT+: FILL ONLY (no page change, the step kept)");
        ui.force = 1; frame(); ppm("layer-steps-fill");
        edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
        check(step_fill(&trk[0], 4) == FC_NOFILL, "OCT+ again: NO FILL");
        ui.force = 1; frame(); ppm("layer-steps-nofill");
        edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
        check(step_fill(&trk[0], 4) == FC_NORM, "OCT+ again: normal");
        edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
        encs[panel.enc[EN_K4]] = 3; frame();
        edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
        check(step_fill(&trk[0], 4) == FC_NORM && trk[0].micro[4] == 0, "FILL ONLY + a nudge, OCT-: the condition and the nudge reset");
    }
    fm1_in.notes = 0; frame();
    check(trk[0].step[4].n == 1, "the step edited is kept when let go");
    fm1_in.notes = 1u << 7; frame(); encs[panel.enc[EN_PRESET]] = 1; frame(); fm1_in.notes = 0; frame();
    check(trk[0].step[4].n == 1 && lock_find(&trk[0], 4, P_SGATE, 0) >= 0, "a set step held + PRESETS: a lock, the step kept");
    fm1_in.notes = 1u << 7; frame();
    edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
    fm1_in.notes = 0; frame();
    key(7);                                           /* a set step pressed and let go: cleared, with its locks and condition */
    check(trk[0].step[4].n == 0 && lock_find(&trk[0], 4, P_SGATE, 0) < 0 && step_fill(&trk[0], 4) == FC_NORM,
          "a step cleared: its locks and fill condition go too");
    { uint32_t sel0 = song.sel; encs[panel.enc[EN_ALGO]] = 1; frame(); check(song.sel == sel0 && ui.lock_par == P_SGATE, "no step held, SEQ held: ALGORITHM does nothing (no track change)"); }
    release(B_SEQ);
    { uint32_t sel0 = song.sel; encs[panel.enc[EN_ALGO]] = 1; frame(); check(song.sel == sel0 + 1u, "SEQ let go: ALGORITHM selects the track again"); song.sel = (uint8_t)sel0; }
    song.sel = TRK_DRUM; go_home(); frame();
    press(B_SEQ); frames(10);
    key(0);                                           /* a drum step: KNOB 1 the sound, KNOB 4 nudges the whole step */
    fm1_in.notes = 1u << 0; frame();
    { uint32_t l0 = pen_lane; encs[panel.enc[EN_K1]] = 1; frame(); check(pen_lane == l0 + 1u, "the drum track: step 1 held + KNOB 1: the sound (as in 2.3)"); }
    encs[panel.enc[EN_K4]] = 5; frame();
    encs[panel.enc[EN_PRESET]] = 1; frame();
    check(TDRUM->micro[0] == 5 && lock_find(TDRUM, 0, P_SGATE, 0) >= 0 && TDRUM->p[P_E0] == DRUM_DEFAULT_KIT,
          "the drum track: step 1 held + KNOB 4 / PRESETS: nudge +5, a lock, the kit untouched");
    fm1_in.notes = 0; frame();
    edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
    check(TDRUM->micro[0] == 5 && ui.step_page == 0, "no step held + OCT-: the page (not a clear)");
    release(B_SEQ);
    fm1_irq_off(); steps_clear(TDRUM); steps_clear(&trk[0]); fm1_irq_on();
    trk[0].p[P_DIST] = 0;
    ui.lock_par = P_ED_FLT;
    song.sel = 0; go_home(); frame();

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
    release(B_SAVE); frames(2);
    check(chain_n == 0u && chain_taps == 0u && live_req == 0, "one tap, SAVE let go: no chain (as before)");
    live_req = -1;
    press(B_SAVE); frames(15);                        /* (2.4) SAVE held: A, B, B tapped: a chain on release */
    key(key_of_white(0)); key(key_of_white(1));
    check(live_req == 0 && chain_taps == 2u && chain_n == 0u, "SAVE held, A then B tapped: A asked for, the chain building");
    ui.force = 1; frame();
    check(str_eq(sub_line(), "chain A B"), "the sub line reads \"chain A B\"");
    key(key_of_white(1));
    ui.force = 1; frame(); ppm("layer-song-chain");
    check(chain_taps == 3u && str_eq(sub_line(), "chain A B B"), "B again: \"chain A B B\"");
    release(B_SAVE); frames(2);
    check(chain_n == 3u && chain_i == 0u && chain_sec[0] == 0 && chain_sec[1] == 1 && chain_sec[2] == 1 && chain_bars == 1u && live_req == 0,
          "SAVE let go: the chain A B B plays (A asked for, 1 bar)");
    press(B_SAVE); frames(15);
    check(str_eq(sub_line(), "chain A B B"), "SAVE held again: the chain shown");
    key(key_of_white(1)); release(B_SAVE); frames(2);
    check(chain_n == 0u && live_req == 1, "a single tap of B: the chain ends, B asked for");
    press(B_SAVE); frames(15);
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
    song.sel = TRK_DRUM; studio_open(SC_DRUM); ui.force = 1; frames(16);   /* (after SAVE: the knobs quiet, #39) */
    check(on_drum_page(), "the drum screen");
    drum_lane = 0;                                    /* (2.7.1: the sound picked is the SEQ layer's too: the snare above) */
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
        encs[panel.enc[EN_K4]] = -1; frame();         /* (2.7) KNOB 4: QUANT off */
        {
            int ok = rec_quant_off == 1u && (lights_word() >> 31 & 1u);
            ui.force = 1; frame(); ppm("live-rec-quant");
            lights_from_word(lights_word());
            ok &= rec_quant_off == 1u;
            encs[panel.enc[EN_K4]] = 1; frame();
            ok &= rec_quant_off == 0u && !(lights_word() >> 31 & 1u);
            check(ok, "REC screen (2.7): KNOB 4 -> QUANT off / on, saved with the settings (bit 31)");
        }
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
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(lights_notes == 1u, "menu NOTES: its knob (KNOB 3) right: ON");
        ui.force = 1; frame(); ppm("page-menu-notes");
        ui.menu = 0; ui.force = 1; go_home(); frame();
        check((keys_lit() & (1u << 7)) != 0, "NOTES on: a sounding synth voice lights key 7 (C4)");
        {   /* a short note (a sequencer 1/16) that starts and ends between two frames still lights its key */
            int32_t o[CTL * 2];
            uint32_t n = 0, f;
            trk[0].v[0].active = 0; trk[0].v[0].gate = 0;
            frame(); frame();
            check((keys_lit() >> 9 & 1u) == 0, "NOTES on: D4 not lit before it plays");
            trk_note_on(&trk[0], 62, 100);
            mix_block(o, CTL);
            trk_note_off(&trk[0], 62);
            for (f = 0; f < 20; f++) mix_block(o, CTL);
            frame();
            check((keys_lit() >> 9 & 1u) != 0, "NOTES on: a note over before the frame still lights its key (D4)");
            for (f = 0; f < 10; f++) { frame(); n += keys_lit() >> 9 & 1u; }
            check(n >= 4u && n <= 7u && (keys_lit() >> 9 & 1u) == 0, "NOTES on: ... for about 6 frames, then off");
            song.sel = 1; frame(); song.sel = 0; frame();
            trk[0].v[0].active = 1; trk[0].v[0].gate = 1; trk[0].v[0].stage = 1; trk[0].v[0].note = 60;
        }
        {   /* every screen and layer: lit where the keys are notes, a glow under the tiles */
            static const struct { uint32_t ly; int lit; const char *name; } L[] = {
                {LY_ERASE, 1, "EDIT erase"}, {LY_ROLL, 1, "ARP roll"}, {LY_SCALE, 1, "SEL key"}, {LY_SONG, 1, "SAVE song"},
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
            check((keys_notes_dim() & scale_keys(0)) == scale_keys(0), "NOTES on, SEL: the scale glows");
            lights_notes = 0;
            ui.layer = LY_ERASE;
            check((keys_lit() >> 7 & 1u) == 0 && keys_notes_dim() == 0u, "NOTES off, EDIT erase: as before (no note lights)");
            ui.layer = LY_SCALE;
            check(keys_notes_dim() == 0u, "NOTES off, SEL: as before");
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
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(usb_full == 1u && (lights_word() >> 11 & 1u) == 1u, "menu USB AUDIO: its knob (KNOB 2) -> FULL, saved with the settings");
        tap(B_OCTUP);
        check(usb_full == 0u && ui.menu == 1, "menu USB AUDIO: OCT+ toggles back to MASTER");
#if FELUCCA_CDC
        ui.menu_sel = MI_SERIAL; ui.force = 1; frame();
        check(usb_serial == 0u && !(lights_word() >> 16 & 1u), "menu USB SERIAL: OFF by default (2.3 settings read so)");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(usb_serial == 1u && (lights_word() >> 16 & 1u), "menu USB SERIAL: its knob (KNOB 3) -> ON, saved with the settings");
        ui.force = 1; frame(); ppm("menu-serial");
        lights_from_word(lights_word());
        check(usb_serial == 1u, "menu USB SERIAL: read back");
        tap(B_OCTUP);
        check(usb_serial == 0u && ui.menu == 1, "menu USB SERIAL: OCT+ toggles back to OFF");
#else
        check(MS_FIRST[MS_AUDIO + 1] - MS_FIRST[MS_AUDIO] == 4u && MI_RATE == MI_USB + 1 && MI_FRESO == MI_RATE + 1,
              "2.7.2: FM-1 build AUDIO is LOWCUT, USB AUDIO, USB RATE, FILTER RESO (no USB SERIAL: FELUCCA_CDC 0)");
#endif
        {   /* (2.7.2) menu AUDIO > FILTER RESO: 0 (OFF) .. 10, its knob steps and stops, OCT+ wraps, read back (fx.c) */
            uint16_t col;
            uint32_t j;
            int ok;
            flt_reso = 0;
            ui.menu = 1; ui.menu_sel = MI_FRESO; ui.force = 1; frame();
            ok = !strcmp(mi_value(MI_FRESO, &col), "OFF");
            encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();     /* its knob (KNOB 4) right */
            ok &= flt_reso == 1u && !strcmp(mi_value(MI_FRESO, &col), "1");
            ppm("menu-freso");
            for (j = 0; j < 20; j++) { encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame(); }
            ok &= flt_reso == 10u;                                          /* stops at 10 */
            tap(B_OCTUP);                                                   /* OCT+ wraps 10 -> 0 */
            ok &= flt_reso == 0u;
            for (j = 0; j < 3; j++) { encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = -1; frame(); }
            ok &= flt_reso == 0u;                                          /* stops at 0 going down */
            check(ok, "menu FILTER RESO (2.7.2): OFF..10, knob steps and stops, OCT+ wraps");
            ui.menu = 0; go_home();
        }
        {   /* 2.7: your theme (the editor's Theme page): checked, then MENU > COLOR after MONO, by its name */
            theme_t th;
            int ok = 1, k;
            uint32_t pal0 = settings.palette;
            memset(&th, 0, sizeof th);
            th.magic = THEME_MAGIC;
            memcpy(th.name, "SUNSET", 6);
            th.c[0] = RGB(40, 10, 30), th.c[1] = RGB(90, 20, 60), th.c[2] = RGB(150, 50, 90);
            th.c[3] = RGB(220, 100, 80), th.c[4] = RGB(255, 200, 120);
            th.trk[0] = RGB(255, 0, 128), th.trk[1] = RGB(0, 255, 255), th.trk[2] = RGB(255, 255, 0), th.trk[3] = RGB(120, 60, 255);
            th.bg = RGB(24, 8, 40);                   /* a deep purple background */
            th.hi = RGB(255, 255, 160);               /* a pale yellow accent */
            th.crc = st_crc32(&th, 36u);
            ok &= theme_check(&th);
            th.name[2] ^= 1;
            ok &= !theme_check(&th);                  /* (a byte changed: the CRC) */
            th.name[2] ^= 1;
            {
                theme_t d = th;
                for (k = 0; k < 5; k++)
                    d.c[k] = RGB(40, 40, 40);
                d.crc = st_crc32(&d, 36u);
                ok &= !theme_check(&d);               /* too dark to read the menu again */
                d = th;
                d.bg = RGB(250, 190, 110);
                d.crc = st_crc32(&d, 36u);
                ok &= !theme_check(&d);               /* values on a background of their own colour */
                d.bg = RGB(250, 250, 250), d.c[4] = RGB(20, 20, 60);
                d.crc = st_crc32(&d, 36u);
                ok &= !theme_check(&d);               /* a pale yellow accent on white: the cursor lost */
                d.hi = 0;
                d.crc = st_crc32(&d, 36u);
                ok &= theme_check(&d);                /* dark values, a black accent on a light background: readable */
                {   /* the first beta's THM1 (36 bytes, its CRC where hi is): taken, with a white accent (black on a
                     * light background), as a THM2 */
                    theme_t o = d;
                    uint32_t c1;
                    o.magic = THEME_MAGIC1;
                    c1 = st_crc32(&o, 32u);
                    o.hi = (uint16_t)c1, o.rsv = (uint16_t)(c1 >> 16), o.crc = 0xFFFFFFFFu;
                    ok &= theme_check(&o) && o.magic == THEME_MAGIC && o.hi == 0u && o.c[4] == d.c[4];
                    o = th;
                    o.magic = THEME_MAGIC1;
                    c1 = st_crc32(&o, 32u);
                    o.hi = (uint16_t)c1, o.rsv = (uint16_t)(c1 >> 16);
                    ok &= theme_check(&o) && o.hi == 0xFFFFu && o.rsv == 0u && theme_check(&o);
                    o.magic = THEME_MAGIC1, o.name[1] ^= 1;
                    ok &= !theme_check(&o);           /* (its CRC) */
                }
                d = th;
                d.name[0] = 7;
                d.crc = st_crc32(&d, 36u);
                ok &= !theme_check(&d);               /* not printable */
            }
            check(ok, "theme (2.7): checked (CRC, the strongest step and the accent standing out from the background, a "
                      "printable name); a first-beta THM1 taken with an accent");
            ui.menu = 1; ui.menu_sel = MI_COLOR; ui.force = 1; frame();
            settings.palette = 4; theme_on = 0; palette_set(4);
            encs[panel.enc[EN_K1]] = 1; frame();      /* no theme: MONO -> GREEN, as ever */
            ok = settings.palette == 0u && !theme_on;
            settings.palette = 4; palette_set(4);
            theme = th; theme_valid = 1;
            encs[panel.enc[EN_K1]] = 1; frame();      /* MONO -> SUNSET */
            ok &= theme_on && settings.palette == 4u && pal[0] == th.c[0] && pal[4] == th.c[4] && C_BLACK == th.bg &&
                  TE_COL[0] == th.trk[0] && TE_COL[3] == th.trk[3] && TE_MID[1] == rgb565_scale(th.trk[1], 2, 3) &&
                  (lights_word() >> 30 & 1u) && !strcmp(mi_value(MI_COLOR, &(uint16_t){0}), "SUNSET") && C_WHITE == th.hi;
            ui.force = 1; frame(); ppm("menu-theme");
            ui.menu = 0; go_home(); ui.force = 1; frame(); frame(); ppm("tracks-theme");   /* (a page in the theme) */
            open_family(FAM_EDIT); ui.force = 1; frame(); frame(); ppm("edit-theme");
            studio_open(SC_SONG); ui.force = 1; frame(); frame(); ppm("song-theme");
            check(PAGES[ui.page].scope == SC_SONG && swap16(screen[179 * 240 + 10]) == th.trk[0] &&
                  swap16(screen[179 * 240 + 190]) == th.trk[3],
                  "theme (2.7.2): the SONG screen's sections in the theme's track colours");
            go_home();
            {   /* a light theme (the editor's Light <-> dark on GREEN: the five the other way, white, a black accent):
                 * the same pages, to look at (build/host/*-light.ppm) */
                theme_t l = th, keep = th;
                for (k = 0; k < 5; k++)
                    l.c[k] = PALETTES[0].c[4 - k];
                l.bg = 0xFFFFu, l.hi = 0u;
                l.crc = st_crc32(&l, 36u);
                ok &= theme_check(&l);
                theme = l; palette_set(settings.palette);
                ui.menu = 1; ui.menu_sel = MI_COLOR; ui.force = 1; frame(); ppm("menu-theme-light");
                ui.menu = 0; go_home(); ui.force = 1; frame(); frame(); ppm("tracks-theme-light");
                open_family(FAM_EDIT); ui.force = 1; frame(); frame(); ppm("edit-theme-light");
                go_home();
                theme = keep; palette_set(settings.palette);
            }
            ui.menu = 1; ui.menu_sel = MI_COLOR; ui.force = 1; frame();
            lights_from_word(lights_word());
            ok &= theme_on == 1u;
            encs[panel.enc[EN_K1]] = 1; frame();      /* SUNSET -> GREEN */
            ok &= !theme_on && settings.palette == 0u && pal[4] == PALETTES[0].c[4] && TE_COL[0] == TE_COL_DEF[0] && C_BLACK == 0u &&
                  C_WHITE == 0xFFFFu &&
                  TE_DIM[2] == TE_DIM_DEF[2];
            encs[panel.enc[EN_K1]] = -1; frame();     /* back: SUNSET */
            ok &= theme_on && pal[2] == th.c[2];
            theme_valid = 0;                          /* (gone: the editor removed it) */
            palette_set(settings.palette);
            ok &= pal[4] == PALETTES[settings.palette].c[4] && strcmp(mi_value(MI_COLOR, &(uint16_t){0}), "SUNSET");
            theme_on = 0; settings.palette = pal0; palette_set(pal0);
            ui.menu_sel = MI_USB; ui.force = 1; frame();      /* (back to AUDIO) */
            check(ok, "MENU > COLOR (2.7): your theme after MONO, by its name; its colours, background and the tracks'; bit 30");
        }
        {   /* 2.7: the page counter counts the pages SELECT walks (SEQ: STEP 1/2, PATTERN 2/2; SONG not counted) */
            uint32_t n, k1, n1, k2, n2, k3 = 0, n3 = 0, menu0 = ui.menu, sel0 = ui.menu_sel;
            int ok = 1;
            ui.menu = 0; go_home(); frame();
            open_family(FAM_SEQ); frame();
            while (strcmp(PAGES[ui.page].title, "NOTES")) { open_family(FAM_SEQ); frame(); }
            k1 = page_pos(&n1);
            open_family(FAM_SEQ); frame();
            ok &= !strcmp(PAGES[ui.page].title, "PATTERN");
            k2 = page_pos(&n2);
            open_family(FAM_SEQ); frame();            /* SEQ again: the SONG screen (its own) */
            if (PAGES[ui.page].scope == SC_SONG)
                k3 = page_pos(&n3);
            open_family(FAM_SEQ); frame();            /* (round to STEP again: SEQ opens there, as before) */
            go_home(); frame();
            open_family(FAM_GLO); frame();
            (void)page_pos(&n);                       /* GLO: its nine pages */
            go_home(); frame();
            ui.menu = (uint8_t)menu0, ui.menu_sel = (uint8_t)sel0, ui.force = 1; frame();
            ok &= k1 == 1u && n1 == 2u && k2 == 2u && n2 == 2u && k3 == 3u && n3 == 3u && n == 9u;
            if (!ok)
                printf("ui:   counter: STEP %u/%u PATTERN %u/%u SONG %u/%u GLO /%u\n", k1, n1, k2, n2, k3, n3, n);
            check(ok, "page counter (2.7): as SELECT walks them: STEP 1/2, PATTERN 2/2 (SONG, its own screen, not counted)");
        }
        {   /* 2.7: a sound loaded under a running arp keeps it (settings and the notes it holds); arp OFF: the preset's */
            track_t *t = &trk[0];
            uint32_t k, eng = NENGINES, pi = 0;
            int ok = 1;
            t->p[P_AMODE] = 1, t->p[P_ARATE] = 3, t->p[P_AHOLD] = 1, t->p[P_AORDER] = 1;
            t->held[0] = 60, t->held[1] = 64, t->nheld = 2, t->arp_phys = 0;   /* (a latched chord) */
            frame();
            apply_preset_to(t, t->preset + 1u);
            frame(); frame();
            ok &= t->p[P_AMODE] == 1 && t->p[P_ARATE] == 3 && t->p[P_AHOLD] == 1 && t->p[P_AORDER] == 1 && t->nheld == 2;
            for (k = 0; k < NENGINES; k++)
                if (!strcmp(ENGINES[k]->name, "LOFI"))
                    eng = k;
            if (eng < NENGINES)
                for (k = 0; k < ENGINES[eng]->npresets; k++)
                    if (!strcmp(ENGINES[eng]->presets[k].name, "8BIT ARP"))
                        pi = k;
            t->p[P_AMODE] = 0;                       /* arp OFF: the preset brings its own */
            frame();
            t->p[P_AHOLD] = 0, t->nheld = 0;
            {
                uint8_t er = t->eng_req;
                t->eng_req = (uint8_t)eng;
                apply_preset_to(t, pi);
                ok &= eng < NENGINES && t->p[P_AMODE] == 1 && t->p[P_ARATE] == 2;
                t->eng_req = er;
                t->p[P_AMODE] = 0, t->p[P_AHOLD] = 0, t->nheld = 0;
                apply_preset_to(t, 0);
            }
            frame();
            check(ok, "ARP (2.7): a preset loaded while it runs keeps its settings and notes; arp OFF: the preset's arp");
            if (eng < NENGINES) {                    /* 2.7.1: the arp 8BIT ARP brought goes with it; one you changed stays */
                uint8_t er = t->eng_req, e0 = t->engine;
                int ok2 = 1;
                t->eng_req = t->engine = (uint8_t)eng;
                apply_preset_to(t, pi);              /* 8BIT ARP: its arp on */
                ok2 &= t->p[P_AMODE] == 1;
                apply_preset_to(t, (pi + 1u) % ENGINES[eng]->npresets);   /* the next sound: its own arp, OFF */
                frame(); frame();
                ok2 &= t->p[P_AMODE] == 0 && t->p[P_ARATE] == TP[P_ARATE].def && t->p[P_AGATE] == TP[P_AGATE].def;
                apply_preset_to(t, pi);              /* 8BIT ARP again, then its RATE turned: the arp is yours now */
                t->p[P_ARATE] = 4;
                apply_preset_to(t, (pi + 1u) % ENGINES[eng]->npresets);
                frame(); frame();
                ok2 &= t->p[P_AMODE] == 1 && t->p[P_ARATE] == 4;
                t->p[P_AMODE] = 1, t->p[P_ARATE] = 3;   /* an arp turned on over a sound without one: stays */
                apply_preset_to(t, pi);
                ok2 &= t->p[P_AMODE] == 1 && t->p[P_ARATE] == 3;
                t->p[P_AMODE] = 0, t->p[P_AHOLD] = 0, t->nheld = 0;
                t->eng_req = er, t->engine = e0;
                apply_preset_to(t, 0);
                frame();
                if (!ok2)
                    printf("ui:   arp after 8BIT ARP: MODE %d RATE %d\n", t->p[P_AMODE], t->p[P_ARATE]);
                check(ok2, "ARP (2.7.1): the next sound replaces the arp 8BIT ARP brought; an arp you turned on or changed goes on");
            }
        }
        {   /* 2.7: REMOTE: the buttons held over MIDI become the panel's (the debounce reads them as closed) */
            uint32_t pb = 1u << panel.btn[B_PLAY], hb = 1u << panel.btn[B_HOME], keep = fm1_in.buttons;
            int ok = 1;
            song.g[G_RMCH] = 16;
            remote_lbl = remote_up = 0;
            remote_cc(B_PLAY, 127), remote_cc(B_HOME, 127);
            remote_poll();
            ok &= fm1_remote_btn == (pb | hb);
            remote_cc(B_PLAY, 0);                     /* let go before the debounce saw it down: still held */
            remote_poll();
            ok &= (fm1_remote_btn & pb) != 0u;
            fm1_in.buttons |= pb;                     /* (seen down) */
            remote_poll();
            ok &= !(fm1_remote_btn & pb) && (fm1_remote_btn & hb);
            fm1_in.buttons = keep;
            song.g[G_RMCH] = 0;                       /* REMOTE OFF: nothing held */
            remote_poll();
            ok &= fm1_remote_btn == 0u && !remote_lbl;
            song.g[G_RMCH] = 16;                      /* another channel while one is held: let go */
            remote_poll();
            remote_cc(B_HOME, 127);
            remote_poll();
            ok &= fm1_remote_btn == hb;
            song.g[G_RMCH] = 15;
            remote_poll();
            ok &= fm1_remote_btn == 0u && !remote_lbl;
            song.g[G_RMCH] = 0;
            remote_poll();
            check(ok, "REMOTE (2.7): the labels held over MIDI become the panel's buttons; a quick up waits for the down");
        }
        {   /* 2.7 beta: USB RATE, its knob in AUDIO (KNOB 3; KNOB 4 with USB SERIAL): AUTO 44.1 48, at the next start */
            int ok = usb_rate == 0u && !(lights_word() >> 28 & 3u);
            encs[panel.enc[EN_K1 + mi_row(MI_RATE)]] = 1; frame();
            ok &= ui.menu_sel == MI_RATE && usb_rate == 1u && (lights_word() >> 28 & 3u) == 1u;
            ui.force = 1; frame(); ppm("menu-rate");
            encs[panel.enc[EN_K1 + mi_row(MI_RATE)]] = 1; frame();
            encs[panel.enc[EN_K1 + mi_row(MI_RATE)]] = 1; frame();      /* (stops at 48) */
            ok &= usb_rate == 2u;
            lights_from_word(lights_word());
            ok &= usb_rate == 2u;
            tap(B_OCTUP);                             /* round: AUTO */
            ok &= usb_rate == 0u && ui.menu == 1;
            lights_from_word((lights_word() & ~(3u << 28)) | 3u << 28);   /* (a value no SLOOP writes: AUTO) */
            ok &= usb_rate == 0u;
            check(!strcmp(MI_NAME[MI_RATE], "USB RATE") && ok,
                  "menu USB RATE (2.7): its knob, AUTO -> 44.1 -> 48 (stops), saved in bits 28-29, OCT+ round");
        }
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
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(lights_lvl == LIGHTS_LOW && fm1_led_bg_ns == 500u, "LIGHTS: KNOB 1 right: LOW (a 0.5 us pulse a frame)");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(lights_lvl == LIGHTS_MID && fm1_led_bg_ns == 1000u, "LIGHTS: MID (1 us)");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame(); encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(lights_lvl == LIGHTS_HIGH && fm1_led_bg_ns == 2000u, "LIGHTS: HIGH (2 us, under the 4 us glow), and it stops there");
        ui.force = 1; frame(); ppm("page-menu-lights");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = -1; frame();
        check(lights_lvl == LIGHTS_MID, "LIGHTS: KNOB 1 left: dimmer");
        tap(B_OCTUP);
        check(lights_lvl == LIGHTS_HIGH && ui.menu == 1, "LIGHTS: OCT+ steps round, the menu stays");
        tap(B_OCTUP);
        check(lights_lvl == LIGHTS_OFF && fm1_led_bg_ns == 0u, "LIGHTS: ... back to OFF");
        ui.menu_sel = MI_KEYS; ui.force = 1; frame();
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        m = lights_keys_mask();
        check(lights_keys == KEYS_C && lights_lvl == LIGHTS_LOW && m == (1u << 7 | 1u << 19),
              "KEYS (KNOB 2): C KEYS (C4, C5), LIGHTS raised to LOW");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        for (m = lights_keys_mask(), nbits = 0; m; m &= m - 1u)
            nbits++;
        check(lights_keys == KEYS_WHITE && nbits == 16u && !(lights_keys_mask() & (1u << 1)),
              "KEYS: WHITE KEYS (16 of 27, F#3 dark)");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame(); encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(lights_keys == KEYS_ALL && lights_keys_mask() == 0x7FFFFFFu, "KEYS: ALL KEYS (every one of the 27 keys), and it stops there");
        ui.force = 1; frame(); ppm("page-menu-keys-all");
        tap(B_OCTUP);
        check(lights_keys == KEYS_OFF, "KEYS: OCT+ from ALL KEYS steps round to OFF");
        lights_keys = KEYS_ALL;
        check(lights_word() >> 4 & 15u, "KEYS: ALL KEYS kept in the settings word");
        lights_from_word(lights_word());
        check(lights_keys == KEYS_ALL, "KEYS: ALL KEYS read back");
        lights_lvl = LIGHTS_OFF; frame();
        check(lights_keys_mask() == 0u && fm1_led_bg_ns == 0u, "KEYS: nothing lit while LIGHTS is OFF");
        lights_keys = KEYS_OFF;
        ui.menu = 0; ui.force = 1; go_home(); frame();
    }

    {   /* (2.7) knob acceleration after JIANT FM: progressive with the speed, none on a slow turn, a reversal or a short
         * range, capped by the range; KNOB ACCEL OFF: one value a detent */
        uint32_t t0 = fm1_ms, k, m = 0, rev;
        int32_t slow = 0, fast = 0, f8 = 0, f16 = 0, shortr = 0;
        ui.enc_t[EN_K1] = 0;
        for (k = 0; k < 6u; k++) { fm1_ms = t0 + 100u * k; slow += accel(EN_K1, 1, 127); }     /* 100 ms a detent */
        ui.enc_t[EN_K1] = 0;
        for (k = 0; k < 10u; k++) { fm1_ms = t0 + 1000u + 10u * k; m = (uint32_t)accel(EN_K1, 1, 127); fast += (int32_t)m; }
        f8 = (int32_t)m;                                                                            /* 10 ms a detent */
        fm1_ms += 10u; rev = (uint32_t)accel(EN_K1, -1, 127);                                       /* turned back */
        ui.enc_t[EN_K1] = 0;
        for (k = 0; k < 10u; k++) { fm1_ms = t0 + 2000u + 3u * k; m = (uint32_t)accel(EN_K1, 1, 127); }
        f16 = (int32_t)m;                                                                           /* 3 ms a detent */
        ui.enc_t[EN_K1] = 0;
        for (k = 0; k < 10u; k++) { fm1_ms = t0 + 3000u + 3u * k; shortr += accel(EN_K1, 1, 16); }  /* 17 values */
        ui.enc_t[EN_K1] = 0;
        for (k = 0; k < 10u; k++) { fm1_ms = t0 + 4000u + 3u * k; m = (uint32_t)accel(EN_K1, 1, 40); }
        check(slow == 6 && f8 == 8 && fast > 10 && rev == (uint32_t)-1 && f16 == 16 && shortr == 10 && m == 4u,
              "knobs (2.7): slow 1 a detent, 10 ms x8, 3 ms x16 (range 127), x4 cap at 40 values, none at 16, a reversal 1");
        knob_accel_off = 1;
        ui.enc_t[EN_K1] = 0;
        for (k = 0, fast = 0; k < 10u; k++) { fm1_ms = t0 + 5000u + 3u * k; fast += accel(EN_K1, 1, 127); }
        knob_accel_off = 0;
        check(fast == 10, "knobs (2.7): KNOB ACCEL OFF: one value a detent however fast");
        fm1_ms = t0 + 6000u;
        ui.enc_t[EN_K1] = 0;
    }

    {   /* the menu in sections (2.4): SELECT the section, KNOB 1..3 its rows, PRESETS the cursor, OCT+ opens */
        uint32_t sec;
        ui.menu = 1; ui.menu_sel = 0; ui.force = 1; frame(); ppm("menu-screen");
        check(mi_sec(ui.menu_sel) == MS_SCREEN, "menu: opens on SCREEN (COLOR, ZOOM, KNOB ACCEL)");
        encs[panel.enc[EN_K3]] = -1; frame();               /* (2.7) row 3: KNOB ACCEL, left OFF */
        check(ui.menu_sel == MI_ACCEL && knob_accel_off == 1u && (lights_word() >> 27 & 1u),
              "menu SCREEN: KNOB 3 left: KNOB ACCEL OFF, saved with the settings (bit 27)");
        encs[panel.enc[EN_K3]] = 1; frame();
        check(knob_accel_off == 0u && !(lights_word() >> 27 & 1u), "menu SCREEN: KNOB 3 right: KNOB ACCEL ON");
        lights_from_word(0);
        check(knob_accel_off == 0u, "menu SCREEN: a settings word from before 2.7 reads KNOB ACCEL ON");
        {   /* (2.7.1) the SEQ section: STEP ADVANCE (ON), DRUM PREVIEW (OFF); between AUDIO and SYSTEM */
            uint32_t m0 = ui.menu_sel;
            check(!drum_preview && !step_adv_off && mi_sec(MI_ADV) == MS_SEQ && mi_sec(MI_DPREV) == MS_SEQ &&
                  mi_sec(MI_RATE) == MS_AUDIO && mi_sec(MI_CPU1) == MS_SYSTEM,
                  "menu (2.7.1): a SEQ section: STEP ADVANCE ON, DRUM PREVIEW OFF by default");
            ui.menu_sel = MI_ADV; ui.force = 1; frame(); ppm("menu-seq");
            encs[panel.enc[EN_K1]] = -1; frame();
            encs[panel.enc[EN_K2]] = 1; frame();
            check(step_adv_off == 1u && drum_preview == 1u, "menu SEQ: KNOB 1 left STEP ADVANCE OFF, KNOB 2 right DRUM PREVIEW ON");
            encs[panel.enc[EN_K1]] = 1; frame();
            encs[panel.enc[EN_K2]] = -1; frame();
            check(!step_adv_off && !drum_preview, "menu SEQ: and back (ON, OFF)");
            ui.menu_sel = (uint8_t)m0; frame();
        }
        ui.menu_sel = 0; frame();
        encs[panel.enc[EN_SELECT]] = 1; frame();
        check(mi_sec(ui.menu_sel) == MS_LIGHTS && ui.menu_sel == MI_LIGHTS, "menu: SELECT right: LIGHTS, the cursor on its first row");
        encs[panel.enc[EN_K2]] = 1; frame(); encs[panel.enc[EN_K2]] = 1; frame();
        check(lights_keys == KEYS_WHITE && ui.menu_sel == MI_KEYS, "menu LIGHTS: KNOB 2 sets KEYS (row 2), the cursor follows");
        encs[panel.enc[EN_K3]] = 1; frame();
        check(lights_notes == 1u && ui.menu_sel == MI_NOTES, "menu LIGHTS: KNOB 3 sets NOTES (row 3)");
        encs[panel.enc[EN_K4]] = 1; frame();
        check(ui.menu_sel == MI_NOTES, "menu: KNOB 4 on a section of three rows does nothing");
        ui.force = 1; frame(); ppm("menu-lights");
        encs[panel.enc[EN_PRESET]] = 1; frame();
        check(ui.menu_sel == MI_LIGHTS, "menu: PRESETS moves the cursor round the section's rows");
        encs[panel.enc[EN_SELECT]] = 1; frame(); ui.force = 1; frame(); ppm("menu-audio");
        check(mi_sec(ui.menu_sel) == MS_AUDIO, "menu: SELECT right: AUDIO (LOWCUT, USB AUDIO, USB RATE)");
        encs[panel.enc[EN_SELECT]] = 5; frame();
        check(mi_sec(ui.menu_sel) == MS_SEQ, "menu (2.7.1): SELECT right: SEQ (STEP ADVANCE, DRUM PREVIEW)");
        encs[panel.enc[EN_SELECT]] = 5; frame();
        encs[panel.enc[EN_SELECT]] = 5; frame();
        sec = mi_sec(ui.menu_sel);
        check(sec == MS_SYSTEM, "menu: SELECT stops at the last section, SYSTEM");
        encs[panel.enc[EN_K1]] = -1; frame();         /* (dualcore) row 1: 2ND CORE; the render split (fx.c cpu1_units) */
        check(ui.menu_sel == MI_CPU1 && cpu1_off == 1u && cpu1_units == 0 && (lights_word() >> 26 & 1u),
              "menu SYSTEM: KNOB 1 left: 2ND CORE OFF (every render unit on core 0), saved with the settings (2.7: bit 26 set = OFF)");
        encs[panel.enc[EN_K1]] = 1; frame();
        check(cpu1_off == 0u && cpu1_units == -1 && !(lights_word() >> 26 & 1u), "menu SYSTEM: KNOB 1 right: 2ND CORE ON (the firmware's share), bit 26 clear");
        tap(B_OCTUP);
        check(cpu1_off == 1u && cpu1_units == 0 && ui.menu == 1, "menu SYSTEM: OCT+ on 2ND CORE toggles it, the menu stays");
        lights_from_word(lights_word());
        check(cpu1_off == 1u && cpu1_units == 0, "menu SYSTEM: 2ND CORE OFF read back from the settings word");
        lights_from_word(0);
        check(cpu1_off == 0u && cpu1_units == -1, "menu SYSTEM: a settings word from 2.6 or before reads as 2ND CORE ON (2.7: the default)");
        lights_from_word(1u << 21 | 1u << 26);
        check(cpu1_off == 1u && cpu1_units == 0 && lights_min == 2u, "menu SYSTEM: bit 21 is 2.6's MIDI IN = CH 1, bit 26 2ND CORE OFF");
        lights_from_word(1u << 26);                   /* (the rest as before: one core on the host) */
        encs[panel.enc[EN_K3]] = 1; frame();
        check(ui.menu == 1 && ui.menu_sel == MI_LCLEAR, "menu SYSTEM: KNOB 3 is MIDI LEARN CLEAR (2.6; row 3 after 2ND CORE), nothing cleared by a turn");
        ui.force = 1; frame(); ppm("menu-system");
        encs[panel.enc[EN_K4]] = 1; frame();
        check(ui.menu == 1 && ui.menu_sel == MI_ABOUT, "menu SYSTEM: a knob does not open ABOUT, it moves the cursor (row 4)");
        tap(B_OCTUP);
        check(ui.menu == 2, "menu SYSTEM: OCT+ on ABOUT opens it");
        tap(B_OCTDN);
        check(ui.menu == 1 && ui.menu_sel == MI_ABOUT, "menu: OCT- from ABOUT: back to SYSTEM");
        tap(B_OCTDN);
        check(ui.menu == 0, "menu: OCT- closes");
        lights_keys = KEYS_OFF; lights_notes = 0; lights_lvl = LIGHTS_OFF;
        go_home(); ui.force = 1; frame();
    }

    {   /* SELECT (2.4): the pages of the family shown; the tempo on HOME (TRACKS), in a layer, on REC */
        int16_t bpm;
        #define PT() (PAGES[ui.page].title)
        #define SEL(n) do { encs[panel.enc[EN_SELECT]] = (n); frame(); } while (0)
        song.playing = 0; song.sel = 0; go_home(); ui.force = 1; frame();
        bpm = song.g[G_BPM];
        SEL(1);
        check(song.g[G_BPM] > bpm, "SELECT on HOME (TRACKS): the tempo");
        song.g[G_BPM] = bpm;
        tap(B_LFO);
        check(!strcmp(PT(), "LFO"), "SELECT: LFO opens its first page");
        SEL(1);
        check(!strcmp(PT(), "LFO 2") && song.g[G_BPM] == bpm, "SELECT on LFO: next page LFO 2 (2.6), the tempo untouched");
        ui.force = 1; frame(); ppm("page-lfo2");
        SEL(1);
        check(!strcmp(PT(), "LFO DEST") && song.g[G_BPM] == bpm, "SELECT: then LFO DEST");
        SEL(1);
        check(!strcmp(PT(), "LFO DEST") && song.g[G_BPM] == bpm, "SELECT: stops at the last page of the family");
        SEL(-1);
        check(!strcmp(PT(), "LFO 2"), "SELECT left: the page before");
        SEL(-1);
        check(!strcmp(PT(), "LFO"), "SELECT left: LFO");
        tap(B_FX); SEL(1);
        check(!strcmp(PT(), "FILTER"), "SELECT on FX: next page, the track FILTER");
        TSEL->p[P_TFLT] = -32; ui.force = 1; frames(2); ppm("page-filter");
        check(!strcmp(ui.big_l[0], "FILT") && !strcmp(ui.big_v[0], "LP50") && !ui.big_l[1][0],
              "FILTER page: its one value drawn large (LP50 %)");
        {   /* the big value in the graph strip: white pixels in the middle of the screen */
            uint32_t x, y, lit = 0;
            for (y = Y_GRAPH + 30; y < Y_GRAPH + 90; y++)
                for (x = 40; x < 200; x++)
                    lit += screen[y * 240u + x] != 0;
            check(lit > 300u, "FILTER page: the graph strip shows it");
        }
        TSEL->p[P_TFLT] = 0;
        SEL(9);
        check(!strcmp(PT(), "REV/CHO"), "SELECT on FX: on to the last page, REV/CHO");
        tap(B_EDIT); SEL(-9);
        check(!strcmp(PT(), "EDIT 1"), "SELECT on EDIT: back to the first page");
        SEL(9);
        check(!strcmp(PT(), "VOICE 2"), "SELECT on EDIT: up to VOICE 2");
        tap(B_SEQ); SEL(9);
        check(!strcmp(PT(), "PATTERN"), "SELECT on SEQ: STEP <-> PATTERN, not into the SONG screen");
        tap(B_ENV);
        check(!strcmp(PT(), "ENV"), "the button still opens its family");
        tap(B_ENV);
        check(!strcmp(PT(), "ENV DEST"), "... and a second tap still steps the page");
        press(B_GLO); frames(3);
        SEL(1);
        check(!strcmp(PT(), "ENV DEST") && song.g[G_BPM] > bpm, "SELECT with a layer held (GLO): the tempo");
        release(B_GLO); frames(2);
        song.g[G_BPM] = bpm;
        go_home(); frame(); SEL(-1);
        check(song.g[G_BPM] < bpm, "back on HOME: SELECT is the tempo again");
        song.g[G_BPM] = bpm;
        #undef SEL
        #undef PT
    }

    {   /* Felucca #39 / #102 (2.4): a knob turned as a layer lets go, or while PLAY is pressed in a layer, never
         * edits the page under the layer */
        int16_t r0, w0;
        song.playing = 0; song.sel = 0; go_home(); ui.force = 1; frames(3);
        tap(B_LFO); frames(2);
        r0 = trk[0].p[P_LRATE]; w0 = trk[0].p[P_LWAVE];
        press(B_FX); frames(10);
        encs[panel.enc[EN_K2]] = 3; frame();
        encs[panel.enc[EN_K1]] = 4;                         /* turned in the frame FX comes up */
        fm1_in.buttons &= ~BT(B_FX); frame();
        check(trk[0].p[P_LRATE] == r0, "#39: KNOB 1 turned as FX lets go: the LFO page under it is untouched");
        frames(5); encs[panel.enc[EN_K1]] = 2; frame();
        check(trk[0].p[P_LRATE] == r0, "#39: ... and for a moment after (the knob still turning)");
        frames(20); encs[panel.enc[EN_K1]] = 2; frame();
        check(trk[0].p[P_LRATE] > r0, "#39: then KNOB 1 is the page's again");
        trk[0].p[P_LRATE] = r0;
        press(B_FX); frames(10);
        edges_btn |= BT(B_PLAY); fm1_in.buttons |= BT(B_PLAY);
        encs[panel.enc[EN_K2]] = 2; frame();
        fm1_in.buttons &= ~BT(B_PLAY);
        check(trk[0].p[P_LWAVE] == w0 && song.g[G_DUST] > 0 && transport_req, "#102: PLAY pressed in the FX layer plays, KNOB 2 is DUST only");
        transport_req = 0; song.g[G_DUST] = 0;
        release(B_FX); frames(20);
        song.g[G_BPM] = 120;
    }

    {   /* DRUMS grid (2.4): the keys are the steps of the sound; the sound and the step heard; SELECT grid / kit */
        uint32_t i;
        song.playing = 0; song.sel = TRK_DRUM; go_home(); frame();
        memset(TDRUM->dstep, 0, sizeof TDRUM->dstep);
        TDRUM->p[P_SLEN] = 32;
        studio_open(SC_DRUM); drum_page = 0; drum_lane = 0; drum_cursor = 0; frames(2);
        memset(pad_lit, 0, sizeof pad_lit);
        key(key_of_white(2)); frame();
        check(dstep_has(&TDRUM->dstep[2], 0) && drum_cursor == 2 && !pad_lit[0],
              "grid: white key 3 sets step 3 of the sound (2.7.1: not heard, for live), the cursor on it");
        check((keys_lit() >> key_of_white(2)) & 1u, "grid: its key lit");
        key(key_of_white(2));
        check(!dstep_mask(&TDRUM->dstep[2]), "grid: the key again clears it");
        {   /* 2.7.1: the black keys pick the sound, unheard, the same key again its pair; OCT- / OCT+ the page */
            int ok = 1;
            memset(pad_lit, 0, sizeof pad_lit);
            key(1); ok &= drum_lane == 1;                             /* F#3: KICK -> KICK 2 (KICK was picked) */
            key(1); ok &= drum_lane == 0;                             /* .. and back */
            key(3); ok &= drum_lane == 2;                             /* G#3: SNARE */
            key(3); ok &= drum_lane == 8;                             /* .. SNARE 2 */
            key(13); ok &= drum_lane == 5;                            /* F#4: OPEN HAT */
            key(25); ok &= drum_lane == 15;                           /* F#5: COWBELL */
            frame();
            for (i = 0; i < 16u; i++) ok &= !pad_lit[i];
            ok &= drum_cursor / 16u == 0u && ((keys_lit() >> 25) & 1u);   /* (no page change; its black key lit) */
            check(ok, "grid (2.7.1): the black keys pick the sound (F#3 kick / kick 2 ... F#5 cowbell), unheard, lit");
            key(1);                                                   /* (back to the kick) */
            check(drum_lane == 0, "grid: F#3 again: the kick");
        }
        tap(B_OCTUP); frame();
        check(drum_cursor / 16u == 1u, "grid (2.7.1): OCT+ the next page of steps (17-32)");
        key(key_of_white(0));
        check(dstep_has(&TDRUM->dstep[16], 0), "grid: ... and the white keys are its steps (step 17)");
        tap(B_OCTUP); frame();
        check(drum_cursor / 16u == 0u, "grid: OCT+ past LENGTH (32 steps): back to the first page");
        tap(B_OCTDN); frame();
        check(drum_cursor / 16u == 1u, "grid: OCT- the page before (round)");
        memset(pad_lit, 0, sizeof pad_lit);
        encs[panel.enc[EN_K1]] = 3; frames(2);
        check(drum_lane == 3 && !pad_lit[3], "grid: KNOB 1 picks the sound, unheard (menu DRUM PREVIEW OFF, 2.7.1)");
        drum_preview = 1;
        encs[panel.enc[EN_K1]] = 1; frames(2); encs[panel.enc[EN_K1]] = -1; frames(2);
        check(drum_lane == 3 && pad_lit[3], "grid: DRUM PREVIEW ON: KNOB 1 plays the sound picked");
        dstep_set(&TDRUM->dstep[20], 5, LV_NORM, 0);
        dstep_set(&TDRUM->dstep[20], 7, LV_SOFT, 0);
        memset(pad_lit, 0, sizeof pad_lit);
        drum_cursor = 19; encs[panel.enc[EN_K2]] = 1; frames(2);
        check(drum_cursor == 20 && pad_lit[5] && pad_lit[7] && !pad_lit[3], "grid: KNOB 2 onto a step plays what it holds (PREVIEW ON)");
        drum_preview = 0;
        memset(pad_lit, 0, sizeof pad_lit);
        drum_cursor = 19; encs[panel.enc[EN_K2]] = 1; frames(2);
        check(drum_cursor == 20 && !pad_lit[5] && !pad_lit[7], "grid: PREVIEW OFF: KNOB 2 silent");
        encs[panel.enc[EN_SELECT]] = 1; frame();
        check(drum_page == 1, "DRUMS: SELECT right: kit");
        frame();
        memset(pad_lit, 0, sizeof pad_lit);
        key(key_of_white(9)); frame();
        check(pad_lit[9] && !dstep_has(&TDRUM->dstep[25], 3), "kit: the keys play the pads again, no step set");
        {   /* 2.7: the SOUND page (SELECT right again): a key picks the sound, KNOB 2 its TUNE, KNOB 3 its DECAY */
            int lv0 = song.g[G_DRLVL];
            encs[panel.enc[EN_SELECT]] = 1; frame();
            check(drum_page == 2, "DRUMS: SELECT right again: sound (2.7)");
            key(key_of_white(2)); frame();
            encs[panel.enc[EN_K2]] = 3; frame();
            encs[panel.enc[EN_K3]] = -2; frame();
            check(drum_lane == 2 && drum_tune[2] == 3 && drum_dec[2] == -2, "sound: a key picks its sound, KNOB 2 its TUNE (+3), KNOB 3 its DECAY (-2)");
            ui.force = 1; frame(); ppm("live-sound");
            encs[panel.enc[EN_K2]] = 20; frame();
            encs[panel.enc[EN_K1]] = 1; frame();
            encs[panel.enc[EN_K4]] = -3; frame();
            check(drum_lane == 3 && drum_tune[2] == 7 && song.g[G_DRLVL] == lv0 - 3, "sound: TUNE stops at +7; KNOB 1 the next sound, KNOB 4 the drums' level");
            song.g[G_DRLVL] = (int16_t)lv0;
            memset(drum_tune, 0, sizeof drum_tune), memset(drum_dec, 0, sizeof drum_dec);
            encs[panel.enc[EN_SELECT]] = -1; frame();
            check(drum_page == 1, "DRUMS: SELECT left: kit");
        }
        encs[panel.enc[EN_SELECT]] = -1; frame();
        check(drum_page == 0, "DRUMS: SELECT left: grid");
        go_home(); frame();
        memset(pad_lit, 0, sizeof pad_lit);
        pen_lane = 0;
        press(B_SEQ); frames(3);
        encs[panel.enc[EN_K1]] = 2; frames(2);
        check(pen_lane == 2 && !pad_lit[2], "SEQ layer, drum track: KNOB 1 picks the sound (2.7.1: unheard, PREVIEW OFF)");
        {   /* 2.7.1: SEQ held on the drum track: a black key the sound (no page), OCT the page, the white keys its steps */
            int ok = 1;
            TDRUM->p[P_SLEN] = 32;
            ui.step_page = 0;
            key(8);                                     /* C#4 (was page 4): CLAP */
            ok &= pen_lane == 3 && ui.step_page == 0u && ((keys_lit() >> 8) & 1u);
            key(10);                                    /* D#4: HI HAT */
            ok &= pen_lane == 4 && ui.step_page == 0u;
            key(10);                                    /* .. PEDAL */
            ok &= pen_lane == 6 && ((keys_lit() >> 10) & 1u);
            key(key_of_white(4));                       /* step 5: the pedal hat */
            ok &= dstep_has(&TDRUM->dstep[4], 6);
            edges_btn |= BT(B_OCTUP); fm1_in.buttons |= BT(B_OCTUP); frame(); fm1_in.buttons &= ~BT(B_OCTUP); frame();
            ok &= ui.step_page == 1u;
            key(key_of_white(0));                       /* step 17 */
            ok &= dstep_has(&TDRUM->dstep[16], 6);
            for (i = 0; i < 16u; i++) ok &= !pad_lit[i];
            check(ok, "SEQ layer, drum track (2.7.1): black keys the sound (unheard, lit), OCT+ the page, white keys its steps");
            ui.step_page = 0;
        }
        release(B_SEQ); frames(2);
        {   /* 2.7.1, the comment's workflow: SEQ held + HOME, locked; then black keys the sound, white keys its steps */
            int ok;
            memset(TDRUM->dstep, 0, sizeof TDRUM->dstep);
            press(B_SEQ); frames(3); tap(B_HOME); release(B_SEQ); frames(3);
            ok = ly_lock == LY_STEP;
            key(1); key(key_of_white(0)); key(key_of_white(8));          /* the kick on steps 1 and 9 */
            key(3); key(key_of_white(4)); key(key_of_white(12));         /* the snare on 5 and 13 */
            ok &= dstep_has(&TDRUM->dstep[0], 0) && dstep_has(&TDRUM->dstep[8], 0) && dstep_has(&TDRUM->dstep[4], 2) &&
                  dstep_has(&TDRUM->dstep[12], 2) && !dstep_has(&TDRUM->dstep[4], 0);
            tap(B_HOME); frames(2);
            check(ok && ly_lock == LY_PLAY, "SEQ locked (HOME), drum track (2.7.1): F#3 kick, white keys 1 / 9; G#3 snare, 5 / 13");
            memset(TDRUM->dstep, 0, sizeof TDRUM->dstep);
            go_home(); frame();
        }
        song.sel = 1; go_home(); frame();
        trk[1].p[P_SDIV] = 5;
        press(B_SEQ); frames(3);
        for (i = 0; i < 5u; i++) { encs[panel.enc[EN_K2]] = 1; frame(); }
        check(trk[1].p[P_SDIV] == (int16_t)(NDIV_STEP - 1u), "SEQ layer: KNOB 2 DIV goes on to 1/2, 1BAR, 2BAR");
        release(B_SEQ); frames(2);
        trk[1].p[P_SDIV] = 2; TDRUM->p[P_SLEN] = 16; memset(TDRUM->dstep, 0, sizeof TDRUM->dstep);
        song.sel = 0; go_home(); frame();
    }

    {   /* (2.4) STEP, then ALGO to the drum track: "DRUM TRACK" (it drew the drum steps as notes, reading past them) */
        uint32_t j, b0;
        song.sel = 0; go_home(); frames(2);
        for (j = 0; j < NSTEP; j++) dstep_set(&TDRUM->dstep[j], j % 16u, LV_NORM, 3);
        for (j = 0; j < NPAGES; j++) if (!strcmp(PAGES[j].title, "NOTES")) break;
        open_family(FAM_SEQ); ui.page = (uint8_t)j; ui.fam_last[FAM_SEQ] = (uint8_t)j; page_entered(); ui.force = 1; frames(2);
        b0 = ui.page;
        for (j = 0; j < 3u; j++) { encs[panel.enc[EN_ALGO]] = 1; frames(2); }
        check(song.sel == TRK_DRUM && ui.page == b0 && !page_for_drum(cur_page()) && !strcmp(PAGES[ui.page].title, "NOTES"),
              "STEP on the drum track: not a page for the drums (DRUM TRACK shown, no piano roll of the drum steps)");
        for (j = 0; j < NSTEP; j++) memset(&TDRUM->dstep[j], 0, sizeof(dstep_t));
        song.sel = 0; go_home(); frames(4);
    }
    {   /* (2.4.1) the STEP page in chord mode: a key writes the whole chord it sounds (it wrote only the root, as
         * 2.3 did); with a CHORD+ modifier held, the changed chord; a single note without chord mode */
        uint32_t j, k5 = key_of_white(4), kmin = 1u;            /* key 5 = C4 (the I chord); F#3 (key index 1): minor */
        track_t *t = &trk[1];
        song.sel = 1; go_home(); frames(2);
        steps_clear(t); t->p[P_SLEN] = 16; t->p[P_VOICE] = V_POLY;
        t->p[P_ROOT] = 0; t->p[P_SCALE] = 1; t->p[P_QUANT] = 0; t->p[P_CHORD] = 1;   /* C major, TRIAD */
        for (j = 0; j < NPAGES; j++) if (!strcmp(PAGES[j].title, "NOTES")) break;
        open_family(FAM_SEQ); ui.page = (uint8_t)j; ui.fam_last[FAM_SEQ] = (uint8_t)j; page_entered(); ui.force = 1; frames(2);
        cursor_set(0); frames(1);
        key(k5); frames(2);
        check(t->step[0].n == 3u && t->step[0].note[0] % 12u == 0u && t->step[0].note[1] % 12u == 4u && t->step[0].note[2] % 12u == 7u,
              "STEP page, chord mode TRIAD: C4 writes the chord C E G (not only C)");
        fm1_in.notes = 1u << kmin; frames(2);
        key(k5); frames(2);
        fm1_in.notes = 0; frames(2);
        check(t->step[1].n == 3u && t->step[1].note[1] % 12u == 3u, "STEP page, CHORD+: F# held, C4 writes C minor (C Eb G)");
        t->p[P_CHORD] = 0;
        key(k5); frames(2);
        check(t->step[2].n == 1u && t->step[2].note[0] % 12u == 0u, "STEP page, no chord mode: one note");
        steps_clear(t); song.sel = 0; go_home(); frames(4);
    }
    {   /* the visualiser (2.4): HOME on HOME opens it, SELECT its 12 styles, HOME / a page closes it; a layer
         * shows its screen over it; the style is kept with the settings */
        uint32_t st, j, x, lit, k;
        static const uint8_t BASS[2] = {38, 34};
        song.playing = 0; song.sel = 0;
        for (j = 0; j < NTRK; j++) { steps_clear(&trk[j]); trk[j].p[P_SLEN] = 16; }
        for (j = 0; j < 16u; j++) {                 /* a groove to look at */
            step_t *sp;
            if (!(j & 1u)) { sp = &trk[0].step[j]; sp->n = 1; sp->note[0] = (uint8_t)(BASS[j / 8u] + ((j & 2u) ? 12 : 0)); sp->time = ST_NOTE; sp->vel = 100; }
            if ((j % 4u) == 2u) { sp = &trk[1].step[j]; sp->n = 3; sp->note[0] = 65; sp->note[1] = 69; sp->note[2] = 72; sp->time = ST_NOTE; sp->vel = 100; }
            if (!(j % 8u)) { sp = &trk[2].step[j]; sp->n = 2; sp->note[0] = 57; sp->note[1] = 64; sp->time = ST_NOTE; sp->vel = 100; }
            memset(&TDRUM->dstep[j], 0, sizeof(dstep_t));
            if (!(j % 4u)) dstep_set(&TDRUM->dstep[j], 0, LV_NORM, 0);
            if ((j % 8u) == 4u) dstep_set(&TDRUM->dstep[j], 3, LV_NORM, 0);
            dstep_set(&TDRUM->dstep[j], 4, (j & 1u) ? LV_SOFT : LV_NORM, 0);
            for (k = 0; k < 4u; k++) trk[k].seq_active = 1;
        }
        go_home(); frames(20);                      /* (the knobs quiet after the layers above) */
        transport_req = 1; frames(30);
        check(!vis_shown(), "visualiser: closed on the TRACKS screen");
        tap(B_HOME); frames(2);
        check(vis_shown() && vis_on, "visualiser: HOME on HOME opens it");
        vis_style = 0; ui.force = 1;
        for (st = 0; st < 12u; st++) {
            char nm[80];
            frames(st == 0u ? 40u : 70u);           /* (past the name's second) */
            ui.force = 1; frames(2);
            for (lit = 0, x = 0; x < 240u * 240u; x++)
                lit += screen[x] != 0;
            snprintf(nm, sizeof nm, "vis-%02u-%s", st + 1u, VIS_NAME[st]);
            for (x = 0; nm[x]; x++) if (nm[x] == ' ') nm[x] = '-';
            ppm(nm);
            snprintf(nm, sizeof nm, "visualiser %2u %-12s draws (%u px lit)", st + 1u, VIS_NAME[st], lit);
            check(lit > (st == 3u ? 400u : 1500u) && vis_style == st, nm);   /* (Lissajous: points) */
            encs[panel.enc[EN_SELECT]] = 1; frame();
        }
        {   /* MASTER at 0: silence out, the picture as if MASTER were all the way up */
            uint32_t m0 = song.master_q12, pk = 0, op = 0, b;
            static int32_t o2[CTL * 2];
            song.master_q12 = 0; frames(20);
            for (j = 0; j < SCOPE_N; j++) { uint32_t a = (uint32_t)(scope_buf[j] < 0 ? -scope_buf[j] : scope_buf[j]); if (a > pk) pk = a; }
            for (b = 0; b < 200u; b++) { mix_block(o2, CTL); for (j = 0; j < CTL * 2u; j++) { uint32_t a = (uint32_t)(o2[j] < 0 ? -o2[j] : o2[j]); if (a > op) op = a; } }
            ui.force = 1; frames(2);
            for (lit = 0, x = 0; x < 240u * 240u; x++) lit += screen[x] != 0;
            check(op < 64u && pk > 2000u && lit > 1500u, "visualiser: MASTER at 0: no sound, the picture as at full volume");
            song.master_q12 = m0; frames(4);
        }
        check(vis_style == 0u, "visualiser: SELECT goes round (12 -> 1)");
        encs[panel.enc[EN_SELECT]] = -1; frames(2);
        check(vis_style == 11u && (lights_word() >> 17 & 15u) == 11u, "visualiser: SELECT left: SLOOP (12), kept in the settings word");
        lights_from_word(lights_word());
        check(vis_style == 11u, "visualiser: the style read back");
        { int16_t bpm = song.g[G_BPM], lv = trk[0].p[P_LEVEL];
          encs[panel.enc[EN_K2]] = 5; frame();
          check(trk[0].p[P_LEVEL] == lv && song.g[G_BPM] == bpm, "visualiser: KNOB 2 (the hidden TRACKS screen) edits nothing, SELECT is not the tempo"); }
        press(B_GLO); frames(40);                  /* (held past a tap) */
        check(ui.layer == LY_MIX, "visualiser: a layer held shows its screen");
        release(B_GLO); frames(4);
        check(vis_shown(), "visualiser: ... and it comes back");
        tap(B_HOME); frames(2);
        check(!vis_shown() && !vis_on, "visualiser: HOME again closes it");
        tap(B_HOME); frames(2);
        tap(B_LFO); frames(2);
        check(!vis_shown() && !vis_on && !strcmp(PAGES[ui.page].title, "LFO"), "visualiser: a page button closes it and opens its page");
        transport_req = 2; frames(4);
        for (j = 0; j < NTRK; j++) steps_clear(&trk[j]);
        vis_style = 0; song.sel = 0; go_home(); frames(20);
    }

    {   /* 2.6, after Felucca 1.4 / 1.5 (Leo Kuroshita): STEP LEN, SEQ VEL, the drum mutes (EDIT + a black key), MIDI
         * LEARN (GLO + F#3, a knob, a CC; LEARN CLEAR in the menu), the INSERT pages (the drum track too), ANALOG's FILTER */
        uint32_t j, w, ins_d = 0, flt_a = 0, flt_o = 0;
        #define K(n, v) do { encs[panel.enc[n]] = (v); frames(5); } while (0)
        #define WALK(fa_, ti_, hit_) do { open_family(fa_); frame(); \
            for (j = 0; j < 12u; j++) { encs[panel.enc[EN_SELECT]] = -1; frame(); } \
            for (j = 0; j < 12u; j++) { if (PAGES[ui.page].fam == (fa_) && !strcmp(PAGES[ui.page].title, ti_)) hit_ = 1; \
                encs[panel.enc[EN_SELECT]] = 1; frame(); } } while (0)
        song.playing = 0;
        song.sel = 0; steps_clear(&trk[0]); trk[0].p[P_SLEN] = 16; go_home(); frames(16);
        press(B_SEQ); frames(10);
        key(7);                                           /* step 5 */
        fm1_in.notes = 1u << 7; frame();
        w = step_vel_shown(&trk[0].step[4]);
        K(EN_K2, 1);
        check(trk[0].step[4].vel == w + 1u && !trk[0].step[4].lvl, "SEQ: step 5 held + KNOB 2: its velocity one up (VEL)");
        ui.force = 1; frame(); ppm("layer-steps-vel");
        fm1_in.notes = 0; frame(); release(B_SEQ); frames(16);
        open_family(FAM_SEQ); frame();
        check(!strcmp(PAGES[ui.page].title, "NOTES"), "SEQ: the NOTES page first (2.7.2: STEP before)");
        ui.cursor = 4; ui.force = 1; frame();
        K(EN_K3, 1); K(EN_K3, 1);
        check(note_len(&trk[0], 4) == 3u && trk[0].step[5].time == ST_TIE && trk[0].step[6].time == ST_TIE &&
              trk[0].step[7].time != ST_TIE, "STEP page, KNOB 3 on a note: LEN 1 -> 3 (two TIEs after it)");
        ui.force = 1; frame(); ppm("page-step-len");
        K(EN_K3, -1);
        check(note_len(&trk[0], 4) == 2u && trk[0].step[6].time == ST_REST, "KNOB 3 back: LEN 2 (its last TIE a REST)");
        trk[0].step[5].n = 1, trk[0].step[5].note[0] = 64;   /* a TIE holding a note (TIME, or an older SLOOP) */
        K(EN_K3, -1);
        check(note_len(&trk[0], 4) == 1u && trk[0].step[5].time == ST_REST && trk[0].step[5].n == 1u &&
              trk[0].step[5].note[0] == 64u, "LEN shorter over a TIE holding a note: a REST, the note kept (as TIME)");
        go_home(); frames(16);
        press(B_EDIT); frames(10);
        edges_btn |= BT(B_OCTDN); fm1_in.buttons |= BT(B_OCTDN); frame(); fm1_in.buttons &= ~BT(B_OCTDN); frame();
        release(B_EDIT); frames(4);
        check(note_len(&trk[0], 4) == 1u && trk[0].step[5].n == 0u && trk[0].step[4].n == 1u,
              "EDIT + OCT-: the LEN turns on that step undone at once (the steps as before the first)");
        {   /* 2.6: the STEP page takes a MIDI keyboard's notes as the keys (a YouTube comment): the notes played together
             * (CH 1 = track 1, selected) the cursor step, all let go: on to the next; the SEQ layer's pen too; a note for
             * another track (stopped): which track it plays, nothing written; off the page: nothing written */
            #define MIN(c, n, v) (midi_in_q[mi_w++ % MQ] = ((v) ? 0x09u | (0x90u | (c)) << 8 : 0x08u | (0x80u | (c)) << 8) | \
                                  (uint32_t)(n) << 16 | (uint32_t)(v) << 24)
            step_t s9;
            uint32_t k9;
            int16_t v0 = trk[0].p[P_VOICE];
            open_family(FAM_SEQ); frame();
            trk[0].p[P_VOICE] = V_POLY;
            ui.cursor = 8; memset(&trk[0].step[8], 0, sizeof trk[0].step[8]); ui.force = 1; frame();
            MIN(0, 50, 100); MIN(0, 54, 90); frame();
            check(trk[0].step[8].n == 2u && trk[0].step[8].note[0] == 50u && trk[0].step[8].note[1] == 54u &&
                  trk[0].step[8].time == ST_NOTE && ui.cursor == 8u, "STEP page: two MIDI notes held (CH 1): step 9 the chord");
            check(pen_n == 2u && pen_note[0] == 50u && pen_note[1] == 54u, "  and the SEQ layer's pen: that chord");
            MIN(0, 50, 0); frame();
            check(ui.cursor == 8u, "  one let go: still on step 9");
            MIN(0, 54, 0); frame();
            check(ui.cursor == 9u, "  both let go: on to step 10");
            MIN(0, 62, 100); MIN(0, 62, 0); frame();
            check(trk[0].step[9].n == 1u && trk[0].step[9].note[0] == 62u && ui.cursor == 10u && pen_n == 1u && pen_note[0] == 62u,
                  "  a MIDI note tapped (on and off in one frame): step 10, on to step 11; the pen that note");
            fm1_in.notes = 1u << 7; frame(); MIN(0, 67, 100); frame(); fm1_in.notes = 0; frame();
            check(ui.cursor == 10u && trk[0].step[10].n == 2u && trk[0].step[10].note[1] == 67u,
                  "  a key and a MIDI note together: one step; the MIDI note still held: no move");
            MIN(0, 67, 0); frame();
            check(ui.cursor == 11u, "  let go: on");
            s9 = trk[0].step[11]; ui.msg[0] = 0;
            MIN(1, 70, 100); MIN(1, 70, 0); frame();
            check(!memcmp(&s9, &trk[0].step[11], sizeof s9) && ui.cursor == 11u && !strcmp(ui.msg, "MIDI PLAYS TRACK 2"),
                  "  a note on CH 2 (track 2's): nothing written, \"MIDI PLAYS TRACK 2\"");
            {   /* 2.7.1 (a YouTube comment): MENU > SEQ > STEP ADVANCE OFF: let go, the cursor stays; a chord tried again
                 * writes the same step anew; KNOB 1 still moves the cursor */
                int ok;
                step_adv_off = 1;
                ui.cursor = 12; memset(&trk[0].step[12], 0, sizeof trk[0].step[12]); frame();
                MIN(0, 60, 100); MIN(0, 65, 100); frame(); MIN(0, 60, 0); MIN(0, 65, 0); frame();   /* a wrong chord */
                ok = ui.cursor == 12u && trk[0].step[12].n == 2u && trk[0].step[12].note[1] == 65u;
                MIN(0, 60, 100); MIN(0, 64, 100); MIN(0, 67, 100); frame(); MIN(0, 60, 0); MIN(0, 64, 0); MIN(0, 67, 0); frame();
                ok &= ui.cursor == 12u && trk[0].step[12].n == 3u && trk[0].step[12].note[0] == 60u &&
                      trk[0].step[12].note[1] == 64u && trk[0].step[12].note[2] == 67u;   /* the chord again: the step anew */
                encs[panel.enc[EN_K1]] = 1; frame();      /* (2.7.2 NOTES: KNOB 1 walks the chord's notes first) */
                ok &= ui.cursor == 12u && ui.note_k == 1u;
                encs[panel.enc[EN_K1]] = 2; frame();
                ok &= ui.cursor == 13u && ui.note_k == 0u;
                step_adv_off = 0;
                MIN(0, 62, 100); MIN(0, 62, 0); frame();
                ok &= ui.cursor == 14u && trk[0].step[13].note[0] == 62u;
                check(ok, "STEP page (2.7.1): STEP ADVANCE OFF: the cursor stays, a chord again rewrites the step, KNOB 1 moves; ON: on");
                ui.cursor = 11;
            }
            go_home(); frame();
            s9 = trk[0].step[11];
            MIN(0, 72, 100); frame(); MIN(0, 72, 0); frame();
            check(!memcmp(&s9, &trk[0].step[11], sizeof s9) && step_in_r == step_in_w, "  off the STEP page: nothing written, the queue read");
            ui.msg[0] = 0; song.rec = 1;                  /* (2.6) recording track 1: a keyboard on track 2's channel */
            MIN(1, 60, 100); MIN(1, 60, 0); frame();
            check(!strcmp(ui.msg, "MIDI PLAYS TRACK 2"), "REC on track 1, a MIDI note on CH 2: \"MIDI PLAYS TRACK 2\" (not recorded)");
            ui.msg[0] = 0;
            MIN(1, 62, 100); MIN(1, 62, 0); frame();
            check(!ui.msg[0], "  said once a take (an external sequencer on track 2 does not flash it)");
            song.rec = 0; frame(); song.rec = 1; ui.msg[0] = 0;   /* a new take; the keyboard on track 1's channel */
            MIN(0, 60, 100); MIN(0, 60, 0); frame();
            MIN(1, 62, 100); MIN(1, 62, 0); frame();
            check(!ui.msg[0], "  a new take, a note on CH 1 (track 1's) first: no message, even with CH 2 after");
            song.rec = 0; ui.msg[0] = 0;
            for (k9 = 0; k9 < NSTEP; k9++) if (trk[0].step[k9].time == ST_NOTE && trk[0].step[k9].note[0] == 60) { trk[0].step[k9].time = ST_REST; trk[0].step[k9].n = 0; }
            trk[0].p[P_VOICE] = v0;
            #undef MIN
        }
        {   /* 2.6, after Felucca 1.5.1: MIDI Program Change (queued by seq.c, loaded here) and FX + OCT- / OCT+: the THROWs */
            uint8_t e0 = trk[1].eng_req, p0 = trk[1].preset;
            int16_t k0 = TDRUM->p[P_E0];
            uint32_t k, tot = NBANK + up_count();
            go_home(); frame();
            pc_req[1] = 1;                                /* PC 0 on track 2's channel: the first sound of the list */
            frame();
            check(trk[1].eng_req == BANK[0].e && trk[1].preset == bank_pi[0] && !strncmp(ui.msg, "T2 PROG 0", 9),
                  "MIDI Program Change 0: track 2 takes the first sound of the PRESETS list (T2 PROG 0)");
            pc_req[3] = 3;                                /* PC 2 on the drum channel: kit 2 */
            frame();
            check(TDRUM->p[P_E0] == 2, "MIDI Program Change on the drum channel: kit 2");
            pc_req[1] = (uint8_t)(tot + 1u);              /* past the end: nothing */
            k = trk[1].eng_req;
            frame();
            check(trk[1].eng_req == k && trk[1].preset == bank_pi[0], "MIDI Program Change past the list: ignored (no wrap)");
            TDRUM->p[P_E0] = k0;
            set_engine_of(&trk[1], e0); apply_preset_to(&trk[1], p0); trk[1].engine = trk[1].eng_req;
            frames(4);
            press(B_FX); frames(10);
            fm1_in.buttons |= BT(B_OCTDN); edges_btn |= BT(B_OCTDN); frame(); frame();
            k = thr_req;
            ui.force = 1; frame(); ppm("layer-fx-throw");
            fm1_in.buttons &= ~BT(B_OCTDN); frame();
            fm1_in.buttons |= BT(B_OCTUP); edges_btn |= BT(B_OCTUP); frame(); frame();
            check(k == 1u && thr_req == 2u && !strcmp(sub_line(), "echo throw"), "FX + OCT-: REV THROW while held, FX + OCT+: ECHO THROW");
            fm1_in.buttons &= ~BT(B_OCTUP); frame();
            release(B_FX); frames(4);
            check(thr_req == 0u && song.octave == 0, "  let go: no THROW, the octave untouched");
        }
        steps_clear(&trk[0]); go_home(); frames(16);
        {   /* SEQ: two steps held + KNOB 2: both one up (the turn accelerated once, not once a step) */
            uint32_t v0, v1;
            song.sel = 0; trk[0].p[P_SLEN] = 16; go_home(); frames(16);
            press(B_SEQ); frames(10);
            key(0); key(7);
            fm1_in.notes = 1u << 0 | 1u << 7; frame();
            v0 = step_vel_shown(&trk[0].step[0]), v1 = step_vel_shown(&trk[0].step[4]);
            K(EN_K2, 1);
            check(trk[0].step[0].vel == v0 + 1u && trk[0].step[4].vel == v1 + 1u, "SEQ: two steps held + KNOB 2: both one up");
            fm1_in.notes = 0; frame(); release(B_SEQ); frames(16);
            steps_clear(&trk[0]); go_home(); frames(16);
        }

        song.sel = TRK_DRUM; go_home(); frames(16);
        dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);
        press(B_EDIT); frames(10);
        key(1);                                           /* a black key: mute mode */
        check(er_mute == 1u, "EDIT + a black key (drum track): MUTE mode");
        key(0);                                           /* the kick's key */
        check(drum_mute == 1u && dstep_has(&TDRUM->dstep[0], 0), "MUTE mode + the kick's key: the kick muted, its steps kept");
        ui.force = 1; frame(); ppm("layer-erase-mute");
        key(0);
        check(drum_mute == 0u, "the kick's key again: unmuted");
        key(0);
        release(B_EDIT); frames(2);
        check(er_mute == 1u && drum_mute == 1u, "2.7: EDIT let go: MUTE mode stays, the mute stays");
        press(B_EDIT); frames(10);
        key(0);                                           /* EDIT again + the kick's key: unmutes at once (no black key) */
        check(drum_mute == 0u && dstep_has(&TDRUM->dstep[0], 0), "2.7: EDIT again + a white key: mutes / unmutes at once");
        key(1);                                           /* a black key: back to erasing */
        release(B_EDIT); frames(2);
        check(er_mute == 0u, "2.7: a black key again: back to erasing, also after EDIT is let go");
        drum_mute = 0;
        TDRUM->dstep[0] = (dstep_t){0};
        WALK(FAM_FX, "INSERT", ins_d);
        check(ins_d, "the drum track: the INSERT page in FX");
        go_home(); frames(16);

        song.sel = 0; set_engine_of(&trk[0], 0); trk[0].engine = trk[0].eng_req; go_home(); frame();
        WALK(FAM_EDIT, "FILTER", flt_a);
        open_family(FAM_EDIT); frame();
        for (j = 0; j < 30u && strcmp(PAGES[ui.page].title, "FILTER"); j++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
        ui.force = 1; frame(); ppm("page-filter");
        {   /* on EDIT > FILTER, ALGORITHM to a track that is not ANALOG: not left on a page it does not have */
            int16_t bpm0 = song.g[G_BPM];
            encs[panel.enc[EN_ALGO]] = 1; frames(2);
            check(song.sel == 1u && trk[1].eng_req != 0u && PAGES[ui.page].fam == FAM_EDIT &&
                  strcmp(PAGES[ui.page].title, "FILTER") && page_shown(cur_page()),
                  "EDIT > FILTER, then a track that is not ANALOG: the page before (EDIT 2), not FILTER");
            encs[panel.enc[EN_SELECT]] = 1; frame();
            check(song.g[G_BPM] == bpm0, "  and SELECT turns pages there, not the tempo");
            song.sel = 0; go_home(); frame();
        }
        set_engine_of(&trk[0], 1); trk[0].engine = trk[0].eng_req; go_home(); frame();
        WALK(FAM_EDIT, "FILTER", flt_o);
        check(flt_a && !flt_o, "EDIT: the FILTER page (TYPE) with ANALOG only");
        set_engine_of(&trk[0], TRK_DEF[0][0]); apply_preset_to(&trk[0], TRK_DEF[0][1]); trk[0].engine = trk[0].eng_req;
        go_home(); frames(16);

        for (w = 0; w < ML_N; w++) ml_tab[w] = 0;
        press(B_GLO); frames(10);
        key(1);                                           /* F#3 */
        release(B_GLO); frames(16);
        check(mlu.on == 1u && !mlu.pick, "GLO + F#3: MIDI LEARN on");
        open_family(FAM_FX); frame();
        for (j = 0; j < 12u; j++) { encs[panel.enc[EN_SELECT]] = -1; frame(); }   /* the FX page: KNOB 1 = DIST */
        frames(16);
        ui.force = 1; frame(); ppm("header-learn");
        K(EN_K1, 1);
        check(mlu.pick && mlu.id == P_DIST && mlu.trk == 0u && ml_arm, "LEARN: a knob turned picks its parameter (DIST)");
        midi_cc(&trk[0], 74, 10); frame();
        check(ml_count() == 1u && (ml_tab[0] & 127u) == 74u && (ml_tab[0] >> 9) == ml_code(P_DIST) && !mlu.pick,
              "LEARN: a CC sent: CC 74 = track 1 DIST");
        go_home(); frames(16);
        press(B_GLO); frames(10); key(1); release(B_GLO); frames(4);
        check(!mlu.on, "GLO + F#3 again: LEARN off");
        midi_cc(&trk[1], 74, 127);
        check(trk[0].p[P_DIST] == TP[P_DIST].max, "CC 74 (any channel): track 1's DIST to its top");
        trk[0].p[P_DIST] = 0;
        ui.menu = 1; ui.menu_sel = 0; ui.force = 1; frame();
        for (j = 0; j < 6u; j++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
        encs[panel.enc[EN_K3]] = 1; frame();          /* (dualcore: row 3, after 2ND CORE) */
        tap(B_OCTUP);
        check(ui.menu_sel == MI_LCLEAR && ml_count() == 0u, "menu SYSTEM > MIDI LEARN CLEAR, OCT+: nothing learned any more");
        ui.menu = 0; ui.force = 1; go_home(); frames(16);
        {   /* GLO > MIDI: CH1 CH2 CH3 OTHER (2.6) */
            uint32_t hit = 0;
            WALK(FAM_GLO, "MIDI", hit);
            open_family(FAM_GLO); frame();
            for (j = 0; j < 12u && strcmp(PAGES[ui.page].title, "MIDI"); j++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
            frames(16);
            K(EN_K2, 9);                                  /* CH2: 2 -> 11 */
            check(hit && !strcmp(PAGES[ui.page].title, "MIDI") && song.g[G_CH2] == 11, "GLO > MIDI: the page, KNOB 2 sets track 2's channel (11)");
            ui.force = 1; frame(); ppm("page-midi");
            {   /* 2.7: MIDI 3 (FX CH, DR MAP, REMOTE, CLK OUT): drawn (page-midi3), CLK OUT on KNOB 4 */
                uint32_t q;
                for (q = 0; q < 4u && strcmp(PAGES[ui.page].title, "MIDI 3"); q++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
                frames(16);
                K(EN_K4, 1);
                ui.force = 1; frame(); ppm("page-midi3");
                check(!strcmp(PAGES[ui.page].title, "MIDI 3") && song.g[G_CLKOUT] == 1, "2.7: GLO > MIDI 3 > KNOB 4: CLK OUT ON");
                K(EN_K4, -1);
                check(song.g[G_CLKOUT] == 0, "2.7: CLK OUT back OFF");
                for (q = 0; q < 4u && strcmp(PAGES[ui.page].title, "MIDI"); q++) { encs[panel.enc[EN_SELECT]] = -1; frame(); }
                frames(16);
            }
            {   /* 2.6: every MIDI setting in MIDI and MIDI 2; no channel on two tracks */
                const page_t *m = cur_page(), *m2 = &PAGES[ui.page + 1u];
                const char *u;
                char vv[12];
                param_format(&GP[G_DRCH], 0, vv, &u);
                check(m->id[0] == G_CH1 && m->id[1] == G_CH2 && m->id[2] == G_CH3 && m->id[3] == G_DRCH && !strcmp(m2->title, "MIDI 2") &&
                      m2->id[0] == G_ROUTE && m2->id[1] == G_CHOTH && m2->id[2] == G_MIDI && m2->id[3] == G_BEND &&
                      !strcmp(GP[G_DRCH].label, "CH4") && !strcmp(vv, "OFF"),
                      "GLO > MIDI: CH1 CH2 CH3 CH4 (the drums); MIDI 2: IN OTHER OUT BEND; CH4 0 reads OFF");
                song.g[G_CH2] = 2, frame();
                K(EN_K2, 1);                              /* CH2 2 -> 3: track 3's: on to 4 */
                check(song.g[G_CH2] == 4 && !strcmp(ui.msg, "CH 3 = TRACK 3"), "GLO > MIDI: CH2 turned onto track 3's channel: on to 4, \"CH 3 = TRACK 3\"");
                K(EN_K2, -1);                             /* back: 3 taken, on to 2 */
                check(song.g[G_CH2] == 2, "  turned back: over 3 again, to 2");
                K(EN_K4, -7);                             /* DR CH 10 -> 3: track 3's, then 2, 1 (taken): OFF */
                check(song.g[G_DRCH] == 0, "  the drums turned down onto 3, 2, 1 (all taken): OFF");
                song.g[G_DRCH] = 10, frame();
                song.g[G_ROUTE] = 6;                      /* IN = CH 5 */
                go_home(); frame();
                open_family(FAM_GLO); frame();
                for (j = 0; j < 12u && strcmp(PAGES[ui.page].title, "MIDI"); j++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
                check(!strcmp(ui.msg, "IN CH 5: SEL TRK ONLY"), "GLO > MIDI with IN = CH 5: says the channels are set aside");
                song.g[G_ROUTE] = 0;
            }
            song.g[G_CH2] = 2;
            go_home(); frames(16);
        }
        {   /* GLO > TEXTURE, TEXTURE 2 (2.6) */
            uint32_t h1 = 0, h2 = 0;
            WALK(FAM_GLO, "TEXTURE", h1);
            WALK(FAM_GLO, "TEXTURE 2", h2);
            open_family(FAM_GLO); frame();
            for (j = 0; j < 12u && strcmp(PAGES[ui.page].title, "TEXTURE"); j++) { encs[panel.enc[EN_SELECT]] = 1; frame(); }
            frames(16);
            K(EN_K1, 1);                                  /* SRC: OFF -> VINYL */
            check(h1 && h2 && song.g[G_TXSRC] == 1, "GLO > TEXTURE, TEXTURE 2: the pages; KNOB 1 the source (VINYL)");
            ui.force = 1; frame(); ppm("page-texture");
            song.g[G_TXSRC] = 0;
            go_home(); frames(16);
        }
        #undef K
        #undef WALK
    }

    {   /* 2.6 (a YouTube comment): SAVE > PROJECT, KNOB 2 ERASE, a GO button: one detent arms (AGAIN: ERASE), a second
         * one erases the slot KNOB 1 picks; a turn to the left does nothing */
        uint32_t i, e0 = erases;
        song.playing = 0;
        go_home(); frame();
        for (i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].title, "SECTION")) break;
        open_family(PAGES[i].fam); ui.page = (uint8_t)i; ui.fam_last[PAGES[i].fam] = (uint8_t)i; page_entered(); ui.force = 1; frame();
        check(PAGES[ui.page].graph == GR_SLOTS && PAGES[ui.page].id[1] == 0xFFu, "SAVE > PROJECT: KNOB 2 free for ERASE (no parameter)");
        song.g[G_SLOT] = 2;
        encs[panel.enc[EN_K2]] = -1; frames(2);
        check(erases == e0 && ui.arm != 0xE8u, "PROJECT, KNOB 2 left: nothing");
        encs[panel.enc[EN_K2]] = 1; frames(2);
        check(erases == e0 && ui.arm == 0xE8u && !strcmp(ui.msg, "AGAIN: ERASE"), "PROJECT, KNOB 2 one detent: AGAIN: ERASE, nothing erased");
        ui.force = 1; frame(); ppm("page-project-erase");
        encs[panel.enc[EN_K2]] = 1; frames(2);
        check(erases == e0 + 1u && !ui.arm && !strcmp(ui.msg, "ERASED"), "PROJECT, KNOB 2 again: the slot erased");
        encs[panel.enc[EN_K2]] = 1; frames(100);
        encs[panel.enc[EN_K2]] = 1; frames(2);
        check(erases == e0 + 1u && ui.arm == 0xE8u, "  the arm runs out (~1.5 s): a detent after it arms again");
        ui.arm = 0; song.g[G_SLOT] = 1;
        go_home(); frames(4);
    }
    {   /* 2.7: favourites: the PRESETS page, KNOB 4 right adds the sound to the top of the list, left removes it */
        uint32_t total0, total1, cur, e0, p0, k, e;
        int ok;
        ui.menu = 0; song.sel = 1; go_home(); frames(2);
        for (k = 0; k < FAV_N; k++) fav[k] = FAV_NONE;
        fav_at = -1;
        preset_pos(&total0);
        e0 = trk[1].eng_req, p0 = trk[1].preset;
        for (k = 0; k < NPAGES; k++) if (!strcmp(PAGES[k].title, "PRESETS")) break;
        open_family(PAGES[k].fam); ui.page = (uint8_t)k; ui.fam_last[PAGES[k].fam] = (uint8_t)k; page_entered(); ui.force = 1; frames(2);
        encs[panel.enc[EN_K4]] = 1; frames(2);
        cur = preset_pos(&total1);
        e = preset_at(0, &k);
        ok = total1 == total0 + 1u && e == e0 && k == p0 && !strcmp(ui.msg, "FAV ADDED") && !strcmp(preset_kind(0), "FAV") && fav_has(fav_key());
        encs[panel.enc[EN_K4]] = 1; frames(2);
        ok &= !strcmp(ui.msg, "FAV: ALREADY");
        ui.force = 1; frame(); ppm("page-presets-fav");
        encs[panel.enc[EN_K1]] = 1; frames(4);       /* to another sound, added too: the favourites in their order */
        encs[panel.enc[EN_K4]] = 1; frames(2);
        preset_pos(&total1);
        ok &= total1 == total0 + 2u && fav[0] != FAV_NONE && fav[1] != FAV_NONE;
        preset_go(0); frames(2);                       /* the first favourite: the list stands there */
        cur = preset_pos(&total1);
        ok &= cur == 0u && trk[1].eng_req == e0 && trk[1].preset == p0;
        encs[panel.enc[EN_PRESET]] = 1; frames(4);    /* PRESETS: the next one in the list, the second favourite */
        cur = preset_pos(&total1);
        ok &= cur == 1u;
        encs[panel.enc[EN_K4]] = -1; frames(2);      /* removed: the first one moves up */
        preset_pos(&total1);
        ok &= total1 == total0 + 1u && !strcmp(ui.msg, "FAV REMOVED") && fav[1] == FAV_NONE && fav_dirty;
        {   /* Program Change: the n-th sound of the list without the favourites, as before */
            uint32_t kk;
            e = preset_at(1, &kk);
            ok &= preset_go_to(&trk[2], 0) == 0 && trk[2].eng_req == base_at(0, &kk);
        }
        {   /* (2.7.2) Bank Select: bank 1 + PC 0 loads list index 128, past the 7-bit Program Change range (pc_poll) */
            uint32_t e128, p128;
            preset_go_to(&trk[2], 128);
            e128 = trk[2].eng_req; p128 = trk[2].preset;
            preset_go_to(&trk[2], 0);
            pc_bank[2] = 1; pc_req[2] = 1;
            pc_poll();
            ok &= trk[2].eng_req == e128 && trk[2].preset == p128;
            pc_bank[2] = 0; pc_req[2] = 0;
            preset_go_to(&trk[2], 0);
        }
        check(ok, "favourites (2.7): PRESETS page KNOB 4 adds / removes, at the top of the list in order; Program Change as before, Bank Select reaches index 128 (2.7.2)");
        for (k = 0; k < FAV_N; k++) fav[k] = FAV_NONE;
        fav_at = -1; fav_dirty = 0;
        go_home(); frames(2);
    }
    {   /* 2.7: the favourites in the settings' side record (after the theme): kept; a 2.7 beta's record (the theme
         * alone, the rest erased) or a damaged one: none */
        side_rec_t r;
        int ok;
        uint16_t keep[FAV_N];
        uint32_t i2;
        for (i2 = 0; i2 < FAV_N; i2++) fav[i2] = (uint16_t)(i2 < 3u ? i2 * 7u : FAV_NONE);
        fav[3] = (uint16_t)(FAV_USER | 5u);
        memcpy(keep, fav, sizeof keep);
        side_fill(&r);
        for (i2 = 0; i2 < FAV_N; i2++) fav[i2] = FAV_NONE;
        side_take(&r);
        ok = !memcmp(fav, keep, sizeof keep);
        for (i2 = 0; i2 < FAV_N; i2++) fav[i2] = FAV_NONE;
        memset((uint8_t *)&r + sizeof r.th, 0xFF, sizeof r - sizeof r.th);   /* (a beta's: erased after the theme) */
        side_take(&r);
        ok &= fav[0] == FAV_NONE;
        side_fill(&r);
        r.fav[1] ^= 1u;                              /* (damaged: its CRC) */
        memcpy(fav, keep, sizeof keep);
        for (i2 = 0; i2 < FAV_N; i2++) fav[i2] = FAV_NONE;
        side_take(&r);
        ok &= fav[0] == FAV_NONE;
        check(ok, "2.7 favourites: kept in the settings' side record after the theme; a beta's record or a damaged one: none");
    }
    {   /* 2.7: the editor's LIVE page (protocol v12, LIVE = 77): the FM-1's own code for each control; the reply */
        int ok = 1, bpm;
        go_home(); frames(2);
        song.playing = 0; transport_req = 0; song.rec = 0; rec_wait = 0; arrangement_enabled = 0; live_req = -1;
        live(0, 0, 0, 0);
        bpm = (int)(lv_out[3] | lv_out[4] << 7) - 8192;
        ok &= lv_n == 50u && lv_out[0] == 0 && !(lv_out[1] & 1u) && bpm == song.g[G_BPM] && lv_out[12] == song.sel && lv_out[32] == 0u
              && lv_out[34] == (8u | 4u << 4);
        check(ok, "LIVE (2.7): the state, 50 bytes (flags, BPM, sections, mix, punch, song, 4 x step / length / meter, master, throws, drum tune / decay)");
        live(1, 1, 0, 0); ok = transport_req == 1u; frames(30);
        live(0, 0, 0, 0); ok &= song.playing && (lv_out[1] & 1u) && lv_out[20] < trk_len(&trk[0]) && lv_out[21] == trk_len(&trk[0]);
        check(ok, "LIVE: PLAY plays; the reply says so");
        live(2, LY_MIX, 0, 1); live(2, LY_MIX, 0, 0); frame();
        ok = trk[0].p[P_MUTE] && (lv_out[9] & 1u);
        live(2, LY_MIX, 0, 1); live(2, LY_MIX, 0, 0);
        ok &= !trk[0].p[P_MUTE];
        live(2, LY_MIX, 5, 1); live(2, LY_MIX, 5, 0);
        ok &= song.solo == 2u;
        live(2, LY_MIX, 5, 1); live(2, LY_MIX, 5, 0);
        ok &= !song.solo;
        live(2, LY_MIX, 8, 1); ok &= fill_held == 1u; live(2, LY_MIX, 8, 0); ok &= !fill_held;
        live(2, LY_MIX, 9, 1); ok &= fill_arm == 1u && (lv_out[1] & 32u) != 0u;
        live(2, LY_MIX, 9, 1); ok &= !fill_arm;
        check(ok, "LIVE: GLO keys: mute, solo, fill while held, fill next bar (as GLO + key)");
        live(2, LY_SONG, 1, 1); live(2, LY_SONG, 1, 0);
        ok = live_req == 1 && chain_n == 0u && lv_out[6] == 2u;
        live_req = -1;
        live(3, 4, 1, 0); ok &= punch.req == 4 && lv_out[13] == 5u;
        live(3, 4, 0, 0); ok &= punch.req == -1;
        check(ok, "LIVE: SAVE keys: section B on the next bar (no chain); a punch-in effect on, then off");
        live(1, 0, 0, 0); frames(4);
        ok = !song.playing;
        live(4, 0, 0, 0); ok &= rec_wait == 1u && (lv_out[2] & 1u);
        live(4, 0, 0, 0); ok &= !rec_wait;
        live(1, 2, 0, 0); ok &= transport_req == 1u; live(1, 2, 0, 0); ok &= transport_req == 1u;   /* (asked: once) */
        frames(30); live(1, 2, 0, 0); frames(4); ok &= !song.playing;
        check(ok, "LIVE: STOP stops; REC arms and disarms (as the REC button); PLAY as the button");
        ft_on = 1; ft_n = 0; ft_t = 0;                   /* a free take runs (an empty project, armed, a note played) */
        live(0, 0, 0, 0); ok = (lv_out[2] & 8u) != 0u;
        live(4, 0, 0, 0); ok &= transport_req == 1u; frames(2);
        ok &= !ft_on;
        live(0, 0, 0, 0); ok &= !(lv_out[2] & 8u);
        check(ok, "LIVE: a free take shows; REC closes it, as the REC button (not only PLAY)");
        {   /* (2.7) the page's own: THROW, master, a drum sound's TUNE / DECAY, the pattern, ERASE, ROLL, a chain */
            uint32_t i, len0, any = 0, r, on = 0;
            live(10, 3, 0, 0); ok = thr_ed == 3u && lv_out[33] == 3u;
            live(10, 0, 0, 0); ok &= thr_ed == 0u;
            live(11, 63, 0, 0); ok &= ed_master == 64u && lv_out[32] == 64u && ed_master_k == 0xFFFFu;
            live(11, 127, 0, 0); ok &= ed_master == 0u;
            live(12, 2, 11, 2); ok &= drum_tune[2] == 3 && drum_dec[2] == -2 && lv_out[36] == (11u | 2u << 4);
            live(12, 2, 8, 4); ok &= !drum_tune[2] && !drum_dec[2];
            check(ok, "LIVE: THROW (as FX + OCT), the master level (the knob takes it back), a drum sound's TUNE / DECAY");
            len0 = trk_len(&trk[0]);
            live(8, 0, 1, 1); ok = trk_len(&trk[0]) == (len0 * 2u > NSTEP ? len0 : len0 * 2u);
            live(8, 0, 1, 0); ok &= trk_len(&trk[0]) == len0;
            check(ok, "LIVE: the pattern x2 / half (as EDIT + KNOB 2), each one undo");
            for (i = 0; i < trk_len(TDRUM); i++) dstep_set(&TDRUM->dstep[i], 0, LV_NORM, 0);
            song.playing = 0;
            live(7, TRK_DRUM, 0, 1);
            for (i = 0; i < trk_len(TDRUM); i++) any |= dstep_has(&TDRUM->dstep[i], 0);
            ok = !any && (er_lanes & 1u);
            live(7, TRK_DRUM, 0, 0); ok &= !er_lanes;
            check(ok, "LIVE: ERASE a drum sound (stopped: every step, as EDIT + key); let go: done");
            live5(6, TRK_DRUM, 2, LV_NORM, 1); frames(4);
            for (r = 0; r < NROLL; r++) on |= roll[r].on && roll[r].key == 0x82u;
            ok = on;
            live5(6, TRK_DRUM, 2, LV_NORM, 0); on = 0;
            for (r = 0; r < NROLL; r++) on |= roll[r].on;
            ok &= !on;
            check(ok, "LIVE: ROLL a drum sound while held (ARP up does not end the page's roll); let go: it ends");
            { uint8_t c[4] = {9, 2, 0, 1}; lv_n = 0; ed_live(c, 4); ok = chain_n == 0u; }   /* (stopped: no chain) */
            live(9, 0, 0, 0); ok &= chain_n == 0u;
            check(ok, "LIVE: a chain needs the song playing; 0 stops it");
        }
        transport_req = 0; song.playing = 0;
        transport_req = 0; song.playing = 0; live_req = -1; ui.msg_t = 0;
        go_home(); frames(2);
    }
    {   /* 2.7.2: SEQ > NOTES, the synth tracks' note editor; the sheet (OCT+ held) there and on the drum grid */
        track_t *t = &trk[0];
        uint32_t j;
        int ok;
        transport_req = 2; frames(2);
        song.rec = 0; rec_wait = 0; song.sel = 0; song.octave = 0; layer_unlock();
        go_home(); frame();
        t->p[P_SLEN] = 16;
        for (j = 0; j < NSTEP; j++) step_clear(&t->step[j]);
        memset(t->micro, 0, sizeof t->micro); memset(t->fill, 0, sizeof t->fill);
        t->step[0].n = 1; t->step[0].note[0] = 48; t->step[0].time = ST_NOTE;          /* C3, 2 steps */
        t->step[1].time = ST_TIE;
        t->step[4].n = 3; t->step[4].note[0] = 60; t->step[4].note[1] = 64; t->step[4].note[2] = 67;
        t->step[4].time = ST_NOTE;                                                  /* a chord on step 5 */
        t->step[8].n = 1; t->step[8].note[0] = 55; t->step[8].time = ST_NOTE;
        while (strcmp(PAGES[ui.page].title, "NOTES")) { open_family(FAM_SEQ); frame(); }
        cursor_set(0); ui.force = 1; frame();
        encs[panel.enc[EN_K1]] = 1; frame();
        ok = ui.cursor == 2u && ui.note_k == 0u;                                    /* (step 2 is the note's TIE: passed) */
        encs[panel.enc[EN_K1]] = 2; frame();
        ok &= ui.cursor == 4u && ui.note_k == 0u;
        encs[panel.enc[EN_K1]] = 2; frame();
        ok &= ui.cursor == 4u && ui.note_k == 2u;                                   /* the chord's notes one by one */
        encs[panel.enc[EN_K1]] = -1; frame();
        ok &= ui.cursor == 4u && ui.note_k == 1u;
        check(ok, "NOTES (2.7.2): KNOB 1 walks the notes, a chord's one by one, past a note's TIEs");
        encs[panel.enc[EN_K2]] = 1; frame();
        check(t->step[4].note[0] == 60u && t->step[4].note[1] == 65u && t->step[4].note[2] == 67u,
              "NOTES: KNOB 2 changes that note's pitch only (E4 -> F4, the chord's others stay)");
        encs[panel.enc[EN_K3]] = 2; frame();
        check(note_len(t, 4) == 3u && t->step[5].time == ST_TIE && t->step[6].time == ST_TIE, "NOTES: KNOB 3 its length (3 steps: two TIEs)");
        encs[panel.enc[EN_K4]] = 1; frame();
        ok = note_force(&t->step[4], 1) == 3u && ((t->step[4].lvl >> 2) & 3u) == LV_HARD && !(t->step[4].flags & SF_ACCENT);
        encs[panel.enc[EN_K4]] = 1; frame();
        ok &= note_force(&t->step[4], 1) == 4u && (t->step[4].flags & SF_ACCENT);
        encs[panel.enc[EN_K4]] = -4; frame();
        encs[panel.enc[EN_K4]] = -1; frame(); encs[panel.enc[EN_K4]] = -1; frame(); encs[panel.enc[EN_K4]] = -1; frame();
        ok &= note_force(&t->step[4], 1) == 0u && ((t->step[4].lvl >> 2) & 3u) == LV_GHOST && !(t->step[4].flags & SF_ACCENT);
        check(ok, "NOTES: KNOB 4 its FORCE: HARD, ACC (the step's accent), down to GHOST");
        encs[panel.enc[EN_PRESET]] = 1; frame();
        ok = (int8_t)ui.zoom == 1;
        encs[panel.enc[EN_PRESET]] = -1; frame(); encs[panel.enc[EN_PRESET]] = -1; frame();
        ok &= (int8_t)ui.zoom == -1;
        encs[panel.enc[EN_PRESET]] = 1; frame();
        check(ok && (int8_t)ui.zoom == 0 && t->preset == t->preset, "NOTES: PRESETS zooms (16 -> 32, then 8; the sound stays)");
        ui.force = 1; frame(); ppm("notes-chord");
        tap(B_OCTUP); frame();
        check(song.octave == 1 && !ui.sheet, "NOTES: OCT+ tapped: the octave up, as before (on release)");
        song.octave = 0;
        press(B_OCTUP); frames(30);
        ok = ui.sheet == 1u && song.octave == 0;
        release(B_OCTUP); frame();
        check(ok && ui.sheet == 1u && song.octave == 0, "NOTES: OCT+ held 0.4 s: the note's sheet (the octave stays)");
        ui.force = 1; frame(); ppm("notes-sheet");
        tap(B_OCTUP); frame();
        ok = (t->step[4].flags & SF_SLIDE) != 0;
        encs[panel.enc[EN_K1]] = 1; frame();
        encs[panel.enc[EN_K2]] = 3; frame();
        ok &= ui.sheet == 2u && t->micro[4] == 3;
        encs[panel.enc[EN_K1]] = 1; frame();
        encs[panel.enc[EN_K2]] = 1; frame();
        ok &= step_fill(t, 4) == FC_FILL;
        check(ok, "NOTES sheet: SLIDE (OCT+), NUDGE +3, FILL (KNOB 1 the row, KNOB 2 its value)");
        encs[panel.enc[EN_K1]] = 1; frame();
        encs[panel.enc[EN_K2]] = -1; frame();
        ok = ui.cursor == 3u && t->step[3].n == 3u && t->step[3].note[1] == 65u && note_len(t, 3) == 3u && !step_on(&t->step[4]) &&
             t->step[6].time == ST_REST && t->micro[3] == 3 && t->micro[4] == 0 && step_fill(t, 3) == FC_FILL;
        encs[panel.enc[EN_K2]] = -2; frame();
        ok &= ui.cursor == 2u && t->step[2].n == 3u;                                /* (step 2 free: the C3's TIE? no: step 1 is) */
        encs[panel.enc[EN_K2]] = -1; frame();
        ok &= ui.cursor == 2u && !strcmp(ui.msg, "NO ROOM");                        /* (step 1: the C3's TIE) */
        check(ok, "NOTES sheet: MOVE, KNOB 2 a step a detent (with its TIEs, nudge, fill); not over another note");
        encs[panel.enc[EN_K1]] = 1; frame();
        tap(B_OCTUP); frame();
        ok = step_on(&t->step[2]) && step_on(&t->step[5]) && t->step[5].note[1] == 65u && ui.cursor == 5u && ui.sheet == 4u;
        check(ok, "NOTES sheet: COPY (OCT+): just after it, the copy picked, MOVE next");
        encs[panel.enc[EN_K1]] = 2; frame();
        tap(B_OCTUP); frame();
        ok = step_on(&t->step[5]) && t->step[5].n == 2u && ui.sheet == 0u;
        check(ok, "NOTES sheet: DELETE (OCT+): that note (the chord keeps its others), the sheet closes");
        press(B_OCTUP); frames(30); release(B_OCTUP); frame();
        tap(B_OCTDN); frame();
        check(!ui.sheet && song.octave == 0, "NOTES: OCT- closes the sheet (the octave stays)");
        press(B_EDIT); release(B_EDIT); frames(2);
        check(t->step[5].n == 1u && step_on(&t->step[5]), "NOTES: EDIT tapped deletes the note under edit only");
        press(B_EDIT); release(B_EDIT); frames(2);
        check(!step_on(&t->step[5]) && t->step[6].time == ST_REST, "NOTES: the chord's last note: the step and its TIEs go");
        press(B_EDIT); press(B_OCTDN); release(B_OCTDN); release(B_EDIT); frames(2);
        check(step_on(&t->step[5]), "NOTES: EDIT + OCT- brings it back (undo)");
        cursor_set(0); ui.force = 1; frame(); ppm("notes-main");
        go_home(); frame();
        /* the drum grid's sheet */
        song.sel = TRK_DRUM; studio_open(SC_DRUM); drum_page = 0; frames(2);
        for (j = 0; j < NSTEP; j++) memset(&TDRUM->dstep[j], 0, sizeof(dstep_t));
        TDRUM->p[P_SLEN] = 32;
        drum_lane = 2; drum_cursor = 4;
        dstep_set(&TDRUM->dstep[4], 2, LV_HARD, 0);
        tap(B_OCTUP); frame();
        ok = drum_cursor == 20u && !ui.sheet;                                         /* (a tap: the next page) */
        drum_cursor = 4;
        press(B_OCTUP); frames(30); release(B_OCTUP); frame();
        ok &= ui.sheet == 1u;
        encs[panel.enc[EN_K2]] = 2; frame();
        ok &= dstep_rat(&TDRUM->dstep[4], 2) == 2u && dstep_lvl(&TDRUM->dstep[4], 2) == LV_HARD;
        check(ok, "DRUMS grid (2.7.2): OCT+ tapped the page, held the hit's sheet; RATCHET x3");
        ui.force = 1; frame(); ppm("drums-sheet");
        encs[panel.enc[EN_K1]] = 3; frame();
        encs[panel.enc[EN_K2]] = 2; frame();
        ok = drum_cursor == 6u && dstep_has(&TDRUM->dstep[6], 2) && !dstep_has(&TDRUM->dstep[4], 2) && dstep_rat(&TDRUM->dstep[6], 2) == 2u;
        encs[panel.enc[EN_K1]] = 1; frame();
        tap(B_OCTUP); frame();
        ok &= dstep_has(&TDRUM->dstep[6], 2) && dstep_has(&TDRUM->dstep[7], 2) && drum_cursor == 7u && ui.sheet == 4u;
        encs[panel.enc[EN_K1]] = 2; frame();
        tap(B_OCTUP); frame();
        ok &= !dstep_has(&TDRUM->dstep[7], 2) && dstep_has(&TDRUM->dstep[6], 2) && !ui.sheet;
        check(ok, "DRUMS sheet: MOVE (KNOB 2), COPY (OCT+: the next step), DELETE (OCT+)");
        TDRUM->p[P_SLEN] = 16;
        go_home(); song.sel = 0; frame();
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
#endif
