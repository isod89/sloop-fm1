/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP (based on FELUCCA): one compilation unit (the HAL is header-only). Order matters. */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#if BOARD_SMK37
#include "smk_input.h"           /* keys, buttons, encoders, LEDs, pads, faders, wheels, battery */
#else
#include "fm1_input.h"
#endif
#include "fm1_timer.h"
#include "fm1_audio.h"
#if !BOARD_SMK37
#include "fm1_adc.h"
#endif
#include "fm1_lcd_hw.h"
#include "fm1_cpu1.h"      /* the second core (X0X): started in main.c, used by the render later */
#include "felucca_tables.h"
#ifndef SNOOP37
#define SNOOP37 0                /* 1: the Snoop37 look: its boot splash, update screens, name (snoop_*) */
#endif
#if SNOOP37
#include "snoop_theme.h"         /* the SNOOP37 name on the screens */
#else
#define BRAND_NAME "SLOOP"
#define BRAND_SAIL_X 96
#define BRAND_BASED "BASED ON FELUCCA"
#define BRAND_CRASH "FELUCCA CRASH"
#define BRAND_RESCUE "SLOOP USB RESCUE"
#endif
#define VIS_STYLES 12u
#define VIS_LAST_NAMES "SLOOP"
#define VIS_N_STR "/12"

#include "libc.c"
#include "lcd.c"
#include "gfx.c"
#include "core.h"
#include "cpu1.h"           /* the second core's job API (defined in cpu1.c; the host tests have their own) */
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"          /* per-track SLICER insert, used by fx.c */
#if SNOOP37
#include "snoop_jingle.c"    /* the boot jingle's synth (mixed in by fx.c mix_block) */
#endif
#include "fx.c"
#ifndef FELUCCA_OTA
#define FELUCCA_OTA 1            /* M-UPGRADE update entry; needs FELUCCA_FLASH */
#endif
#ifndef FELUCCA_OTA_DRYRUN
#define FELUCCA_OTA_DRYRUN 0     /* 1 = stage, ask "success", then undo: no record, no reset */
#endif
#ifndef FELUCCA_CPU1
#define FELUCCA_CPU1 1          /* the second core (cpu1.c); 0: one core, as SLOOP 2.5 */
#endif
#ifndef FELUCCA_ID
#if BOARD_SMK37
#define FELUCCA_ID "SMK-37 Pro_900"   /* package identity (build.py: the .fwsc marker string) */
#else
#define FELUCCA_ID "FM-1_900"    /* package identity (build.py: the .fwsc marker string) */
#endif
#endif
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 0            /* USB CDC-ACM serial console, a developer's tool (menu USB SERIAL). 2.7: out of the FM-1
                                  * build (its 6-8 KB: the flash is full); FELUCCA_CDC=1 builds it back */
#endif
#ifndef FELUCCA_UAC                      /* (before usb.c: it defaults to 0) */
#define FELUCCA_UAC 1            /* USB audio input: the master output, 16-bit stereo 44.1 kHz (usb.c, after Felucca 1.0) */
#endif
#ifndef FELUCCA_UAC_TONE
#define FELUCCA_UAC_TONE 0       /* bench: the USB input sends test triangles instead of the music */
#endif
#if BOARD_SMK37
#include "smk_uart_tx.h"
#include "smk_midi_out.h"     /* SMK-37: TRS MIDI OUT (UART1 TX, PC7), fed by usb.c midi_out_event */
#endif
#include "usb.c"
#ifndef FELUCCA_UART
#if BOARD_SMK37
#define FELUCCA_UART 0           /* SMK-37 Pro: the jack is MIDI OUT (smk_midi_out.h), no MIDI IN; PH8 is a key-matrix
                                  * row there and must never become UART RX */
