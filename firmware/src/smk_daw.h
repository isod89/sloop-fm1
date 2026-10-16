/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37: DAW mode (HOME menu > MIDI > MODE, or < + > together). Every control sends MIDI (USB, and the TRS jack with
 * TRS MIDI OUT on) instead of playing SLOOP: the keys notes on CHANNEL, the knobs, faders, pads, buttons and wheels as
 * the map says. This header is the map and its messages, no hardware: tests/smk_daw_test.c runs it. The keys and pads
 * are read in the audio context (seq.c daw_*), the rest in the UI (ui_daw.c); both send through DAW_TX.
 *
 * The map: 71 entries (type, number, channel), DAW_KNOB0.. two banks of the 8 knobs (KNOB BANK), DAW_FADER0.. two banks
 * of the 4 faders (FADER BANK), DAW_PAD0.. two banks of the 16 pads (KNOB BANK + FADER BANK), DAW_BTN0.. 13 buttons, the
 * pitch and the mod wheel. A pad's number is its place: PAD 1..8 the top row, PAD 9..16 the bottom row (as printed on
 * the stock firmware's charts: the hardware pad index + 1). Channel 0: CHANNEL (the menu's), 1..16 that one.
 * CC MAP DEFAULT: daw_default (SMK37.md has the table); USER: the map edited on the device (ui_daw.c) or in the web
 * editor (editor_daw.c), kept in flash (storage.c OBJ_DAW, daw_store_t; none or older data: the defaults). */
#pragma once
#include <stdint.h>

enum { DAW_KNOB0 = 0, DAW_FADER0 = 16, DAW_PAD0 = 24, DAW_BTN0 = 56, DAW_PITCH = 69, DAW_MOD = 70, DAW_N = 71 };
#define DAW_NBTN 13u
#define DAW_KEYS DAW_N                    /* (the last control touched: the keys, not in the map) */
enum { DC_KNOB, DC_FADER, DC_PAD, DC_BTN, DC_PITCH, DC_MOD };
/* types: OFF; CC (a knob, fader or wheel: its value; a pad or button: 127 while held, 0 let go); CC TOGGLE (127 / 0,
 * each press); NOTE (a pad: its velocity; a button: 127); MMC (a button: MIDI Machine Control, number = the command);
 * PITCH BEND (the pitch wheel) */
enum { DT_OFF, DT_CC, DT_TOG, DT_NOTE, DT_MMC, DT_BEND, DT_N };
static const char *const DT_NAME[DT_N] = {"OFF", "CC", "CC TOGGLE", "NOTE", "MMC", "PITCH BEND"};
static const uint8_t DC_TYPES[6] = {1u << DT_OFF | 1u << DT_CC, 1u << DT_OFF | 1u << DT_CC,
                                    1u << DT_OFF | 1u << DT_NOTE | 1u << DT_CC | 1u << DT_TOG,
                                    1u << DT_OFF | 1u << DT_CC | 1u << DT_TOG | 1u << DT_NOTE | 1u << DT_MMC,
                                    1u << DT_OFF | 1u << DT_BEND | 1u << DT_CC, 1u << DT_OFF | 1u << DT_CC};
#define DAW_MMC_N 14u                     /* MMC commands 1..13 */
static const char *const DAW_MMC_NAME[DAW_MMC_N] = {"", "STOP", "PLAY", "DEFERRED PLAY", "FAST FORWARD", "REWIND",
                                                    "RECORD", "RECORD EXIT", "RECORD PAUSE", "PAUSE", "EJECT", "CHASE",
                                                    "ERROR RESET", "MMC RESET"};
/* the 13 buttons, in the map's order (ui_daw.c: their matrix ids) */
static const char *const DAW_BTN_NAME[DAW_NBTN] = {"<", ">", "SEQ PLAY", "SEQ REC", "SEQ", "FX", "PARA", "ARP",
                                                   "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT"};
static const uint8_t DAW_BTN_CC[DAW_NBTN] = {110, 111, 115, 117, 112, 113, 114, 118, 119, 85, 86, 87, 88};
#define DAW_RATE_MS 8u                    /* a knob, fader or wheel: one message per this at most (the last value always) */
#define DAW_TOG_MS 180u                   /* a toggle: once per press, presses this far apart at least */
#define DAW_NOTE0 48u                     /* the lowest key (C3) at octave 0 */

