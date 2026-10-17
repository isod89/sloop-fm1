/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP in the browser: the firmware's sources compiled to WebAssembly and run in an AudioWorklet
 * against a simulated FM-1.
 *
 * The design is Felucca's web/emu/felucca_web.c (GPL-3.0, Leo Kuroshita / Hügelton Instruments),
 * itself after X0X's web/emu/x0x_web.c (charlesvestal/fm1-x0x, GPL-3.0). The include order is SLOOP's
 * own, firmware/src/felucca.c.
 *
 *   - the clock: TIMER4 ticks (24 MHz) and fm1_ms advance with the audio the worklet asks for; the
 *     audio ISR (audio.c) renders each I2S half as the DMA would ask for it, and one pass of main.c's
 *     loop runs every 15 ms of device time, with ui_input on every millisecond in between;
 *   - the panel: buttons and encoders by their printed labels, the keys as fm1_in.notes, the LEDs read
 *     back from fm1_led / fm1_led_dim / fm1_led_bg;
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame;
 *   - the flash: a 1 MiB NOR image behind storage.c's hooks (settings, projects, user presets, the user
 *     sample slots read through the XIP window); the page keeps its sectors and gives them back at boot.
 *
 * Not here: USB (MIDI, audio, the editor, updates), TRS MIDI, the serial console, the second core, the
 * battery, the crash / UBOOT paths. Nothing of the firmware changes for this file.
 *   web/emu/build.sh -> build/emu/{sloop.wasm, worklet.js, index.html, fonts/} */
#include <stdint.h>

#define FELUCCA_WEB 1
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1
#endif
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#define FELUCCA_CPU1 0
#define FELUCCA_ARRANGER 1        /* the sections, the chain and SONG REC live in seq.c's FELUCCA_ARRANGER block */
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "WEB"
#endif

/* ---------------------------------------------------------------- NOR ---- */
#define NOR_SIZE 0x100000u
static uint8_t nor[NOR_SIZE];

/* --------------------------------------------------------------- libc ---- */
/* the firmware's own libc (src/libc.c), renamed so it never meets the host's; the macros STAY defined,
 * so every source below calls it by the usual name */
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../../firmware/src/libc.c"

/* ---------------------------------------------------- panel: in and LEDs --- */
/* the symbols hal/fm1_input.h provides, without its bit-banged matrix */
#define FM1_NCOL 11u
#define FM1_NKEY 41u
#define FM1_NENC 7u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static volatile struct {
    uint32_t notes, buttons;
    uint32_t pressed, released, notes_pressed;
    uint32_t frames;
} fm1_in;
static volatile uint32_t fm1_remote_btn;
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_bg[FM1_NCOL];
static volatile uint16_t fm1_led_bg_ns;

static uint32_t web_pressed, web_released, web_notes_pressed;
static int32_t web_enc_steps[FM1_NENC];

static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released)
        *released = web_released;
    web_pressed = web_released = 0;
    fm1_in.pressed = 0;
    fm1_in.released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void)
{
    uint32_t n = web_notes_pressed;
    web_notes_pressed = 0;
    fm1_in.notes_pressed = 0;
    return n;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = web_enc_steps[e % FM1_NENC];
    web_enc_steps[e % FM1_NENC] = 0;
    return s;
}
static void fm1_input_init(void) {}
static void fm1_input_scan(void) {}
static void fm1_input_tick(void) {}
static void fm1_led_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void fm1_wdt_feed(void) {}
static void fm1_delay_ms(uint32_t ms) { (void)ms; }

/* ------------------------------------------------------------- the clock --- */
#define FM1_TICKS_PER_US 24u
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* --------------------------------------------------------- audio (ALNK0) --- */
static uint32_t web_half;
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
static void fm1_audio_stop(void) {}
void isr_alnk0(void) {}

/* ----------------------------------------------------------- the screen --- */
static uint16_t web_fb[240 * 240];
static uint32_t web_draws;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = sw;
    web_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}
