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
static uint32_t saves, loads;
static int project_used(uint32_t i) { return i < 2; }
static void project_save(uint32_t i) { (void)i; saves++; ui_message("SAVED"); }
static void project_load(uint32_t i) { (void)i; loads++; }
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

/* a CC message on channel ch: status 0xB0 | ch, controller cc, value val */
static uint32_t cc_pkt(uint32_t ch, uint32_t cc, uint32_t val)
{
    return 0xBu | (0xB0u | (ch & 15u)) << 8 | (cc & 127u) << 16 | (val & 127u) << 24;
}
static void send_cc(uint32_t ch, uint32_t cc, uint32_t val)
{
    midi_in_q[mi_w % MQ] = cc_pkt(ch, cc, val);
    mi_w++;
    events_block(CTL);
}

static void midi_cc_tests(void)
{
    printf("midi CC: Felucca 1.1.5 standard map (#103) per channel\n");
    host_tracks_init();
    song.g[G_DRCH] = 10;                              /* drum track on MIDI ch 10 */
    song.g[G_ROUTE] = 0;                             /* normal routing (not clock-only) */

    /* 1. Channel routing to synth parts 1..3 */
    trk[0].p[P_LEVEL] = 50; trk[1].p[P_LEVEL] = 50; trk[2].p[P_LEVEL] = 50;
    send_cc(0, 7, 127);                               /* Ch 1 -> Track 0 */
    check(trk[0].p[P_LEVEL] == 127, "CC 7 on ch 1 sets track 1 level");
    check(trk[1].p[P_LEVEL] == 50 && trk[2].p[P_LEVEL] == 50, "CC 7 on ch 1 leaves other tracks alone");

    send_cc(1, 7, 110);                               /* Ch 2 -> Track 1 */
    check(trk[1].p[P_LEVEL] == 110 && trk[0].p[P_LEVEL] == 127, "CC 7 on ch 2 sets track 2 level");

    send_cc(2, 7, 95);                                /* Ch 3 -> Track 2 */
    check(trk[2].p[P_LEVEL] == 95, "CC 7 on ch 3 sets track 3 level");

    /* 2. Drum channel (ch 10 / index 9) */
    song.g[G_DRLVL] = 40;
    send_cc(9, 7, 120);                               /* CC 7 on drum channel sets G_DRLVL */
    check(song.g[G_DRLVL] == 120, "CC 7 on drum channel sets G_DRLVL");

    send_cc(9, 10, 64);                               /* CC 10 PAN on drum channel */
    check(TDRUM->p[P_PAN] == 0, "CC 10 on drum channel sets drum PAN (centered)");

    song.g[G_DRREV] = 20;
    send_cc(9, 91, 85);                               /* CC 91 REV on drum channel */
    check(song.g[G_DRREV] == 85, "CC 91 on drum channel sets G_DRREV");

    song.g[G_DRDLY] = 15;
    send_cc(9, 94, 75);                               /* CC 94 DLY on drum channel (2.5) */
    check(song.g[G_DRDLY] == 75, "CC 94 on drum channel sets G_DRDLY");

    TDRUM->p[P_TFLT] = 0;
    send_cc(9, 74, 90);                               /* CC 74 on drum channel sets drum track filter */
    check(TDRUM->p[P_TFLT] == 26, "CC 74 on drum channel sets drum track filter P_TFLT");

    /* Synth-only CCs are ignored on the drum track */
    TDRUM->p[P_ATK] = 0;
    send_cc(9, 73, 100);
    check(TDRUM->p[P_ATK] == 0, "CC 73 (ATK) is ignored on drum track");
    TDRUM->p[P_GLIDE] = 0;
    send_cc(9, 5, 100);
    check(TDRUM->p[P_GLIDE] == 0, "CC 5 (GLIDE) is ignored on drum track");

    /* 3. Unassigned channel follows selected track */
    song.sel = 1;                                     /* Track 2 selected */
    trk[1].p[P_REL] = 20;
    send_cc(4, 72, 80);                               /* Ch 5 (unassigned) -> TSEL (Track 2) */
    check(trk[1].p[P_REL] == 80, "CC 72 on unassigned channel reaches selected track");

    /* 4. Standard CC parameters scaling on synth */
    send_cc(0, 5, 127);                               /* CC 5 GLIDE max */
    check(trk[0].p[P_GLIDE] == 127, "CC 5 sets GLIDE to max");
    send_cc(0, 5, 0);                                 /* CC 5 GLIDE min */
    check(trk[0].p[P_GLIDE] == 0, "CC 5 sets GLIDE to min");

    send_cc(0, 10, 0);                                /* CC 10 PAN min (-64) */
    check(trk[0].p[P_PAN] == -64, "CC 10 val 0 scales to PAN -64");
    send_cc(0, 10, 127);                              /* CC 10 PAN max (+63) */
    check(trk[0].p[P_PAN] == 63, "CC 10 val 127 scales to PAN 63");
    send_cc(0, 10, 64);                               /* CC 10 PAN center (0) */
    check(trk[0].p[P_PAN] == 0, "CC 10 val 64 scales to PAN 0");

    send_cc(0, 73, 50); check(trk[0].p[P_ATK] == 50, "CC 73 sets ATK");
    send_cc(0, 75, 60); check(trk[0].p[P_DEC] == 60, "CC 75 sets DEC");
    send_cc(0, 91, 70); check(trk[0].p[P_REV] == 70, "CC 91 sets REV");
    send_cc(0, 93, 80); check(trk[0].p[P_CHOR] == 80, "CC 93 sets CHOR");
    send_cc(0, 94, 90); check(trk[0].p[P_DLY] == 90, "CC 94 sets DLY");

    /* 5. Engine brightness (CC 74) and resonance (CC 71) */
    set_engine_of(&trk[0], 0);                        /* ANALOG */
    send_cc(0, 74, 115);
    check(trk[0].p[P_E4] == 115, "CC 74 sets ANALOG CUT (E4)");
    send_cc(0, 71, 45);
    check(trk[0].p[P_E5] == 45, "CC 71 sets ANALOG RES (E5)");

    set_engine_of(&trk[0], 2);                        /* PHASE */
    send_cc(0, 74, 99);
    check(trk[0].p[P_E2] == 99, "CC 74 sets PHASE DCW (E2)");
    trk[0].p[P_E5] = 12;
    send_cc(0, 71, 55);                               /* PHASE has no resonance */
    check(trk[0].p[P_E5] == 12, "CC 71 ignored for engine without resonance");

    set_engine_of(&trk[0], 9);                        /* FM6 */
    send_cc(0, 74, 127);                              /* bipolar MLVL: 0..127 -> -64..63 */
    check(trk[0].p[P_E2] == 63, "CC 74 sets FM6 MLVL (E2) max");

    set_engine_of(&trk[0], 10);                       /* PHYS (2.5) */
    send_cc(0, 74, 105);
    check(trk[0].p[P_E2] == 105, "CC 74 sets PHYS BRIT (E2)");

    set_engine_of(&trk[0], 11);                       /* NOISE (2.5) */
    send_cc(0, 74, 88);
    check(trk[0].p[P_E2] == 88, "CC 74 sets NOISE FREQ (E2)");
    send_cc(0, 71, 62);
    check(trk[0].p[P_E3] == 62, "CC 71 sets NOISE RES (E3)");

    set_engine_of(&trk[0], 1);                        /* DIGITAL (no internal cut/res, fallback to P_TFLT) */
    trk[0].p[P_TFLT] = 0;
    send_cc(0, 74, 127);
    check(trk[0].p[P_TFLT] == 63, "CC 74 falls back to P_TFLT for engine without internal timbre");

    /* 6. Clock-only mode ignores CCs */
    song.g[G_ROUTE] = 1;                              /* GLO > SYSTEM > IN = CLOCK */
    trk[0].p[P_LEVEL] = 40;
    send_cc(0, 7, 100);
    check(trk[0].p[P_LEVEL] == 40, "CC 7 ignored when IN = CLOCK (G_ROUTE)");
    song.g[G_ROUTE] = 0;

    /* 7. Unmapped CCs are ignored */
    trk[0].p[P_SUS] = 33;
    send_cc(0, 16, 127);                              /* CC 16 unmapped */
    send_cc(0, 64, 127);                              /* CC 64 unmapped */
    check(trk[0].p[P_SUS] == 33, "unmapped CCs do not change parameters");
}

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
    midi_cc_tests();
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
            check(d && !strcmp(d->label, "MIDI") && !strcmp(val, "SEQ"), "GLO > SYSTEM: the first column reads MIDI SEQ");
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
            encs[panel.enc[EN_K3]] = -1; frame();
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
    encs[panel.enc[EN_K2]] = 10; frame(); check(song.g[G_DUST] > 0, "FX + KNOB 2: DUST");
    encs[panel.enc[EN_K1]] = -10; frame(); check(song.g[G_FILT] < 0, "FX + KNOB 1: the filter (low-pass)");
    encs[panel.enc[EN_K4]] = 12; frame(); check(TSEL->p[P_TFLT] > 0 && song.g[G_FILT] < 0, "FX + KNOB 4: the selected track's FILTER (high-pass)");
    ui.force = 1; frame(); ppm("layer-punch-trkflt");
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
        ui.menu_sel = MI_SERIAL; ui.force = 1; frame();
        check(usb_serial == 0u && !(lights_word() >> 16 & 1u), "menu USB SERIAL: OFF by default (2.3 settings read so)");
        encs[panel.enc[EN_K1 + mi_row(ui.menu_sel)]] = 1; frame();
        check(usb_serial == 1u && (lights_word() >> 16 & 1u), "menu USB SERIAL: its knob (KNOB 3) -> ON, saved with the settings");
        ui.force = 1; frame(); ppm("menu-serial");
        lights_from_word(lights_word());
        check(usb_serial == 1u, "menu USB SERIAL: read back");
        tap(B_OCTUP);
        check(usb_serial == 0u && ui.menu == 1, "menu USB SERIAL: OCT+ toggles back to OFF");
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

    {   /* the menu in sections (2.4): SELECT the section, KNOB 1..3 its rows, PRESETS the cursor, OCT+ opens */
        uint32_t sec;
        ui.menu = 1; ui.menu_sel = 0; ui.force = 1; frame(); ppm("menu-screen");
        check(mi_sec(ui.menu_sel) == MS_SCREEN, "menu: opens on SCREEN (COLOR, ZOOM)");
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
        check(mi_sec(ui.menu_sel) == MS_AUDIO, "menu: SELECT right: AUDIO (LOWCUT, USB AUDIO, USB SERIAL)");
        encs[panel.enc[EN_SELECT]] = 5; frame();
        sec = mi_sec(ui.menu_sel);
        check(sec == MS_SYSTEM, "menu: SELECT stops at the last section, SYSTEM");
        encs[panel.enc[EN_K2]] = 1; frame();
        check(ui.menu == 1 && ui.menu_sel == MI_ABOUT, "menu SYSTEM: a knob does not open ABOUT, it moves the cursor");
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
        check(!strcmp(PT(), "LFO DEST") && song.g[G_BPM] == bpm, "SELECT on LFO: next page LFO DEST, the tempo untouched");
        SEL(1);
        check(!strcmp(PT(), "LFO DEST") && song.g[G_BPM] == bpm, "SELECT: stops at the last page of the family");
        SEL(-1);
        check(!strcmp(PT(), "LFO"), "SELECT left: the page before");
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
        check(dstep_has(&TDRUM->dstep[2], 0) && drum_cursor == 2 && pad_lit[0], "grid: white key 3 sets step 3 of the sound (heard), the cursor on it");
        check((keys_lit() >> key_of_white(2)) & 1u, "grid: its key lit");
        key(key_of_white(2));
        check(!dstep_mask(&TDRUM->dstep[2]), "grid: the key again clears it");
        key(3);                                                     /* (G#3: the second black key) */
        check(drum_cursor / 16u == 1u, "grid: the black keys pick the page of steps (17-32)");
        key(key_of_white(0));
        check(dstep_has(&TDRUM->dstep[16], 0), "grid: ... and the white keys are its steps (step 17)");
        key(8);                                                     /* (C#4: the fourth: steps 49-64, none) */
        check(drum_cursor / 16u == 1u, "grid: a page past LENGTH is not picked");
        memset(pad_lit, 0, sizeof pad_lit);
        encs[panel.enc[EN_K1]] = 3; frames(2);
        check(drum_lane == 3 && pad_lit[3], "grid: KNOB 1 picks the sound and plays it");
        dstep_set(&TDRUM->dstep[20], 5, LV_NORM, 0);
        dstep_set(&TDRUM->dstep[20], 7, LV_SOFT, 0);
        memset(pad_lit, 0, sizeof pad_lit);
        drum_cursor = 19; encs[panel.enc[EN_K2]] = 1; frames(2);
        check(drum_cursor == 20 && pad_lit[5] && pad_lit[7] && !pad_lit[3], "grid: KNOB 2 onto a step plays what it holds");
        encs[panel.enc[EN_SELECT]] = 1; frame();
        check(drum_page == 1, "DRUMS: SELECT right: kit");
        frame();
        memset(pad_lit, 0, sizeof pad_lit);
        key(key_of_white(9)); frame();
        check(pad_lit[9] && !dstep_has(&TDRUM->dstep[25], 3), "kit: the keys play the pads again, no step set");
        encs[panel.enc[EN_SELECT]] = -1; frame();
        check(drum_page == 0, "DRUMS: SELECT left: grid");
        go_home(); frame();
        memset(pad_lit, 0, sizeof pad_lit);
        pen_lane = 0;
        press(B_SEQ); frames(3);
        encs[panel.enc[EN_K1]] = 2; frames(2);
        check(pen_lane == 2 && pad_lit[2], "SEQ layer, drum track: KNOB 1 picks the sound and plays it");
        release(B_SEQ); frames(2);
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
        for (j = 0; j < NPAGES; j++) if (!strcmp(PAGES[j].title, "STEP")) break;
        open_family(FAM_SEQ); ui.page = (uint8_t)j; ui.fam_last[FAM_SEQ] = (uint8_t)j; page_entered(); ui.force = 1; frames(2);
        b0 = ui.page;
        for (j = 0; j < 3u; j++) { encs[panel.enc[EN_ALGO]] = 1; frames(2); }
        check(song.sel == TRK_DRUM && ui.page == b0 && !page_for_drum(cur_page()) && !strcmp(PAGES[ui.page].title, "STEP"),
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
        for (j = 0; j < NPAGES; j++) if (!strcmp(PAGES[j].title, "STEP")) break;
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