typedef struct {
    uint8_t type, num, ch;                /* DT_*, CC / note / MMC command, 0 = CHANNEL, 1..16 */
} daw_ent_t;

/* the settings (HOME menu > MIDI, > DAW; kept with the map in OBJ_DAW) */
static uint8_t daw_on;                    /* MODE: 0 SYNTH, 1 DAW */
static uint8_t daw_ch = 1;                /* CHANNEL 1..16: the keys, and every entry with channel 0 */
static uint8_t daw_local;                 /* LOCAL: 1 ON (the keys and NOTE pads also play SLOOP), 0 OFF (the default) */
static uint8_t daw_user;                  /* CC MAP: 0 DEFAULT, 1 USER */
static uint8_t daw_rel;                   /* KNOBS: 0 ABSOLUTE (0..127), 1 RELATIVE (64 +- the steps) */
static daw_ent_t daw_umap[DAW_N] __attribute__((section(".pool")));   /* the USER map (.pool: zeroed at boot) */

static uint32_t daw_class(uint32_t i)
{
    return i < DAW_FADER0 ? DC_KNOB : i < DAW_PAD0 ? DC_FADER : i < DAW_BTN0 ? DC_PAD : i < DAW_PITCH ? DC_BTN
         : i == DAW_PITCH ? DC_PITCH : DC_MOD;
}
/* the DEFAULT map: knobs K1..K8 CC 70..77 (bank B 102..109), faders CC 20..23 (B 24..27), pads notes on channel 10
 * (bank A 36..51, B 52..67; 36 bottom left, the bottom row first), buttons CC (DAW_BTN_CC), pitch bend, mod CC 1 */
static daw_ent_t daw_default(uint32_t i)
{
    daw_ent_t e = {DT_CC, 0, 0};
    if (i < DAW_FADER0) {
        e.num = (uint8_t)(i < 8u ? 70u + i : 102u + i - 8u);
    } else if (i < DAW_PAD0) {
        e.num = (uint8_t)(20u + i - DAW_FADER0);
    } else if (i < DAW_BTN0) {
        uint32_t p = (i - DAW_PAD0) % 16u, b = (i - DAW_PAD0) / 16u;
        e.type = DT_NOTE;
        e.num = (uint8_t)(36u + 16u * b + (p >= 8u ? p - 8u : p + 8u));
        e.ch = 10;
    } else if (i < DAW_PITCH) {
        e.num = DAW_BTN_CC[i - DAW_BTN0];
    } else if (i == DAW_PITCH) {
        e.type = DT_BEND;
    } else {
        e.num = 1;
    }
    return e;
}
static int daw_ent_ok(uint32_t i, const daw_ent_t *e)   /* a type the control has, a number and channel in range */
{
    if (i >= DAW_N || e->type >= DT_N || !((DC_TYPES[daw_class(i)] >> e->type) & 1u) || e->ch > 16u)
        return 0;
    return e->type == DT_MMC ? e->num >= 1u && e->num < DAW_MMC_N : e->num <= 127u;
}
static void daw_reset_user(void)
{
    uint32_t i;
    for (i = 0; i < DAW_N; i++)
        daw_umap[i] = daw_default(i);
}
static uint32_t daw_user_changed(void)    /* the USER map's entries that differ from the DEFAULT */
{
    uint32_t i, n = 0;
    for (i = 0; i < DAW_N; i++) {
        daw_ent_t d = daw_default(i);
        n += daw_umap[i].type != d.type || daw_umap[i].num != d.num || daw_umap[i].ch != d.ch;
    }
    return n;
}
static daw_ent_t daw_ent(uint32_t i)      /* entry i of the map in use */
{
    daw_ent_t z = {DT_OFF, 0, 0};
    if (i >= DAW_N)
        return z;
    return daw_user ? daw_umap[i] : daw_default(i);
}
static uint32_t daw_chan(const daw_ent_t *e) { return (e->ch ? e->ch - 1u : (daw_ch - 1u)) & 15u; }   /* 0..15 */
/* the next / previous type control i has (a knob turn), round */
static uint32_t daw_type_step(uint32_t i, uint32_t t, int32_t s)
{
    uint32_t k, m = DC_TYPES[daw_class(i)];
    for (k = 0; k < DT_N; k++) {
        t = (t + (s < 0 ? DT_N - 1u : 1u)) % DT_N;
        if ((m >> t) & 1u)
            break;
    }
    return t;
}
/* a new type for entry e of control i: the number stays if it fits, else a sensible one (MMC: PLAY; from MMC: the
 * default's number) */