#else
#define FELUCCA_UART 1           /* TRS MIDI IN on UART1 (3.5 mm jack) */
#endif
#endif
#if BOARD_SMK37 && FELUCCA_UART
#error "BOARD_SMK37: midi_uart.c is the FM-1 MIDI IN driver (RX on PH8, a key-matrix row on the SMK)"
#endif
#if FELUCCA_UART
#include "midi_uart.c"
#endif
#if BOARD_SMK37
#include "smk_vel.h"          /* SMK-37: menu KEY VELOCITY (DYNAMIC, curves, FIXED), read by seq.c */
#include "smk_wheel.h"        /* SMK-37: the wheels, their calibration, menu BEND UP / DOWN (main.c smk_wheels) */
#include "smk_daw.h"          /* SMK-37: DAW mode, the map and its messages (seq.c: keys, pads; ui_daw.c: the rest) */
#endif
#define FELUCCA_ARRANGER 1
#include "arranger.c"
#include "seq.c"
#include "audio.c"
#include "cpu1.c"           /* the second core: boot, self-test, state (ABOUT, console) */
#include "panel.c"
#include "ui.c"
#include "ui_song.c"
#include "ui_studio.c"
#include "icons.c"           /* parameter icons (FELUCCA_ICONS), used by ui_draw.c */
#if BOARD_SMK37
#include "ui_pick.c"          /* SMK-37 Pro: the pad sound picker */
#endif
#include "ui_draw.c"
#include "ui_vis.c"           /* the full-screen visualiser (HOME on HOME, 2.4) */
#include "ui_layers.c"       /* hold a function button: what the keys and knobs do (TE style) */
#include "ui_menu.c"
#if BOARD_SMK37
#include "ui_padgrid.c"       /* SMK-37 Pro: steps and tiles in reading order */
#endif
#include "ui_input.c"
#if BOARD_SMK37
#include "ui_fbank.c"         /* SMK-37 Pro: the FADER BANK screen */
#include "ui_daw.c"           /* SMK-37: DAW mode: its input, screens, lights and map editor */
#endif
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH 1          /* flash driver + storage.c */
#endif
#if FELUCCA_OTA && !FELUCCA_FLASH
#error "FELUCCA_OTA needs FELUCCA_FLASH"
#endif
#if FELUCCA_FLASH
#include "fm1_flash.h"
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* a sector erase with the audio silenced inside the same interrupts-off window (silenced before it, the
 * audio interrupt could render a fresh half that the DMA then loops for the whole erase: a buzz) */
#if BOARD_SMK37
static void st_stock_touch(uint32_t off);        /* storage.c: the stock mapping's end marker, before USRFLASH */
#endif
static int fl_erase4k_quiet(uint32_t off, uint32_t *took)
{
#if BOARD_SMK37
    uint32_t f;
    int rc;
    st_stock_touch(off);                         /* (an erase in USRFLASH: the stock marker goes first) */
    f = irq_save();
#else
    uint32_t f = irq_save();
    int rc;
#endif
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, took);
    irq_restore(f);
    return rc;
}
static int st_erase(uint32_t off)
{
    uint32_t took;
#if BOARD_SMK37
    if (!FL_STORE_OK(off, 0x1000u) && !FL_STOCKTAG_OK(off, 0x1000u))
#else
    if (!FL_STORE_OK(off, 0x1000u))
#endif
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))
        return -8;
    return fl_write(off, src, n);
}
#define ST_XIP_PTR(off) fm1_xip_ptr(off)   /* sloop-lab: storage.c st_xip, an object read where it is (fm1_xip.h) */
#include "storage.c"
#endif
#include "upreset.c"          /* user presets (read in the XIP window; the host test keeps a RAM mirror) */
#include "project.c"
#if FELUCCA_OTA
static uint8_t recovery_active;
#if BOARD_SMK37
#define OTA_IDENTITY (recovery_active ? SMK_MODEL "_000" : FELUCCA_ID)
#else
#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FELUCCA_ID)
#endif
/* 2.7 beta: the update (ota.c) keeps its own buffers, as in 2.6: the update path never shares RAM */
#include "ota.c"
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active) recovery_poll();
}
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
#if SNOOP37
static void snoop_ota(uint32_t step, int32_t code);
#endif
static void ota_show(uint32_t step, int32_t code)
{
#if BOARD_SMK37   /* step 5 stays on screen while the loader writes the flash: say so */
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "UPDATING..."};
#else
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
#endif
    char b[24];
    if (recovery_active) return;             /* keep polled USB alive during OTA */
#if SNOOP37
    snoop_ota(step, code);                   /* (snoop_splash.c) */
    return;
#endif
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_HI, 1);
        return;
    }
    if (code == 1)
        str_cpy(b, "DRY RUN OK", sizeof b);
    else {
        uint32_t k = 0, v = (uint32_t)-code;
        str_cpy(b, "FAILED  -", sizeof b);
        while (b[k])
            k++;
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
    }
    draw_text_box(0, 124, 240, &FONT_S, b, C_HI, 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);                        /* intentional update reset */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);                             /* record into RAM, core reset (fm1_sys.h) */
}
#endif
#if FELUCCA_OTA
#include "editor.c"          /* web editor SysEx (needs the OTA SysEx plumbing) */
#endif
#if FELUCCA_CDC
#include "console.c"
#endif
#if FELUCCA_OTA
#include "recovery.c"        /* early, polled USB updater; no synth or settings */
#endif
#include "splash.c"          /* SLOOP boot logo (build/gen/sloop_logo.h) */
#if SNOOP37
#include "snoop_splash.c"    /* animated SNOOP37 boot splash instead of the logo */
#endif
#include "main.c"
