/* SPDX-License-Identifier: GPL-3.0-only */
/* LAYERS, teenage-engineering style: hold a function button, touch a key (or turn a knob). While the
 * button is held the screen shows what the 16 white keys and KNOB 1..4 do now; tapped (pressed and
 * let go without touching anything) the button opens its pages as before.
 *   FX    punch-in effects (punch.c)       knobs: FILTER  DUST  DUCK
 *   EDIT  erase that sound / note (seq.c)  knobs: SHIFT  LENGTH x2 / half  TRANSPOSE   OCT- undo  OCT+ redo
 *   ARP   note repeat (seq.c roll)         knobs: RATE
 *   SEQ   steps 1..16 of the page          knobs: SOUND / NOTE  DIV  SWING  LENGTH;  a step key held:
 *         (black keys 1..4: pages)                SOUND / NOTE  LEVEL  RATCHET
 *   SCL   any key: the key of the song     knobs: CHORD  SCALE  KEYS  TRANSPOSE
 *   GLO   keys 1..4 mute, 5..8 solo,       knobs: the levels of tracks 1..4
 *         the last white key: tap tempo
 *   SAVE  keys 1..4 play section A..D (on the next bar), 5..8 store the loop into A..D, 13 loop / song,
 *         14 SONG REC (the order you play becomes the song), 16 the song page
 * The keys' part runs in the audio ISR (seq.c layer_now: no lag, no lost press); the SEQ, SCL and
 * GLO keys come to the UI through seq.c lk_q. HOLD: REC held clears the track, SAVE held saves the
 * project (a ring fills; let go before and nothing happens). */
static const uint8_t LAYER_BTN[LY_COUNT] = {NB, B_FX, B_EDIT, B_ARP, B_SEQ, B_SCL, B_GLO, B_SAVE};
static const char *const LAYER_NAME[LY_COUNT] = {"", "punch", "erase", "roll", "steps", "key", "mix", "song"};
static void section_store(uint32_t s);                  /* project.c */
static void section_load(uint32_t s);
static uint8_t sec_armed;                               /* store over a used section: the key again within 3 s */
static uint32_t sec_armed_ms;
#define TAP_MS 450u                                     /* a press shorter than this, untouched: a tap */
#define SHOW_MS 140u                                    /* the layer shows after this (a tap does not flash it) */

/* the ISR's view of the panel: the layers' buttons, OCT- / OCT+, REC / PLAY of a free take */
static void layers_init(void)
{
    uint32_t l;
    for (l = LY_FX; l < LY_COUNT; l++)
        ly_bit[l] = 1u << panel.btn[LAYER_BTN[l]];
    dyn_bit[0] = 1u << panel.btn[B_OCTDN];
    dyn_bit[1] = 1u << panel.btn[B_OCTUP];
    ft_btn_mask = 1u << panel.btn[B_REC];
    ft_drop_mask = 1u << panel.btn[B_PLAY];
}

static uint32_t key_of_white(uint32_t w) { return key_of_lane(w & 15u); }   /* white key w -> key index */

/* ------------------------------------------------------------ tools --- */
/* EDIT layer knobs; each a step of one undo (the layer's hold) */
static void layer_undo_mark(track_t *t)
{
    if (!ui.step_sess)
        ui.step_sess = (undo_sess += 4u) | 3u;
    undo_mark(t, ui.step_sess);
}
static void pattern_rotate(track_t *t, int32_t d)       /* every step one later (d > 0) / earlier */
{
    uint32_t len = trk_len(t), i;
    step_t keep;
    layer_undo_mark(t);
    fm1_irq_off();
    if (d > 0) {
        keep = t->step[len - 1u];
        for (i = len - 1u; i > 0; i--)
            t->step[i] = t->step[i - 1u];
        t->step[0] = keep;
    } else {
        keep = t->step[0];
        for (i = 0; i + 1u < len; i++)
            t->step[i] = t->step[i + 1u];
        t->step[len - 1u] = keep;
    }
    fm1_irq_on();
}
static void pattern_length(track_t *t, int32_t d)       /* x2 (the pattern again after itself), or half */
{
    uint32_t len = trk_len(t), i;
    layer_undo_mark(t);
    fm1_irq_off();
    if (d > 0 && len * 2u <= NSTEP) {
        for (i = 0; i < len; i++)
            t->step[len + i] = t->step[i];
        t->p[P_SLEN] = (int16_t)(len * 2u);
    } else if (d < 0 && len >= 2u) {
        t->p[P_SLEN] = (int16_t)(len / 2u);
    }
    fm1_irq_on();
    {
        char b[8];
        fmt_int(b, t->p[P_SLEN]);
        ui_say("STEPS ", b);
    }
}
static void pattern_transpose(track_t *t, int32_t d)    /* every note a semitone up / down (synth parts) */
{
    uint32_t i, j;
    if (is_drum(t))
        return;
    layer_undo_mark(t);
    fm1_irq_off();
    for (i = 0; i < NSTEP; i++)
        for (j = 0; j < t->step[i].n && j < 4u; j++)
            t->step[i].note[j] = (uint8_t)clamp(t->step[i].note[j] + (d > 0 ? 1 : -1), 0, 127);
    fm1_irq_on();
}

