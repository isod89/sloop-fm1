/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA boot and main loop. Boot order: WDT first, boot-loop guard, fatal
 * vectors, guards; then LCD, input (TIMER5 IRQ, 10 kHz), audio (ALNK0 IRQ). */
#ifndef UI_SMALL                 /* (2.7) compiled for size: not the audio path (gfx.c UI_SMALL); the host tests: nothing */
#define UI_SMALL
#endif
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
extern uint32_t _c1_start[], _c1_end[], _c1_load[];
extern uint32_t _lr_start[], _lr_end[];

/* .lowram (app.ld): the SRAM between the second core's loop and the boot status words at 0x01C07F00, which no
 * FM-1 firmware had used before (Felucca's map keeps it for RAM-run debug, which SLOOP has no build for). It holds
 * the reverb lines, the second core's stacks and the render split's scratch (fx.c), so the split costs the RAM
 * region nothing. Proved once at boot: every word takes and gives back a pattern, then the region is zeroed as
 * .pool is. Console: lowram (1 proved, 2 a word failed: the build must not rely on this SRAM). */
static uint8_t lowram_ok;
UI_SMALL static uint8_t lowram_test(void)
{
    uint32_t *d, bad = 0;
    for (d = _lr_start; d < _lr_end; d++)
        *d = ~(uint32_t)(uintptr_t)d;
    for (d = _lr_start; d < _lr_end; d++)
        bad |= *d != ~(uint32_t)(uintptr_t)d;
    for (d = _lr_start; d < _lr_end; d++)
        *d = 0;
    return bad ? 2u : 1u;
}
UI_SMALL static uint32_t lowram_state(void) { return lowram_ok; }   /* console.c */

/* core 0's two stacks (app.ld: 24 KB from _ustack_lo, 8 KB from _sstack_lo), never measured before: painted at boot
 * with a mark up to 512 B below whichever of sp / ssp / usp sits in each region (the live frames), so the console
 * can say how deep each has gone since (stack_free, sstack_free: bytes above the guard band never reached) and which
 * region the main loop runs on (sp). What they say decides whether _guard0 may move up and the pool grow. */
extern char _ustack_top[];
#define STACK_MARK 0x4B4D5453u                  /* "STMK" */
UI_SMALL static void stack_paint(char *lo, char *top)
{
    uint32_t a = (uint32_t)(uintptr_t)lo, lim = (uint32_t)(uintptr_t)top, p[3], i, *d;
    p[0] = fm1_sp();                            /* (fm1_guard.h) */
    p[1] = fm1_ssp();
    p[2] = fm1_usp();
    for (i = 0; i < 3u; i++)
        if (p[i] >= a + 512u && p[i] < lim + 512u && p[i] - 512u < lim)
            lim = p[i] - 512u;
    for (d = (uint32_t *)lo; (uint32_t)(uintptr_t)d < lim; d++)
        *d = STACK_MARK;
}
UI_SMALL static uint32_t stack_free(const char *lo, const char *top)   /* bytes of marks still in place, from the guard band up */
{
    const uint32_t *d = (const uint32_t *)lo;
    while ((const char *)d < top && *d == STACK_MARK)
        d++;
    return (uint32_t)((const char *)d - lo);
}
UI_SMALL static uint32_t stack_free_main(void) { return stack_free(_ustack_lo, _ustack_top); }   /* console.c */
UI_SMALL static uint32_t stack_free_sup(void) { return stack_free(_sstack_lo, _sstack_top); }
UI_SMALL static uint32_t stack_sp(void) { return fm1_sp(); }


/* TIMER5 outranks ALNK0 (timer5_start): the scan keeps its pace while a half buffer renders. Nested in
 * the render it only scans and counts ms; the USB and UART polls wait for the first tick after the
 * render, so usb_poll never runs nested. The USB audio stream cannot wait (one packet per 1 ms frame, a
 * render takes up to a few ms): uac_service also runs nested. It touches only EP4 (INDEX set on every
 * access) and the consumer side of the audio ring, and usb_poll never runs nested, so the two never
 * interleave. The time spent nested is handed to the audio ISR: its load figures stay render-only.
 * (After Felucca 1.0.) */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    felucca_dbg.timer_irqs++;
    fm1_input_tick();
