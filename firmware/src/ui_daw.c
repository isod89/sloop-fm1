/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37: DAW mode, the UI's part (the map and its messages: smk_daw.h; the keys and pads: seq.c daw_*).
 * < + > together (the second pressed while the first is held) switches SYNTH <-> DAW, with a screen that says which;
 * so does HOME menu > MIDI > MODE. In DAW mode:
 *   the eight knobs (as printed, K1..K8; KNOB BANK: bank A / B), the four faders (FADER BANK: A / B), the pads (KNOB BANK
 *   + FADER BANK: A / B), 13 buttons and the wheels send what the map says; the keys notes on CHANNEL (OCT- / OCT+ the
 *   octave, both: 0). < and > send when let go (so both together stay the mode switch). PATCH tapped: LOCAL ON / OFF;
 *   held: the menu. PATCH held + OCT+ (or HOME menu > DAW > EDIT MAP): the map editor: touch a control (turn a knob,
 *   move a fader or a wheel, hit a pad, press a button), then KNOB 1 its TYPE, KNOB 2 its NUMBER, KNOB 3 its CHANNEL;
 *   OCT+ the next control, OCT- done. The editor edits the USER map and puts it in use (CC MAP USER).
 * The screen: the mode, CHANNEL, LOCAL, the last control touched (its message and value), the banks, the eight knobs
 * as bars. The pads light by kind (NOTE bank A blue, B green; CC yellow; a CC TOGGLE lit while on; OFF faint), held:
 * bright. The sequencer stops on the way in; MIDI IN still plays SLOOP (a sound module for the DAW). */

static uint8_t daw_kbank, daw_fbank;              /* the knobs' and faders' banks (the pads': seq.c daw_pbank) */
static uint8_t daw_kval[16];                      /* ABSOLUTE: each knob's value, per bank */
static daw_cv_t daw_cv[DAW_N] __attribute__((section(".pool")));   /* the continuous controls: the value to send */
static uint16_t daw_bdown;                        /* the 13 buttons held (bit per button) */
static uint32_t daw_brel[DAW_NBTN];               /* a button's release: its packet (0 none) */
static uint8_t daw_arrows;                        /* < + > pressed together: neither sends (until both are up) */
static uint8_t daw_flast[4] = {0xFF, 0xFF, 0xFF, 0xFF};   /* the faders as last seen */
static uint32_t daw_splash_t;                     /* the mode's screen: since (ms | 1), 0 = none */
static uint8_t daw_splash_on;                     /* (the mode it shows) */
static uint8_t daw_esel;                          /* the editor: the control picked */
static uint32_t daw_frame;                        /* the frame the DAW screen was drawn last */
static uint32_t daw_knob_t[8];                    /* (a knob's last detent: fast turns go further) */
#define DAW_SPLASH_MS 1100u
#ifndef DAW_FADER
#define DAW_FADER(f) fm1_in.fader[f]              /* (tests/daw_ui_test.c: its own faders) */
#define DAW_FRAMES() fm1_in.frames
#endif

/* the 13 buttons' matrix ids: < > fixed, the others as HARDWARE CALIBRATION taught their labels (panel.c) */
static uint32_t daw_btn_id(uint32_t b)
{
    static const uint8_t L[DAW_NBTN] = {NB, NB, B_PLAY, B_REC, B_SEQ, B_FX, B_EDIT, B_ARP, B_LFO, B_SCL, B_ENV, B_GLO, B_SAVE};
    return b == 0u ? SMK_BTN_LEFT : b == 1u ? SMK_BTN_RIGHT : panel.btn[L[b % DAW_NBTN]];
}
/* the encoder of the knob printed K(p + 1) and its direction: K1 the MASTER encoder, K2..K8 the calibration's slots */
static uint32_t daw_knob_enc(uint32_t p, int32_t *dir)
{
    if (!p) {
        *dir = 1;
        return SMK_ENC_MASTER;
    }
    *dir = panel.dir[(p - 1u) % NE];
    return panel.enc[(p - 1u) % NE] % FM1_NENC;
}
static void daw_knobs_drop(void)                  /* turns made elsewhere: not sent */
{
    uint32_t p;
    int32_t d;
    for (p = 0; p < 8u; p++)
        fm1_enc_take(daw_knob_enc(p, &d));
}

/* the UI's sends: the IRQ off (the audio context sends too); MMC also whole to the TRS jack */
static void daw_send(const daw_ent_t *e, uint32_t v)
{
    uint32_t p[2], n = daw_msg(e, v, p), i;
    if (!n)
        return;
    fm1_irq_off();
    for (i = 0; i < n; i++)
        midi_out_event(p[i]);
    if (e->type == DT_MMC) {
        uint8_t b[6] = {0xF0, 0x7F, 0x7F, 0x06, (uint8_t)(e->num & 127u), 0xF7};
        smo_raw(b, 6);
    }
    fm1_irq_on();
}
static void daw_send_pkt(uint32_t pkt)
{
    fm1_irq_off();
    midi_out_event(pkt);
    fm1_irq_on();
}

