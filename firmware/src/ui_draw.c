/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: status bar (top), columns + gauges, graphs, focus readout,
 * footer (steps + engine / preset / page). */
static void draw_menu(void);
static uint32_t str_hash(uint32_t h, const char *s);

/* --------------------------------------------------------- drawing --- */
static int is_eng_name(const char *s)                 /* one of the ENGINES[]->name strings */
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (s == ENGINES[e]->name)
            return 1;
    return 0;
}

/* at most 5 characters, and no wider than maxw */
static void fit(char *d, const char *src, const felucca_font_t *f, int32_t maxw)
{
    str_cpy(d, src, is_eng_name(src) ? 8 : 6);           /* engine names are kept whole */
    while (d[0] && text_w(f, d) > maxw)
        d[str_len(d) - 1u] = 0;
}

static int32_t batt_level(void)                         /* ADC ch3 thresholds */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* charging (a USB host powers us; there is no charger status line): bars fill 1, 2, 3
 * every 0.6 s, so the header is redrawn twice a second at most; else the level */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 1 + (int32_t)((fm1_ms / 600u) % 3u);
    return batt_level();
}

/* top bar: transport, BPM, octave | USB, battery, CPU; messages replace it */
static void draw_head(void)
{
    char b[16];
    uint32_t i;
    int32_t x;
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : 0u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)batt_shown() * 7777u + (usb.config && !usb.suspended) * 99991u;
    if (!ui.force && sig == ui.head_sig)
        return;
    ui.head_sig = sig;
    cv_begin(240, H_HEAD, C_BLACK);
    if (ui.msg_t) {
        cv_text(4, 1, &FONT_S, ui.msg, C_HI);
        cv_blit(0, Y_HEAD);
        return;
    }
    if (song.playing) {                               /* > play, square stop */
        for (i = 0; i < 5u; i++)
            cv_rect(4 + (int32_t)i * 2, 4 + (int32_t)i, 2, 10 - 2 * (int32_t)i, C_WHITE);
    } else {
        cv_rect(4, 5, 8, 8, C_HI);
    }
    if (rec)                                          /* recording armed: white = this track, gray = another */
        te_disc(21, 9, 4, rec == 2u ? TE_RED : C_GRAY);
    fmt_int(b, song.g[G_BPM]);
    x = 32;
    if (FELUCCA_ICONS) {                              /* metronome, then the BPM */
        cv_icon(x, 2, ICON_TEMPO, C_GRAY);
        x += 14;
    }
    x = cv_text(x, 1, &FONT_S, b, ui.bpm_t ? C_WHITE : C_HI);   /* white while SELECT turns it */
    if (song.octave) {
        str_cpy(b, song.octave > 0 ? "+" : "", 4);
        fmt_int(b + str_len(b), song.octave);
        cv_text(x + 12, 1, &FONT_S, "OCT", C_GRAY);
        cv_text(x + 40, 1, &FONT_S, b, C_HI);
    }
    if (FELUCCA_ICONS) {                              /* the selected track: tape + number */
        cv_icon(156, 2, ICON_TAPE, C_GRAY);
        b[0] = (char)('1' + song.sel);
        b[1] = 0;
        cv_rect(168, 2, 12, 16, TE_COL[song.sel & 3u]);
        cv_text(170, 1, &FONT_S, b, C_BLACK);
    } else {
        b[0] = 'T';
        b[1] = (char)('1' + song.sel);
        b[2] = 0;
        cv_text(158, 1, &FONT_S, b, C_HI);
    }
    {   /* battery, 3 bars; USB when a host is there */
        int32_t lvl = batt_shown(), k;
        int32_t bx = 236 - 19;                          /* right edge (the CPU figure is in the console) */
        cv_rect(bx, 4, 17, 1, C_GRAY);
        cv_rect(bx, 12, 17, 1, C_GRAY);
        cv_rect(bx, 4, 1, 9, C_GRAY);
        cv_rect(bx + 16, 4, 1, 9, C_GRAY);
        cv_rect(bx + 17, 6, 2, 5, C_GRAY);
        for (k = 0; k < lvl; k++)
            cv_rect(bx + 2 + k * 5, 6, 3, 5, lvl == 1 && batt_level() <= 1 ? C_WHITE : C_HI);
        if (usb.config && !usb.suspended)
            cv_text(bx - 28, 1, &FONT_S, "USB", C_DIM);
    }
    cv_blit(0, Y_HEAD);
}
/* full redraw: the strips (head, columns, graph, foot) cover the rest, so only
 * the space between them is cleared, then the rules are drawn */
static void draw_frame(void)
{
    uint32_t i;
    lcd_fill(0, H_HEAD, 240, Y_LABEL - H_HEAD, C_BLACK);
    lcd_fill(0, Y_SEP_END, 240, Y_GRAPH - Y_SEP_END, C_BLACK);
    lcd_fill(0, Y_GRAPH + H_GRAPH, 240, Y_FOOT - Y_GRAPH - H_GRAPH, C_BLACK);
    for (i = 0; i < 4u; i++)                            /* left inset of each column */
        lcd_fill(i * 60u, Y_LABEL, 4, Y_SEP_END - Y_LABEL, C_BLACK);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    lcd_fill(0, Y_FOOT - 2, 240, 1, C_LINE);
    for (i = 1; i < 4u; i++)
        lcd_fill(i * 60u - 1u, H_HEAD + 4, 1, Y_SEP_END - H_HEAD - 4, C_LINE);
}

/* one column: [icon] LABEL / value unit / gauge, redrawn only when it changed.
 * ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label */
#define LABEL_X (FELUCCA_ICONS ? ICON_CELL + ICON_GAP : 0)
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    char l[8], v[8], u[8], key[32];
    int32_t x, gw = 52, fx;
    if (icon == ICON_AUTO)
        icon = icon_for_label(label);
    fit(l, label, &FONT_S, 54 - LABEL_X);
    fit(v, val, &FONT_S, is_eng_name(val) ? 56 : 40);   /* engine names whole */
    fit(u, unit, &FONT_S, 54 - text_w(&FONT_S, v) - 3);
    str_cpy(key, l, 8);                                 /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), v, 8);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), u, 8);
    {
        uint32_t n = str_len(key);
        key[n] = (char)('A' + (vc == C_WHITE) + (vc == C_DIM) * 2);
        key[n + 1] = (char)(' ' + (ratio < 0 ? 0 : 1 + ratio / 20));
        key[n + 2] = (char)(icon == ICON_NONE ? '~' : '!' + icon % 90u);   /* same label, other icon */
        key[n + 3] = 0;
    }
    if (c == ui.hot_col) {
        str_cpy(ui.focus_l, l, 8);
        str_cpy(ui.focus_v, v, 8);
        str_cpy(ui.focus_u, u, 8);
    }
    if (!ui.force && str_eq(key, ui.col[c]))
        return;
    str_cpy(ui.col[c], key, sizeof ui.col[c]);
    cv_begin(55, Y_SEP_END - Y_LABEL, C_BLACK);         /* x 4..58: the rule at 59 stays */
    if (FELUCCA_ICONS && icon != ICON_NONE && l[0])
        cv_icon(0, 1, icon, TE_COL[c & 3u]);            /* icon rows 1..10 = the label's cap height */
    cv_text(l[0] ? LABEL_X : 0, 0, &FONT_S, l, C_GRAY);
    x = cv_text(0, Y_VALUE - Y_LABEL, &FONT_S, v, vc);
    cv_text(x + 3, Y_VALUE - Y_LABEL, &FONT_S, u, C_DIM);
    if (ratio >= 0) {                                   /* gauge: track, fill, 1 px end line */
        int32_t gy = Y_GAUGE - Y_LABEL;
        fx = ratio * gw / 1000;
        cv_rect(0, gy + 1, gw, 1, C_LINE);
        cv_rect(0, gy, fx, 3, vc == C_DIM ? C_DIM : TE_DIM[c & 3u]);
        cv_rect(fx, gy - 1, 1, 5, vc == C_DIM ? C_HI : vc);
    }
    cv_blit(c * 60u + 4u, Y_LABEL);
}

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    int32_t a = 4 + t->p[P_ATK] * 50 / 127, d = 6 + t->p[P_DEC] * 50 / 127, r = 6 + t->p[P_REL] * 60 / 127;
    int32_t top = 8, bot = 90, sus = t->p[P_SUS] * 1000 / 127;          /* 0..1000 */
    int32_t x0 = 6, x1 = x0 + a, x3 = 232 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_line(x0, bot, x1, top, c);                                       /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line(px, py, x1 + i, EGY(lvl), c);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line(px, py, x3, EGY(sus), c);                                    /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line(px, py, x3 + i, EGY(sus * e >> 15), c);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    cv_line(0, bot + 1, 239, bot + 1, C_LINE);