static void lcd_sync(void) {}
static void lcd_init(void) {}

/* ------------------------------------------------------- firmware: core --- */
#include "../../firmware/src/core.h"
#include "../../firmware/src/cpu1.h"
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static int cpu1_on(void) { return 0; }
static uint32_t cpu1_ticks(void) { return 0; }
static void cpu1_boot(void) {}
static void cpu1_off_now(void) {}

/* the XIP window (hal/fm1_xip.h) reads the flash at CPU address 0x02000000 + off - 0x4000; here the
 * flash IS the NOR image, so the sample engine reads it directly. Included first so eng_sample.c's own
 * include is a no-op, then the name is shadowed for every call site. */
#include "../../firmware/hal/fm1_xip.h"
static const uint8_t *nor_xip(uint32_t off) { return nor + (off & (NOR_SIZE - 1u)); }
#define fm1_xip_ptr(off) nor_xip(off)

#include "../../firmware/src/engines.c"
#include "../../firmware/src/drums.c"
#include "../../firmware/src/params.c"
#include "../../firmware/src/voice.c"
#include "../../firmware/src/slicer.c"
#include "../../firmware/src/fx.c"
#include "../../firmware/src/usb.c"
#include "../../firmware/src/midi_uart.c"
#include "../../firmware/src/arranger.c"
#include "../../firmware/src/seq.c"
#include "../../firmware/src/audio.c"

/* ------------------------------------------------------------- the panel --- */
#include "../../firmware/src/gfx.c"
#include "../../firmware/src/panel.c"
#include "../../firmware/src/ui.c"
#include "../../firmware/src/ui_song.c"
#include "../../firmware/src/ui_studio.c"
#include "../../firmware/src/icons.c"
#include "../../firmware/src/ui_draw.c"
#include "../../firmware/src/ui_vis.c"
#include "../../firmware/src/ui_layers.c"
#include "../../firmware/src/ui_menu.c"
#include "../../firmware/src/ui_input.c"

/* ------------------------------------------------------------- the flash --- */
static uint32_t web_flash_writes;
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= NOR_SIZE || n > NOR_SIZE - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off >= NOR_SIZE || 4096u > NOR_SIZE - off)
        return -8;
    memset(nor + (off & ~4095u), 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off >= NOR_SIZE || n > NOR_SIZE - off)
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#define ST_XIP_PTR(off) ((const void *)nor_xip(off))   /* storage.c st_xip: read the object where it is */
#include "../../firmware/src/storage.c"
#include "../../firmware/src/upreset.c"
#include "../../firmware/src/project.c"        /* (fm6_bank.c comes in from project.c) */

/* --------------------------------------------- main.c and its HAL stubs --- */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static uint32_t web_master_adc = 700;
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? (int32_t)web_master_adc : 620; }
static void fm1_adc_init(void) {}
static void fm1_timer5_ack(void) {}
static void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)isr; (void)prio; }
void isr_timer5(void) {}
static void fm1_reboot(void) {}
static void fm1_enter_uboot(void) {}
static void fm1_guard_lock_top(void) {}
static void fm1_irq_enable_all(void) {}
static void fm1_irq_init(void) {}
static void fm1_time_init(void) {}
static void fm1_reset_reason(void) {}
static void fm1_wdt_arm(uint32_t v) { (void)v; }
static void fm1_mailbox_clear(void) {}
static void fm1_guard_enable(uint32_t m) { (void)m; }
enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
#if FELUCCA_CDC
static void cdc_task(void) {}
#endif
/* the symbols main.c expects from the linker script and the HAL (hal/fm1_irq.h, hal/fm1_sys.h,
 * app.ld); nothing of the firmware changes for this file */