static void daw_edit_pick(uint32_t i)
{
    daw_esel = (uint8_t)(i % DAW_N);
    daw_edit = 2;
    ui.force = 1;
}

/* SYNTH <-> DAW. splash: the mode's screen (not from the menu, which shows it) */
static void daw_set(uint32_t on, int splash)
{
    uint32_t b;
    on = on != 0u;
    if (splash) {
        daw_splash_t = fm1_ms | 1u;
        daw_splash_on = (uint8_t)on;
    }
    if (on == daw_on)
        return;
    if (on) {
        if (song.playing || transport_req == 1u)
            transport_req = 2;                    /* the sequencer stops (the controls are the DAW's now) */
        song.rec = 0;
        rec_wait = 0;
        layer_unlock();
        pick_close();
        fb_close();
        vis_on = 0;
        ui.hold_kind = 0;
        daw_knobs_drop();
        for (b = 0; b < DAW_N; b++) {
            daw_cv[b].v = daw_cv[b].sent = daw_class(b) == DC_KNOB ? (int32_t)daw_kval[b % 16u] : -1;
            daw_cv[b].acc = 0;
            daw_cv[b].t = 0;
        }
        for (b = 0; b < 4u; b++)
            daw_flast[b] = 0xFF;
        daw_bdown = 0;
        daw_touch_pad = 0;
        daw_last = 0;
    } else {
        for (b = 0; b < DAW_NBTN; b++)            /* buttons held: their CC 0 / note-off */
            if (daw_brel[b])
                daw_send_pkt(daw_brel[b]), daw_brel[b] = 0;
        if (daw_cv[DAW_PITCH].sent >= 0 && daw_cv[DAW_PITCH].sent != 8192) {   /* a bend: back to the centre */
            daw_ent_t e = daw_ent(DAW_PITCH);
            if (e.type == DT_BEND)
                daw_send(&e, 8192u);
        }
        daw_edit = 0;
        perf_bend = perf_modw = 0;
        ui.force = 1;
    }
    daw_on = (uint8_t)on;
    smk_master_hold = (uint8_t)on;
    settings_later = 1;                           /* (kept, once quiet) */
    ui.force = 1;
}

/* < + > together: the mode, both ways (also over the menu). Once a press, 180 ms apart at least */
static void daw_switch(uint32_t *pressed)
{
    static uint32_t t;
    uint32_t a = 1u << SMK_BTN_LEFT | 1u << SMK_BTN_RIGHT;
    if (!(fm1_in.buttons & a))
        daw_arrows = 0;
    if (!(*pressed & a) || (fm1_in.buttons & a) != a)
        return;
    *pressed &= ~a;
    daw_arrows = 1;
    if (t && fm1_ms - (t & ~1u) < 180u)
        return;
    t = fm1_ms | 1u;
    if (ui.menu)
        menu_close();
    daw_set(!daw_on, 1);
}

static void daw_edit_open(void)                   /* the map editor (from the menu, or PATCH + OCT+) */
{
    if (!daw_on)
        daw_set(1, 0);
    if (!daw_user)
        daw_user = 1;                             /* (the USER map, in use from now) */
    ui.menu = 0;
    daw_touch_pad = 0;
    daw_edit = 1;
    ui.force = 1;
}

/* a continuous control moved: its new value (ABSOLUTE knobs, faders, wheels) or detents (RELATIVE knobs) */
static void daw_cv_put(uint32_t i, int32_t v, int rel)
{
    if (rel)
        daw_cv[i].acc += v;
    else
        daw_cv[i].v = v;
}
static void daw_flush(void)                       /* what is due goes out (DAW_RATE_MS a control at most) */
{
    static const uint8_t CV[2] = {DAW_PITCH, DAW_MOD};
    uint32_t i, k, now = fm1_ms;
    for (k = 0; k < DAW_PAD0 + 2u; k++) {
        daw_cv_t *c;
        daw_ent_t e;
        uint32_t v;
        int rel;
        i = k < DAW_PAD0 ? k : CV[k - DAW_PAD0];
        c = &daw_cv[i];
        rel = daw_class(i) == DC_KNOB && daw_rel;
        if (!daw_cv_due(c, now, (uint32_t)rel))
            continue;
        e = daw_ent(i);
        v = rel ? daw_knob_val(0, c->acc, 1) : (uint32_t)c->v;
        if (i == DAW_PITCH && e.type == DT_CC)
            v >>= 7;                              /* (the pitch wheel as a CC: 0..127, 64 at rest) */
        if (!daw_edit)
            daw_send(&e, v);
        if (c->sent >= 0 || daw_class(i) < DC_PITCH)
            daw_shown(i, v);                      /* (the wheels' first message, as DAW mode starts: not a touch) */
        c->t = now | 1u;
        if (rel)
            c->acc = 0;
        else
            c->sent = c->v;
    }
}