#undef EGY
}

/* ------------------------------------------------------------- FM4 --- */
static const uint8_t FM4_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}
};
static const uint16_t FM4_MOD[8] = {
    0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000
};
static const char *const FM4_ROUTING_NAMES[8] = {
    "4 > 3 > 2 > 1", "(3+4) > 2 > 1", "(2+4) > 1, 3 > 2", "(2+3) > 1, 4 > 3",
    "2 > 1, 4 > 3", "4 > (1, 2, 3)", "4 > 3, 1, 2", "1, 2, 3, 4 [PARALLEL]"
};

/* Digital 4-Operator Classic FM Routing Graph */
static void graph_digital_alg(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, bus = 78;
    char alg_str[8] = {'A', 'L', 'G', ' ', (char)('1' + alg), 0};

    cv_text(8, 2, &FONT_S, alg_str, hot == P_E0 ? C_WHITE : C_HI);

    for (k = 0; k < 4u; k++) {
        int32_t col = FM4_CELL[alg][k] & 7;
        if (col < lo) lo = col;
        if (col > hi) hi = col;
    }
    for (k = 0; k < 4u; k++) {
        int32_t col = FM4_CELL[alg][k] & 7;
        int32_t row = FM4_CELL[alg][k] >> 4;
        x[k] = 120 + col * 32 - (lo + hi) * 16 - 18;
        y[k] = bus - 18 - row * 20;
    }

    /* routes */
    for (k = 0; k < 4u; k++) {
        uint32_t m = (FM4_MOD[alg] >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) {
            if (m & (1u << j)) {
                int32_t sx = x[j] + 18, sy = y[j] + 16;
                int32_t dx = x[k] + 18, dy = y[k];
                if (sx == dx) {
                    cv_line(sx, sy, dx, dy, TE_G3);
                } else {
                    int32_t mid_y = (sy + dy) / 2;
                    cv_line(sx, sy, sx, mid_y, TE_G3);
                    cv_line(sx, mid_y, dx, mid_y, TE_G3);
                    cv_line(dx, mid_y, dx, dy, TE_G3);
                }
                cv_rect(dx - 1, dy - 2, 3, 2, C_WHITE);
            }
        }
        /* Carrier output to bus */
        if ((FM4_CELL[alg][k] >> 4) == 0) {
            cv_line(x[k] + 18, y[k] + 16, x[k] + 18, bus, c);
            cv_rect(x[k] + 17, bus, 3, 2, c);
        }
    }

    /* Audio bus horizontal line */
    cv_line(24, bus, 216, bus, C_LINE);
    cv_text(220, bus - 6, &FONT_S, "OUT", c);

    /* Feedback loop on Op 4 */
    {
        int32_t fx = x[3] + 36, fy = y[3] + 8;
        uint16_t fb_col = t->p[P_E6] > 0 ? rgb_blend(TE_G3, C_WHITE, (uint32_t)clamp(t->p[P_E6] * 2, 0, 255)) : TE_G2;
        cv_line(fx, fy, fx + 6, fy, fb_col);
        cv_line(fx + 6, fy, fx + 6, fy - 12, fb_col);
        cv_line(fx + 6, fy - 12, x[3] + 18, fy - 12, fb_col);
        cv_line(x[3] + 18, fy - 12, x[3] + 18, y[3], fb_col);
        cv_rect(x[3] + 17, y[3] - 2, 3, 2, fb_col);
        if (t->p[P_E6] > 0)
            cv_text(fx + 8, fy - 10, &FONT_S, "FB", fb_col);
    }

    /* Draw operator boxes */
    for (k = 0; k < 4u; k++) {
        int32_t bx = x[k], by = y[k];
        int is_car = (FM4_CELL[alg][k] >> 4) == 0;
        uint32_t op_hot = (k == 0 && hot == P_E0) || (k == 1 && hot == P_E1) || (k == 2 && hot == P_E2) || (k == 3 && hot == P_E3);
        uint16_t border = op_hot ? C_WHITE : is_car ? c : TE_G3;
        uint16_t bg = op_hot ? TE_G2 : is_car ? C_BLACK : TE_G1;
        char num[2] = {(char)('1' + k), 0};
        const char *r_str = (k == 0) ? "x1" : N_RATIO[t->p[P_E0 + k] % 15];

        cv_rect(bx, by, 36, 16, border);
        cv_rect(bx + 1, by + 1, 34, 14, bg);

        cv_text(bx + 3, by + 1, &FONT_S, num, is_car ? C_WHITE : C_HI);
        cv_text(bx + 14, by + 1, &FONT_S, r_str, op_hot ? C_WHITE : C_AMB);
    }
}

