/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP menu (HOME held): COLOR, LOWCUT, ZOOM, LIGHTS, KEYS, NOTES, USB AUDIO, PUNCH, HARDWARE CALIBRATION, ABOUT. */
/* ------------------------------------------------------------ menu --- */
enum { MI_COLOR, MI_LOWCUT, MI_ZOOM, MI_LIGHTS, MI_KEYS, MI_NOTES, MI_USB, MI_PUNCH, MI_PANEL, MI_ABOUT, MI_BACK, MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "LOWCUT", "ZOOM", "LIGHTS", "KEYS", "NOTES", "USB AUDIO", "PUNCH",
                                              "HARDWARE CALIBRATION", "ABOUT", "BACK"};
static const char *const LIGHTS_NAME[LIGHTS_N] = {"OFF", "LOW", "MID", "HIGH"};   /* every button lit, the labels readable */
static const char *const KEYS_NAME[KEYS_N] = {"OFF", "C KEYS", "WHITE KEYS"};      /* keys lit too, at the LIGHTS level */
#define MI_DY 16                                   /* rows between two menu lines (11 lines + the two help lines in the
                                                    * 219 rows under the header: 18 fitted ten lines, not eleven) */
_Static_assert(4 + MI_COUNT * MI_DY + 18 + 16 <= 240 - H_HEAD - 1, "the menu and its two help lines fit the screen");

static void draw_menu(void)
{
    uint32_t i, pass, sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                            settings.zoom * 104729u + lights_lvl * 1299709u + lights_keys * 15485863u +
                            lights_notes * 32452843u + usb_full * 49979687u + punch_latch * 67867967u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    if (ui.force)                                   /* head + rule + two bands cover rows 0..229 */
        lcd_fill(0, H_HEAD + 1 + 124 + 95, 240, 240 - (H_HEAD + 1 + 124 + 95), C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);
    cv_text(4, 1, &FONT_S, ui.menu == 2 ? "ABOUT" : "MENU", C_HI);
    cv_blit(0, Y_HEAD);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    for (pass = 0; pass < 2u; pass++) {             /* the canvas holds 124 rows: draw in two bands */
        cv_begin(240, pass ? 95u : 124u, C_BLACK);
        cv_oy = pass ? -124 : 0;
        if (ui.menu == 2) {
            cv_text(4, 4, &FONT_L, "SLOOP", C_WHITE);
            cv_rect(96, 10, 8, 4, TE_COL[0]), cv_rect(96, 16, 12, 4, TE_COL[1]);   /* the sail */
            cv_rect(96, 22, 16, 4, TE_COL[2]), cv_rect(96, 28, 20, 4, TE_COL[3]);
            cv_text(4, 36, &FONT_S, "BASED ON FELUCCA", C_AMB);
            cv_text(4, 54, &FONT_S, FELUCCA_VERSION, C_HI);
            cv_text(236 - text_w(&FONT_S, __DATE__), 54, &FONT_S, __DATE__, C_GRAY);   /* build date */
            cv_text(cv_text(4, 72, &FONT_S, "LEO KUROSHITA", C_HI) + 8, 72, &FONT_S, "@KUROGEDELIC", C_AMB);
            cv_text(4, 88, &FONT_S, "H\xDCGELTON INSTRUMENTS", C_HI);   /* Latin-1 U-umlaut */
            cv_text(4, 104, &FONT_S, "HUGELTON.COM", C_AMB);
            cv_text(4, 119, &FONT_S, "GPL-3.0, NO WARRANTY", C_HI);
            cv_text(4, 132, &FONT_S, "GITHUB.COM/ISOD89/SLOOP-FM1", C_AMB);   /* (the source of this firmware) */
            cv_text(4, 146, &FONT_S, "FONT: TERMINUS (OFL)", C_DIM);
            cv_text(4, 159, &FONT_S, "SAMPLES: VERSILIAN (CC0)", C_DIM);
            cv_text(4, 172, &FONT_S, "+ SONIC PI (CC0)", C_DIM);
            cv_text(4, 185, &FONT_S, "PHASE: CRISPYZEBRA (GPL)", C_DIM);
            cv_text(4, 198, &FONT_S, "VOICE: REF. KLATTSCH (MIT)", C_DIM);
        } else {
            for (i = 0; i < MI_COUNT; i++) {
                int32_t y = 4 + (int32_t)i * MI_DY;
                int sel = i == ui.menu_sel;
                if (sel)
                    cv_rect(4, y + 6, 3, 3, C_WHITE);
                cv_text(14, y, &FONT_S, MI_NAME[i], sel ? C_WHITE : C_GRAY);
                if (i == MI_LOWCUT || i == MI_ZOOM || i == MI_NOTES)
                    cv_text(100, y, &FONT_S, (i == MI_LOWCUT ? settings.lowcut : i == MI_ZOOM ? settings.zoom : lights_notes)
                                                ? "ON" : "OFF", C_HI);
                if (i == MI_LIGHTS)
                    cv_text(100, y, &FONT_S, LIGHTS_NAME[lights_lvl % LIGHTS_N], C_HI);
                if (i == MI_USB)                       /* the USB audio input: follows MASTER, or full level */
                    cv_text(100, y, &FONT_S, usb_full ? "FULL" : "MASTER", C_HI);
                if (i == MI_PUNCH)                     /* the punch-in effects: while the key is held, or latched */
                    cv_text(100, y, &FONT_S, punch_latch ? "LATCH" : "HOLD", C_HI);
                if (i == MI_KEYS)
                    cv_text(100, y, &FONT_S, KEYS_NAME[lights_keys % KEYS_N], lights_lvl ? C_HI : C_DIM);   /* (needs LIGHTS) */
                if (i == MI_COLOR) {
                    uint32_t k;
                    cv_text(100, y, &FONT_S, PALETTES[settings.palette].name, C_HI);
                    for (k = 0; k < 5u; k++)
                        cv_rect(160 + (int32_t)k * 14, y + 3, 10, 10, pal[k]);
                }
            }
            cv_text(4, 4 + MI_COUNT * MI_DY + 4, &FONT_S, "PRESETS MOVE  KNOB 1 SET", C_DIM);
            cv_text(4, 4 + MI_COUNT * MI_DY + 18, &FONT_S, "OCT+ OK   OCT- BACK", C_DIM);
        }
        cv_oy = 0;
        cv_blit(0, H_HEAD + 1 + pass * 124u);
    }
}