static void daw_faders(void)
{
    uint32_t f;
    for (f = 0; f < 4u; f++) {
        uint32_t v = DAW_FADER(f), i = DAW_FADER0 + 4u * (daw_fbank & 1u) + f;
        if (daw_flast[f] == 0xFF || DAW_FRAMES() < 200u) {
            daw_flast[f] = (uint8_t)v;
            continue;
        }
        if (v == daw_flast[f])
            continue;
        daw_flast[f] = (uint8_t)v;
        ui_input_ms = fm1_ms;
        if (daw_edit) {
            if (daw_edit == 1u || daw_esel != i)
                daw_edit_pick(i);
            continue;
        }
        daw_cv_put(i, (int32_t)v, 0);
    }
}

/* the wheels (main.c smk_wheels): b the bend (+-8192), m the mod wheel 0..127. The editor: a wheel moved from rest
 * picks it */
static void daw_wheels(int32_t b, int32_t m)
{
    static uint8_t pw_out, mw_out;
    int32_t v = 8192 + b;
    daw_cv_put(DAW_PITCH, v < 0 ? 0 : v > 16383 ? 16383 : v, 0);
    daw_cv_put(DAW_MOD, m, 0);
    if (daw_edit && !pw_out && (b > 2048 || b < -2048))
        daw_edit_pick(DAW_PITCH);
    if (daw_edit && !mw_out && m > 24)
        daw_edit_pick(DAW_MOD);
    pw_out = (uint8_t)(b > 1024 || b < -1024);
    mw_out = (uint8_t)(m > 8);
}

static void daw_btn_press(uint32_t b)
{
    uint32_t i = DAW_BTN0 + b, v = 127;
    daw_ent_t e = daw_ent(i);
    daw_brel[b] = 0;
    if (e.type == DT_TOG) {
        if (!daw_toggle(&daw_tog[i], &daw_tog_t[i], fm1_ms))
            return;
        v = daw_tog[i] ? 127u : 0u;
    }
    daw_send(&e, v);
    daw_shown(i, e.type == DT_OFF ? 0u : v);
    if (e.type == DT_CC)
        daw_brel[b] = daw_cc_pkt(daw_chan(&e), e.num, 0);
    else if (e.type == DT_NOTE)
        daw_brel[b] = daw_note_pkt(daw_chan(&e), e.num, 0);
}

/* the map editor's knobs: KNOB 1 TYPE, KNOB 2 NUMBER, KNOB 3 CHANNEL (the screen's dials as KNOB LAYOUT has them) */
static void daw_edit_knob(uint32_t r, int32_t s)
{
    daw_ent_t *e = &daw_umap[daw_esel % DAW_N];
    if (r == 0u) {
        daw_set_type(daw_esel, e, daw_type_step(daw_esel, e->type, s));
    } else if (r == 1u) {
        int32_t n = (int32_t)e->num + s, lo = e->type == DT_MMC ? 1 : 0, hi = e->type == DT_MMC ? (int32_t)DAW_MMC_N - 1 : 127;
        e->num = (uint8_t)(n < lo ? lo : n > hi ? hi : n);
    } else if (r == 2u) {
        int32_t c = (int32_t)e->ch + s;
        e->ch = (uint8_t)(c < 0 ? 0 : c > 16 ? 16 : c);
    }
    ui.force = 1;
}