/* EDIT 2: timbre heatmap envelope */
static void graph_digital_timbre(const track_t *t, uint16_t c)
{
    int32_t a = 4 + t->p[P_ATK] * 50 / 127, d = 6 + t->p[P_DEC] * 50 / 127, r = 6 + t->p[P_REL] * 60 / 127;
    int32_t top = 12, bot = 88, sus = t->p[P_SUS] * 1000 / 127;
    int32_t x0 = 8, x1 = x0 + a, x3 = 230 - r, i;
    int32_t base_idx = t->p[P_E4];          /* 0..127 */
    int32_t moddec = t->p[P_E5];            /* 0..127 */
    int32_t px, py, pe, e;

#define TIMBRE_COLOR(idx_val) \
    ((idx_val) <= 32 ? rgb_blend(c, TE_MID[song.sel & 3u], (uint32_t)((idx_val) * 255) / 32u) : \
     (idx_val) <= 80 ? rgb_blend(TE_MID[song.sel & 3u], RGB(255, 180, 24), (uint32_t)(((idx_val) - 32) * 255) / 48u) : \
                       rgb_blend(RGB(255, 180, 24), C_WHITE, (uint32_t)(((idx_val) - 80) * 255) / 47u))

    /* Header text */
    cv_text(8, 0, &FONT_S, "TIMBRE HEATMAP", C_GRAY);
    if (base_idx >= 80)
        cv_text(160, 0, &FONT_S, "HOT / BITE", C_WHITE);
    else if (base_idx >= 35)
        cv_text(150, 0, &FONT_S, "HARMONIC", RGB(255, 180, 24));
    else
        cv_text(156, 0, &FONT_S, "MELLOW", C_DIM);

    cv_line(0, bot + 1, 239, bot + 1, C_LINE);

    px = x0; py = bot;
    for (i = 0; i <= a; i++) {
        int32_t cur_x = x0 + i;
        int32_t lvl = (1000 * i) / (a ? a : 1);
        int32_t cur_y = bot - lvl * (bot - top) / 1000;
        int32_t cur_idx = (base_idx * i) / (a ? a : 1);
        uint16_t col = TIMBRE_COLOR(cur_idx);
        if (cur_y < bot)
            cv_line(cur_x, cur_y + 1, cur_x, bot, rgb_blend(col, C_BLACK, 160));
        cv_line(px, py, cur_x, cur_y, col);
        px = cur_x; py = cur_y;
    }

    e = 32768; pe = 32768;
    for (i = 1; i <= d; i++) {
        int32_t cur_x = x1 + i;
        e = (e * (32768 - 150733 / d)) >> 15;
        pe = (pe * (32768 - clamp(moddec * 2500 / 127 + 500, 500, 32000) / d)) >> 15;
        int32_t lvl = sus + ((1000 - sus) * e >> 15);
        int32_t cur_y = bot - lvl * (bot - top) / 1000;
        int32_t cur_idx = (base_idx * 25 / 100) + ((base_idx * 75 / 100) * pe >> 15);
        uint16_t col = TIMBRE_COLOR(cur_idx);
        if (cur_y < bot)
            cv_line(cur_x, cur_y + 1, cur_x, bot, rgb_blend(col, C_BLACK, 160));
        cv_line(px, py, cur_x, cur_y, col);
        px = cur_x; py = cur_y;
    }

    {
        int32_t cur_idx = base_idx * 25 / 100;
        uint16_t col = TIMBRE_COLOR(cur_idx);
        int32_t cur_y = bot - sus * (bot - top) / 1000;
        for (i = px; i <= x3; i++) {
            if (cur_y < bot)
                cv_line(i, cur_y + 1, i, bot, rgb_blend(col, C_BLACK, 180));
        }
        cv_line(px, py, x3, cur_y, col);
        px = x3; py = cur_y;
    }

    e = 32768;
    for (i = 1; i <= r; i++) {
        int32_t cur_x = x3 + i;
        e = (e * (32768 - 150733 / r)) >> 15;
        int32_t lvl = (sus * e) >> 15;
        int32_t cur_y = bot - lvl * (bot - top) / 1000;
        int32_t cur_idx = (base_idx * 25 / 100) * e >> 15;
        uint16_t col = TIMBRE_COLOR(cur_idx);
        if (cur_y < bot)
            cv_line(cur_x, cur_y + 1, cur_x, bot, rgb_blend(col, C_BLACK, 200));
        cv_line(px, py, cur_x, cur_y, col);
        px = cur_x; py = cur_y;
    }

    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &t->v[i];
        if (v->active && v->gate && v->stage <= 2) {
            int32_t vx = x1;
            if (v->stage == 1) vx = x0 + a / 2;
            else if (v->stage == 2) vx = x1 + d / 2;
            int32_t vy = bot - (v->env >> 14) * (bot - top) / 1000;
            vy = clamp(vy, top - 2, bot);
            cv_rect(vx - 2, vy - 2, 5, 5, C_WHITE);
            cv_rect(vx - 1, vy - 1, 3, 3, RGB(255, 60, 40));
        }
    }
#undef TIMBRE_COLOR
}