static void daw_set_type(uint32_t i, daw_ent_t *e, uint32_t t)
{
    daw_ent_t d = daw_default(i);
    if (t == DT_MMC && (e->num < 1u || e->num >= DAW_MMC_N))
        e->num = 2;
    else if (e->type == DT_MMC && t != DT_MMC)
        e->num = d.num ? d.num : 64u;
    e->type = (uint8_t)t;
}

/* ---- messages: USB-MIDI event packets (cable 0) through DAW_TX (felucca: usb.c midi_out_event, IRQ-safe) ---- */
#define DAW_PKT(cin, a, b, c) ((uint32_t)(cin) | (uint32_t)(a) << 8 | (uint32_t)(b) << 16 | (uint32_t)(c) << 24)
static uint32_t daw_cc_pkt(uint32_t ch, uint32_t cc, uint32_t v) { return DAW_PKT(0x0B, 0xB0u | (ch & 15u), cc & 127u, v & 127u); }
static uint32_t daw_note_pkt(uint32_t ch, uint32_t n, uint32_t v)   /* v 0: a note-off */
{
    return v ? DAW_PKT(0x09, 0x90u | (ch & 15u), n & 127u, v & 127u) : DAW_PKT(0x08, 0x80u | (ch & 15u), n & 127u, 0);
}
static uint32_t daw_bend_pkt(uint32_t ch, uint32_t v14) { return DAW_PKT(0x0E, 0xE0u | (ch & 15u), v14 & 127u, (v14 >> 7) & 127u); }
/* MMC: F0 7F 7F 06 cmd F7 (every device), two packets; the TRS jack takes it whole (smk_midi_out.h smo_raw) */
static void daw_mmc_pkts(uint32_t cmd, uint32_t *p)
{
    p[0] = DAW_PKT(0x04, 0xF0, 0x7F, 0x7F);
    p[1] = DAW_PKT(0x07, 0x06, cmd & 127u, 0xF7);
}

/* the message of entry e at value v (a knob / fader / wheel: 0..127, the pitch wheel: 0..16383, centre 8192; a pad /
 * button: its velocity, 0 = let go). Returns the packets (0..2) in p */
static uint32_t daw_msg(const daw_ent_t *e, uint32_t v, uint32_t *p)
{
    uint32_t ch = daw_chan(e);
    switch (e->type) {
    case DT_CC:
    case DT_TOG:
        p[0] = daw_cc_pkt(ch, e->num, v > 127u ? 127u : v);
        return 1;
    case DT_NOTE:
        p[0] = daw_note_pkt(ch, e->num, v > 127u ? 127u : v);
        return 1;
    case DT_BEND:
        p[0] = daw_bend_pkt(ch, v > 16383u ? 16383u : v);
        return 1;
    case DT_MMC:
        daw_mmc_pkts(e->num, p);
        return 2;
    default:
        return 0;
    }
}

/* a knob turned s detents (+ clockwise): ABSOLUTE its value (0..127, kept per knob), RELATIVE 64 + s (binary offset,
 * 1..127). Returns the value to send */
static uint32_t daw_knob_val(uint8_t *kv, int32_t s, uint32_t rel)
{
    int32_t v;
    if (rel)
        return (uint32_t)(64 + (s < -63 ? -63 : s > 63 ? 63 : s));
    v = (int32_t)*kv + s;
    *kv = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    return *kv;
}

/* a toggle pressed at now (ms): flips once per press, presses DAW_TOG_MS apart at least (a pad's pressure wobbling,
 * a button bouncing). Returns 1 when it flipped; *st the state (1 on) */
static int daw_toggle(uint8_t *st, uint32_t *t, uint32_t now)
{
    if (*t && now - (*t & ~1u) < DAW_TOG_MS)
        return 0;
    *t = now | 1u;
    *st = (uint8_t)!*st;
    return 1;
}

/* the continuous controls (knobs, faders, wheels): the value to send, sent at most every DAW_RATE_MS (the last one
 * always). RELATIVE knobs add up their detents (acc) until sent */