#if BOARD_SMK37
    smo_tick();                                 /* TRS MIDI OUT: a byte when the last has gone (also nested) */
#endif
    {   /* milliseconds from the 24 MHz TIMER4 (robust to a late tick) */
        static uint32_t last, acc;
        acc += t0 - last;
        last = t0;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
    if (usb_due)
        owed |= 1u;                             /* 2 kHz: all USB SIE traffic lives here */
#if FELUCCA_UART
    if (sub % 5u == 2u)
        owed |= 2u;                             /* 2 kHz: <= ~7 bytes per call at 31250 baud */
#endif
    if (++sub == 10u)
        sub = 0;
    if (felucca_dbg.in_audio) {
        felucca_dbg.nested++;
#if FELUCCA_UAC
        if (usb_due)
            uac_service();
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
#if FELUCCA_UART
    if (owed & 2u)
        uart_midi_poll();
#endif
    owed = 0;
}
extern void isr_timer5(void);

UI_SMALL static void timer5_start(void)                 /* OSC /4 = 6 MHz, PRD 600 -> 10 kHz */
{
    /* above ALNK0 (3): the tick may nest into the audio render (a few us; plain registers, the
     * MIDI rings are single-producer / single-consumer). Below it, a tick waited for the render
     * and the column it had lit stayed on longer: the LEDs shimmered, beating against the audio
     * blocks (1378 / s against the 909 / s scan) */
    fm1_timer5_start(isr_timer5, 4);
}

UI_SMALL static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

#if BOARD_SMK37
/* the SMK-37 Pro's 4 faders, tracks 1..4 (4 = drums), by FADER BANK (ui_input.c smk_fbank): A level, B the
 * track's filter (P_TFLT: low-pass left, open in the middle, high-pass right), C reverb send, D delay send.
 * A fader takes over only once it moves (also after a bank change), so nothing jumps. */
static void smk_faders(void)
{
    static uint8_t last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t i;
    if (daw_on) {                                         /* DAW mode: the faders send MIDI (ui_daw.c); back from it */
        smk_fbank_new = 1;                                /* each waits to move again */
        return;
    }
    if (smk_fbank_new) {                                  /* a new bank: wait for each fader to move */
        smk_fbank_new = 0;
        for (i = 0; i < 4u; i++)
            last[i] = 0xFF;
    }
    for (i = 0; i < 4u && i < NTRK; i++) {
        uint32_t v = fm1_in.fader[i];
        int16_t *pv;
        if (last[i] == 0xFF || fm1_in.frames < 200u) {   /* (the first readings settle) */
            last[i] = (uint8_t)v;
            continue;
        }
        if (v == last[i])
            continue;
        last[i] = (uint8_t)v;
        switch (smk_fbank) {
        case 1:
            trk[i].p[P_TFLT] = (int16_t)((int32_t)v - 64);
            break;
        case 2:
            pv = i == TRK_DRUM ? &song.g[G_DRREV] : &trk[i].p[P_REV];
            *pv = (int16_t)v;
            break;
        case 3:
            pv = i == TRK_DRUM ? &song.g[G_DRDLY] : &trk[i].p[P_DLY];
            *pv = (int16_t)v;
            break;
        default:
            pv = i == TRK_DRUM ? &song.g[G_DRLVL] : &trk[i].p[P_LEVEL];
            *pv = (int16_t)v;
            break;
        }
        ui.force = 1;
    }
}

/* the wheels (stock: mux C address 4 = pitch, 5 = mod; the travel is short: raw ~445..640 pitch, ~474..629 mod on a
 * unit): smk_wheel.h, taught by HARDWARE CALIBRATION (wcal) or calibrating themselves; pitch BEND UP / DOWN semitones
 * (bend_up, bend_dn), mod 0..127, up = more (voice.c perf_*) */
static void smk_wheels(void)
{
    static smk_pw_t pw;
    static smk_mw_t mw;
    static uint8_t gen;
    if (fm1_in.frames < 300u)                     /* (the readings settle) */
        return;
    if (gen != wcal_gen) {                        /* a new calibration: start over with it */
        smk_pw_t zp = {0};
        smk_mw_t zm = {0};
        pw = zp;
        mw = zm;
        gen = wcal_gen;
    }
    if (daw_on) {                                 /* DAW mode: pitch bend +-8192 and the mod wheel to MIDI (ui_daw.c); the
                                                   * internal sounds bend (BEND UP / DOWN) with LOCAL ON only */
        int32_t b = smk_pw_bend(&pw, &wcal, fm1_in.wheel[0], fm1_ms, 2, 2), m = smk_mw_val(&mw, &wcal, fm1_in.wheel[1]);
        daw_wheels(b, m);
        perf_bend = daw_local ? (b > 0 ? b * (int32_t)bend_up : b * (int32_t)bend_dn) / 2 : 0;
        perf_modw = daw_local ? m : 0;
        return;
    }
    perf_bend = smk_pw_bend(&pw, &wcal, fm1_in.wheel[0], fm1_ms, bend_up, bend_dn);
    perf_modw = smk_mw_val(&mw, &wcal, fm1_in.wheel[1]);
}
#endif

UI_SMALL static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, BRAND_CRASH, C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(10, 40, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->pc);
    draw_text_box(10, 60, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->emu);
    draw_text_box(10, 84, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->dbg);
    draw_text_box(10, 102, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->rets);
    draw_text_box(10, 120, 220, &FONT_S, b, C_WHITE, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* power-on: three parts with their default sounds (TRK_DEF), the drum track, empty patterns */
UI_SMALL static void felucca_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    fm6_init();                               /* every part's FM6 patch: the init voice */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);   /* with its sends */
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);              /* the sequencers start empty */
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;        /* the 808 kit */
    song.sel = 0;
    song.master_q12 = 2048;
    autosave_resume();                        /* the project as it was left (project.c) */
    song.g[G_SYNC] = (int16_t)lights_sync;    /* a setting of the FM-1 (panel.c) */
    song.g[G_MIDI] = (int16_t)lights_mout;
    song.g[G_ROUTE] = (int16_t)lights_min;
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];   /* (2.6: GLO > MIDI, */
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
    song.g[G_FXCH] = midi_set[6];
    song.g[G_DRMAP] = midi_set[7];                   /* (2.7) GLO > MIDI 3: DR MAP */
    song.g[G_CLKOUT] = midi_set[9];                  /* (2.7) GLO > MIDI 3: CLK OUT */
    song.g[G_RMCH] = midi_set[8];                    /* (2.7) GLO > MIDI 3: REMOTE */                    /* (2.7) GLO > MIDI 3: FX CH */   /* settings of the FM-1) */
    layers_init();                            /* the panel's layer buttons for the keys (ui_layers.c) */
    go_home();
    ui.force = 1;
}