/* Floyd Steinberg Visual FM Synthesizer (Lines + Points Vector UI & Spectral Glow) */
static void graph_floyd(const track_t *t, uint16_t c)
{
    (void)c;
    uint32_t p = floyd_part(t);
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    const page_t *pg = cur_page();

    static const uint8_t FLOYD_MOD_MASK[8] = { 0x0E, 0x0E, 0x0E, 0x0E, 0x0A, 0x0E, 0x08, 0x00 };
    static const uint16_t OP_COLS[4] = { FLOYD_OP1_COL, FLOYD_OP2_COL, FLOYD_OP3_COL, FLOYD_OP4_COL };

    /* 1. Header Bar */
    static const char *const ALG_ROUTING[8] = {
        "ALG 1: 4>3>2>1",
        "ALG 2: (3+4)>2>1",
        "ALG 3: (2+4)>1, 3>2",
        "ALG 4: (2+3)>1, 4>3",
        "ALG 5: 2>1, 4>3",
        "ALG 6: (4+3+2)>1",
        "ALG 7: 4>3, 1, 2",
        "ALG 8: 1+2+3+4"
    };

    if (pg->scope == SC_FLOYD && pg->title[0] == 'O' && pg->title[1] == 'P') {
        static const char *const OP_TITLES[4] = {
            "OP1: CARRIER (CYAN)",
            "OP2: MODULATOR (AMBER)",
            "OP3: MODULATOR (EMERALD)",
            "OP4: MODULATOR (MAGENTA)"
        };
        uint32_t op_idx = (uint32_t)(pg->title[2] - '1');
        cv_text(4, 0, &FONT_S, OP_TITLES[op_idx & 3u], C_WHITE);
    } else if (pg->scope == SC_FLOYD && pg->title[0] == 'L') {
        cv_text(4, 0, &FONT_S, "LEVEL: OP1..OP4 DEPTHS", C_WHITE);
    } else if (pg->scope == SC_FLOYD && pg->title[0] == 'R') {
        cv_text(4, 0, &FONT_S, "RATIO: MULTIPLIERS & FB", C_WHITE);
    } else {
        cv_text(4, 0, &FONT_S, ALG_ROUTING[alg], hot == P_E0 ? C_WHITE : C_HI);
    }

    /* Determine Active Operator */
    int32_t active_op = -1;
    if (pg->scope == SC_FLOYD) {
        if (pg->title[0] == 'O' && pg->title[1] == 'P') {
            active_op = (int32_t)(pg->title[2] - '1');
        } else if (pg->title[0] == 'L') { /* LEVEL page */
            active_op = ui.hot_t ? (int32_t)(ui.hot_col & 3u) : -1;
        } else if (pg->title[0] == 'R') { /* RATIO page */
            if (ui.hot_t) {
                uint32_t hcol = ui.hot_col & 3u;
                active_op = (hcol == 0) ? 3 : (int32_t)hcol;
            }
        }
    } else if (ui.hot_t) {
        if (hot == P_E1 || hot == P_E4) active_op = 1;
        else if (hot == P_E2 || hot == P_E6) active_op = 2;
        else if (hot == P_E3 || hot == P_E7) active_op = 3;
        else if (hot == P_ATK || hot == P_DEC || hot == P_SUS || hot == P_REL) active_op = 0;
    }

    /* Separator line below header */
    cv_line(0, 13, 239, 13, TE_G2);

    /* 2. Canvas Geometry */
    int32_t x0 = 8, w = 224;
    int32_t bot_y = 92, top_y = 22, h = bot_y - top_y;

    /* Base line */
    cv_line(x0, bot_y + 1, x0 + w, bot_y + 1, TE_G2);

    int32_t op_atk[4] = { t->p[P_ATK], floyd_state[p].atk[1], floyd_state[p].atk[2], floyd_state[p].atk[3] };
    int32_t op_dec[4] = { t->p[P_DEC], floyd_state[p].dec[1], floyd_state[p].dec[2], floyd_state[p].dec[3] };
    int32_t op_sus[4] = { t->p[P_SUS], floyd_state[p].sus[1], floyd_state[p].sus[2], floyd_state[p].sus[3] };
    int32_t op_rel[4] = { t->p[P_REL], floyd_state[p].rel[1], floyd_state[p].rel[2], floyd_state[p].rel[3] };
    int32_t op_lvl[4] = { floyd_state[p].lvl[0], t->p[P_E4], t->p[P_E6], (floyd_state[p].lvl[3] + t->p[P_E7]) / 2 };

    /* 3. Compute Envelopes for all 4 Operators */
    struct {
        int32_t x[5], y[5];
        int connected;
        int is_carrier;
    } env[4];

    for (uint32_t k = 0; k < 4u; k++) {
        int is_car = (FLOYD_MOD_MASK[alg] & (1u << k)) == 0;
        env[k].is_carrier = is_car;
        env[k].connected = 1;

        int32_t a = clamp(op_atk[k] * 40 / 127, 2, 40);
        int32_t d = clamp(op_dec[k] * 55 / 127, 2, 55);
        int32_t s_lvl = clamp(op_sus[k] * h / 127, 0, h);
        int32_t r = clamp(op_rel[k] * 50 / 127, 2, 50);
        int32_t pk = is_car ? h : clamp(op_lvl[k] * h / 127, 4, h);

        int32_t sus_len = w - a - d - r;
        if (sus_len < 10) sus_len = 10;
        int32_t sus_end = x0 + a + d + sus_len;
        if (sus_end > x0 + w - r) sus_end = x0 + w - r;

        env[k].x[0] = x0;               env[k].y[0] = bot_y;
        env[k].x[1] = x0 + a;           env[k].y[1] = bot_y - pk;
        env[k].x[2] = x0 + a + d;       env[k].y[2] = bot_y - s_lvl * pk / (h ? h : 1);
        env[k].x[3] = sus_end;          env[k].y[3] = bot_y - s_lvl * pk / (h ? h : 1);
        env[k].x[4] = x0 + w;           env[k].y[4] = bot_y;
    }

    /* 4. Spectral Gradient Background Glow (Dark to Light Harmonic Tones) */
    {
        int32_t tot_mod = (op_lvl[1] + op_lvl[2] + op_lvl[3]) / 3;
        uint32_t lut_idx = (uint32_t)clamp(tot_mod * 11 / 127, 0, 11);
        uint16_t glow_col = SPECTRAL_LUT_RGB565[lut_idx];

        /* Soft luminous aura under the sound */
        for (int32_t seg = 0; seg < 4; seg++) {
            int32_t x_start = env[0].x[seg], x_end = env[0].x[seg + 1];
            int32_t y_start = env[0].y[seg], y_end = env[0].y[seg + 1];
            for (int32_t px = x_start; px <= x_end; px++) {
                int32_t py = (x_end == x_start) ? y_start : y_start + (y_end - y_start) * (px - x_start) / (x_end - x_start);
                if (py < bot_y) {
                    cv_line(px, py + 1, px, bot_y, rgb_blend(glow_col, C_BLACK, 220));
                }
            }
        }
    }

    /* 5. Render Inactive Operators First (Fine 1px Lines + Small Dots) */
    for (uint32_t k = 0; k < 4u; k++) {
        if ((int32_t)k == active_op) continue;
        uint16_t col = OP_COLS[k];
        uint16_t dim_col = rgb_blend(col, C_BLACK, 85);

        /* Polyline */
        for (int32_t seg = 0; seg < 4; seg++) {
            cv_line(env[k].x[seg], env[k].y[seg], env[k].x[seg + 1], env[k].y[seg + 1], dim_col);
        }
        /* Small Node Points */
        for (int32_t p_idx = 1; p_idx <= 3; p_idx++) {
            cv_rect(env[k].x[p_idx] - 1, env[k].y[p_idx] - 1, 3, 3, dim_col);
        }
    }

    /* 6. Render Active Operator on Top (Bold 2px Line + Large White-Cored Points + Halo) */
    if (active_op >= 0 && active_op < 4) {
        uint32_t k = (uint32_t)active_op;
        uint16_t col = OP_COLS[k];

        /* Bold 2px Line */
        for (int32_t seg = 0; seg < 4; seg++) {
            cv_line(env[k].x[seg], env[k].y[seg], env[k].x[seg + 1], env[k].y[seg + 1], col);
            cv_line(env[k].x[seg], env[k].y[seg] - 1, env[k].x[seg + 1], env[k].y[seg + 1] - 1, col);
        }

        /* Large 5x5 Points with White Center */
        for (int32_t p_idx = 0; p_idx <= 4; p_idx++) {
            int32_t px = env[k].x[p_idx], py = env[k].y[p_idx];
            cv_rect(px - 2, py - 2, 5, 5, col);
            cv_rect(px - 1, py - 1, 3, 3, C_WHITE);
        }

        /* Target Halo for Active Parameter Node */
        int32_t hx = -1, hy = -1;
        if (ui.hot_t) {
            uint32_t hcol = ui.hot_col & 3u;
            if (pg->scope == SC_FLOYD && pg->title[0] == 'O' && pg->title[1] == 'P') {
                if (hcol == 0) { hx = env[k].x[1]; hy = env[k].y[1]; }
                else if (hcol == 1) { hx = env[k].x[2]; hy = env[k].y[2]; }
                else if (hcol == 2) { hx = (env[k].x[2] + env[k].x[3]) / 2; hy = env[k].y[2]; }
                else if (hcol == 3) { hx = env[k].x[4]; hy = env[k].y[4]; }
            } else {
                hx = env[k].x[1]; hy = env[k].y[1];
            }
        }

        if (hx >= 0) {
            cv_rect(hx - 4, hy - 4, 9, 1, C_WHITE);
            cv_rect(hx - 4, hy + 4, 9, 1, C_WHITE);
            cv_rect(hx - 4, hy - 3, 1, 7, C_WHITE);
            cv_rect(hx + 4, hy - 3, 1, 7, C_WHITE);
        }
    }

    /* 7. Live Note Tracer (Follows the active operator's real ADSR trajectory) */
    uint32_t trace_op = (active_op >= 0 && active_op < 4) ? (uint32_t)active_op : 0u;
    for (uint32_t i = 0; i < NVOICE; i++) {
        const voice_t *v = &t->v[i];
        if (!v->active) continue;

        int32_t stage = (trace_op == 0) ? v->stage : v->s[trace_op + 2u];
        int32_t env_val = (trace_op == 0) ? v->env : v->s[trace_op - 1u];

        if (stage <= 0 || stage > 3 || env_val <= 0) continue;

        int32_t pk = (trace_op == 0 || (FLOYD_MOD_MASK[alg] & (1u << trace_op)) == 0) ? h : clamp(op_lvl[trace_op] * h / 127, 4, h);
        int32_t x0_p = env[trace_op].x[0], y0_p = env[trace_op].y[0];
        int32_t x1_p = env[trace_op].x[1], y1_p = env[trace_op].y[1];
        int32_t x2_p = env[trace_op].x[2], y2_p = env[trace_op].y[2];
        int32_t x3_p = env[trace_op].x[3], y3_p = env[trace_op].y[3];
        int32_t x4_p = env[trace_op].x[4], y4_p = env[trace_op].y[4];

        int32_t tx = x0_p;
        int32_t ty_env = bot_y - (int32_t)(((int64_t)pk * env_val) >> 24);
        int32_t ty = clamp(ty_env, y1_p, bot_y);

        if (stage == 1) { /* Attack: travels diagonally on segment (x0,y0) -> (x1,y1) */
            int32_t a_yrange = y0_p - y1_p;
            if (a_yrange > 0) {
                tx = x0_p + (x1_p - x0_p) * (y0_p - ty) / a_yrange;
            } else {
                tx = x1_p;
            }
        } else if (stage == 2) { /* Decay & Sustain */
            int32_t d_yrange = y2_p - y1_p;
            if (d_yrange > 0 && ty < y2_p) {
                /* On Decay slope: travels diagonally on segment (x1,y1) -> (x2,y2) */
                tx = x1_p + (x2_p - x1_p) * (ty - y1_p) / d_yrange;
            } else {
                /* On Sustain plateau: travels horizontally on segment (x2,y2) -> (x3,y3) */
                ty = y2_p;
                int32_t s_xrange = x3_p - x2_p;
                int32_t s_prog = clamp((int32_t)v->age * 2, 0, s_xrange > 0 ? s_xrange : 0);
                tx = x2_p + s_prog;
            }
        } else if (stage == 3) { /* Release: travels diagonally on segment (x3,y3) -> (x4,y4) */
            int32_t r_yrange = y4_p - y3_p;
            if (r_yrange > 0) {
                int32_t r_ty = clamp(ty_env, y3_p, y4_p);
                ty = r_ty;
                tx = x3_p + (x4_p - x3_p) * (r_ty - y3_p) / r_yrange;
            } else {
                ty = y4_p;
                tx = x4_p;
            }
        }

        tx = clamp(tx, x0, x0 + w);
        ty = clamp(ty, top_y, bot_y);
        cv_rect(tx - 3, ty - 3, 7, 7, C_WHITE);
        cv_rect(tx - 2, ty - 2, 5, 5, OP_COLS[trace_op]);
        cv_rect(tx - 1, ty - 1, 3, 3, RGB(255, 245, 100));
    }
}