/* ------------------------------------------------------------- SEQ --- */
/* a step key (white key w) of the SEQ layer, Elektron style: an empty step is set at once (drums: the
 * sound's lane; synth: the pen, the chord / note played last); a set one goes when its key is let go,
 * unless a knob changed it meanwhile (level, ratchet, note) */
static uint16_t step_pend_off;                         /* the step keys that clear their step when let go */
static int step_is_on(const track_t *t, uint32_t idx)
{
    return is_drum(t) ? dstep_has(&t->dstep[idx], pen_lane) : step_on(&t->step[idx]);
}
static void step_down(uint32_t w)
{
    track_t *t = TSEL;
    uint32_t idx = ui.step_page * 16u + w;
    if (idx >= trk_len(t))
        return;
    if (song.playing && arrangement_enabled) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    if (step_is_on(t, idx)) {
        step_pend_off |= (uint16_t)(1u << w);
        return;
    }
    layer_undo_mark(t);
    fm1_irq_off();
    if (is_drum(t)) {
        dstep_set(&t->dstep[idx], pen_lane, LV_NORM, 0);
    } else {
        step_t *s = &t->step[idx];
        uint32_t i, n = pen_n ? pen_n : 1u;
        memset(s, 0, sizeof *s);
        for (i = 0; i < n && i < 4u; i++)
            s->note[i] = pen_n ? pen_note[i] : last_note;
        s->n = (uint8_t)(t->p[P_VOICE] == V_POLY ? n : 1u);
        s->time = ST_NOTE;
        s->vel = 100;
    }
    t->seq_active = 1;
    fm1_irq_on();
    sync_reload = 1;
}
static void step_up(uint32_t w)
{
    track_t *t = TSEL;
    uint32_t idx = ui.step_page * 16u + w;
    if (!((step_pend_off >> w) & 1u))
        return;
    step_pend_off &= (uint16_t)~(1u << w);
    if (idx >= trk_len(t) || !step_is_on(t, idx))
        return;
    layer_undo_mark(t);
    fm1_irq_off();
    if (is_drum(t))
        dstep_clr(&t->dstep[idx], pen_lane);
    else
        step_clear(&t->step[idx]);
    fm1_irq_on();
    sync_reload = 1;
}
/* KNOB 2 / 3 with step keys held: their level / ratchet (drums: the sound's lane; synth: every note) */
static void steps_held_edit(uint32_t knob, int32_t s)
{
    track_t *t = TSEL;
    uint32_t w, i;
    layer_undo_mark(t);
    step_pend_off &= (uint16_t)~ui.step_held;            /* (edited: kept when let go) */
    fm1_irq_off();
    for (w = 0; w < 16u; w++) {
        uint32_t idx = ui.step_page * 16u + w;
        if (!((ui.step_held >> w) & 1u) || idx >= trk_len(t))
            continue;
        if (is_drum(t)) {
            dstep_t *d = &t->dstep[idx];
            uint32_t lv = dstep_lvl(d, pen_lane), rt = dstep_rat(d, pen_lane);
            if (!dstep_has(d, pen_lane))
                continue;
            if (knob == 1u)
                lv = LV_UP[clamp((int32_t)lvl_rank(lv) + s, 0, 3)];
            else
                rt = (uint32_t)clamp((int32_t)rt + s, 0, 3);
            dstep_set(d, pen_lane, lv, rt);
        } else {
            step_t *st = &t->step[idx];
            if (!step_on(st))
                continue;
            for (i = 0; i < st->n; i++) {
                uint32_t sh = 2u * i, lv = (st->lvl >> sh) & 3u, rt = (st->rat >> sh) & 3u;
                if (knob == 0u) {
                    st->note[i] = (uint8_t)clamp(st->note[i] + s, 0, 127);
                    continue;
                }
                if (knob == 1u)
                    lv = LV_UP[clamp((int32_t)lvl_rank(lv) + s, 0, 3)];
                else
                    rt = (uint32_t)clamp((int32_t)rt + s, 0, 3);
                st->lvl = (uint8_t)((st->lvl & ~(3u << sh)) | lv << sh);
                st->rat = (uint8_t)((st->rat & ~(3u << sh)) | rt << sh);
            }
        }
    }
    fm1_irq_on();
    sync_reload = 1;
}

