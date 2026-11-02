/* SPDX-License-Identifier: GPL-3.0-only */
/* The song screen's confirmations (ui_song.c): OCT+ (load) and REC over a used section (replace) ask for a second
 * press within 3 s. The arm was compared with a deadline as a signed difference: one left unconfirmed read as still
 * armed 24.9 to 49.7 days later (a single press loaded or replaced a section). Now the time since the arm.
 * Exit status: failed checks. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i,j;
    assert(x+w<=240 && y+h<=240);
    for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i];
}
#include "../firmware/src/gfx.c"
enum {B_FX,B_SCL,B_ENV,B_LFO,B_EDIT,B_GLO,B_HOME,B_SAVE,B_ARP,B_SEQ,B_PLAY,B_REC,B_OCTDN,B_OCTUP,NB};
enum {EN_SELECT,EN_ALGO,EN_PRESET,EN_K1,EN_K2,EN_K3,EN_K4,NE};
static struct {uint8_t btn[NB];} panel;
static struct {uint8_t home,page,force,msg_t;char msg[24];} ui;
static int32_t enc[NE];
static uint32_t ready, scene_saves, order_saves, loads;
static uint32_t arrangement_ready(void) {return ready;}
static void arrangement_apply(uint32_t scene) {(void)scene;}
static uint32_t section_bars(uint32_t s) {(void)s;return 1;}
static const page_t *cur_page(void) {return &PAGES[ui.page];}
static int project_used(uint32_t i) {return (ready>>i)&1u;}
static void project_save(uint32_t i) {ready|=1u<<i;scene_saves++;}
static void project_load(uint32_t i) {(void)i;loads++;}
static void settings_save(void) {}
static void arrangement_save(void) {order_saves++;}
static void ui_message(const char *s) {str_cpy(ui.msg,s,sizeof ui.msg);ui.msg_t=40;}
static void go_home(void) {ui.home=1;}
static void open_family(uint32_t f) {(void)f;ui.home=1;}
static int32_t panel_enc(uint32_t i) {int32_t s=enc[i];enc[i]=0;return s;}
static void song_backup(void) {}
static void song_restore(void) {}
#include "../firmware/src/ui_song.c"
static void press(uint32_t b) {song_screen_input(1u<<panel.btn[b],0);}
int main(void)
{
    uint32_t i, fails = 0, s0, l0;
    arr_defaults(&arrangement);
    for(i=0;i<NB;i++) panel.btn[i]=(uint8_t)i;
    for(i=0;i<NPAGES;i++) if(PAGES[i].scope==SC_SONG) ui.page=(uint8_t)i;
    ready = 1;                                        /* section A saved */
    fm1_ms = 1000000u;
    press(B_OCTUP);                                   /* "OCT+ AGAIN: LOAD", then left */
    fm1_ms += 0x80000000u + 5000u;                    /* 24.9 days later */
    l0 = loads;
    press(B_OCTUP);                                   /* one press: must ask again, not load */
    printf("song_arm: OCT+ once, 24.9 days after an OCT+ left unconfirmed: %s (%s)\n", loads == l0 ? "asks again" : "LOADED at once", ui.msg);
    fails += loads != l0;
    press(B_OCTDN);                                   /* (no arm left: LOOP / SONG MODE toggled) */
    s0 = scene_saves;
    fm1_ms = 2000000u;
    press(B_REC);                                     /* "REC AGAIN: REPLACE", then left */
    fm1_ms += 0x80000000u + 5000u;
    press(B_REC);                                     /* one press: must ask again, not replace */
    printf("song_arm: REC once, 24.9 days after a REC left unconfirmed: %s (%s)\n", scene_saves == s0 ? "asks again" : "REPLACED at once", ui.msg);
    fails += scene_saves != s0;
    return (int)fails;
}