static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, py = 50;
    uint32_t ph = (uint32_t)t->p[P_LPHASE] << 25;
    for (x = 0; x < 240; x++) {                      /* (lfo_wave only reads the track) */
        int32_t y = 50 - lfo_wave((track_t *)t, ph + (uint32_t)x * (0xFFFFFFFFu / 120u)) * 38 / 32768;
        if (t->p[P_LWAVE] == 4)
            y = 50 - ((int32_t)((x / 20 * 2654435761u) >> 16) - 32768) * 38 / 32768;
        if (x)
            cv_line(x - 1, py, x, y, c);
        py = y;
    }
    cv_line(0, 50, 239, 50, C_LINE);
}

static void graph_steps(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    for (i = 0; i < NSTEP; i++) {                    /* 4 rows of 16 thin bars */
        int32_t x = 6 + (int32_t)(i % 16u) * 14 + (int32_t)(i % 16u) / 4 * 4, y = 6 + (int32_t)(i / 16u) * 24;
        const step_t *st = &t->step[i];
        if (i >= len)
            continue;
        if (step_on(st))
            cv_rect(x, y, 1, 14, c);
        else if (st->time == ST_TIE)
            cv_rect(x, y + 6, 1, 8, C_DIM);
        else
            cv_rect(x, y + 13, 1, 1, C_DIM);
        if (st->flags & SF_ACCENT)
            cv_rect(x - 1, y - 2, 3, 1, c);
        if ((song.playing && i == t->seq_idx) || i == ui.cursor)
            cv_rect(x - 1, y + 16, 3, 3, C_WHITE);
    }
}
/* STEP page: the cursor's 16-step bank as a little piano roll */
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = 0, prev_y = -1;
    for (i = 0; i < len; i++)
        for (j = 0; j < t->step[i].n; j++) {
            if (t->step[i].note[j] < lo)
                lo = t->step[i].note[j];
            if (t->step[i].note[j] > hi)
                hi = t->step[i].note[j];
        }
    if (lo > hi) {
        lo = 48;
        hi = 72;
    }
    if (hi - lo < 12)
        hi = lo + 12;
    for (i = 0; i < 16u; i++) {
        uint32_t si = base + i;
        const step_t *st = &t->step[si];
        int32_t x = (int32_t)i * 15, yb = 86;
        if (si >= len)
            break;
        if (si == ui.cursor)
            cv_rect(x + 5, 92, 3, 3, C_WHITE);
        if (song.playing && si == t->seq_idx)
            cv_rect(x + 1, 97, 12, 1, C_WHITE);
        if (st->time == ST_TIE && prev_y >= 0) {
            cv_rect(x, prev_y, 14, 1, C_GRAY);
            continue;
        }
        if (!step_on(st)) {
            cv_rect(x + 6, yb, 2, 1, C_DIM);
            prev_y = -1;
            continue;
        }
        for (j = 0; j < st->n; j++) {
            int32_t y = 80 - (st->note[j] - lo) * 74 / (hi - lo);
            cv_rect(x + 2, y, 10, 1, (st->flags & SF_ACCENT) ? C_WHITE : c);
            if (j == 0)
                prev_y = y;
        }
        if (st->flags & SF_SLIDE)
            cv_line(x + 11, prev_y, x + 17, prev_y + 2, c);
    }
}
static void graph_scale(const track_t *t, uint16_t c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t i, mask = scale_mask(t);
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - (uint32_t)t->p[P_ROOT]) % 12u;
        int32_t x = 6 + (int32_t)i * 19;
        uint16_t col = (mask >> deg) & 1u ? (deg == 0u ? C_WHITE : c) : C_DIM;
        cv_rect(x, BLACK[i] ? 10 : 40, 16, 1, col);
        cv_rect(x + 7, BLACK[i] ? 10 : 40, 1, 40, col);
    }
}

static void graph_fx(const track_t *t, uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t h = t->p[P_DIST + i] * 80 / 127, x = (int32_t)i * 60 + 28;
        cv_rect(x, 10, 1, 80, C_LINE);
        cv_rect(x, 90 - h, 1, h, c);
        cv_rect(x - 3, 90 - h, 7, 1, c);
    }
}
/* SLICER page: the pattern's 16 steps, a 'x' step a full bar; a '.' step: GATE a bar as high as
 * it stays open (DEPTH), STUT hatched (it repeats the last 'x'); the step playing underlined.
 * Grey when the SLICER is OFF. */