#define FM1_CRASH_MAGIC 0x43525348u
typedef struct {
    uint32_t magic, count, vec, pc, rets, emu, dbg, sp, psr, icfg, uptime_ms;
    uint32_t etm[4];
    uint32_t early;
} fm1_crash_t;
static fm1_crash_t fm1_crash;
static struct {
    uint8_t p3_rst;
    uint32_t rst_src;
    uint8_t wdt_con;
} fm1_boot;
static uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
static uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];
static char _ustack_lo[1], _ustack_top[1], _sstack_lo[1], _sstack_top[1];
static uint32_t fm1_sp(void) { return 0; }
static uint32_t fm1_ssp(void) { return 0; }
static uint32_t fm1_usp(void) { return 0; }
static void fm1_supplies_stock(void) {}
static uint8_t cpu1_state, cpu1_timeouts;
/* the modules the page leaves out (USB updates, the editor's SysEx, the serial console): main.c's loop
 * names them, and never runs here */
static void ota_service(void) {}
static void ota_session(void) {}
static void ed_service(void) {}

#define usb_poll() ((void)0)
#define usb_start() ((void)0)
#define usb_retry(ms) ((void)(ms))
#define usb_detach() ((void)0)
#include "../../firmware/src/splash.c"      /* (felucca.c's order: splash before main, which calls sloop_splash) */
#include "../../firmware/src/main.c"
#undef usb_poll
#undef usb_start
#undef usb_retry
#undef usb_detach

/* ------------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame;

static void web_power_on(void)
{
    persist_boot();
    settings_init();
    lcd_init();
    sloop_splash();
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    felucca_dbg.magic = DBG_MAGIC;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    felucca_init();
    cpu1_boot();
    audio_init();
    web_boot_ms = fm1_ms + 430u;
    web_booted = 1;
}

static void web_frame(void)
{
    static int32_t knob = 512 * 16;                  /* main.c's, across the loop's passes */
    int32_t b = fm1_adc_read(FM1_ADC_BATT);
    if (b > 0)
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);    /* main.c: the MASTER pot, 0..1023 -> master_q12 */
        if (a >= 0) {
            uint32_t k10;
            knob += (a * 16 - knob) / 8;
            k10 = (uint32_t)(knob / 16);
            song.master_q12 = (k10 * k10) >> 8;
        }
    }
    felucca_dbg.ui_frames++;
    felucca_dbg.page = ui.page;
    felucca_dbg.home = ui.home;
    felucca_dbg.stage = 1;
    ui_input();
    felucca_dbg.stage = 2;
    ui_leds();
    ui_draw();
    felucca_dbg.stage = 8;
    autosave_tick();                                 /* the working project into flash, when quiet */
    sections_flush();                                /* live sections / the recorded song, when quiet */
    felucca_dbg.stage = 9;
}

/* output ring, stereo float: web_ms renders the audio halves into it, web_render drains it */
#define RING 16384u
static float ring[RING * 2];
/* free-running sample counters (index with % RING): ring_w - ring_r is how much is waiting. Comparing
 * wrapped indices instead reads "not empty" for ever after one write and the clock stops. */
static uint32_t ring_w, ring_r, web_samples_due;
/* PLANAR, as the AudioWorklet reads it: web_out_l holds the left channel, web_out_r the right. An
 * interleaved buffer here plays the two channels as if they were one, and it sounds like noise. */
static float web_pl[4096], web_pr[4096];

/* one millisecond of the device: the clock, the main loop when its 15 ms are up, the audio halves due */
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    if (web_booted) {
        ui_input();                                  /* as main.c's wait loop: every millisecond */
        if ((uint32_t)(fm1_ms - web_boot_ms) >= 15u) {
            web_boot_ms = fm1_ms;
            web_frame();
        }
    }
    web_samples_due += FS;
    while (web_samples_due >= 1000u * HALF_FRAMES && ring_w - ring_r <= RING - HALF_FRAMES) {
        uint32_t k;
        const int32_t *o = &abuf[web_half * HALF_WORDS];
        web_samples_due -= 1000u * HALF_FRAMES;
        fm1_alnk0_irq();
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t w = ring_w % RING;
            ring[w * 2u + 0u] = (float)o[k * 2u + 0u] * (1.0f / 8388608.0f);
            ring[w * 2u + 1u] = (float)o[k * 2u + 1u] * (1.0f / 8388608.0f);
            ring_w++;
        }
        web_half ^= 1u;
    }
}

