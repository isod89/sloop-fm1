/* (from ui_pages_test.c's harness) SPDX-License-Identifier: GPL-3.0-only */
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
static int32_t enc_late[7];                         /* a detent the timer ISR counts just after a read (as on the FM-1, where the
                                                     * scan runs while a UI pass does): it lands in the same pass */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = enc_late[e]; enc_late[e] = 0; return s; }
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

/* SLOOP 2.8.3: SEQ > NOTES, LEN under later notes (POLY), the MONO and STEP FULL messages, shrink and delete */
static int fails;
static void check(int ok, const char *what) { printf("len: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static void put(track_t *t, uint32_t i, uint32_t note, uint32_t ties)
{
    uint32_t k;
    memset(&t->step[i], 0, sizeof t->step[i]);
    t->step[i].note[0] = (uint8_t)note; t->step[i].n = 1; t->step[i].time = ST_NOTE;
    for (k = 1; k <= ties; k++) { memset(&t->step[i + k], 0, sizeof t->step[i]); t->step[i + k].time = ST_TIE; }
}
static void dump(track_t *t, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        step_t *x = &t->step[i];
        printf("   %2u %s", i, x->time == ST_TIE ? "TIE " : x->time == ST_REST ? "REST" : "NOTE");
        for (k = 0; k < x->n; k++) printf(" %u%s", x->note[k], (x->flags & SF_CONT(k)) ? "c" : "");
        printf("\n");
    }
}
static uint64_t blk;
static void block(void) { int32_t out[CTL * 2]; mix_block(out, CTL); blk++; }
static uint32_t gated_blocks(track_t *t, uint32_t note, uint32_t from, uint32_t to, uint32_t *restarts)
{
    uint32_t SB = div_samples(2) / CTL, k, i, n = 0, age = 0;
    *restarts = 0;
    for (k = 0; k < 16u * SB; k++) {
        block();
        if (k / SB < from || k / SB >= to) continue;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].gate && t->v[i].note == note) {
                n++;
                if (age && t->v[i].age != age) (*restarts)++;
                age = t->v[i].age;
                break;
            }
    }
    return n;
}
static void melody(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }
    put(t, 0, 36, 1);                               /* C1, two steps */
    put(t, 2, 48, 1); put(t, 4, 50, 1); put(t, 6, 52, 1);   /* C2 D2 E2 over it, two steps each */
}
int main(void)
{
    track_t *t;
    uint32_t n, r, g, SB;
    host_tracks_init();
    t = &trk[1];
    host_preset(t, 1, 0);                           /* DIGITAL RHODES: POLY */
    t->p[P_SLEN] = 16;
    song.sel = 1;
    SB = div_samples(2) / CTL;
    melody(t);
    check(note_span(t, 0) == 2, "C1: LEN 2 to start with");
    n = note_span_set(t, 0, 10);
    if (getenv("DUMP")) dump(t, 12);
    check(n == 10 && note_span(t, 0) == 10, "C1: LEN 10 under C2 D2 E2 (2.8.1: NOTE AHEAD at 2)");
    check(note_span(t, 2) == 2 && note_span(t, 4) == 2 && note_span(t, 6) == 2, "the melody's notes keep their LEN 2");
    transport_req = 1;
    block();
    g = gated_blocks(t, 36, 0, 10, &r);
    check(g >= 10u * SB - 4u && !r, "plays: C1 sounds 10 steps in one go under the melody");
    transport_req = 2; block();
    for (n = 0; n < 40; n++) block();
    transport_req = 1; block();
    g = gated_blocks(t, 50, 4, 6, &r);
    check(g >= 2u * SB - 4u, "plays: D2 sounds its 2 steps over it");
    transport_req = 2; block();
    n = note_span_set(t, 0, 5);
    if (getenv("DUMP")) dump(t, 12);
    check(n == 5 && note_span(t, 0) == 5 && note_span(t, 4) == 2 && note_span(t, 6) == 2, "shorter: LEN 5 ends inside D2, D2 keeps its length");
    n = note_span_set(t, 0, 2);
    check(n == 2 && t->step[2].n == 1 && t->step[4].n == 1 && t->step[6].n == 1 && t->step[5].time == ST_TIE && t->step[7].time == ST_TIE,
          "back to LEN 2: the melody exactly as it was");
    note_span_set(t, 0, 9);
    note_del(t, 0, 0);
    check(!step_on(&t->step[0]) && t->step[2].n == 1 && t->step[4].n == 1 && t->step[6].n == 1 && t->step[8].time != ST_NOTE,
          "C1 deleted: nothing of it left under the melody");
    /* MONO: one note at a time */
    melody(t);
    t->p[P_VOICE] = V_MONO;
    n = note_span_set(t, 0, 10);
    check(n == 2 && span_why == 2u, "MONO sound: stops at C2, says MONO: SET VOICE POLY");
    t->p[P_VOICE] = V_POLY;
    /* a full step: 4 notes there already */
    melody(t);
    t->step[2].note[1] = 55; t->step[2].note[2] = 59; t->step[2].note[3] = 62; t->step[2].n = 4;
    n = note_span_set(t, 0, 10);
    check(n == 2 && span_why == 1u, "a step with 4 notes: stops, says STEP FULL");
    /* no notes ahead: as 2.8.1 (clean TIEs) */
    melody(t);
    n = note_span_set(t, 6, 6);
    check(n == 6 && t->step[8].time == ST_TIE && t->step[11].time == ST_TIE && t->step[12].time == ST_REST, "nothing ahead: plain TIEs, as before");
    printf("len: %d failed\n", fails);
    return fails;
}