static void graph_slicer(const track_t *t, uint16_t c)
{
    uint32_t i, pat = sl_pattern(t), mode = (uint32_t)t->p[P_SLCR], cur = sl[t - trk].idx;
    int32_t open = 70 - t->p[P_SLDEPTH] * 70 / 127;      /* px a closed GATE step keeps */
    uint16_t col = mode == SL_OFF ? C_DIM : c;
    for (i = 0; i < 16u; i++) {
        int32_t x = 4 + (int32_t)i * 14 + (int32_t)(i / 4u) * 2, y;
        if ((pat >> i) & 1u) {
            cv_rect(x, 10, 11, 70, col);
        } else if (mode == SL_STUT) {
            for (y = 10; y < 80; y += 4)
                cv_rect(x, y, 11, 1, col);
        } else {
            cv_rect(x, 79, 11, 1, C_LINE);
            if (open)
                cv_rect(x, 80 - open, 11, open, C_DIM);
        }
        if (mode != SL_OFF && i == cur)
            cv_rect(x, 85, 11, 3, C_WHITE);
    }
}
static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &t->step[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (ui.hot_t && settings.zoom)
        h = str_hash(str_hash(str_hash(h ^ 0x5555u, ui.focus_v), ui.focus_l), ui.focus_u);
    if (ui.home)
        return h ^ (ui.frame / 2u);                  /* scope: redraw every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u;
    if (pg->graph == GR_SLCR && t->p[P_SLCR])        /* the SLICER's step playing */
        h ^= (sl[song.sel].idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLOTS)                       /* (a checksum over each slot) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)project_used(i) << (20u + i);
    if (pg->graph == GR_STEPS || pg->graph == GR_ROLL) {
        uint32_t ph = song.playing ? t->seq_idx : 0xFFFFu;
        if (pg->graph == GR_ROLL && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u;
    }
    if (pg->scope == SC_FLOYD) {
        uint32_t part = floyd_part(t);
        for (i = 0; i < 4u; i++) {
            h ^= (uint32_t)floyd_state[part].atk[i] * 19u + (uint32_t)floyd_state[part].dec[i] * 23u +
                 (uint32_t)floyd_state[part].sus[i] * 29u + (uint32_t)floyd_state[part].rel[i] * 31u +
                 (uint32_t)floyd_state[part].lvl[i] * 37u;
        }
    }
    if ((pg->scope == SC_ENGINE || pg->scope == SC_FLOYD) && (ENGINES[t->eng_req % NENGINES] == &ENG_DIGITAL || ENGINES[t->eng_req % NENGINES] == &ENG_FLOYD)) {
        uint32_t v_active = 0;
        for (i = 0; i < NVOICE; i++) {
            if (t->v[i].active) {
                v_active |= (1u << i) | ((uint32_t)(t->v[i].env >> 16) << (4u + (i % 4u) * 4u)) | ((uint32_t)t->v[i].stage << (20u + (i % 8u)));
            }
        }
        h ^= v_active + (ui.frame / 2u) * (v_active != 0);
    }
    return h;
}
/* preset browser: the list by kind (ui.c BANK), the current one in white */
static void graph_browse(void)
{
    uint32_t total, cur = preset_pos(&total), e, k;
    int32_t row;
    if (!total)
        return;
    for (row = -3; row <= 3; row++) {
        int32_t y = 4 + (row + 3) * 17;
        char tag[5], nm[13];
        int sel = row == 0;
        uint32_t n = (cur + total * 4u + (uint32_t)row) % total;
        e = preset_at(n, &k);
        if (e == NENGINES) {                             /* user preset: "U07" and its name */
            up_slot_label(tag, k);
            up_name(k, nm);
        } else {                                         /* its kind: BASS, KEYS, PAD... */
            str_cpy(tag, preset_kind(n), sizeof tag);
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
        }
        if (sel)
            cv_rect(4, y + 6, 3, 3, C_WHITE);
        cv_text(14, y, &FONT_S, tag, sel ? C_GRAY : C_DIM);
        cv_text(54, y, &FONT_S, nm, sel ? C_WHITE : C_GRAY);
    }
}

/* user preset slots around the selected one: "U07  NAME" / EMPTY, the selected one in white */
static void graph_user(void)
{
    int32_t row, first = clamp((int32_t)ui.uslot - 3, 0, UP_SLOTS - 7);
    for (row = 0; row < 7; row++) {
        uint32_t k = (uint32_t)(first + row);
        int32_t y = 4 + row * 17;
        char tag[4], nm[13];
        int sel = k == ui.uslot, used = up_used(k);
        up_slot_label(tag, k);
        if (used)
            up_name(k, nm);
        else
            str_cpy(nm, "EMPTY", sizeof nm);
        if (sel)
            cv_rect(4, y + 6, 3, 3, C_WHITE);
        cv_text(14, y, &FONT_S, tag, sel ? C_WHITE : C_GRAY);
        cv_text(54, y, &FONT_S, nm, used ? (sel ? C_WHITE : C_HI) : C_DIM);
    }
}

/* project slots: used / empty, the selected one in white */
static void graph_slots(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t y = 8 + (int32_t)i * 26;
        char b[4];
        int sel = (int32_t)i + 1 == song.g[G_SLOT];
        b[0] = (char)('1' + i);
        b[1] = 0;
        if (sel)
            cv_rect(4, y + 6, 3, 3, C_WHITE);
        cv_text(14, y, &FONT_S, b, sel ? C_WHITE : C_GRAY);
        cv_text(40, y, &FONT_S, project_used(i) ? "USED" : "EMPTY", project_used(i) ? (sel ? C_WHITE : C_HI) : C_DIM);
    }
}

/* TRACKS page: four channel strips (mixer style), one under each column: number +
 * REC / ARM / MUTE, the sound's short name, then a level fader with the output meter
 * (note activity), the pattern over its LEN (time running down; bar width = notes in
 * the step) and the play head. The selected track is drawn bright. Every part is
 * its own small canvas with its own signature: while the transport runs only the
 * head markers and the meters move. (No ZOOM focus here.) */
#define TS_HEAD_Y (Y_GRAPH + 2)
#define TS_NAME_Y (Y_GRAPH + 20)
#define TS_Y (Y_GRAPH + 38)
#define TS_H 84                                      /* body: rows Y_GRAPH + 38 .. + 121 */
static struct {
    uint32_t head[NTRK], name[NTRK], fader[NTRK], ov[NTRK], mark[NTRK];
    uint8_t meter[NTRK];
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 6 dB = TS_H / 10 */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_H - 2) / 80, 0, TS_H - 2);
}

static uint32_t trk_level(uint32_t c)                /* LEVEL 0..127 (the drum track: GLO > DRUMS LEVEL) */
{
    return (uint32_t)(c == TRK_DRUM ? song.g[G_DRLVL] : trk[c].p[P_LEVEL]) & 127u;
}

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    if (c == TRK_DRUM)
        str_cpy(b, "DRUM", 13);
    else if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

static void draw_tracks(void)
{
    uint32_t c;
    if (ui.force) {
        lcd_fill(0, Y_GRAPH, 240, H_GRAPH, C_BLACK);
        for (c = 0; c < NTRK; c++)
            ts.meter[c] = 0;
    }
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        uint32_t x0 = c * 60u + 4u, sel = c == song.sel, lvl = trk_level(c), mute = !lvl || t->p[P_MUTE];
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, row = 0xFFFFu;
        int32_t pk, m;
        char b[16];
        if (c == TRK_DRUM) {
            pk = drums.peak;
            drums.peak = 0;
        } else {
            pk = t->peak;
            t->peak = 0;
        }
        /* head: number (white = selected), REC / ARM / MUTE */
        sig = 1u + sel + st * 2u;
        if (ui.force || sig != ts.head[c]) {
            ts.head[c] = sig;
            b[0] = (char)('1' + c);
            b[1] = 0;
            cv_begin(54, 16, C_BLACK);
            cv_text(0, 0, &FONT_S, b, sel ? C_WHITE : C_GRAY);
            if (sel)
                cv_rect(0, 15, 8, 1, C_WHITE);
            if (st == 1u || st == 2u) {
                cv_rect(14, 5, 6, 6, st == 1u ? C_WHITE : C_AMB);
                cv_text(24, 0, &FONT_S, st == 1u ? "REC" : "ARM", st == 1u ? C_WHITE : C_AMB);
            } else if (st) {
                cv_text(14, 0, &FONT_S, "MUTE", C_DIM);
            }
            cv_blit(x0, TS_HEAD_Y);
        }
        /* the sound: preset name (user preset, DRUM), cut to the column */
        trk_short_name(c, b);
        while (b[0] && text_w(&FONT_S, b) > 54)
            b[str_len(b) - 1u] = 0;
        sig = str_hash(2u + sel, b);
        if (ui.force || sig != ts.name[c]) {
            ts.name[c] = sig;
            cv_begin(54, 16, C_BLACK);
            cv_text(0, 0, &FONT_S, b, sel ? C_HI : C_DIM);
            cv_blit(x0, TS_NAME_Y);
        }
        /* fader (LEVEL) + meter of the output */
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 3)
            m = ts.meter[c] - 3;                     /* falls ~20 dB/s */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = 1u + lvl + ts.meter[c] * 128u + sel * 65536u + (sel && ui.hot_t && ui.hot_col == 1u) * 131072u +
              (t->p[P_MUTE] != 0) * 262144u;
        if (ui.force || sig != ts.fader[c]) {
            int32_t fy = (int32_t)(TS_H - 2u) - (int32_t)lvl * (TS_H - 4) / 127;
            ts.fader[c] = sig;
            cv_begin(12, TS_H, C_BLACK);
            cv_rect(3, 0, 1, TS_H, C_LINE);
            if (lvl) {
                cv_rect(2, fy, 3, TS_H - fy, C_DIM);
                cv_rect(0, fy - 1, 7, 2, (sig & 131072u) ? C_WHITE : sel ? C_HI : C_GRAY);
            } else {
                cv_rect(0, TS_H - 2, 7, 2, C_DIM);
            }
            if (ts.meter[c])
                cv_rect(9, TS_H - ts.meter[c], 3, ts.meter[c], C_AMB);
            cv_blit(x0, TS_Y);
        }
        /* the pattern: one band per step over LEN, a note step as wide as its notes */
        sig = steps_hash(t) * 31u + len * 7u + sel;
        if (ui.force || sig != ts.ov[c]) {
            uint32_t i;
            ts.ov[c] = sig;
            cv_begin(34, TS_H, C_BLACK);
            cv_rect(17, 0, 1, TS_H, C_LINE);
            for (i = 0; i < len; i++) {
                const step_t *s = &t->step[i];
                int32_t r0 = (int32_t)(i * TS_H / len), r1 = (int32_t)((i + 1u) * TS_H / len), h = r1 - r0;
                if (h > 2)
                    h--;                             /* a gap between steps when there is room */
                if (h < 1)
                    h = 1;
                if (step_on(s)) {
                    int32_t w = 2 + 6 * (int32_t)s->n;
                    cv_rect(17 - w / 2, r0, w, h, (s->flags & SF_ACCENT) && sel ? C_WHITE : sel ? C_HI : C_GRAY);
                } else if (s->time == ST_TIE) {
                    cv_rect(16, r0, 3, r1 - r0, C_DIM);
                }
            }
            cv_blit(x0 + 14u, TS_Y);
        }
        /* play head: a small arrow right of the pattern (white while recording) */
        if (song.playing)
            row = ((t->seq_idx % len) * 2u + 1u) * TS_H / (2u * len);
        sig = 1u + row + (st == 1u) * 65536u;
        if (ui.force || sig != ts.mark[c]) {
            int32_t i;
            ts.mark[c] = sig;
            cv_begin(5, TS_H, C_BLACK);
            if (row != 0xFFFFu)
                for (i = -2; i <= 2; i++) {
                    int32_t w = 5 - 2 * (i < 0 ? -i : i);
                    cv_rect(5 - w, (int32_t)row + i, w, 1, st == 1u ? C_WHITE : C_AMB);
                }
            cv_blit(x0 + 49u, TS_Y);
        }
    }
}

