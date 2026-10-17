/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* The pad sound picker. The 16 pads (and the screen, 8 x 2) are the sounds of one kind, in the PRESETS
 * order (ui.c BANK), 16 a page, then the user presets; on the drum track the kits. A pad loads its sound
 * (as PRESETS does) and plays a short preview; < > step the pages. KNOB BANK pins it open; a PRESETS
 * turn opens it for PICK_MS. Pad hits never reach the notes while it is up (smk_input.h smk_pad_pick). */
#define PICK_MS 1500u
enum { PK_USER = 8, PK_KITS = 9 };
static const char *const PICK_KIND[10] = {"BASS", "KEYS", "ORGN", "PAD", "LEAD", "PLCK", "STAB", "FX", "USER", "KITS"};
static const uint8_t PICK_RGB[10][3] = {           /* a colour a kind (pads and screen) */
    {255, 40, 40},  {255, 140, 0},  {255, 220, 0}, {40, 220, 80},  {0, 200, 255},
    {60, 80, 255},  {170, 60, 255}, {255, 60, 170}, {220, 210, 170}, {255, 98, 26}};
static uint8_t pick_pin, pick_tmp, pick_sel = 0xFF;
static uint16_t pick_page;
static uint32_t pick_t, pick_frame;                /* last touch (ms); the frame drawn last */
static uint32_t pick_k4_until;                     /* KNOB BANK lit until (ms): a PRESETS detent */
typedef struct { uint16_t first, len, sub, nsub, kind; } pick_pg_t;

static uint32_t pick_total(void)
{
    uint32_t total;
    if (is_drum(TSEL))
        return DRUM_KITS;
    preset_pos(&total);
    return total;
}
static uint32_t pick_cur(void)
{
    uint32_t total;
    return is_drum(TSEL) ? drum_kit() : preset_pos(&total);
}
static uint32_t pick_kind_of(uint32_t n)
{
    if (is_drum(TSEL))
        return n >= DRUM_USR ? PK_USER : PK_KITS;
    return n < NBANK ? BANK[n].kind : PK_USER;
}
static void pick_name(uint32_t n, char *b)        /* 13 bytes */
{
    uint32_t k, e;
    if (is_drum(TSEL)) {
        str_cpy(b, DRUM_KIT_NAMES[n % DRUM_KITS], 13);
        return;
    }
    e = preset_at(n, &k);
    if (e == NENGINES)
        up_name(k, b);
    else
        str_cpy(b, ENGINES[e]->presets[k].name, 13);
}
/* a cell's two short lines: the first two words' first 3 letters ("PLUGG BASS": PLU / BAS), one word: 3 + 3 */
static void pick_abbr(const char *s, char *l1, char *l2)
{
    const char *w2 = s, *r;
    uint32_t j;
    while (*w2 && *w2 != ' ')
        w2++;
    while (*w2 == ' ')
        w2++;
    for (j = 0; s[j] && s[j] != ' ' && j < 3u; j++)
        l1[j] = s[j];
    l1[j] = 0;
    r = *w2 ? w2 : s + j;
    for (j = 0; r[j] && r[j] != ' ' && j < 3u; j++)
        l2[j] = r[j];
    l2[j] = 0;
}

/* the pages: each kind's run of entries, 16 a page. Fills *pg for page want, *of = the page of entry n;
 * returns the number of pages */
static uint32_t pick_walk(uint32_t want, pick_pg_t *pg, uint32_t n_of, uint32_t *of)
{
    uint32_t total = pick_total(), n = 0, p = 0;
    while (n < total) {
        uint32_t k = pick_kind_of(n), end = n, nsub, i;
        while (end < total && pick_kind_of(end) == k)
            end++;
        nsub = (end - n + 15u) / 16u;
        for (i = 0; i < nsub; i++, p++) {
            uint32_t f = n + i * 16u, l = end - f > 16u ? 16u : end - f;
            if (p == want && pg) {
                pg->first = (uint16_t)f;
                pg->len = (uint16_t)l;
                pg->sub = (uint16_t)i;
                pg->nsub = (uint16_t)nsub;
                pg->kind = (uint16_t)k;
            }
            if (of && n_of >= f && n_of < f + l)
                *of = p;
        }
        n = end;
    }
    return p;
}
static void pick_home(void)                        /* to the page of the current sound */
{
    uint32_t p = 0;
    pick_walk(0xFFFFu, 0, pick_cur(), &p);
    pick_page = (uint16_t)p;
    pick_sel = song.sel;
}
static uint32_t pick_info(pick_pg_t *pg)          /* the page shown (back to the current one on a new track) */
{
    uint32_t np;
    if (pick_sel != song.sel)
        pick_home();
    np = pick_walk(pick_page, pg, 0xFFFFu, 0);
    if (np && pick_page >= np) {
        pick_page = 0;
        pick_walk(0, pg, 0xFFFFu, 0);
    }
    return np;
}

