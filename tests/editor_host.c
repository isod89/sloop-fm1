/* SPDX-License-Identifier: GPL-3.0-only */
/* Host harness for the web editor's SysEx protocol (firmware/src/editor.c, editor_fm6.c, editor_dsyn.c,
 * editor_live.c) with the real storage.c / upreset.c / project.c (fm6_bank.c, arranger_scene.c) on a RAM image
 * of the 1 MiB NOR, the real usb.c SysEx assembly (FELUCCA_OTA), the real UI (as tests/ui_pages_test.c) and the
 * audio (mix_block) between frames. Included by tests/editor_fuzz.c. Same include order as felucca.c. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define FELUCCA_ARRANGER 1
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 1
/* the flash: a RAM image, read where it is (the XIP window) */
static uint8_t nor[0x100000] __attribute__((aligned(4096)));
#define SMP_USER_XIP(k) ((const uint8_t *)nor + SMP_USER_OFF((uint32_t)(k)))
#define FM6_BANK_XIP(copy) ((const uint8_t *)nor + st_sector(OBJ_FM6BANK, copy) + ST_PAYLOAD_OFF)
#define ST_XIP_PTR(off) ((const uint8_t *)nor + (off))
/* (the user slots' zones are rebased onto SMP_DATA, eng_sample.c smp_user_scan: one 32-bit offset space, as the flash
 * is on the FM-1, so their reads index SMP_DATA past its own size into the flash image. Under -fsanitize=bounds, build
 * with -I to a copy of build/gen/felucca_samples.h whose SMP_DATA is read through a pointer: tests/editor_fuzz.sh) */
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
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
/* ---- felucca.c's flash part, on the RAM image (as hal/fm1_flash.h: ranges, one page a program) */
#define FL_DATA_LO 0x00097000u
#define FL_DATA_HI 0x000E0000u
#define FL_GLOB_LO 0x000FC000u
#define FL_GLOB_HI 0x000FF000u
#define FL_OTA_LO 0x000E0000u
#define FL_OTA_HI 0x000E5000u
#define FL_FM6_LO 0x000E5000u
#define FL_FM6_HI 0x000E7000u
#define FL_USR4_LO 0x000E7000u
#define FL_USR4_HI 0x000FB000u
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && (uint32_t)(n) <= (hi) - (uint32_t)(off))
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI) || \
                             FL_IN(off, n, FL_FM6_LO, FL_FM6_HI) || FL_IN(off, n, FL_USR4_LO, FL_USR4_HI))
#define FL_RANGE_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI))
#define FL_FAR(fn) fn
static uint32_t fl_erases, fl_progs, fl_bad;          /* counters; fl_bad: a refused (out of range) access */
static int fl_fail_at = -1;                           /* > 0: the n-th program from now fails (a torn write) */
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static int fl_erase4k_ram(uint32_t off, uint32_t *took)
{
    *took = 0;
    if ((off & 0xFFFu) || !FL_RANGE_OK(off, 0x1000u)) { fl_bad++; return -1; }
    memset(nor + off, 0xFF, 0x1000u);
    fl_erases++;
    return 0;
}
static int fl_prog_ram(uint32_t off, const uint8_t *src, uint32_t n, uint32_t *took)
{
    uint32_t i;
    *took = 0;
    if (!FL_RANGE_OK(off, n) || n == 0 || n > 256u || ((off & 0xFFu) + n) > 256u) { fl_bad++; return -1; }
    if (fl_fail_at > 0 && --fl_fail_at == 0) { fl_fail_at = -1; return -3; }
    for (i = 0; i < n; i++)
        nor[off + i] &= src[i];
    fl_progs++;
    return 0;
}
static int fl_read_ram(uint32_t off, uint8_t *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off) { fl_bad++; return -1; }
    memcpy(dst, nor + off, n);
    return 0;
}
static int fl_write(uint32_t off, const uint8_t *src_ram, uint32_t n)
{
    uint32_t took;
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu);
        int rc;
        if (chunk > n) chunk = n;
        rc = fl_prog_ram(off, src_ram, chunk, &took);
        if (rc) return rc;
        off += chunk; src_ram += chunk; n -= chunk;
    }
    return 0;
}
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n;
        int rc = fl_read_ram(off, d, k);
        if (rc) return rc;
        off += k; d += k; n -= k;
    }
    return 0;
}
static void audio_silence(void) {}
static int fl_erase4k_quiet(uint32_t off, uint32_t *took) { audio_silence(); return fl_erase4k_ram(off, took); }
static int st_erase(uint32_t off) { uint32_t took; if (!FL_STORE_OK(off, 0x1000u)) return -8; return fl_erase4k_quiet(off, &took); }
static int st_prog(uint32_t off, const void *src, uint32_t n) { if (!FL_STORE_OK(off, n)) return -8; return fl_write(off, src, n); }
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
/* ---- the ota hooks usb.c needs (felucca.c) */
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void rx_take(void);
static void ota_idle(void) { rx_take(); }             /* the host takes the replies */
#include "../firmware/src/editor.c"
#include "../firmware/src/splash.c"