/* oscilloscope of the output, triggered on a rising zero crossing */
static void graph_scope(uint16_t c)
{
    static int16_t snap[SCOPE_N];
    uint32_t w = scope_w, i, trig = 0;
    int32_t py = 50, x, peak = 1500;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_line(0, 50, 239, 50, C_LINE);
    for (x = 0; x < 240; x++) {
        int32_t y = 50 - snap[trig + (uint32_t)x] * 44 / peak;   /* auto-scaled */
        if (x)
            cv_line(x - 1, py, x, y, c);
        py = y;
    }
}

static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = TE_COL[song.sel & 3u];               /* LIVE: the curves in the track's colour */
    uint32_t sig, top, drum_note = !ui.home && is_drum(t) && !page_for_drum(pg);
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        ui.graph_top = 1;                            /* the next graph draws its top rows again */
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    cv_begin(240, H_GRAPH, C_BLACK);
    cv_oy = G_OY;
    if (ui.home) {
        graph_scope(c);
    } else if (drum_note) {                          /* a page the drum track has no use for */
        static const char *const L[2] = {"DRUM TRACK", "SEQ  TRACKS  GLO DRUMS"};
        cv_text((240 - text_w(&FONT_S, L[0])) / 2, 26, &FONT_S, L[0], C_HI);
        cv_text((240 - text_w(&FONT_S, L[1])) / 2, 52, &FONT_S, L[1], C_DIM);
    } else if (pg->scope == SC_ENGINE && ENGINES[t->eng_req % NENGINES] == &ENG_DIGITAL) {
        if (pg->id[0] == P_E0)
            graph_digital_alg(t, c);
        else if (pg->id[0] == P_E4)
            graph_digital_timbre(t, c);
    } else if ((pg->scope == SC_ENGINE || pg->scope == SC_FLOYD) && ENGINES[t->eng_req % NENGINES] == &ENG_FLOYD) {
        graph_floyd(t, c);
    } else {
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_LFO:
            graph_lfo(t, c);
            break;
        case GR_STEPS:
            graph_steps(t, c);
            break;
        case GR_ROLL:
            graph_roll(t, c);
            break;
        case GR_SCALE:
            graph_scale(t, c);
            break;
        case GR_FX:
            graph_fx(t, c);
            break;
        case GR_SLCR:
            graph_slicer(t, c);
            break;
        case GR_BROWSE:
            cv_oy = 0;
            graph_browse();
            break;
        case GR_SLOTS:
            cv_oy = 0;
            graph_slots();
            break;
        case GR_USER:
            cv_oy = 0;
            graph_user();
            break;
        default:
            break;
        }
    }
    top = !ui.home && !drum_note && (pg->graph == GR_BROWSE || pg->graph == GR_SLOTS || pg->graph == GR_USER);   /* these draw from the top */
    cv_oy = 0;
    if (ui.hot_t && settings.zoom) {                 /* focus (menu ZOOM): the touched value, large and white */
        int32_t x;
        top = 1;
        cv_rect(0, 0, 150, 50, C_BLACK);
        cv_text(4, 0, &FONT_S, ui.focus_l, C_GRAY);
        x = cv_text(4, 16, &FONT_L, ui.focus_v, C_WHITE);
        cv_text(x + 4, 30, &FONT_S, ui.focus_u, C_DIM);
    }
    /* graphs keep out of the top G_OY rows: skip them unless something is (or was) there */
    cv_blit_from(0, Y_GRAPH, top || ui.graph_top || ui.force ? 0u : G_OY);
    ui.graph_top = (uint8_t)top;
}