static int pick_shown(void)
{
    return (pick_pin || (pick_tmp && fm1_ms - pick_t < PICK_MS)) && !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind;
}
static void pick_close(void)
{
    pick_pin = pick_tmp = 0;
}
static void pick_pin_toggle(void)                  /* KNOB BANK */
{
    pick_pin = !pick_pin && !ui.menu;
    pick_tmp = 0;
    if (pick_pin)
        pick_home();
}
static void pick_open_tmp(void)                    /* a PRESETS turn: the current sound's page */
{
    pick_tmp = 1;
    pick_t = fm1_ms;
    pick_k4_until = fm1_ms + 150u;
    pick_home();
}
static void pick_turn(int32_t d)                   /* < > */
{
    pick_pg_t pg;
    uint32_t np = pick_info(&pg);
    if (np)
        pick_page = (uint16_t)((pick_page + np + (uint32_t)(d > 0 ? 1 : (int32_t)np - 1)) % np);
    pick_t = fm1_ms;
    ui.force = 1;
}
static void pick_select(uint32_t w)                /* white key / cell w (0..15) of the page */
{
    pick_pg_t pg;
    uint32_t n;
    pick_t = fm1_ms;
    if (!pick_info(&pg) || w >= pg.len)
        return;
    n = pg.first + w;
    if (is_drum(TSEL)) {
        TDRUM->p[P_E0] = (int16_t)n;
        audition_req(1u << 0 | 1u << 4, 0u);      /* kick + closed hat */
    } else {
        preset_go(n);
#if BOARD_SMK37
        pick_aud_req = (uint8_t)((pg.kind == 0u ? 48u : 60u) + 1u);   /* basses an octave down */
#endif
    }
    ui.force = 1;
}

static uint16_t pick_col(uint32_t kind, uint32_t sh)   /* the kind's colour, >> sh */
{
    const uint8_t *c = PICK_RGB[kind % 10u];
    return RGB(c[0] >> sh, c[1] >> sh, c[2] >> sh);
}
#if BOARD_SMK37
static void pick_leds(void)                         /* the current sound white, the others their kind's colour dim */
{
    pick_pg_t pg;
    uint32_t w, cur = pick_cur();
    const uint8_t *c;
    if (!pick_info(&pg))
        pg.len = 0;
    c = PICK_RGB[pg.kind % 10u];
    for (w = 0; w < 16u; w++) {
        uint32_t p = w;                            /* reading order: pads 0..7 the top row */
        if (w >= pg.len)
            padled_set(p, 0, 0, 0);
        else if (pg.first + w == cur)
            padled_set(p, 150, 150, 150);
        else
            padled_set(p, (uint8_t)(c[0] >> 2), (uint8_t)(c[1] >> 2), (uint8_t)(c[2] >> 2));
    }
}
#endif

/* the screen mirrors the pads: the current sound's name, 8 x 2 cells, < BASS 1/2 > */
static void pick_draw(void)
{
    static uint32_t shown;
    pick_pg_t pg;
    uint32_t np = pick_info(&pg), cur = pick_cur(), half, w, sig;
    uint16_t kc = pick_col(pg.kind, 0);
    char nm[13], l1[4], l2[4], foot[16];
    if (pick_frame + 1u != ui.frame) {             /* another screen was up: all of it */
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    pick_frame = ui.frame;
    sig = (np ? pg.first * 977u + pg.kind * 31u : 0u) + cur * 7919u + song.sel * 131u + pick_total() * 3u;
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    pick_name(cur, nm);
    str_cpy(foot, PICK_KIND[pg.kind % 10u], sizeof foot);
    if (pg.nsub > 1u) {
        uint32_t n = str_len(foot);
        foot[n++] = ' ';
        fmt_int(foot + n, pg.sub + 1);
        n = str_len(foot);
        foot[n++] = '/';
        fmt_int(foot + n, pg.nsub);
    }
    for (half = 0; half < 2u; half++) {
        int32_t y0 = half ? 2 : 38;
        cv_begin(240, 120, C_BLACK);
        if (!half) {
            char tn[3] = {'T', (char)('1' + song.sel % NTRK), 0};
            cv_text(4, 2, &FONT_L, nm, C_WHITE);
            cv_text(240 - 4 - text_w(&FONT_S, tn), 4, &FONT_S, tn, TE_COL[song.sel & 3u]);
        }
        for (w = half * 8u; np && w < half * 8u + 8u && w < pg.len; w++) {
            int32_t x = (int32_t)(w & 7u) * 30, sel = pg.first + w == cur;
            pick_name(pg.first + w, nm);
            pick_abbr(nm, l1, l2);
            cv_rect(x + 1, y0, 28, 78, sel ? C_WHITE : kc);   /* filled like a button: the kind's colour, the current one white */
            cv_text(x + 15 - text_w(&FONT_S, l1) / 2, y0 + 20, &FONT_S, l1, C_BLACK);
            cv_text(x + 15 - text_w(&FONT_S, l2) / 2, y0 + 42, &FONT_S, l2, C_BLACK);
        }
        if (half) {
            cv_text(4, 86, &FONT_L, "<", C_WHITE);
            cv_text(236 - text_w(&FONT_L, ">"), 86, &FONT_L, ">", C_WHITE);
            cv_text((240 - text_w(&FONT_L, foot)) / 2, 86, &FONT_L, foot, kc);
        }
        cv_blit(0, half * 120u);
    }
}
