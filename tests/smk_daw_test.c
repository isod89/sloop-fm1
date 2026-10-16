/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 DAW mode's map and messages (firmware/src/smk_daw.h), no hardware: the DEFAULT map (no CC with a fixed meaning),
 * each control's types, the USB-MIDI packets (CC, notes, pitch bend, MMC), ABSOLUTE / RELATIVE knobs, a toggle once per
 * press, the rate of the continuous controls, and the flash object (round trip, older data, a bad entry, a bad magic). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../firmware/src/smk_daw.h"

static int fails;
static void check(int ok, const char *what) { printf("smk_daw: %-80s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
#define PKT(cin, a, b, c) ((uint32_t)(cin) | (uint32_t)(a) << 8 | (uint32_t)(b) << 16 | (uint32_t)(c) << 24)

int main(void)
{
    uint32_t i, ok, p[2];
    daw_ent_t e;
    uint8_t seen[128] = {0};

    /* ---- the DEFAULT map ---- */
    for (ok = 1, i = 0; i < DAW_N; i++) {
        e = daw_default(i);
        ok &= daw_ent_ok(i, &e);
    }
    check(ok, "the DEFAULT map: every entry valid for its control");
    for (ok = 1, i = 0; i < DAW_N; i++) {
        e = daw_default(i);
        if (e.type == DT_CC || e.type == DT_TOG) {
            uint32_t c = e.num;
            ok &= !(c == 0 || c == 32 || c == 6 || c == 38 || (c >= 96 && c <= 101) || c >= 120 || c == 7 || c == 10 ||
                    c == 64 || (c >= 65 && c <= 69)) || (i == DAW_MOD && c == 1);
            ok &= !seen[c];
            seen[c] = 1;
        }
    }
    check(ok, "DEFAULT CCs: none with a fixed meaning (bank, data entry, RPN, volume, pan, pedals, mode), each once");
    check(daw_default(DAW_KNOB0).num == 70 && daw_default(DAW_KNOB0 + 7).num == 77 && daw_default(DAW_KNOB0 + 8).num == 102 &&
          daw_default(DAW_KNOB0 + 15).num == 109, "knobs K1..K8 CC 70..77, bank B CC 102..109");
    check(daw_default(DAW_FADER0).num == 20 && daw_default(DAW_FADER0 + 3).num == 23 && daw_default(DAW_FADER0 + 4).num == 24 &&
          daw_default(DAW_FADER0 + 7).num == 27, "faders CC 20..23, bank B CC 24..27");
    e = daw_default(DAW_PAD0 + 8);
    check(e.type == DT_NOTE && e.num == 36 && e.ch == 10, "PAD 9 (bottom left): note 36 (C2) on channel 10");
    check(daw_default(DAW_PAD0 + 15).num == 43 && daw_default(DAW_PAD0).num == 44 && daw_default(DAW_PAD0 + 7).num == 51,
          "the bottom row 36..43, the top row 44..51 (PAD 1 = 44)");
    check(daw_default(DAW_PAD0 + 16 + 8).num == 52 && daw_default(DAW_PAD0 + 31).num == 59 && daw_default(DAW_PAD0 + 16 + 7).num == 67,
          "pad bank B: 16 notes higher (52..67)");
    check(daw_default(DAW_BTN0 + 2).num == 115 && daw_default(DAW_BTN0 + 3).num == 117 && daw_default(DAW_BTN0).num == 110 &&
          daw_default(DAW_BTN0 + 12).num == 88, "SEQ PLAY CC 115, SEQ REC CC 117, < CC 110, BT CC 88");
    check(daw_default(DAW_PITCH).type == DT_BEND && daw_default(DAW_MOD).type == DT_CC && daw_default(DAW_MOD).num == 1,
          "pitch wheel: pitch bend; mod wheel: CC 1");
    for (ok = 1, i = 0; i < DAW_N; i++)
        ok &= daw_default(i).ch == (daw_class(i) == DC_PAD ? 10 : 0);
    check(ok, "channel: MAIN (the menu's CHANNEL) for all but the pads (10)");

    /* ---- types per control ---- */
    e.type = DT_NOTE, e.num = 60, e.ch = 0;
    check(!daw_ent_ok(DAW_KNOB0, &e) && !daw_ent_ok(DAW_FADER0, &e) && daw_ent_ok(DAW_PAD0, &e) && daw_ent_ok(DAW_BTN0, &e),
          "NOTE: pads and buttons, not knobs or faders");
    e.type = DT_MMC, e.num = 2;
    check(daw_ent_ok(DAW_BTN0, &e) && !daw_ent_ok(DAW_PAD0, &e), "MMC: buttons only");
    e.num = 0;
    check(!daw_ent_ok(DAW_BTN0, &e), "MMC command 0: refused");
    e.type = DT_BEND, e.num = 0;
    check(daw_ent_ok(DAW_PITCH, &e) && !daw_ent_ok(DAW_MOD, &e), "PITCH BEND: the pitch wheel only");
    e.type = DT_CC, e.num = 128;
    check(!daw_ent_ok(DAW_KNOB0, &e), "a number past 127: refused");
    e.num = 5, e.ch = 17;
    check(!daw_ent_ok(DAW_KNOB0, &e), "a channel past 16: refused");
    {
        uint32_t t = DT_OFF, n = 0, seen2 = 0;
        do {
            t = daw_type_step(DAW_PAD0, t, 1);
            seen2 |= 1u << t;
            n++;
        } while (t != DT_OFF && n < 10u);
        check(n == 4u && seen2 == (1u << DT_OFF | 1u << DT_NOTE | 1u << DT_CC | 1u << DT_TOG), "a pad's types round: OFF CC TOGGLE NOTE");
        check(daw_type_step(DAW_KNOB0, DT_CC, 1) == DT_OFF && daw_type_step(DAW_KNOB0, DT_OFF, -1) == DT_CC,
              "a knob: CC <-> OFF, both ways");
    }
    e = daw_default(DAW_BTN0 + 2);
    daw_set_type(DAW_BTN0 + 2, &e, DT_MMC);
    check(e.type == DT_MMC && e.num == 2, "a button to MMC: PLAY");
    daw_set_type(DAW_BTN0 + 2, &e, DT_CC);
    check(e.type == DT_CC && e.num == 115, "back to CC: the default number");

    /* ---- messages ---- */
    daw_ch = 3;
    e = daw_default(DAW_KNOB0 + 2);
    check(daw_msg(&e, 99, p) == 1 && p[0] == PKT(0x0B, 0xB2, 72, 99), "K3 at 99: B2 48 63 (CC 72 on channel 3 = MAIN)");
    e.ch = 16;
    check(daw_msg(&e, 200, p) == 1 && p[0] == PKT(0x0B, 0xBF, 72, 127), "a fixed channel 16, a value clamped to 127");
    e = daw_default(DAW_PAD0 + 8);
    check(daw_msg(&e, 100, p) == 1 && p[0] == PKT(0x09, 0x99, 36, 100), "PAD 9 hit at 100: 99 24 64 (note on, channel 10)");
    check(daw_msg(&e, 0, p) == 1 && p[0] == PKT(0x08, 0x89, 36, 0), "PAD 9 let go: 89 24 00 (note off)");
    e = daw_default(DAW_PITCH);
    check(daw_msg(&e, 8192, p) == 1 && p[0] == PKT(0x0E, 0xE2, 0, 64), "pitch wheel at rest: E2 00 40");
    check(daw_msg(&e, 16383, p) == 1 && p[0] == PKT(0x0E, 0xE2, 127, 127) && daw_msg(&e, 0, p) == 1 && p[0] == PKT(0x0E, 0xE2, 0, 0),
          "full up E2 7F 7F, full down E2 00 00");
    e.type = DT_MMC, e.num = 2;
    check(daw_msg(&e, 127, p) == 2 && p[0] == PKT(0x04, 0xF0, 0x7F, 0x7F) && p[1] == PKT(0x07, 0x06, 0x02, 0xF7),
          "MMC PLAY: F0 7F 7F 06 02 F7 in two packets (CIN 4, 7)");
    e.type = DT_OFF;
    check(daw_msg(&e, 127, p) == 0, "OFF: nothing");
    daw_ch = 1;

    /* ---- knobs, toggles, rate ---- */
    {
        uint8_t kv = 0;
        check(daw_knob_val(&kv, 3, 0) == 3 && daw_knob_val(&kv, -10, 0) == 0 && daw_knob_val(&kv, 300, 0) == 127,
              "ABSOLUTE: 0..127, stops at the ends");
        check(daw_knob_val(&kv, 1, 1) == 65 && daw_knob_val(&kv, -2, 1) == 62 && daw_knob_val(&kv, -100, 1) == 1 &&
              daw_knob_val(&kv, 100, 1) == 127 && kv == 127, "RELATIVE: 64 + detents (1..127), the value untouched");
    }
    {
        uint8_t st = 0;
        uint32_t t = 0;
        check(daw_toggle(&st, &t, 1000) && st == 1, "a toggle: on at the first press");
        check(!daw_toggle(&st, &t, 1100) && st == 1, "a second press 100 ms later (a wobbling pad): nothing");
        check(daw_toggle(&st, &t, 1000 + DAW_TOG_MS) && st == 0, "180 ms later: off");
        t = 0, st = 0;
        check(daw_toggle(&st, &t, 0) && st == 1 && t, "at ms 0 too");
    }
    {
        daw_cv_t c = {0, -1, 0, 0};
        check(daw_cv_due(&c, 5, 0), "a value never sent: due");
        c.v = 10;
        check(daw_cv_due(&c, 5, 0), "a new value, nothing sent yet: due");
        c.sent = 10, c.t = 5 | 1u, c.v = 11;
        check(!daw_cv_due(&c, 8, 0) && daw_cv_due(&c, 5 + DAW_RATE_MS, 0), "the next one waits DAW_RATE_MS (8 ms)");
        c.v = 11, c.sent = 11;
        check(!daw_cv_due(&c, 100, 0), "unchanged: never");
        c.v = 50;
        check(!daw_cv_due(&c, 100, 1), "RELATIVE: no detents, nothing (whatever the value)");
        c.acc = -2;
        check(daw_cv_due(&c, 100, 1), "RELATIVE: detents to send");
    }

    /* ---- the flash object ---- */
    {
        daw_store_t s, s2;
        uint8_t b[sizeof s];
        daw_reset_user();
        check(daw_user_changed() == 0, "the USER map starts as the DEFAULT");
        daw_on = 1, daw_ch = 7, daw_local = 1, daw_user = 1, daw_rel = 1;
        daw_umap[DAW_PAD0 + 3].type = DT_TOG, daw_umap[DAW_PAD0 + 3].num = 80, daw_umap[DAW_PAD0 + 3].ch = 2;
        daw_umap[DAW_KNOB0].num = 21;
        check(daw_user_changed() == 2, "two entries changed");
        daw_store_fill(&s);
        daw_store_fill(&s2);
        check(memcmp(&s, &s2, sizeof s) == 0 && sizeof s <= 256u, "the object: the same bytes each time (padding too), small");
        memcpy(b, &s, sizeof s);
        daw_on = 0, daw_ch = 1, daw_local = 0, daw_user = 0, daw_rel = 0;
        daw_reset_user();
        check(daw_store_take(b, sizeof s) && daw_on == 1 && daw_ch == 7 && daw_local == 1 && daw_user == 1 && daw_rel == 1 &&
              daw_umap[DAW_PAD0 + 3].type == DT_TOG && daw_umap[DAW_PAD0 + 3].num == 80 && daw_umap[DAW_PAD0 + 3].ch == 2 &&
              daw_umap[DAW_KNOB0].num == 21, "round trip: MODE, CHANNEL, LOCAL, CC MAP, KNOBS, the USER map");
        b[8 + 3 * DAW_PAD0] = DT_MMC;                   /* a pad as MMC: not a pad's type */
        b[8 + 3 * 1 + 2] = 40;                          /* K2 on channel 40 */
        daw_store_take(b, sizeof s);
        check(daw_umap[DAW_PAD0].type == DT_NOTE && daw_umap[DAW_PAD0].num == 44 && daw_umap[DAW_KNOB0 + 1].num == 71 &&
              daw_umap[DAW_KNOB0 + 1].ch == 0, "an entry out of range: its default (the others kept)");
        s.n = 60;                                       /* an older, shorter map */
        memcpy(b, &s, sizeof s);
        daw_reset_user();
        daw_umap[DAW_MOD].num = 9;
        daw_store_take(b, 8u + 3u * 60u);
        check(daw_umap[DAW_PAD0 + 3].type == DT_TOG && daw_umap[DAW_MOD].num == 1 && daw_umap[DAW_PITCH].type == DT_BEND,
              "an older map (60 entries): the rest the defaults");
        b[8 + 3 * DAW_KNOB0 + 1] = 30;
        daw_store_take(b, 8u + 3u * 10u + 1u);
        check(daw_umap[DAW_KNOB0].num == 30 && daw_umap[DAW_KNOB0 + 10].num == 104, "cut short: what is there, the rest the defaults");
        b[0] ^= 0x55u;
        daw_on = 0, daw_ch = 5;
        check(!daw_store_take(b, sizeof s) && daw_on == 0 && daw_ch == 5, "a bad magic (another format): not taken, nothing changes");
        check(!daw_store_take(b, 4), "too short: not taken");
        b[0] ^= 0x55u;
        b[5] = 0;                                       /* channel 0 */
        daw_store_take(b, sizeof s);
        check(daw_ch == 1, "a channel out of range: 1");
    }
    {
        char n[20];
        daw_name(DAW_KNOB0 + 10, n);
        ok = !strcmp(n, "K3");
        daw_name(DAW_PAD0 + 27, n);
        ok &= !strcmp(n, "PAD 12");
        daw_name(DAW_FADER0 + 5, n);
        ok &= !strcmp(n, "FADER 2");
        daw_name(DAW_BTN0 + 2, n);
        ok &= !strcmp(n, "SEQ PLAY");
        daw_name(DAW_PITCH, n);
        ok &= !strcmp(n, "PITCH WHEEL");
        check(ok && daw_bank_of(DAW_KNOB0 + 10) == 1 && daw_bank_of(DAW_PAD0 + 27) == 1 && daw_bank_of(DAW_FADER0 + 1) == 0,
              "names for the screen (K3, PAD 12, FADER 2, SEQ PLAY, PITCH WHEEL), their banks");
    }
    printf("smk_daw: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