static void daw_input(uint32_t pressed, uint32_t now)
{
    uint32_t home = btn_hold(&ui.home_t0, B_HOME, now, 1), cur = fm1_in.buttons, b, p;
    uint32_t ob = 1u << panel.btn[B_OCTDN], pb = 1u << panel.btn[B_OCTUP], kb = 1u << SMK_BTN_KNOB_BANK,
             fb = 1u << SMK_BTN_FADER_BANK;
    static uint8_t kb0, fb0;
    static uint32_t pbank_t;
    smk_master_hold = 1;                          /* (the volume knob is a K here, also after a power-on in DAW mode) */
    if (pressed)
        ui_input_ms = fm1_ms;
    pc_poll();                                    /* (MIDI IN still plays SLOOP: Program Change, channels, learn, step entry) */
    midi_chan_fix();
    fm6_poll();
    ml_poll();
    midi_entry();
    daw_faders();
    daw_flush();
    if ((pressed & pb) && ui.home_t0 && !(ui.home_t0 & 2u) && !ui.menu) {   /* PATCH held + OCT+: the map editor */
        ui.home_t0 |= 2u;                         /* (PATCH's tap and hold: eaten) */
        pressed &= ~pb;
        if (daw_edit)
            daw_edit = 0, settings_later = 1, ui.force = 1;
        else
            daw_edit_open();
    }
    if (home == BT_HOLD) {                        /* PATCH held: the menu, at MIDI (or leave it) */
        if (ui.menu) {
            menu_close();
        } else {
            daw_edit = 0;
            ui.menu = 1;
            ui.menu_sel = MI_TRS;
            ui.confirm = 0;
            ui.force = 1;
        }
    }
    if (ui.menu) {
        fm1_enc_take(smk_master_enc % FM1_NENC);  /* (the volume knob: no volume here, no stale turns after) */
        if (!ui.home_t0)
            menu_input(pressed);
        return;
    }
    if (home == BT_TAP) {                         /* PATCH: LOCAL ON / OFF (the stock firmware's PATCH: the sound on) */
        daw_local = (uint8_t)!daw_local;
        settings_later = 1;
        ui.force = 1;
    }
    /* the banks: KNOB BANK the knobs', FADER BANK the faders', both the pads' (the first one's change undone) */
    if ((pressed & (kb | fb)) && (cur & (kb | fb)) == (kb | fb)) {
        if (!(pressed & kb))
            daw_kbank = kb0;
        if (!(pressed & fb))
            daw_fbank = fb0;
        if (!pbank_t || fm1_ms - (pbank_t & ~1u) >= 180u) {
            pbank_t = fm1_ms | 1u;
            daw_pbank ^= 1u;
        }
        ui.force = 1;
    } else {
        if (pressed & kb)
            kb0 = daw_kbank, daw_kbank ^= 1u, ui.force = 1;
        if (pressed & fb)
            fb0 = daw_fbank, daw_fbank ^= 1u, ui.force = 1;
    }
    if (daw_edit) {                               /* the map editor */
        uint32_t t = daw_touch_pad;
        if (pressed & ob) {
            daw_edit = 0;
            settings_later = 1;
            ui.force = 1;
            return;
        }
        if ((pressed & pb) && daw_edit == 2u)
            daw_edit = 1, ui.force = 1;
        if (t) {
            daw_touch_pad = 0;
            daw_edit_pick(t - 1u);
        }
        for (b = 0; b < DAW_NBTN; b++)
            if ((pressed >> daw_btn_id(b)) & 1u)
                daw_edit_pick(DAW_BTN0 + b);
        for (p = 0; p < 8u; p++) {
            int32_t d, s = fm1_enc_take(daw_knob_enc(p, &d)) * d;
            uint32_t r;
            if (!s)
                continue;
            ui_input_ms = fm1_ms;
            if (daw_edit == 1u) {
                daw_edit_pick(DAW_KNOB0 + 8u * (daw_kbank & 1u) + p);
                continue;
            }
            for (r = 0; r < 4u && knob_of(EN_K1 + r) != p; r++)
                ;
            daw_edit_knob(r, r == 1u ? accel(EN_K1 + 1u, s, 127) : s);
        }
        return;
    }
    daw_touch_pad = 0;
    if (pressed & (ob | pb)) {                    /* OCT- / OCT+: the keys' octave (both: 0) */
        if ((cur & (ob | pb)) == (ob | pb))
            daw_oct = 0;
        else if (pressed & ob)
            daw_oct = (int8_t)(daw_oct > -3 ? daw_oct - 1 : -3);
        else
            daw_oct = (int8_t)(daw_oct < 3 ? daw_oct + 1 : 3);
        ui.force = 1;
    }
    for (b = 0; b < DAW_NBTN; b++) {              /* the buttons; < > when let go, if not both */
        uint32_t bit = 1u << daw_btn_id(b), arrow = b < 2u;
        if (pressed & bit) {
            daw_bdown |= (uint16_t)(1u << b);
            if (!arrow)
                daw_btn_press(b);
        }
        if (((daw_bdown >> b) & 1u) && !(cur & bit)) {
            daw_bdown &= (uint16_t)~(1u << b);
            if (arrow && !daw_arrows)
                daw_btn_press(b);
            if (daw_brel[b])
                daw_send_pkt(daw_brel[b]), daw_brel[b] = 0;
        }
    }
    for (p = 0; p < 8u; p++) {                    /* the knobs */
        int32_t d, s = fm1_enc_take(daw_knob_enc(p, &d)) * d;
        uint32_t i = DAW_KNOB0 + 8u * (daw_kbank & 1u) + p;
        if (!s)
            continue;
        ui_input_ms = fm1_ms;
        if (fm1_ms - daw_knob_t[p] < 40u)        /* fast: further */
            s *= 4;
        daw_knob_t[p] = fm1_ms;
        if (daw_rel)
            daw_cv_put(i, s, 1);
        else
            daw_cv_put(i, (int32_t)daw_knob_val(&daw_kval[i], s, 0), 0);
    }
    daw_flush();
}