UI_SMALL static void fm1_main(void)
{
    int32_t knob = 512 * 16;
    uint32_t healthy_since;
    persist_boot();
#if FELUCCA_OTA
    if (flash_ok)
        ota_boot_cleanup();                             /* staging area left by an update */
#endif
    settings_init();
    lcd_init();
    sloop_splash();                                     /* the SLOOP logo (splash.c) */
    if (felucca_dbg.magic != DBG_MAGIC) {
        memset(&felucca_dbg, 0, sizeof felucca_dbg);
        felucca_dbg.magic = DBG_MAGIC;
    }
    felucca_dbg.boots++;
    felucca_dbg.max_us = 0;
    felucca_dbg.in_audio = 0;                       /* .noinit: a reset inside the audio ISR left it set, and
                                                       TIMER5 would treat every tick as nested (no USB poll) */
    felucca_dbg.prev_stage = felucca_dbg.stage;     /* a WDT reset leaves the last breadcrumb here */
    felucca_dbg.prev_page = felucca_dbg.page;
    felucca_dbg.prev_home = felucca_dbg.home;
    felucca_dbg.prev_frames = felucca_dbg.ui_frames;
    felucca_dbg.prev_rst = fm1_boot.p3_rst;
    fm1_input_init();
    fm1_adc_init();
#if BOARD_SMK37 && FELUCCA_FLASH
    if (flash_ok)                                   /* the factory key / pad velocity curves (USRTRIM, read only) */
        smk_trim_load(fm1_xip_ptr(SMK_TRIM_FLASH));
#endif
    panel_init();
    felucca_init();
    cpu1_boot();                                        /* the second core: at power-on, before the audio and the
                                                         * timers (X0X: started later it never reaches its entry);
                                                         * if it does not answer, SLOOP runs on one core (cpu1.c) */
    audio_init();
#if FELUCCA_CDC
    usb_cdc_on = usb_serial;                            /* menu USB SERIAL (persist_boot read it): before USB starts */
#if defined(FELUCCA_CDC_ON) && FELUCCA_CDC_ON
    usb_cdc_on = 1;                                     /* (bring-up builds: the console without the menu) */
#endif
#endif
#if FELUCCA_UAC && FELUCCA_UAC_48K
    usb_rate_mode = usb_rate;                           /* menu USB RATE (2.7 beta), the same way */
#endif
    usb_start();
#if FELUCCA_UART
    uart_midi_init();
#endif
#if BOARD_SMK37
    smk_uart1_tx_init();                                /* TRS MIDI OUT (PORTC, IOMAP RMW: before TIMER5) */
#endif
    timer5_start();
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & FM1_OCT_BOTH) == FM1_OCT_BOTH) {
        panel_setup();                        /* OCT- + OCT+ held at power-on */
        settings_save();
    }
    fm1_delay_ms(900);                                  /* (the logo stays a moment) */
    lcd_fill(0, 0, 240, 240, C_BLACK);

    healthy_since = fm1_ms;
    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if ((uint32_t)(m - healthy_since) >= 30000u && bootguard.pending) {
            bootguard_clear(&bootguard);     /* 30 s of this boot, even if TIMER4 survived reset */
        }
        {
            int32_t b = fm1_adc_read(FM1_ADC_BATT);     /* battery: slow IIR */
            if (b > 0)
                song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
        }
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                if (ed_master) {                               /* (2.7) the LIVE page's level, until the knob turns */
                    if (ed_master_k == 0xFFFFu)
                        ed_master_k = (uint16_t)k10;
                    if (k10 + 48u < ed_master_k || k10 > ed_master_k + 48u)
                        ed_master = 0;
                    else
                        k10 = (ed_master - 1u) * 1023u / 126u;
                }