static void draw_foot(void)
{
    char s[48], pn[16], en[10], ti[20];
    const track_t *t = TSEL;
    uint32_t sig;
    const page_t *pg = cur_page();
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const char *ename = is_drum(t) ? "DRUM" : e->name;
    int32_t x;
    pn[0] = 0;
    if (is_drum(t))
        str_cpy(pn, DRUM_KIT_NAMES[drum_kit()], sizeof pn);
    else if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), pn);                       /* a user preset */
    else if (e->npresets)
        str_cpy(pn, e->presets[TSEL->preset % e->npresets].name, sizeof pn);
    if (ui.home) {
        str_cpy(ti, "HOME", sizeof ti);
    } else {                                           /* page title + number in its family: "ENV DEST 2/2" */
        uint32_t i, n = 0, k = 0;
        const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == pg->fam && page_valid_for_trk(&PAGES[i])) {
                n++;
                if (i == ui.page)
                    k = n;
            }
        str_cpy(ti, pt ? pt : pg->title, 10);
        if (n > 1) {
            str_cpy(ti + str_len(ti), " ", 4);
            fmt_int(ti + str_len(ti), (int32_t)k);
            str_cpy(ti + str_len(ti), "/", 4);
            fmt_int(ti + str_len(ti), (int32_t)n);
        }
    }
    str_cpy(s, ename, sizeof s);
    s[str_len(s) + 1u] = 0;
    s[str_len(s)] = (char)('1' + song.sel);
    str_cpy(s + str_len(s), pn, 16);
    str_cpy(s + str_len(s), ti, sizeof ti);
    {   /* step markers: the playhead only when it is in the shown bank, the cursor only in SEQ */
        uint32_t ph = song.playing && t->seq_idx / 16u == ui.bank ? t->seq_idx : 0xFFu;
        sig = str_hash(0x9E3779B9u, s) + ph * 97u + (song.seq_mode ? ui.cursor : 0xFFu) * 3001u + steps_hash(t) +
              ui.bank * 7u + (uint32_t)t->p[P_SLEN] * 13u;
    }
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    cv_begin(240, H_FOOT, C_BLACK);
    {
        uint32_t i;
        for (i = 0; i < 16u; i++) {                   /* row 1: the cursor's bank as 16 thin bars */
            uint32_t si = ui.bank * 16u + i;
            int32_t sx = 6 + (int32_t)i * 14 + (int32_t)(i / 4u) * 4;
            const step_t *st = &t->step[si];
            if (si >= (uint32_t)t->p[P_SLEN])
                continue;
            if (step_on(st))
                cv_rect(sx, 2, 2, 9, TE_COL[song.sel & 3u]);
            else
                cv_rect(sx, 10, 1, 1, C_DIM);
            if ((song.playing && si == t->seq_idx) || (song.seq_mode && si == ui.cursor))
                cv_rect(sx - 1, 13, 3, 3, C_WHITE);
        }
    }
    x = 4;
    if (FELUCCA_ICONS) {                              /* row 2: engine icon + name, preset, page */
        cv_icon(x, 21, engine_icon(ename), C_GRAY);
        x += 14;
    }
    fit(en, ename, &FONT_S, 90 - (x - 4));
    x = cv_text(x, 20, &FONT_S, en, C_HI);
    {
        char pf[16];
        int32_t room = 236 - text_w(&FONT_S, ti) - 10 - (x + 10);
        str_cpy(pf, pn, sizeof pf);
        while (pf[0] && text_w(&FONT_S, pf) > room)
            pf[str_len(pf) - 1u] = 0;
        cv_text(x + 10, 20, &FONT_S, pf, C_AMB);
    }
    cv_text(236 - text_w(&FONT_S, ti), 20, &FONT_S, ti, C_GRAY);
    cv_blit(0, Y_FOOT);
}
static void draw_columns(void)
{
    uint32_t c;
    char val[12];
    const char *unit;
    if (ui.home) {
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            param_format(d, *vp, val, &unit);
            draw_column(c, d->label, val, unit, VAL(c), RATIO(d, *vp), param_icon(d, *vp));
        }
        return;
    }
    if (is_drum(TSEL) && !page_for_drum(cur_page())) {   /* "DRUM TRACK" (the graph says so) */
        for (c = 0; c < 4u; c++)
            draw_column(c, "", "", "", C_HI, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_TRK) {                 /* TRACK LEVEL LEN PAN of the selected track */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        fmt_int(val, (int32_t)song.sel + 1);
        draw_column(0, "TRACK", val, "/4", VAL(0u), (int32_t)song.sel * 1000 / (NTRK - 1), ICON_AUTO);
        if (!lvl || t->p[P_MUTE]) {                    /* (MUTE: a turn of KNOB 2 unmutes, tracks_edit) */
            str_cpy(val, "MUTE", 12);
            unit = "";
        } else if (is_drum(t)) {
            fmt_int(val, (int32_t)lvl);
            unit = "";
        } else {
            param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);
        }
        draw_column(1, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(1u) : C_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        param_format(&TP[P_SLEN], t->p[P_SLEN], val, &unit);
        draw_column(2, "LEN", val, unit, VAL(2u), RATIO(&TP[P_SLEN], t->p[P_SLEN]), ICON_AUTO);
        param_format(&TP[P_PAN], t->p[P_PAN], val, &unit);
        draw_column(3, "PAN", val, unit, VAL(3u), RATIO(&TP[P_PAN], t->p[P_PAN]), param_icon(&TP[P_PAN], t->p[P_PAN]));
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {
        uint32_t total, cur = preset_pos(&total);
        char u[8];
        fmt_int(val, (int32_t)cur + 1);
        str_cpy(u, "/", 8);
        fmt_int(u + 1, (int32_t)total);
        draw_column(0, "No.", val, u, VAL(0u), -1, ICON_NONE);
        draw_column(1, "ENG", ENGINES[TSEL->eng_req]->name, "", VAL(1u), -1, engine_icon(ENGINES[TSEL->eng_req]->name));
        draw_column(2, "", "", "", C_HI, -1, ICON_AUTO);
        draw_column(3, "", "", "", C_HI, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_USER) {                  /* SLOT, then three GO buttons */
        int used = up_used(ui.uslot);
        up_slot_label(val, ui.uslot);
        draw_column(0, "SLOT", val, "", VAL(0u), (int32_t)ui.uslot * 1000 / (int32_t)(UP_SLOTS - 1u), ICON_AUTO);
        draw_column(1, "LOAD", "--", "", used ? C_HI : C_DIM, -1, ICON_AUTO);
        draw_column(2, "ERASE", "--", "", used ? C_HI : C_DIM, -1, ICON_AUTO);
        draw_column(3, "SAVE", "--", "", C_HI, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &TSEL->step[ui.cursor];
        char u[8];
        if (st->n) {
            note_name(val, st->note[0]);
            u[0] = 0;
            if (st->n > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, st->n - 1);
            }
        } else {
            str_cpy(val, "--", 12);
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8];
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : C_DIM, -1, ICON_AUTO);
            draw_column(2, "TIME", TIME_N[st->time % 3u], "", VAL(2u), -1, ICON_AUTO);
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), -1, ICON_AUTO);
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", C_HI, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_MIDI && cur_page()->scope == SC_GLOBAL) {
            str_cpy(val, !usb.up ? "OFF" : usb.config ? "MIDI" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT", 12);
            unit = "USB";
            draw_column(c, "USB", val, unit, C_HI, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_INFO && cur_page()->scope == SC_GLOBAL) {
            fmt_int(val, (int32_t)(song.cpu_q8 * 100u / 256u));
            unit = "%";
        } else {
            param_format(d, *vp, val, &unit);
        }
        draw_column(c, d->label, val, unit, VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp),
                    param_icon(d, *vp));
    }
}


/* the UI's timers, once a frame whatever the screen (a message, the BPM highlight, an armed GO, the
 * white value of the knob just turned) */
static void ui_timers(void)
{
    if (ui.msg_t)
        ui.msg_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.arm_t && !--ui.arm_t)
        ui.arm = 0;
    if (ui.hot_t)
        ui.hot_t--;
}

static void ui_draw(void)
{
    ui.frame++;
    pads_tick();
    if (rec_go) {                                       /* the take started: say so */
        rec_go = 0;
        ui_message("RECORDING");
    }
    if (ft_bars) {                                      /* a free take closed: the loop it made */
        char m[24];
        uint32_t n = ft_bars;
        ft_bars = 0;
        if (n == 0xFFu) {
            ui_message("TAKE DROPPED");
        } else {
            str_cpy(m, n == 1u ? "LOOP 1 BAR " : n == 2u ? "LOOP 2 BARS " : "LOOP 4 BARS ", sizeof m);
            fmt_int(m + str_len(m), song.g[G_BPM]);
            str_cpy(m + str_len(m), " BPM", sizeof m - str_len(m));
            ui_message(m);
        }
    }
    if (er_flash) {                                     /* EDIT + key took something out */
        er_flash = 0;
        if (!ui.msg_t)
            ui_message("ERASED");
    }
    if (!ui.menu && (ui.layer != LY_PLAY || ui.hold_kind)) {   /* a layer held / a hold to confirm */
        if (ui.hold_kind)
            hold_screen_draw();
        else
            layer_screen_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    if (layer_shown) {                                  /* back to the page */
        layer_shown = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    if ((rec_wait || ft_on) && !ui.menu && !on_song_page()) {   /* armed / free take */
        rec_screen_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    if (rec_shown) {                                    /* back to the page */
        rec_shown = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
#if FELUCCA_ARRANGER
    if (!ui.menu && on_song_page()) {
        song_screen_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
#endif
    if (ui.menu) {
        draw_menu();
        ui_timers();
        ui.force = 0;
        return;
    }
    if (!ui.home && cur_page()->scope == SC_TRK) {
        studio_tracks_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    if (on_drum_page()) {
        drum_screen_draw();
        ui_timers();
        ui.force = 0;
        return;
    }
    cursor_fix();
    if (ui.force)
        draw_frame();
    felucca_dbg.stage = 3;
    draw_head();
    felucca_dbg.stage = 4;
    draw_columns();
    felucca_dbg.stage = 5;
    draw_graph();
    ui_timers();
    felucca_dbg.stage = 6;
    draw_foot();
    ui.force = 0;
}