/* ------------------------------------------------------------------ lights --- */
static void daw_leds(void)
{
    static const uint8_t PADC[2][3] = {{40, 124, 255}, {30, 204, 112}};   /* NOTE pads: bank A blue, B green */
    uint32_t c, blink = (fm1_ms / 200u) & 1u;
    for (c = 0; c < SMK_NLED; c++)
        fm1_led[c] = 0;
    smk_pad_daw = 1;
    smk_pad_piano = 0;
    smk_pad_pick = 0;
    fm1_led[SMK_BTN_KNOB_BANK] = daw_kbank & 1u;
    fm1_led[SMK_BTN_FADER_BANK] = daw_fbank & 1u;
    fm1_led[panel.btn[B_OCTDN]] = daw_edit ? 0u : daw_oct < 0;
    fm1_led[panel.btn[B_OCTUP]] = daw_edit ? 0u : daw_oct > 0;
    fm1_led[panel.btn[B_HOME]] = daw_edit ? blink : daw_local;   /* PATCH: LOCAL ON (the editor: blinks) */
    for (c = 0; c < DAW_NBTN; c++) {
        uint32_t i = DAW_BTN0 + c, id = daw_btn_id(c);
        daw_ent_t e = daw_ent(i);
        if (daw_edit)
            fm1_led[id] = daw_edit == 2u && daw_esel == i;
        else
            fm1_led[id] = e.type == DT_TOG ? daw_tog[i] : (daw_bdown >> c) & 1u;
    }
    if ((int32_t)(fm1_ms - bled_hold_until) < 0)  /* (console bled: only that LED) */
        for (c = 0; c < SMK_NLED; c++)
            fm1_led[c] = c == bled_id;
    if ((int32_t)(fm1_ms - padled_hold_until) < 0)
        return;
    for (c = 0; c < 16u; c++) {
        uint32_t i = DAW_PAD0 + 16u * (daw_pbank & 1u) + c, held = (daw_pdown >> c) & 1u;
        daw_ent_t e = daw_ent(i);
        const uint8_t *k = PADC[daw_pbank & 1u];
        if (daw_edit == 2u && daw_esel == i)
            padled_set(c, blink ? 200 : 40, blink ? 200 : 40, blink ? 200 : 40);
        else if (e.type == DT_NOTE)
            padled_set(c, held ? k[0] : (uint8_t)(k[0] * 3u >> 3), held ? k[1] : (uint8_t)(k[1] * 3u >> 3),
                       held ? k[2] : (uint8_t)(k[2] * 3u >> 3));
        else if (e.type == DT_CC || e.type == DT_TOG)
            padled_set(c, (e.type == DT_TOG ? daw_tog[i] : held) ? 255 : 100, (e.type == DT_TOG ? daw_tog[i] : held) ? 198 : 80,
                       (e.type == DT_TOG ? daw_tog[i] : held) ? 24 : 10);
        else
            padled_set(c, 16, 16, 16);
    }
}

/* ------------------------------------------------------------------ screen --- */
static void daw_val_text(uint32_t i, uint32_t v, const daw_ent_t *e, char *msg, char *val, const char **vl)
{
    char n[8];
    *vl = "VALUE";
    msg[0] = val[0] = 0;
    if (i == DAW_KEYS) {                          /* a key: NOTE C4, its velocity */
        note_name(n, v >> 8);
        str_cpy(msg, "NOTE ", 16);
        str_cpy(msg + str_len(msg), n, 8);
        fmt_int(val, (int32_t)(v & 127u));
        *vl = "VELOCITY";
        return;
    }
    switch (e->type) {
    case DT_CC:
    case DT_TOG:
        str_cpy(msg, "CC ", 16);
        fmt_int(msg + 3, e->num);
        if (daw_class(i) == DC_KNOB && daw_rel) {
            int32_t d = (int32_t)v - 64;
            if (d > 0)
                val[0] = '+', fmt_int(val + 1, d);
            else
                fmt_int(val, d);
            *vl = "STEPS";
        } else {
            fmt_int(val, (int32_t)v);
        }
        break;
    case DT_NOTE:
        note_name(n, e->num);
        str_cpy(msg, "NOTE ", 16);
        str_cpy(msg + 5, n, 8);
        fmt_int(val, (int32_t)v);
        *vl = v ? "VELOCITY" : "OFF";
        break;
    case DT_BEND:
        str_cpy(msg, "BEND", 16);
        fmt_int(val, (int32_t)v - 8192);
        break;
    case DT_MMC:
        str_cpy(msg, DAW_MMC_NAME[e->num % DAW_MMC_N], 16);
        *vl = "MMC";
        break;
    default:
        str_cpy(msg, "OFF", 16);
        *vl = "";
        break;
    }
}
static void daw_chan_text(char *b, uint32_t ch)    /* an entry's channel: "CH 10", or "CH 1 (MAIN)" */
{
    str_cpy(b, "CH ", 4);
    fmt_int(b + 3, ch ? (int32_t)ch : daw_ch);
    if (!ch)
        str_cpy(b + str_len(b), " (MAIN)", 8);
}