static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

static void menu_close(void)
{
    if (song.playing || transport_req)
        settings_later = 1;                            /* (a flash write stops the audio: once stopped) */
    else
        settings_save();                               /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u;
    if (back) {
        if (ui.menu == 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1)
        ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
    s = panel_enc(EN_K1);
    if (s != 0 && ui.menu == 1 && ui.menu_sel == MI_COLOR) {
        settings.palette = (settings.palette + (s > 0 ? 1u : NPALETTES - 1u)) % NPALETTES;
        palette_set(settings.palette);              /* (the menu signature redraws) */
    }
    if ((s != 0 || ok) && ui.menu == 1 && (ui.menu_sel == MI_LOWCUT || ui.menu_sel == MI_ZOOM)) {
        /* KNOB 1: right = ON, left = OFF; OCT+ toggles */
        uint32_t *v = ui.menu_sel == MI_LOWCUT ? &settings.lowcut : &settings.zoom;
        *v = s > 0 ? 1u : s < 0 ? 0u : !*v;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_USB) {     /* right FULL, left MASTER; OCT+ toggles */
        usb_full = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_full);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_PUNCH) {   /* right LATCH, left HOLD; OCT+ toggles */
        punch_latch = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !punch_latch);
        if (!punch_latch && !punch.keybit)
            punch.req = -1;                           /* back to HOLD: a latched effect ends */
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_NOTES) {   /* (the same: right ON, left OFF) */
        lights_notes = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !lights_notes);
        ok = 0;
    }
    if ((s != 0 || ok) && ui.menu == 1 && (ui.menu_sel == MI_LIGHTS || ui.menu_sel == MI_KEYS)) {
        /* KNOB 1: brighter / dimmer (stops at the ends); OCT+ steps round */
        uint8_t *v = ui.menu_sel == MI_LIGHTS ? &lights_lvl : &lights_keys;
        uint32_t n = ui.menu_sel == MI_LIGHTS ? LIGHTS_N : KEYS_N;
        if (s > 0 && *v + 1u < n)
            (*v)++;
        else if (s < 0 && *v > 0u)
            (*v)--;
        else if (!s)
            *v = (uint8_t)((*v + 1u) % n);
        if (ui.menu_sel == MI_KEYS && lights_keys && !lights_lvl)
            lights_lvl = LIGHTS_LOW;                   /* keys lit need a level: the lowest */
        ok = 0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_COLOR:                                 /* OCT+ steps through the palettes too */
            settings.palette = (settings.palette + 1u) % NPALETTES;
            palette_set(settings.palette);
            break;
        case MI_PANEL:
            panel_setup();
            ui.force = 1;
            break;
        case MI_ABOUT:
            ui.menu = 2;
            ui.force = 1;
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}