/* ------------------------------------------------------------- GLO --- */
static void tap_tempo(void)
{
    uint32_t now = fm1_ms, i, sum = 0;
    if (ui.tap_n && now - ui.tap_ms[(ui.tap_n - 1u) & 3u] > 2000u)
        ui.tap_n = 0;                                   /* a pause: a new count */
    ui.tap_ms[ui.tap_n & 3u] = now;
    ui.tap_n++;
    if (ui.tap_n < 2u)
        return;
    {
        uint32_t n = ui.tap_n - 1u > 3u ? 3u : ui.tap_n - 1u;
        for (i = 0; i < n; i++)
            sum += ui.tap_ms[(ui.tap_n - 1u - i) & 3u] - ui.tap_ms[(ui.tap_n - 2u - i) & 3u];
        if (sum)
            song.g[G_BPM] = (int16_t)clamp((int32_t)((60000u * n + sum / 2u) / sum), 40, 240);
        ui.bpm_t = 40;
    }
}

/* a key of the layers the UI runs (seq.c lk_q): SEQ steps, SCL key, GLO mix */
static void layer_key(uint32_t layer, uint32_t k, uint32_t down)
{
    int32_t w = punch_key(k);
    if (!down) {
        if (w >= 0) {
            ui.step_held &= (uint16_t)~(1u << w);
            if (layer == LY_STEP)
                step_up((uint32_t)w);
        }
        return;
    }
    switch (layer) {
    case LY_STEP:
        if (w < 0) {                                    /* the first four black keys: pages 1..4 */
            static const int8_t PG[12] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, -1, -1};
            if (k < 12u && PG[k] >= 0 && (uint32_t)PG[k] * 16u < trk_len(TSEL))
                ui.step_page = (uint8_t)PG[k];
            return;
        }
        ui.step_held |= (uint16_t)(1u << w);
        step_down((uint32_t)w);
        return;
    case LY_SCALE: {                                    /* the key of the song: every synth part */
        uint32_t i, root = (53u + k) % 12u;
        for (i = 0; i < NTRK; i++)
            if (trk_is_part(i))
                trk[i].p[P_ROOT] = (int16_t)root;
        ui_say("KEY ", N_NOTE[root]);
        return;
    }
    case LY_SONG: {                                     /* sections A..D: play, store; loop / song; SONG REC */
        char b[2] = {0, 0};
        if (w < 0)
            return;
        b[0] = (char)('A' + (w & 3));
        if (w < 4) {
            if (arrangement_clock.running) {
                ui_message("SONG PLAYS");
            } else if (!((arrangement_ready() >> w) & 1u)) {
                ui_say("EMPTY ", b);
            } else if (song.playing) {
                live_req = (int8_t)w;
                ui_say("NEXT: ", b);
            } else {
                section_load((uint32_t)w);
                ui_say("LOADED ", b);
            }
        } else if (w < 8) {
            uint32_t s = (uint32_t)w - 4u;
            if (((arrangement_ready() >> s) & 1u) && !(sec_armed == s + 1u && fm1_ms - sec_armed_ms < 3000u)) {
                sec_armed = (uint8_t)(s + 1u);
                sec_armed_ms = fm1_ms;
                ui_say("AGAIN: ", b);
            } else {
                sec_armed = 0;
                section_store(s);
                ui_say("SAVED ", b);
            }
        } else if (w == 12) {
            if (srec) {
                ui_message("REC IS ON");
            } else {
                arrangement_enabled ^= 1u;
                ui_message(arrangement_enabled ? "SONG MODE" : "LOOP MODE");
            }
        } else if (w == 13) {
            if (srec) {
                fm1_irq_off();
                srec_stop();                            /* (playing: the order so far is the song) */
                fm1_irq_on();
            } else if (arrangement_clock.running) {
                ui_message("STOP FIRST");
            } else {
                arrangement_enabled = 0;
                srec = 1;
                ui_message(live_sec < 0 ? "PICK A PART" : "REC: NEXT BAR");
            }
        } else if (w == 15) {
            studio_open(SC_SONG);
        }
        return;
    }
    case LY_MIX:                                        /* white 1..8 mute, 9..16 solo; the first black key: tap */
        if (w >= 0 && w < (int32_t)NTRK) {
            trk[w].p[P_MUTE] = (int16_t)!trk[w].p[P_MUTE];
        } else if (w >= (int32_t)NTRK && w < 2 * (int32_t)NTRK) {
            song.solo ^= (uint8_t)(1u << (w - (int32_t)NTRK));
        } else if (k == 1u) {
            tap_tempo();
        }
        return;
    default:
        return;
    }
}