static void daw_head(const char *tag, uint16_t tc)
{
    char b[16];
    int32_t x;
    cv_rect(0, 0, 240, 22, C_BLACK);
    x = text_w(&FONT_S, tag) + 8;
    cv_rect(2, 2, x, 18, tc);
    cv_text(6, 3, &FONT_S, tag, C_BLACK);
    str_cpy(b, "CH ", 4);
    fmt_int(b + 3, daw_ch);
    cv_text(x + 10, 3, &FONT_S, b, C_WHITE);
    cv_text(236 - text_w(&FONT_S, daw_local ? "LOCAL ON" : "LOCAL OFF"), 3, &FONT_S, daw_local ? "LOCAL ON" : "LOCAL OFF",
            daw_local ? TE_COL[2] : TE_G3);
    cv_rect(0, 22, 240, 1, TE_G2);
}

static void daw_main_draw(void)
{
    static uint32_t shown;
    uint32_t half, i = daw_last ? daw_last - 1u : DAW_N + 1u, v = daw_last_v, p, sig;
    sig = daw_last_n * 7u + daw_kbank * 131u + daw_fbank * 1031u + daw_pbank * 7919u + (uint32_t)(daw_oct + 3) * 104729u +
          daw_ch * 1299709u + daw_local * 15485863u + daw_user * 32452843u + daw_rel * 49979687u;
    for (p = 0; p < 8u; p++)
        sig = sig * 31u + daw_kval[8u * (daw_kbank & 1u) + p] + (uint32_t)daw_ent(8u * (daw_kbank & 1u) + p).num * 977u +
              daw_ent(8u * (daw_kbank & 1u) + p).type;
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    for (half = 0; half < 2u; half++) {
        cv_begin(240, 120, C_BLACK);
        if (!half) {
            char nm[20], msg[20], val[12], b[16];
            const char *vl = "";
            daw_head("DAW", C_WHITE);
            if (i <= DAW_N) {                     /* the last control: its name (and bank), message, value */
                daw_ent_t e = daw_ent(i);
                if (i == DAW_KEYS)
                    str_cpy(nm, "KEYS", sizeof nm);
                else
                    daw_name(i, nm);
                if (i < DAW_BTN0) {
                    str_cpy(nm + str_len(nm), daw_bank_of(i) ? "  BANK B" : "  BANK A", 9);
                }
                daw_val_text(i, v, &e, msg, val, &vl);
                cv_text(4, 28, &FONT_S, nm, TE_G4);
                cv_text(236 - text_w(&FONT_S, vl), 28, &FONT_S, vl, TE_G3);
                cv_text(4, 46, &FONT_L, msg, C_WHITE);
                if (i < DAW_N && e.type != DT_OFF && e.type != DT_MMC) {
                    daw_chan_text(b, e.ch);
                    cv_text(4, 80, &FONT_S, i == DAW_KEYS ? "" : b, TE_G3);
                }
                if (i == DAW_KEYS) {
                    daw_chan_text(b, 0);
                    cv_text(4, 80, &FONT_S, b, TE_G3);
                }
                cv_text(236 - text_w(&FONT_L, val), 46, &FONT_L, val, TE_COL[2]);
                {                                 /* the value as a bar (a bend: from the middle) */
                    int32_t w = 0, x0 = 4;
                    if (i == DAW_KEYS) {
                        w = (int32_t)((v & 127u) * 232u / 127u);
                    } else if (e.type == DT_BEND) {
                        w = ((int32_t)v - 8192) * 116 / 8192;
                        x0 = w < 0 ? 120 + w : 120;
                        w = w < 0 ? -w : w;
                    } else if (e.type != DT_MMC && e.type != DT_OFF && !(daw_class(i) == DC_KNOB && daw_rel)) {
                        w = (int32_t)((v > 127u ? 127u : v) * 232u / 127u);
                    }
                    cv_rect(4, 98, 232, 4, TE_G1);
                    if (w)
                        cv_rect(x0, 98, w, 4, TE_COL[2]);
                }
            } else {
                cv_text(4, 34, &FONT_L, "TOUCH A", C_WHITE);
                cv_text(4, 66, &FONT_L, "CONTROL", C_WHITE);
                cv_text(4, 100, &FONT_S, "< + > SYNTH  PATCH+OCT+ MAP", TE_G3);
            }
        } else {
            char b[24];
            str_cpy(b, daw_fbank ? "FADERS B" : "FADERS A", sizeof b);
            cv_text(4, 0, &FONT_S, b, TE_G4);
            cv_text(84, 0, &FONT_S, daw_pbank ? "PADS B" : "PADS A", TE_G4);
            str_cpy(b, "OCT ", sizeof b);
            if (daw_oct > 0)
                b[4] = '+', fmt_int(b + 5, daw_oct);
            else
                fmt_int(b + 4, daw_oct);
            cv_text(236 - text_w(&FONT_S, b), 0, &FONT_S, b, daw_oct ? C_WHITE : TE_G4);
            cv_rect(0, 18, 240, 1, TE_G2);
            cv_text(4, 22, &FONT_S, daw_kbank ? "KNOBS B" : "KNOBS A", TE_G4);
            cv_text(236 - text_w(&FONT_S, daw_rel ? "RELATIVE" : "ABSOLUTE"), 22, &FONT_S, daw_rel ? "RELATIVE" : "ABSOLUTE",
                    TE_G3);
            for (p = 0; p < 8u; p++) {            /* the eight knobs: K1..K8, a bar, the CC */
                uint32_t k = 8u * (daw_kbank & 1u) + p, h;
                daw_ent_t e = daw_ent(DAW_KNOB0 + k);
                int32_t x = (int32_t)p * 30;
                char l[4] = {'K', (char)('1' + p), 0, 0};
                cv_text(x + 15 - text_w(&FONT_S, l) / 2, 40, &FONT_S, l, TE_COL[p & 3u]);
                cv_rect(x + 9, 58, 12, 42, TE_G1);
                if (e.type == DT_CC && !daw_rel) {
                    h = daw_kval[k] * 42u / 127u;
                    if (h)
                        cv_rect(x + 9, 100 - (int32_t)h, 12, (int32_t)h, TE_COL[p & 3u]);
                }
                if (e.type == DT_CC)
                    fmt_int(b, e.num);
                else
                    str_cpy(b, "--", sizeof b);
                cv_text(x + 15 - text_w(&FONT_S, b) / 2, 102, &FONT_S, b, TE_G3);
            }
        }
        cv_blit(0, half * 120u);
    }
}