/* --------------------------------------------------------------- exports --- */
#define EXPORT(name) __attribute__((used, visibility("default"))) name
EXPORT(uint32_t *) web_nor(void) { return (uint32_t *)nor; }
EXPORT(uint32_t) web_nor_size(void) { return NOR_SIZE; }
EXPORT(void) web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT(uint32_t) web_flash_writes_count(void) { return web_flash_writes; }
EXPORT(void) web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
EXPORT(void) web_boot(void) { if (!web_booted) web_power_on(); }
EXPORT(uint32_t) web_sample_rate(void) { return FS; }
EXPORT(uint32_t) web_now_ms(void) { return fm1_ms; }

/* The page counts by LABEL (bit i = button i of panel.c's enum; bit k = note key k), the firmware by
 * MATRIX id (panel.btn[] maps a label to one, and the note keys are ids 14..40). Both conversions are
 * here, so neither side has to know about the other. */
EXPORT(void) web_buttons(uint32_t mask)
{
    uint32_t i, m = 0, ch;
    for (i = 0; i < NB; i++)
        if ((mask >> i) & 1u)
            m |= 1u << (panel.btn[i] & 31u);
    ch = m ^ fm1_in.buttons;
    web_pressed |= ch & m;
    web_released |= ch & ~m;
    fm1_in.pressed |= ch & m;
    fm1_in.released |= ch & ~m;
    fm1_in.buttons = m;
}
EXPORT(void) web_keys(uint32_t mask)   /* bit k = note key k (0 = F3 .. 26 = G5): the page's own counting */
{
    uint32_t m = mask & 0x07FFFFFFu, ch = m & ~fm1_in.notes;
    web_notes_pressed |= ch;
    fm1_in.notes_pressed |= ch;
    fm1_in.notes = m;
}
EXPORT(void) web_enc(uint32_t role, int32_t n) { web_enc_steps[panel.enc[role % 7u] % FM1_NENC] += n; }
EXPORT(void) web_midi(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }

/* ---------------------------------------------- files from the page: songs, backups --- */
/* The page hands an editor object over here (the editor's ids: 0 the working loop, 1 the settings, 2..5 the
 * sections A-D, 6/7 the user preset banks, 8 the FM6 bank), and it is written the way the editor's BK_PUT
 * writes it. A .sloopsong's objects are the firmware's own structs, so nothing is parsed here. */
static uint8_t web_upload[0x20000];                  /* 128 KiB: one object at a time */
EXPORT(uint32_t) web_upload_buf(void) { return (uint32_t)(uintptr_t)web_upload; }
EXPORT(uint32_t) web_upload_size(void) { return (uint32_t)sizeof web_upload; }

EXPORT(int32_t) web_obj_put(uint32_t id, uint32_t n)
{
    if (n > sizeof web_upload)
        return -1;
    if (id == 0u) return st_save(OBJ_AUTOSAVE, web_upload, n);
    if (id == 1u) return st_save(OBJ_SETTINGS, web_upload, n);
    if (id >= 2u && id <= 5u) return st_save(OBJ_PROJECT0 + (id - 2u), web_upload, n);
    if (id >= 6u && id <= 7u) return st_save(OBJ_UPRESET0 + (id - 6u), web_upload, n);
    if (id == 8u) return st_save(OBJ_FM6BANK, web_upload, n);
    return -2;                                       /* (9..11 live inside the settings object) */
}