/* KNOB 1..4 while a layer is held: what the layer gives them (the page does not see them) */
static void layer_knobs(uint32_t layer)
{
    uint32_t k;
    int32_t s;
    track_t *t = TSEL;
    for (k = 0; k < 4u; k++) {
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        ui.layer_used = 1;
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
        switch (layer) {
        case LY_FX:
            if (k == 0u)
                song.g[G_FILT] = (int16_t)clamp(song.g[G_FILT] + accel(EN_K1, s, 127), -64, 63);
            else if (k == 1u)
                song.g[G_DUST] = (int16_t)clamp(song.g[G_DUST] + accel(EN_K2, s, 127), 0, 127);
            else if (k == 2u)
                song.g[G_DUCK] = (int16_t)clamp(song.g[G_DUCK] + accel(EN_K3, s, 127), 0, 127);
            break;
        case LY_ERASE:
            if (k == 0u)
                pattern_rotate(t, s);
            else if (k == 1u)
                pattern_length(t, s);
            else if (k == 2u)
                pattern_transpose(t, s);
            break;
        case LY_ROLL:
            if (k == 0u)
                song.g[G_ROLL] = (int16_t)clamp(song.g[G_ROLL] + s, 0, 4);
            break;
        case LY_STEP:
            if (ui.step_held && k < 3u) {
                if (k == 0u && is_drum(t))
                    pen_lane = (uint8_t)clamp(pen_lane + s, 0, DRUM_LANES - 1);
                else
                    steps_held_edit(k, s);
            } else if (k == 0u) {
                if (is_drum(t)) {
                    pen_lane = (uint8_t)clamp(pen_lane + s, 0, DRUM_LANES - 1);
                } else {
                    pen_note[0] = (uint8_t)clamp(pen_note[0] + s, 0, 127);
                    pen_n = 1;
                }
            } else if (k == 1u) {
                t->p[P_SDIV] = (int16_t)clamp(t->p[P_SDIV] + s, 0, 5);
            } else if (k == 2u) {
                t->p[P_SSWING] = (int16_t)clamp(t->p[P_SSWING] + accel(EN_K3, s, 100), 0, 100);
            } else {
                t->p[P_SLEN] = (int16_t)clamp(t->p[P_SLEN] + accel(EN_K4, s, 63), 1, NSTEP);
                if ((uint32_t)ui.step_page * 16u >= trk_len(t))
                    ui.step_page = (uint8_t)((trk_len(t) - 1u) / 16u);
            }
            break;
        case LY_SCALE:
            if (k == 0u) {
                if (!is_drum(t))
                    t->p[P_CHORD] = (int16_t)clamp(t->p[P_CHORD] + s, 0, 5);
            } else if (k == 1u) {
                uint32_t i;
                int16_t v = (int16_t)clamp(trk[0].p[P_SCALE] + s, 0, NSCALES - 1);
                for (i = 0; i < NTRK; i++)
                    if (trk_is_part(i))
                        trk[i].p[P_SCALE] = v;
            } else if (k == 2u) {
                if (!is_drum(t))
                    t->p[P_QUANT] = (int16_t)clamp(t->p[P_QUANT] + s, 0, 2);
            } else if (!is_drum(t)) {
                t->p[P_TRANS] = (int16_t)clamp(t->p[P_TRANS] + s, -24, 24);
            }
            break;
        case LY_MIX: {                                  /* the levels of the four tracks shown (the bank) */
            uint32_t i = (song.sel & ~3u) + k;
            int16_t *lv = i == TRK_DRUM ? &song.g[G_DRLVL] : &trk[i].p[P_LEVEL];
            *lv = (int16_t)clamp(*lv + accel(EN_K1 + k, s, 127), 0, 127);
            break;
        }
        default:
            break;
        }
    }
}

/* --------------------------------------------------------- drawing --- */
/* 16 tiles (the white keys, 4 x 4) under a title band, the knobs' dials at the bottom */
typedef struct {
    char lab[8];
    uint16_t bg, fg, top;        /* fill, text, the 3-pixel top band (0 = none) */
    uint8_t marks;               /* small marks under the label (a ratchet), 0 = none */
} tile_t;

static void tiles_draw(const tile_t *tl, uint32_t *cache)
{
    uint32_t r, c, sig = 7u;
    for (r = 0; r < 16u; r++)
        sig = studio_hash(sig * 31u + tl[r].bg * 3u + tl[r].fg * 5u + tl[r].top * 7u + tl[r].marks, tl[r].lab);
    if (!ui.force && sig == *cache)
        return;
    *cache = sig;
    for (r = 0; r < 4u; r++) {
        cv_begin(240, 36, C_BLACK);
        for (c = 0; c < 4u; c++) {
            const tile_t *t = &tl[r * 4u + c];
            int32_t x = 2 + (int32_t)c * 60;
            uint32_t m;
            cv_rect(x, 2, 56, 32, t->bg);
            if (t->top)
                cv_rect(x, 2, 56, 3, t->top);
            te_text_c(x + 28, 9, t->lab, t->fg);
            for (m = 0; m < t->marks; m++)
                cv_rect(x + 22 + (int32_t)m * 5, 27, 3, 3, t->fg);
        }
        cv_blit(0, 40 + r * 36);
    }
}