static void daw_edit_draw(void)
{
    static uint32_t shown;
    uint32_t half, i = daw_esel % DAW_N, r, sig;
    const daw_ent_t *e = &daw_umap[i];
    sig = daw_edit * 7u + i * 131u + e->type * 1031u + e->num * 7919u + e->ch * 104729u + daw_ch * 1299709u +
          daw_kbank * 15485863u + daw_fbank * 32452843u + daw_pbank * 49979687u + knob_layout * 86028121u;
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    for (half = 0; half < 2u; half++) {
        cv_begin(240, 120, C_BLACK);
        if (!half) {
            char b[24];
            daw_head("EDIT MAP", TE_COL[2]);
            if (daw_edit == 1u) {
                cv_text(4, 34, &FONT_L, "TOUCH A", C_WHITE);
                cv_text(4, 66, &FONT_L, "CONTROL", C_WHITE);
                cv_text(4, 100, &FONT_S, "KNOB FADER PAD BUTTON WHEEL", TE_G3);
            } else {
                daw_name(i, b);
                cv_text(4, 28, &FONT_S, i < DAW_BTN0 ? (daw_bank_of(i) ? "BANK B" : "BANK A") : "", TE_G3);
                cv_text(4, 46, &FONT_L, b, C_WHITE);
                {
                    char msg[20], val[12];
                    const char *vl;
                    daw_val_text(i, 0, e, msg, val, &vl);
                    if (e->type != DT_OFF && e->type != DT_MMC) {
                        daw_chan_text(b, e->ch);
                        str_cpy(msg + str_len(msg), "  ", 3);
                        str_cpy(msg + str_len(msg), b, sizeof msg - str_len(msg));
                    }
                    cv_text(4, 90, &FONT_S, msg, TE_COL[2]);
                }
            }
        } else {
            static const char *const RN[3] = {"TYPE", "NUMBER", "CHANNEL"};
            if (daw_edit == 2u) {
                for (r = 0; r < 3u; r++) {        /* KNOB 1..3: TYPE, NUMBER, CHANNEL */
                    char k[4] = {'K', knob_digit(EN_K1 + r), 0, 0}, v[20];
                    int32_t y = (int32_t)r * 30, xv;
                    xv = cv_text(4, y + 8, &FONT_S, k, TE_COL[r & 3u]);
                    cv_text(xv + 6, y + 8, &FONT_S, RN[r], TE_G4);
                    if (r == 0u)
                        str_cpy(v, DT_NAME[e->type % DT_N], sizeof v);
                    else if (r == 1u && e->type == DT_MMC)
                        str_cpy(v, DAW_MMC_NAME[e->num % DAW_MMC_N], sizeof v);
                    else if (r == 1u && e->type == DT_NOTE) {
                        fmt_int(v, e->num);
                        str_cpy(v + str_len(v), " ", 2);
                        note_name(v + str_len(v), e->num);
                    } else if (r == 1u)
                        fmt_int(v, e->num);
                    else if (e->ch)
                        fmt_int(v, e->ch);
                    else
                        str_cpy(v, "MAIN", sizeof v);
                    if ((r == 1u && (e->type == DT_OFF || e->type == DT_BEND)) || (r == 2u && e->type == DT_OFF))
                        str_cpy(v, "-", sizeof v);
                    if (text_w(&FONT_L, v) <= 150)
                        cv_text(236 - text_w(&FONT_L, v), y, &FONT_L, v, C_WHITE);
                    else
                        cv_text(236 - text_w(&FONT_S, v), y + 8, &FONT_S, v, C_WHITE);
                }
                cv_text(4, 100, &FONT_S, "OCT+ NEXT  OCT- DONE", TE_G3);
            } else {
                cv_text(4, 20, &FONT_S, "KNOB BANK, FADER BANK:", TE_G3);
                cv_text(4, 38, &FONT_S, "THE OTHER BANK (BOTH: PADS)", TE_G3);
                cv_text(4, 100, &FONT_S, "OCT- DONE", TE_G3);
            }
        }
        cv_blit(0, half * 120u);
    }
}