#if BOARD_SMK37
                {   /* the volume knob is endless here, with no position to see: a turn shows MASTER and its level in
                     * the top bar */
                    static int32_t shown = -1;
                    if (shown >= 0 && a != shown && !smk_master_hold) {
                        uint32_t p = (uint32_t)a * 100u / 1023u;
                        char d[5] = {0}, *q = d;
                        if (p >= 100u)
                            *q++ = '1';
                        if (p >= 10u)
                            *q++ = (char)('0' + p / 10u % 10u);
                        *q++ = (char)('0' + p % 10u);
                        *q = '%';
                        ui_say("MASTER ", d);
                    }
                    shown = a;
                }
                song.master_q12 = smk_master_q12(k10, lim_classic);   /* (HOME menu > AUDIO > LIMITER: its knob law, fx.c) */
#else
                song.master_q12 = (k10 * k10) >> 8;            /* 0 .. ~4096 */
#endif
            }
        }
        {   /* OCT- + OCT+ held 5 s, stopped: enter UBOOT with RAM intact (debug / update); a countdown
             * shows from 2 s, letting go cancels it. Not while playing: on the drum track these are the
             * ghost / hard modifiers, held for a long time */
            static uint32_t t0, shown;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both || song.playing) {
                if (shown)
                    ui_say("UPDATE MODE ", "CANCELLED");
                shown = 0;
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != shown) {
                    char d[4] = {(char)('0' + left), '.', '.', 0};
                    ui_say("UPDATE MODE IN ", d);
                    shown = left;
                }
            } else if (fm1_ms - t0 > 5000u) {
                fm1_audio_stop();
#if FELUCCA_ARRANGER
                persist_flush_now();                    /* (live sections, song, project: RAM-only so far) */
#endif
                lcd_fill(0, 0, 240, 240, C_BLACK);
                draw_text_box(0, 110, 240, &FONT_S, "UBOOT", RGB(80, 120, 255), 1);
                usb_detach();
                fm1_delay_ms(30);
                bootguard.pending = 0;                  /* intentional reset: not a failed boot */
                fm1_enter_uboot();
            }
        }