static void layer_title(const char *name, const char *sub, uint16_t col, uint32_t *cache)
{
    uint32_t locked = ly_lock != LY_PLAY;
    uint32_t sig = studio_hash(studio_hash(col, name), sub) + (ui.msg_t ? studio_hash(3u, ui.msg) : 0u) + locked * 7919u;
    if (!ui.force && sig == *cache)
        return;
    *cache = sig;
    cv_begin(240, 40, C_BLACK);
    cv_text(4, 2, &FONT_L, name, col);
    cv_text(4 + text_w(&FONT_L, name) + 10, 18, &FONT_S, ui.msg_t ? ui.msg : sub, ui.msg_t ? C_WHITE : TE_G3);
    if (locked) {                                         /* locked open (HOME): any button lets it go */
        int32_t w = (int32_t)text_w(&FONT_S, "LOCK") + 8;
        cv_rect(236 - w, 4, w, 15, C_WHITE);
        cv_text(240 - w, 4, &FONT_S, "LOCK", C_BLACK);
    }
    cv_rect(0, 38, 240, 1, TE_G1);
    cv_blit(0, 0);
}

static uint8_t layer_shown;                              /* the screen holds a layer (or a hold) */
static void layer_screen_draw(void)
{
    static uint32_t head, tiles, foot;
    static tile_t tl[16];
    static char v[4][10], sub[24];
    const char *lab[4] = {"", "", "", ""}, *val[4] = {v[0], v[1], v[2], v[3]};
    int32_t ratio[4] = {-1, -1, -1, -1};
    uint32_t i, layer = ui.layer, sel = song.sel;
    track_t *t = TSEL;
    uint16_t col = TE_COL[sel % NTRK];
    if (!layer_shown) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
        layer_shown = 1;
    }
    for (i = 0; i < 4u; i++)
        v[i][0] = 0;
    memset(tl, 0, sizeof tl);
    for (i = 0; i < 16u; i++) {
        tl[i].bg = TE_G1;
        tl[i].fg = TE_G3;
    }
    str_cpy(sub, "track ", sizeof sub);
    sub[6] = (char)('1' + sel);
    sub[7] = 0;
    switch (layer) {
    case LY_FX:                                         /* the 16 punch-in effects */
        col = TE_DRUM;
        str_cpy(sub, "hold + key", sizeof sub);
        for (i = 0; i < 16u; i++) {
            static const char *const PSHORT[16] = {"loop 4", "loop 8", "loop16", "loop32", "stutt", "rev", "stop", "half",
                                                   "low", "high", "phone", "crush", "alias", "gate", "echo", "wobble"};
            int on = punch.req == (int8_t)i;
            str_cpy(tl[i].lab, PSHORT[i], 8);
            tl[i].bg = on ? C_WHITE : TE_G1;
            tl[i].fg = on ? C_BLACK : TE_G4;
            tl[i].top = on ? 0 : TE_DIM[i / 4u];
        }
        lab[0] = "filter", lab[1] = "dust", lab[2] = "duck";
        {
            const char *u;
            param_format(&GP[G_FILT], song.g[G_FILT], v[0], &u);
            te_lower(v[0], v[0], 8);
            fmt_int(v[1], song.g[G_DUST] * 100 / 127);
            fmt_int(v[2], song.g[G_DUCK] * 100 / 127);
        }
        ratio[0] = (song.g[G_FILT] + 64) * 1000 / 127;
        ratio[1] = song.g[G_DUST] * 1000 / 127;
        ratio[2] = song.g[G_DUCK] * 1000 / 127;
        break;
    case LY_ERASE:
    case LY_ROLL: {                                     /* the keys' sounds: lit = held */
        uint32_t held = fm1_in.notes;
        col = layer == LY_ERASE ? TE_RED : col;
        str_cpy(sub, layer == LY_ERASE ? (song.playing ? "as it plays" : "every step") : "hold + key", sizeof sub);
        for (i = 0; i < 16u; i++) {
            uint32_t k = key_of_white(i), down = (held >> k) & 1u, present = 0, j;
            if (is_drum(t)) {
                str_cpy(tl[i].lab, LANE_SHORT[i], 8);
                for (j = 0; j < trk_len(t); j++)
                    present |= dstep_has(&t->dstep[j], i);
            } else {
                uint32_t n = kb_map(t, k);
                if (n == KB_SILENT) {
                    str_cpy(tl[i].lab, "-", 8);
                } else {
                    note_name(tl[i].lab, n);
                    for (j = 0; j < trk_len(t); j++)
                        if (t->step[j].time == ST_NOTE) {
                            uint32_t q;
                            for (q = 0; q < t->step[j].n; q++)
                                present |= t->step[j].note[q] == n;
                        }
                }
            }
            tl[i].bg = down ? (layer == LY_ERASE ? TE_RED : C_WHITE) : TE_G1;
            tl[i].fg = down ? C_BLACK : present ? C_WHITE : TE_G3;
            tl[i].top = present && !down ? (layer == LY_ERASE ? TE_RED : col) : 0;
        }
        if (layer == LY_ERASE) {
            lab[0] = "shift", lab[1] = "length", lab[2] = is_drum(t) ? "" : "transp";
            str_cpy(v[0], "<  >", 8);
            fmt_int(v[1], t->p[P_SLEN]);
            str_cpy(v[2], is_drum(t) ? "" : "-  +", 8);
            str_cpy(sub, undo.valid ? (undo.undone ? "oct+ redo" : "oct- undo") : sub, sizeof sub);
        } else {
            lab[0] = "rate";
            str_cpy(v[0], N_ROLL[clamp(song.g[G_ROLL], 0, 4)], 8);
            ratio[0] = song.g[G_ROLL] * 250;
        }
        break;
    }
    case LY_STEP: {                                     /* the 16 steps of the page */
        uint32_t len = trk_len(t), page = ui.step_page;
        for (i = 0; i < 16u; i++) {
            uint32_t idx = page * 16u + i, on, lv = LV_NORM, rt = 0;
            if (idx >= len) {
                tl[i].bg = C_BLACK;
                continue;
            }
            if (is_drum(t)) {
                const dstep_t *d = &t->dstep[idx];
                on = dstep_has(d, pen_lane);
                if (on) {
                    lv = dstep_lvl(d, pen_lane);
                    rt = dstep_rat(d, pen_lane);
                }
            } else {
                const step_t *st = &t->step[idx];
                on = step_on(st);
                if (on) {
                    lv = st->lvl & 3u;
                    rt = st->rat & 3u;
                    note_name(tl[i].lab, st->note[0]);
                } else if (st->time == ST_TIE) {
                    str_cpy(tl[i].lab, "--", 8);
                }
            }
            if (!tl[i].lab[0])
                fmt_int(tl[i].lab, (int32_t)idx + 1);
            tl[i].bg = on ? (lv == LV_GHOST ? TE_DIM[sel & 3u] : lv == LV_SOFT ? TE_MID[sel & 3u] : lv == LV_HARD ? C_WHITE : col)
                          : TE_G1;
            tl[i].fg = on ? C_BLACK : TE_G3;
            tl[i].marks = (uint8_t)(on ? rt : 0u);
            if (song.playing && idx == t->seq_idx)
                tl[i].top = C_WHITE;
            if ((ui.step_held >> i) & 1u)
                tl[i].top = TE_RED;
        }
        if (len > 16u) {
            char p[4] = {'1', '/', '1', 0};
            p[0] = (char)('1' + page);
            p[2] = (char)('0' + (len + 15u) / 16u);
            str_cpy(sub, "page ", sizeof sub);
            str_cpy(sub + 5, p, 4);
        }
        lab[0] = is_drum(t) ? "sound" : "note";
        if (is_drum(t))
            str_cpy(v[0], LANE_SHORT[pen_lane], 8);
        else
            note_name(v[0], pen_note[0]);
        ratio[0] = is_drum(t) ? pen_lane * 1000 / 15 : pen_note[0] * 1000 / 127;
        if (ui.step_held) {
            lab[1] = "level", lab[2] = "ratchet";
            str_cpy(v[1], "-  +", 8);
            str_cpy(v[2], "x1 x4", 8);
        } else {
            lab[1] = "div", lab[2] = "swing", lab[3] = "steps";
            str_cpy(v[1], N_DIV[t->p[P_SDIV] % 6], 8);
            swing_str(v[2], t->p[P_SSWING]);
            fmt_int(v[3], t->p[P_SLEN]);
            ratio[1] = t->p[P_SDIV] * 200;
            ratio[2] = t->p[P_SSWING] * 10;
            ratio[3] = (t->p[P_SLEN] - 1) * 1000 / 63;
        }
        break;
    }
    case LY_SCALE: {                                    /* the white keys' notes / chords; the root lit */
        uint32_t root = (uint32_t)trk[0].p[P_ROOT] % 12u, mask = SCALE_MASK[clamp(trk[0].p[P_SCALE], 0, NSCALES - 1)];
        col = TE_COL[1];
        str_cpy(sub, "key: ", sizeof sub);
        str_cpy(sub + 5, N_NOTE[root], 4);
        str_cpy(sub + str_len(sub), " ", 2);
        te_lower(sub + str_len(sub), N_SCALE[clamp(trk[0].p[P_SCALE], 0, NSCALES - 1)], 8);
        for (i = 0; i < 16u; i++) {
            uint32_t k = key_of_white(i), pc = (53u + k) % 12u, in = (mask >> ((pc + 12u - root) % 12u)) & 1u;
            if (!is_drum(t) && t->p[P_CHORD]) {       /* chord mode: the chord this key plays (the i chord lit) */
                uint8_t c[4];
                uint32_t n = kb_map(t, k), m;
                if (n != KB_SILENT && (m = chord_notes(t, n, c)) != 0u) {
                    uint32_t third = m > 1u ? (uint32_t)(c[1] - c[0]) : 4u;
                    str_cpy(tl[i].lab, N_NOTE[c[0] % 12u], 8);
                    if (t->p[P_CHORD] == 5)
                        str_cpy(tl[i].lab + str_len(tl[i].lab), "5", 2);
                    else if (third == 3u)
                        str_cpy(tl[i].lab + str_len(tl[i].lab), "m", 2);
                    pc = c[0] % 12u;
                    in = 1;
                }
            } else {
                str_cpy(tl[i].lab, N_NOTE[pc], 8);
            }
            tl[i].bg = pc == root ? col : TE_G1;
            tl[i].fg = pc == root ? C_BLACK : in ? C_WHITE : TE_G2;
        }
        lab[0] = "chord", lab[1] = "scale", lab[2] = "keys", lab[3] = "transp";
        te_lower(v[0], N_CHORD[clamp(t->p[P_CHORD], 0, 5)], 8);
        te_lower(v[1], N_SCALE[clamp(trk[0].p[P_SCALE], 0, NSCALES - 1)], 8);
        te_lower(v[2], N_QUANT[clamp(t->p[P_QUANT], 0, 2)], 8);
        fmt_int(v[3], t->p[P_TRANS]);
        if (is_drum(t))
            v[0][0] = v[2][0] = v[3][0] = 0;
        ratio[0] = t->p[P_CHORD] * 200;
        ratio[1] = trk[0].p[P_SCALE] * 1000 / (NSCALES - 1);
        ratio[2] = t->p[P_QUANT] * 500;
        ratio[3] = (t->p[P_TRANS] + 24) * 1000 / 48;
        break;
    }
    case LY_MIX: {                                      /* mute 1..8, solo 1..8; tap on the first black key */
        char b[8];
        uint32_t first = song.sel & ~3u;
        col = C_WHITE;
        str_cpy(sub, "tap: black 1  bpm ", sizeof sub);
        fmt_int(b, song.g[G_BPM]);
        str_cpy(sub + str_len(sub), b, sizeof sub - str_len(sub));
        for (i = 0; i < NTRK; i++) {
            int m = trk[i].p[P_MUTE] != 0, so = (song.solo >> i) & 1u;
            str_cpy(tl[i].lab, "mute 1", 8);
            tl[i].lab[5] = (char)('1' + i);
            tl[i].bg = m ? TE_G2 : TE_COL[i];
            tl[i].fg = m ? TE_G3 : C_BLACK;
            str_cpy(tl[NTRK + i].lab, "solo 1", 8);
            tl[NTRK + i].lab[5] = (char)('1' + i);
            tl[NTRK + i].bg = so ? C_WHITE : TE_G1;
            tl[NTRK + i].fg = so ? C_BLACK : TE_G3;
            tl[NTRK + i].top = TE_DIM[i];
        }
        for (i = 0; i < 4u; i++) {                      /* KNOB 1..4: the levels of the bank shown on TRACKS */
            uint32_t lv = trk_level(first + i);
            static const char *const L[NTRK] = {"1", "2", "3", "4", "5", "6", "7", "8"};
            lab[i] = L[first + i];
            fmt_int(v[i], (int32_t)lv * 100 / 127);
            ratio[i] = (int32_t)lv * 1000 / 127;
        }
        break;
    }
    case LY_SONG: {                                     /* A..D (playing lit, next one framed), store, modes */
        uint32_t ready = arrangement_ready();
        static const char *const SL[4] = {"A", "B", "C", "D"};
        col = C_WHITE;
        if (srec == 2u) {
            char b[8];
            str_cpy(sub, "rec ", sizeof sub);
            str_cpy(sub + 4, SL[srec_e[srec_n ? srec_n - 1u : 0u].scene & 3u], 2);
            str_cpy(sub + str_len(sub), " bar ", 6);
            fmt_int(b, srec_n ? srec_e[srec_n - 1u].bars + 1 : 1);
            str_cpy(sub + str_len(sub), b, 6);
        } else {
            str_cpy(sub, arrangement_enabled ? "song mode" : srec ? "rec armed" : "play  store", sizeof sub);
        }
        for (i = 0; i < 4u; i++) {
            int used = (ready >> i) & 1u, playing = live_sec == (int8_t)i && !arrangement_clock.running;
            str_cpy(tl[i].lab, SL[i], 8);
            tl[i].bg = used ? (playing ? TE_COL[i] : TE_DIM[i]) : TE_G1;
            tl[i].fg = used ? C_BLACK : TE_G3;
            tl[i].top = live_req == (int8_t)i ? C_WHITE : 0;
            str_cpy(tl[4 + i].lab, "save A", 8);
            tl[4 + i].lab[5] = (char)('A' + i);
            tl[4 + i].bg = sec_armed == i + 1u ? TE_RED : TE_G1;
            tl[4 + i].fg = sec_armed == i + 1u ? C_BLACK : TE_G4;
            tl[4 + i].top = TE_DIM[i];
        }
        str_cpy(tl[12].lab, arrangement_enabled ? "song" : "loop", 8);
        tl[12].bg = arrangement_enabled ? C_WHITE : TE_G2;
        tl[12].fg = arrangement_enabled ? C_BLACK : C_WHITE;
        str_cpy(tl[13].lab, "rec", 8);
        tl[13].bg = srec == 2u ? TE_RED : TE_G1;
        tl[13].fg = srec == 2u ? C_BLACK : TE_RED;
        tl[13].top = srec == 1u ? TE_RED : 0;
        str_cpy(tl[15].lab, "chain", 8);
        tl[15].bg = TE_G1;
        tl[15].fg = TE_G4;
        break;
    }
    default:
        break;
    }
    layer_title(LAYER_NAME[layer % LY_COUNT], sub, col, &head);
    tiles_draw(tl, &tiles);
    {   /* (a message shows in the title: the dials stay) */
        uint8_t m = ui.msg_t;
        ui.msg_t = 0;
        te_dials(184, lab, val, ratio, layer * 7919u, &foot);
        ui.msg_t = m;
    }
}