static void daw_splash_draw(void)
{
    static uint32_t shown;
    uint32_t half, sig = daw_splash_on * 7u + daw_ch * 131u + daw_local * 1031u + 1u;
    if (!ui.force && sig == shown)
        return;
    shown = sig;
    for (half = 0; half < 2u; half++) {
        const char *t = daw_splash_on ? "DAW MODE" : "SYNTH MODE";
        cv_begin(240, 120, daw_splash_on ? C_WHITE : C_BLACK);
        if (!half) {
            cv_text(120 - text_w(&FONT_L, t) / 2, 84, &FONT_L, t, daw_splash_on ? C_BLACK : C_WHITE);
        } else {
            char b[24];
            if (daw_splash_on) {
                str_cpy(b, "CHANNEL ", sizeof b);
                fmt_int(b + 8, daw_ch);
                str_cpy(b + str_len(b), daw_local ? "  LOCAL ON" : "  LOCAL OFF", sizeof b - str_len(b));
            } else {
                str_cpy(b, "THE CONTROLS PLAY SLOOP", sizeof b);
            }
            cv_text(120 - text_w(&FONT_S, b) / 2, 4, &FONT_S, b, daw_splash_on ? C_BLACK : TE_G4);
            cv_text(120 - text_w(&FONT_S, daw_splash_on ? "< + > SYNTH MODE" : "< + > DAW MODE") / 2, 26, &FONT_S,
                    daw_splash_on ? "< + > SYNTH MODE" : "< + > DAW MODE", daw_splash_on ? TE_G2 : TE_G3);
        }
        cv_blit(0, half * 120u);
    }
}

/* ui_draw: 1 when the DAW screens took the frame (the mode's screen, DAW mode without the menu) */
static int daw_screen(void)
{
    int splash = daw_splash_t && fm1_ms - (daw_splash_t & ~1u) < DAW_SPLASH_MS;
    if (!splash)
        daw_splash_t = 0;
    if (splash || (daw_on && !ui.menu)) {
        static uint8_t was_splash, was_edit;
        if (daw_frame + 1u != ui.frame || was_splash != (uint8_t)splash || was_edit != daw_edit)
            ui.force = 1;                         /* another screen was up (the canvases cover it all) */
        was_splash = (uint8_t)splash;
        was_edit = daw_edit;
        daw_frame = ui.frame;
        if (splash)
            daw_splash_draw();
        else if (daw_edit)
            daw_edit_draw();
        else
            daw_main_draw();
        return 1;
    }
    if (daw_frame && daw_frame + 1u == ui.frame) {   /* back from them: the page, whole */
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    return 0;
}