/* ---- the harness: frames, replies */
static uint8_t rx_last[1024];
static uint32_t rx_n;                                  /* the last reply (F0..F7), from the SysEx ring */
static void rx_take(void)
{
    while (so_r != so_w) {
        uint32_t pkt = sx_out_q[so_r % SXQ], cin = pkt & 15u, k, nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        so_r++;
        for (k = 0; k < nb; k++) {
            uint8_t b = (uint8_t)(pkt >> (8u * (k + 1u)));
            if (b == 0xF0u) rx_n = 0;
            if (rx_n < sizeof rx_last) rx_last[rx_n++] = b;
        }
    }
}
/* a frame of n bytes (between F0 and F7) straight into the frame buffer, as usb.c leaves it: ed_service */
static void ed_inject(const uint8_t *f, uint32_t n)
{
    if (n > sizeof sx_frame) n = sizeof sx_frame;
    memcpy(sx_frame, f, n);
    sx_frame_len = n;
    sx_ready = 1;
    rx_n = 0;
    ed_service();
    rx_take();
    sx_ready = 0;                                      /* (ota_service would take one that is not the editor's) */
}
/* the same through USB-MIDI packets (usb.c midi_in_event / sysex_byte) */
static void usb_sysex(const uint8_t *f, uint32_t n)
{
    uint8_t b[700];
    uint32_t m = 0, i;
    b[m++] = 0xF0;
    for (i = 0; i < n && m < sizeof b - 1; i++) b[m++] = f[i];
    b[m++] = 0xF7;
    for (i = 0; i < m; i += 3) {
        uint32_t k = m - i >= 3 ? 3 : m - i, cin = k == 3 && i + 3 < m ? 4u : k == 3 ? 7u : 4u + k;
        uint32_t pkt = cin | (uint32_t)b[i] << 8 | (k > 1 ? (uint32_t)b[i + 1] << 16 : 0) | (k > 2 ? (uint32_t)b[i + 2] << 24 : 0);
        midi_in_event(pkt);
    }
    rx_n = 0;
    ed_service();
    rx_take();
    sx_ready = 0;
}
static int32_t audio_peak;
static uint32_t audio_bad;
static void audio_blocks(uint32_t nb)
{
    static int32_t o[CTL * 2];
    uint32_t q, i;
    for (q = 0; q < nb; q++) {
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = o[i] < 0 ? -o[i] : o[i];
            if (a > audio_peak) audio_peak = a;
            if (a > 40000) audio_bad++;
        }
        for (i = 1; i < CTL; i += 2u) {
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
    }
}
static void ui_frame(void)
{
    audio_blocks(22);
    ed_service();
    ui_input(); ui_leds(); ui_draw();
    autosave_tick();
    sections_flush();
    fm1_ms += 16;
}