/* HOLD to confirm: a ring fills while REC (clear the track) or SAVE (save the project) is held */
#define HOLD_CLEAR_MS 1300u                             /* after the 0.7 s that make REC a hold */
#define HOLD_SAVE_MS 1000u
static void hold_screen_draw(void)
{
    static uint32_t cache;
    uint32_t span = ui.hold_kind == 1u ? HOLD_CLEAR_MS : HOLD_SAVE_MS, el = fm1_ms - ui.hold_t0;
    int32_t ratio = el >= span ? 1000 : (int32_t)(el * 1000u / span);
    uint32_t sig = ui.hold_kind * 7u + (uint32_t)ratio / 20u + ui.hold_trk * 131u;
    char b[24];
    if (!layer_shown) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        layer_shown = 1;
        cache = ~sig;
    }
    if (sig == cache && !ui.force)
        return;
    cache = sig;
    cv_begin(240, 124, C_BLACK);
    cv_text(120 - text_w(&FONT_L, ui.hold_kind == 1u ? "clear" : "save") / 2, 4, &FONT_L,
            ui.hold_kind == 1u ? "clear" : "save", ui.hold_kind == 1u ? TE_RED : C_WHITE);
    {   /* the ring: 36 px, 7 thick, filling clockwise from the top */
        int32_t a, rr, end = ratio * 1024 / 1000;
        for (a = 0; a < 1024; a += 2) {
            int32_t co = SINE[((uint32_t)a + 768u) & 1023u], si = SINE[(uint32_t)a & 1023u];   /* from 12 o'clock */
            uint16_t c = a <= end ? (ui.hold_kind == 1u ? TE_RED : C_WHITE) : TE_G2;
            for (rr = 30; rr <= 36; rr++)
                cv_pset(120 + ((si * rr) >> 15), 80 + ((co * rr) >> 15), c);
        }
    }
    cv_blit(0, 20);
    cv_begin(240, 60, C_BLACK);
    if (ui.hold_kind == 1u) {
        str_cpy(b, "track 1", sizeof b);
        b[6] = (char)('1' + ui.hold_trk);
    } else {
        str_cpy(b, "project in slot 1", sizeof b);
        b[16] = (char)('0' + clamp(song.g[G_SLOT], 1, 4));
    }
    te_text_c(120, 4, b, TE_G4);
    te_text_c(120, 30, "keep holding", TE_G3);
    cv_blit(0, 160);
}