/* a .sloopsong's song order: 36 bytes at offset 48 of the settings object (the editor's SONGF.ORDER_OFF) */
EXPORT(int32_t) web_order_put(uint32_t n)
{
    static persist_t p;
    int32_t len;
    if (n != 36u)
        return -1;
    len = st_load(OBJ_SETTINGS, &p, (uint32_t)sizeof p);
    if (len < 48 + 36) {                             /* nothing saved yet in this browser: the settings as
                                                      * they are now, so the song order has somewhere to go */
        persist_fill(&p);
        len = (int32_t)sizeof p;
    }
    memcpy((uint8_t *)&p + 48, web_upload, 36);
    return st_save(OBJ_SETTINGS, &p, (uint32_t)len);
}

/* for the tests: what the firmware's debounce holds (the matrix's own bit numbers), and a storage object's
 * CRC as it is in the flash now (storage.c's ids: 0 settings, 1..4 sections, 5/6 presets, 7 the autosave,
 * 8 the FM6 bank), 0 when it is not there */
EXPORT(uint32_t) web_debug_obj_crc(uint32_t obj)
{
    static uint8_t buf[ST_PAYLOAD_MAX];
    int n = st_load(obj, buf, (uint32_t)sizeof buf);
    return n < 0 ? 0u : st_crc32(buf, (uint32_t)n);
}
EXPORT(uint32_t) web_debug_buttons(void) { return fm1_in.buttons; }
EXPORT(uint32_t) web_debug_notes(void) { return fm1_in.notes; }

EXPORT(uint32_t *) web_screen(void) { return (uint32_t *)web_fb; }
EXPORT(uint32_t) web_screen_draws(void) { return web_draws; }
/* the LEDs, as the scan would light them: bit i = button i, bit (14 + k) = note key k */
/* the LED layers are addressed by matrix id (FM1_KEYMAP's 0..13 buttons, 14..40 note keys); the page
 * counts by label (bit i = button i) and by key number (bit k = key k), so both are translated here */
static uint32_t led_buttons(const uint8_t *lm)
{
    uint32_t c, r, m = 0;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if ((lm[c] & (1u << r)) != 0u && FM1_KEYMAP[r][c] >= 0) {
                uint32_t id = (uint32_t)FM1_KEYMAP[r][c];
                if (id < 14u) {
                    uint32_t lbl = panel_btn_of(id);
                    if (lbl < NB)
                        m |= 1u << lbl;
                }
            }
    return m;
}
static uint32_t led_keys(const uint8_t *lm)
{
    uint32_t c, r, m = 0;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if ((lm[c] & (1u << r)) != 0u && FM1_KEYMAP[r][c] >= 14)
                m |= 1u << ((uint32_t)FM1_KEYMAP[r][c] - 14u);
    return m;
}
EXPORT(uint32_t) web_lit_buttons(void) { return led_buttons(fm1_led); }
EXPORT(uint32_t) web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT(uint32_t) web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT(uint32_t) web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT(uint32_t) web_dim_level(void) { return fm1_led_bg_ns; }
/* SLOOP 2.7 has no breathing or mid LED layers (those are Felucca's); the page asks for them anyway */
EXPORT(uint32_t) web_breath_buttons(void) { return 0; }
EXPORT(uint32_t) web_breath_keys(void) { return 0; }
EXPORT(uint32_t) web_mid_keys(void) { return 0; }

EXPORT(uint32_t) web_out_l(void) { return (uint32_t)(uintptr_t)web_pl; }
EXPORT(uint32_t) web_out_r(void) { return (uint32_t)(uintptr_t)web_pr; }

EXPORT(void) web_render(uint32_t n)
{
    uint32_t i;
    if (!web_booted)
        return;
    if (n > 4096u)
        n = 4096u;
    for (i = 0; i < n; i++) {
        uint32_t r;
        while (ring_r == ring_w)
            web_ms();                       /* the device runs until there is a sample to hand over */
        r = ring_r % RING;
        web_pl[i] = ring[r * 2u + 0u];
        web_pr[i] = ring[r * 2u + 1u];
        ring_r++;
    }
}
