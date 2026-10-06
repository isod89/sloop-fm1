/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Physical panel: which matrix button / encoder carries which printed label.
 * The default table can be overridden by HARDWARE CALIBRATION (hold OCT- and
 * OCT+ while powering on), which asks for each label in turn. The learned
 * table lives in .noinit and, with FELUCCA_FLASH, in flash with the settings
 * (project.c). */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const E_NAME[NE] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2",
                                        "KNOB 3", "KNOB 4"};
#define PANEL_MAGIC 0x50414E35u          /* "PAN5": bump when PANEL_DEFAULT changes */

typedef struct {
    uint32_t magic;
    uint8_t btn[NB];             /* matrix button id (0..13) per label */
    uint8_t enc[NE];             /* matrix encoder (0..6) per role */
    int8_t dir[NE];              /* +1 / -1 so that clockwise is + */
} panel_t;
panel_t panel __attribute__((section(".noinit")));

static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},   /* PLAY = 12, REC = 13 */
    {0, 1, 6, 2, 3, 4, 5},                           /* SELECT = enc 0, ALGORITHM = enc 1, PRESETS = enc 6 */
    {1, 1, 1, 1, 1, 1, 1},
};

static void panel_init(void)                     /* also after a flash load: ids are used as array indexes and shifts */
{
    uint32_t i, ok = panel.magic == PANEL_MAGIC;
    for (i = 0; ok && i < NB; i++)
        ok = panel.btn[i] < 14u;
    for (i = 0; ok && i < NE; i++)
        ok = panel.enc[i] < 7u && (panel.dir[i] == 1 || panel.dir[i] == -1);
    if (!ok)
        panel = PANEL_DEFAULT;
}

static uint32_t panel_btn_of(uint32_t matrix_id)        /* label of a matrix button, NB if none */
{
    uint32_t b;
    for (b = 0; b < NB; b++)
        if (panel.btn[b] == matrix_id)
            return b;
    return NB;
}

static void panel_led(uint32_t label, int on) { fm1_led_key(panel.btn[label], on); }

/* steps of a role, + = clockwise */
static uint32_t ui_input_ms;                    /* the last button, key or knob turn (ui_input.c; autosave) */
static int32_t panel_enc(uint32_t role)
{
    int32_t s = fm1_enc_take(panel.enc[role]) * panel.dir[role];
    if (s)
        ui_input_ms = fm1_ms;                      /* a knob turning is not idle either: autosave waits */
    return s;
}

/* user settings that survive a reset */
#define SETTINGS_MAGIC 0x53455433u              /* "SET3" */
struct { uint32_t magic, palette, lowcut, zoom; } settings __attribute__((section(".noinit")));

/* the lights (menu LIGHTS / KEYS / NOTES), for playing in the dark: kept in flash with the settings
 * (project.c persist_t.lights), read at every boot; not in .noinit, so nothing there moves. The same
 * word keeps SYNC and the REC screen's MODE / START (seq.c rec_tempo, rec_count) */
enum { LIGHTS_OFF, LIGHTS_LOW, LIGHTS_MID, LIGHTS_HIGH, LIGHTS_N };
enum { KEYS_OFF, KEYS_C, KEYS_WHITE, KEYS_N };
static uint8_t lights_lvl, lights_keys;
static uint8_t lights_notes;                   /* 1: on a synth track the sounding notes light their keys */
static uint8_t lights_sync;                    /* GLO > SYSTEM > SYNC (G_SYNC), kept here: 0 INT, 1 USB, 2 TRS */
static const uint16_t LIGHTS_NS[LIGHTS_N] = {0u, 500u, 1000u, 2000u};   /* the backlight pulse a frame (ns): a lit
                                                * LED ~95 us, the glow (landmarks) 4 us (fm1_input.h) */
static uint32_t lights_word(void)
{
    return (uint32_t)lights_lvl | (uint32_t)lights_keys << 4 | (uint32_t)(lights_notes != 0u) << 8 |
           (uint32_t)(rec_tempo != 0u) << 9 | (uint32_t)(rec_count != 0u) << 10 | (uint32_t)(usb_full != 0u) << 11 |
           (uint32_t)(lights_sync % 3u) << 12 | (uint32_t)(punch_latch != 0u) << 14;
}
static void lights_from_word(uint32_t w)
{
    lights_lvl = (uint8_t)((w & 15u) < LIGHTS_N ? (w & 15u) : LIGHTS_OFF);
    lights_keys = (uint8_t)(((w >> 4) & 15u) < KEYS_N ? ((w >> 4) & 15u) : KEYS_OFF);
    lights_notes = (uint8_t)((w >> 8) & 1u);
    rec_tempo = (uint8_t)((w >> 9) & 1u);       /* the REC screen: MODE, START (seq.c) */
    rec_count = (uint8_t)((w >> 10) & 1u);
    usb_full = (uint8_t)((w >> 11) & 1u);       /* menu USB AUDIO (fx.c) */
    lights_sync = (uint8_t)(((w >> 12) & 3u) % 3u);
    punch_latch = (uint8_t)((w >> 14) & 1u);     /* menu PUNCH (punch.c) */
}

static void settings_save(void);              /* project.c: flash copy (FELUCCA_FLASH) */
static uint8_t settings_later;                 /* changed while playing: saved once stopped (project.c) */

static void settings_init(void)
{
    if (settings.magic != SETTINGS_MAGIC || settings.palette >= NPALETTES) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = 4;                  /* MONO (default) */
        settings.lowcut = 0;
        settings.zoom = 0;                     /* large readout of the touched value: off */
    }
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut != 0);
}