static void harness_init(void)
{
    uint32_t i;
    memset(nor, 0xFF, sizeof nor);
    if ((uintptr_t)nor < (uintptr_t)SMP_DATA || (uintptr_t)nor - (uintptr_t)SMP_DATA > 0xF0000000u) {
        printf("editor: the flash image must lie above SMP_DATA within 4 GiB (32-bit offsets)\n");
        exit(2);
    }
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    persist_boot();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    usb.config = 1;
}

/* every parameter of every track inside its range (as the editor / a load must leave them) */
static uint32_t vel_over;                              /* steps with a velocity > 127 seen (a restored project) */
static int params_check(char *why, size_t nw)
{
    uint32_t k, i;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        for (i = 0; i < P_COUNT; i++) {
            const param_desc_t *d = k == TRK_DRUM && i == P_E0 ? &DRUM_KIT_DESC :
                                    i >= P_E0 && i <= P_E7 ? &ENGINES[t->eng_req % NENGINES]->edit[i - P_E0] : &TP[i];
            if (k == TRK_DRUM && i > P_E0 && i <= P_E7)
                continue;                                   /* (the drum track has no engine values) */
            if (k != TRK_DRUM && i >= P_E0 && t->engine != t->eng_req)
                continue;                                   /* (an engine switch fading: the values are the next one's) */
            if (t->p[i] < d->min || t->p[i] > d->max) {
                snprintf(why, nw, "trk %u p[%u] (%s) = %d outside %d..%d (eng %u)", k, i, d->label, t->p[i], d->min, d->max,
                         t->eng_req);
                return 0;
            }
        }
        if (t->eng_req >= NENGINES) { snprintf(why, nw, "trk %u eng_req %u", k, t->eng_req); return 0; }
        for (i = 0; i < NLOCK; i++) {
            const plock_t *l = &t->lock[i];
            const param_desc_t *d;
            if (l->step == LOCK_FREE) continue;
            if (l->step >= NSTEP || l->param >= P_COUNT) { snprintf(why, nw, "trk %u lock %u step %u param %u", k, i, l->step, l->param); return 0; }
            d = k == TRK_DRUM && l->param == P_E0 ? &DRUM_KIT_DESC :
                l->param >= P_E0 && l->param <= P_E7 ? 0 : &TP[l->param];
            if (d && (l->val < d->min || l->val > d->max)) {   /* (an engine value: clamped when it plays, seq.c lock_write) */
                snprintf(why, nw, "trk %u lock %u (step %u, %s) = %d outside %d..%d", k, i, l->step, d->label, l->val, d->min, d->max);
                return 0;
            }
        }
        if (!is_drum(t))
            for (i = 0; i < NSTEP; i++) {
                const step_t *s = &t->step[i];
                uint32_t j;
                if (s->vel > 127u) vel_over++;
                if (s->n > 4u || s->time > ST_REST) { snprintf(why, nw, "trk %u step %u n %u time %u vel %u", k, i, s->n, s->time, s->vel); return 0; }
                for (j = 0; j < 4u; j++)
                    if (s->note[j] > 127u) { snprintf(why, nw, "trk %u step %u note %u", k, i, s->note[j]); return 0; }
            }
        for (i = 0; i < NSTEP; i++)
            if (t->micro[i] < MICRO_MIN || t->micro[i] > MICRO_MAX) { snprintf(why, nw, "trk %u micro[%u] %d", k, i, t->micro[i]); return 0; }
    }
    for (i = 0; i < G_COUNT; i++)
        if (song.g[i] < GP[i].min || song.g[i] > GP[i].max) {
            snprintf(why, nw, "g[%u] (%s) = %d outside %d..%d", i, GP[i].label, song.g[i], GP[i].min, GP[i].max);
            return 0;
        }
    if (song.sel >= NTRK) { snprintf(why, nw, "sel %u", song.sel); return 0; }
    return 1;
}