typedef struct {
    int32_t v, sent;                      /* sent: -1 nothing yet */
    int32_t acc;                          /* RELATIVE: detents not sent yet */
    uint32_t t;                           /* when the last went out (ms | 1, 0 = never) */
} daw_cv_t;
static int daw_cv_due(const daw_cv_t *c, uint32_t now, uint32_t rel)
{
    if (rel ? c->acc == 0 : c->v == c->sent)
        return 0;
    return !c->t || now - (c->t & ~1u) >= DAW_RATE_MS;
}

/* ---- kept in flash: storage.c OBJ_DAW (project.c daw_boot / daw_save) ---- */
#define DAW_MAGIC 0x31574144u             /* "DAW1" */
typedef struct {
    uint32_t magic;
    uint8_t flags;                        /* bit 0 DAW mode, 1 LOCAL ON, 2 CC MAP USER, 3 KNOBS RELATIVE */
    uint8_t ch, n, rsv;                   /* CHANNEL; the entries that follow (DAW_N) */
    daw_ent_t map[DAW_N];                 /* the USER map */
} daw_store_t;
static void daw_store_fill(daw_store_t *s)
{
    uint32_t i;
    for (i = 0; i < sizeof *s; i++)
        ((uint8_t *)s)[i] = 0;                /* (the padding too: saves compare the bytes) */
    s->magic = DAW_MAGIC;
    s->flags = (uint8_t)((daw_on & 1u) | (daw_local & 1u) << 1 | (daw_user & 1u) << 2 | (daw_rel & 1u) << 3);
    s->ch = daw_ch;
    s->n = DAW_N;
    s->rsv = 0;
    for (i = 0; i < DAW_N; i++)
        s->map[i] = daw_umap[i];
}
/* the object as read (len bytes): 1 taken. A bad magic or a short one: 0, nothing changes (the defaults stay). Fewer
 * entries (an older map): the rest the defaults; an entry out of range: its default */
static int daw_store_take(const uint8_t *b, uint32_t len)
{
    daw_store_t s;
    uint32_t i, n;
    if (len < 8u)
        return 0;
    for (i = 0; i < sizeof s; i++)
        ((uint8_t *)&s)[i] = i < len ? b[i] : 0u;
    if (s.magic != DAW_MAGIC)
        return 0;
    n = s.n < DAW_N ? s.n : DAW_N;
    if (8u + 3u * n > len)
        n = (len - 8u) / 3u;
    daw_on = (uint8_t)(s.flags & 1u);
    daw_local = (uint8_t)((s.flags >> 1) & 1u);
    daw_user = (uint8_t)((s.flags >> 2) & 1u);
    daw_rel = (uint8_t)((s.flags >> 3) & 1u);
    daw_ch = (uint8_t)(s.ch >= 1u && s.ch <= 16u ? s.ch : 1u);
    for (i = 0; i < DAW_N; i++)
        daw_umap[i] = i < n && daw_ent_ok(i, &s.map[i]) ? s.map[i] : daw_default(i);
    return 1;
}

/* a control's name for the screen: "K3", "FADER 2", "PAD 12", "SEQ PLAY", "PITCH WHEEL" (bank: in the caller) */
static void daw_name(uint32_t i, char *b)
{
    const char *s = "";
    uint32_t n = 0, k = 0;
    switch (daw_class(i)) {
    case DC_KNOB: s = "K", n = (i - DAW_KNOB0) % 8u + 1u; break;
    case DC_FADER: s = "FADER ", n = (i - DAW_FADER0) % 4u + 1u; break;
    case DC_PAD: s = "PAD ", n = (i - DAW_PAD0) % 16u + 1u; break;
    case DC_BTN: s = DAW_BTN_NAME[(i - DAW_BTN0) % DAW_NBTN]; break;
    case DC_PITCH: s = "PITCH WHEEL"; break;
    default: s = "MOD WHEEL"; break;
    }
    while (*s && k < 14u)
        b[k++] = *s++;
    if (n >= 10u)
        b[k++] = (char)('0' + n / 10u);
    if (n)
        b[k++] = (char)('0' + n % 10u);
    b[k] = 0;
}
static uint32_t daw_bank_of(uint32_t i)   /* 0 A, 1 B; the buttons and wheels: 0 */
{
    switch (daw_class(i)) {
    case DC_KNOB: return (i - DAW_KNOB0) / 8u;
    case DC_FADER: return (i - DAW_FADER0) / 4u;
    case DC_PAD: return (i - DAW_PAD0) / 16u;
    default: return 0;
    }
}