#if FELUCCA_OTA
        ed_service();                                   /* web editor SysEx */
        ota_service();                                  /* M-UPGRADE handshake */
        if (usb.ota_req) {                              /* M-UPGRADE upgrade command */
            usb.ota_req = 0;
            panic_req = (1u << NTRK) - 1u;               /* every track (bit per track) */
#if FELUCCA_ARRANGER
            persist_flush_now();                        /* an update ends in a reset: RAM-only work first */
#endif
            if (flash_ok)
                ota_session();                          /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, C_BLACK);
            ui.force = 1;
        }
#endif
        if (usb.uboot_req) {                            /* SysEx F0 22 24 35 7D F7 from the host */
            fm1_audio_stop();
#if FELUCCA_ARRANGER
            persist_flush_now();                    /* (live sections, song, project: RAM-only so far) */
#endif
            lcd_fill(0, 0, 240, 240, C_BLACK);
            draw_text_box(0, 110, 240, &FONT_S, "UBOOT (USB)", C_WHITE, 1);
            fm1_delay_ms(20);
            usb_detach();
            fm1_delay_ms(30);
            bootguard.pending = 0;
            fm1_enter_uboot();
        }
#if FELUCCA_CDC
        cdc_task();
#endif
        felucca_dbg.ui_frames++;
        felucca_dbg.page = ui.page;
        felucca_dbg.home = ui.home;
        felucca_dbg.stage = 1;
        ui_input();
        felucca_dbg.stage = 2;
        ui_leds();
#if BOARD_SMK37
        smk_faders();
        smk_wheels();
        padled_flush();                                 /* the pads' colours, between LCD transfers */
#endif
        ui_draw();
        felucca_dbg.stage = 8;
        autosave_tick();                                /* the working project into flash, when quiet */
#if FELUCCA_ARRANGER
        sections_flush();                               /* live sections / the recorded song, when quiet */
#endif
        felucca_dbg.stage = 9;
        while (fm1_ms - m < 15u) {                               /* ~60 UI frames/s at most */
            ui_input();
#if BOARD_SMK37
            smk_wheels();                       /* (bends follow the wheel, not the screen) */
#endif
#if FELUCCA_OTA
            ed_service();                       /* editor replies without waiting for the next frame */
#endif
        }
    }
}

UI_SMALL void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt, boot_mode;
    fm1_time_init();
    fm1_supplies_stock();                       /* (2.7) VDC14 1.60 V, SYSVDD 1.38 V, as M-VAVE's firmware: both cores
                                                 * read the flash and SRAM right on every unit (fm1_sys.h) */
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))
        bootguard_clear(&bootguard);           /* a power-on (not a watchdog or soft reset): no failed boot to count */
    boot_mode = bootguard_begin(&bootguard);
    if (boot_mode == BOOT_ROM) {
        fm1_enter_uboot();
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    lowram_ok = lowram_test();                  /* .lowram proved and zeroed (app.ld) */
    stack_paint(_ustack_lo, _ustack_top);       /* the stacks painted below their live frames (console: stack_free) */
    stack_paint(_sstack_lo, _sstack_top);
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;                                /* flash driver code that must run from RAM */
    for (s = _c1_load, d = _c1_start; d < _c1_end; s++, d++)
        *d = *s;                                /* the second core's waiting loop (fm1_cpu1.h) */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
#if FELUCCA_OTA
    if (boot_mode == BOOT_RECOVERY || recovery_key())
        recovery_main();
#else
    if (boot_mode == BOOT_RECOVERY) fm1_enter_uboot();
#endif
    fm1_main();
    for (;;)
        ;
}
