/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 DAW mode on the UI harness (firmware/src/ui_daw.c, the HOME menu's SMK-37 sections): < + > switches SYNTH <-> DAW
 * (the screen says so; the sequencer stops; no arrow message), the knobs / faders / buttons / wheels send what the map says
 * (the packets as USB MIDI has them), the banks, OCT, PATCH (LOCAL), < > only when let go, CC TOGGLE once a press, the
 * rate of a fader, the map editor (pick a control, TYPE / NUMBER / CHANNEL, the USER map), the menu (PLAY, MIDI, DAW: rows
 * and knobs, MODE / CHANNEL / LOCAL / CC MAP / KNOBS / RESET MAP), the lights. Renders DIR/daw-*.ppm. Build with
 * -DSMK_MIX=1 (the menu's LIMITER). */
#define UI_PAGES_HARNESS_ONLY 1
#define UI_MENU_SMK "daw_ui_host.h"
#include "ui_pages_test.c"

enum { SMK_BTN_KNOB_BANK = 0, SMK_BTN_FADER_BANK = 1, SMK_BTN_LEFT = 2, SMK_BTN_RIGHT = 3 };   /* (smk_input.h) */
#define SMK_ENC_MASTER 0u
#define SMK_NLED 19u
#define SMK_NPAD 16u
static uint8_t smk_pad_daw, smk_pad_piano, smk_master_hold, smk_master_enc;
static uint16_t smk_pad_pick;
static uint32_t padled_hold_until, bled_hold_until;
static uint8_t bled_id, pads[16][3], leds[SMK_NLED];
static void padled_set(uint32_t p, uint8_t r, uint8_t g, uint8_t b) { pads[p & 15u][0] = r, pads[p & 15u][1] = g, pads[p & 15u][2] = b; }
static uint32_t closes;
static void pick_close(void) { closes++; }
static void fb_close(void) {}
static uint8_t fad[4];
static uint32_t frames_seen = 1000u;
#define DAW_FADER(f) fad[f]
#define DAW_FRAMES() frames_seen
static int32_t enc8[8];
static int32_t daw_enc_take(uint32_t e) { int32_t s = enc8[e & 7u]; enc8[e & 7u] = 0; return s; }
static uint32_t pk[512], npk;
static void out_pkt(uint32_t p) { pk[npk++ & 511u] = p; }
#define fm1_enc_take daw_enc_take
#define midi_out_event out_pkt
#define fm1_led leds
#include "../firmware/src/ui_daw.c"
#undef fm1_enc_take
#undef midi_out_event
#undef fm1_led

#define PKT(cin, a, b, c) ((uint32_t)(cin) | (uint32_t)(a) << 8 | (uint32_t)(b) << 16 | (uint32_t)(c) << 24)
static const uint8_t SMK_PANEL_BTN[NB] = {14, 8, 9, 7, 13, 10, 12, 11, 6, 15, 16, 17, 4, 5};   /* (panel.c, SMK-37) */
static void step(void)                           /* one UI pass of the SMK build in DAW mode: input, lights, screen */
{
    uint32_t p = fm1_input_edges(0);
    daw_switch(&p);
    if (daw_on)
        daw_input(p, fm1_ticks());
    if (daw_on)
        daw_leds();
    ui.frame++;
    if (!daw_screen()) {
        if (ui.menu)
            draw_menu();
    }
    ui_timers();
    ui.force = 0;
    fm1_ms += 16;
}
static void id_press(uint32_t id) { edges_btn |= 1u << id; fm1_in.buttons |= 1u << id; step(); }
static void id_release(uint32_t id) { fm1_in.buttons &= ~(1u << id); step(); }
static void id_tap(uint32_t id) { id_press(id); id_release(id); }
static uint32_t BTN(uint32_t label) { return panel.btn[label]; }
static uint32_t last_pkt(void) { return npk ? pk[(npk - 1u) & 511u] : 0u; }
static void shot(const char *n) { ui.force = 1; ui.frame++; if (!daw_screen() && ui.menu) draw_menu(); ui.force = 0; ppm(n); }

int main(int argc, char **argv)
{
    uint32_t i, n0;
    outdir = argc > 1 ? argv[1] : "build/host";
    memcpy(panel.btn, SMK_PANEL_BTN, sizeof panel.btn);
    for (i = 0; i < NE; i++)
        panel.enc[i] = (uint8_t)(i + 1u), panel.dir[i] = 1;
    layers_init();
    host_tracks_init();
    settings.palette = 4;
    palette_set(4);
    go_home(); ui.force = 1; frame();
    daw_reset_user();

    /* ---- the switch ---- */
    song.playing = 1;
    id_press(SMK_BTN_LEFT);
    check(!daw_on, "< alone: still SYNTH");
    id_press(SMK_BTN_RIGHT);
    check(daw_on && smk_master_hold && transport_req == 2 && daw_splash_t && daw_splash_on, "< held + >: DAW mode, the sequencer stops, its screen");
    shot("daw-splash");
    id_release(SMK_BTN_RIGHT); id_release(SMK_BTN_LEFT);
    check(npk == 0, "the arrows of the switch: no message");
    song.playing = 0; transport_req = 0;
    fm1_ms += 2000; step();
    shot("daw-empty");

    /* ---- knobs ---- */
    enc8[SMK_ENC_MASTER] = 2; step();
    check(last_pkt() == PKT(0x0B, 0xB0, 70, 2) && daw_kval[0] == 2, "K1 (the volume's encoder) 2 detents: CC 70 = 2 on channel 1");
    daw_ch = 2;
    fm1_ms += 100;
    enc8[3] = 1; step();
    check(last_pkt() == PKT(0x0B, 0xB1, 73, 1), "K4: CC 73, on CHANNEL 2");
    id_tap(SMK_BTN_KNOB_BANK);
    check(daw_kbank == 1 && leds[SMK_BTN_KNOB_BANK], "KNOB BANK: bank B, lit");
    fm1_ms += 100;
    enc8[1] = 1; step();
    check(last_pkt() == PKT(0x0B, 0xB1, 103, 1), "K2 in bank B: CC 103");
    id_tap(SMK_BTN_KNOB_BANK);
    daw_rel = 1;
    fm1_ms += 100;
    enc8[1] = -1; step();
    check(last_pkt() == PKT(0x0B, 0xB1, 71, 63), "RELATIVE: K2 one left = 63");
    daw_rel = 0;
    fm1_ms += 100;
    n0 = npk;
    enc8[2] = 1; step(); enc8[2] = 1; step();
    check(npk - n0 == 2 && last_pkt() == PKT(0x0B, 0xB1, 72, 4 + 1), "fast detents go further (x4)");

    /* ---- faders: the rate ---- */
    step();
    n0 = npk;
    for (i = 0; i < 6u; i++) { fad[1] = (uint8_t)(10 + i); fm1_ms -= 14; step(); }   /* six moves in 12 ms */
    check(npk - n0 <= 2u, "a fader moving fast: one message per 8 ms at most");
    fm1_ms += 20; step();
    check(last_pkt() == PKT(0x0B, 0xB1, 21, 15), "and the last value always (CC 21 = 15)");
    id_tap(SMK_BTN_FADER_BANK);
    fad[1] = 40; fm1_ms += 20; step();
    check(daw_fbank == 1 && last_pkt() == PKT(0x0B, 0xB1, 25, 40), "FADER BANK: fader 2 is CC 25");
    id_tap(SMK_BTN_FADER_BANK);

    /* ---- buttons ---- */
    id_press(BTN(B_PLAY));
    check(last_pkt() == PKT(0x0B, 0xB1, 115, 127) && leds[BTN(B_PLAY)], "SEQ PLAY pressed: CC 115 = 127, lit");
    id_release(BTN(B_PLAY));
    check(last_pkt() == PKT(0x0B, 0xB1, 115, 0) && !leds[BTN(B_PLAY)], "let go: CC 115 = 0");
    n0 = npk;
    id_press(SMK_BTN_RIGHT);
    check(npk == n0, "> pressed: nothing yet (< + > is the switch)");
    id_release(SMK_BTN_RIGHT);
    check(npk == n0 + 2 && pk[n0 & 511u] == PKT(0x0B, 0xB1, 111, 127) && last_pkt() == PKT(0x0B, 0xB1, 111, 0), "> let go: CC 111 127, 0");
    daw_user = 1;
    daw_umap[DAW_BTN0 + 7].type = DT_TOG;                 /* ARP: a toggle */
    id_tap(BTN(B_ARP));
    check(last_pkt() == PKT(0x0B, 0xB1, 118, 127) && daw_tog[DAW_BTN0 + 7] && leds[BTN(B_ARP)], "ARP as CC TOGGLE: on, lit");
    n0 = npk;
    id_tap(BTN(B_ARP));
    check(npk == n0, "pressed again 32 ms later: nothing (once a press)");
    fm1_ms += 300;
    id_tap(BTN(B_ARP));
    check(last_pkt() == PKT(0x0B, 0xB1, 118, 0) && !leds[BTN(B_ARP)], "later: off");
    daw_umap[DAW_BTN0 + 2].type = DT_MMC, daw_umap[DAW_BTN0 + 2].num = 2;   /* SEQ PLAY: MMC PLAY */
    n0 = npk;
    id_tap(BTN(B_PLAY));
    check(npk == n0 + 2 && pk[n0 & 511u] == PKT(0x04, 0xF0, 0x7F, 0x7F) && last_pkt() == PKT(0x07, 0x06, 0x02, 0xF7),
          "SEQ PLAY as MMC: F0 7F 7F 06 02 F7");
    check(smo.w - smo.r >= 6u && smo.ring[(smo.w - 6u) % SMO_RING] == 0xF0 && smo.ring[(smo.w - 1u) % SMO_RING] == 0xF7,
          "and the TRS jack gets it whole");
    daw_reset_user();
    daw_user = 0;

    /* ---- OCT, PATCH ---- */
    id_tap(BTN(B_OCTUP)); id_tap(BTN(B_OCTUP));
    check(daw_oct == 2 && leds[BTN(B_OCTUP)], "OCT+ twice: +2, lit");
    id_press(BTN(B_OCTDN)); id_press(BTN(B_OCTUP)); id_release(BTN(B_OCTUP)); id_release(BTN(B_OCTDN));
    check(daw_oct == 0, "both: 0");
    id_tap(BTN(B_HOME));
    check(daw_local && leds[BTN(B_HOME)], "PATCH tapped: LOCAL ON, PATCH lit");
    id_tap(BTN(B_HOME));
    check(!daw_local, "again: OFF");

    /* ---- the pads' lights ---- */
    daw_leds();
    check(smk_pad_daw && pads[8][2] >= 90 && pads[8][1] >= 40, "NOTE pads (bank A): blue, dim but plain to see");
    daw_user = 1;
    daw_umap[DAW_PAD0].type = DT_TOG; daw_tog[DAW_PAD0] = 0;
    daw_umap[DAW_PAD0 + 1].type = DT_TOG; daw_tog[DAW_PAD0 + 1] = 1;
    daw_umap[DAW_PAD0 + 2].type = DT_OFF;
    daw_leds();
    check(pads[0][0] >= 90 && pads[0][1] >= 75 && pads[1][0] == 255 && pads[2][0] > 0 && pads[2][0] < 40,
          "a CC TOGGLE off: yellow dim >= (90, 75, 0); on: full; OFF: faint");
    daw_reset_user(); daw_user = 0;
    shot("daw-main");

    /* ---- the map editor ---- */
    id_press(BTN(B_HOME)); id_press(BTN(B_OCTUP)); id_release(BTN(B_OCTUP)); id_release(BTN(B_HOME));
    check(daw_edit == 1 && daw_user && !daw_local && !ui.menu, "PATCH held + OCT+: the editor (CC MAP USER), PATCH's tap eaten");
    shot("daw-edit-learn");
    n0 = npk;
    enc8[5] = 1; step();                                  /* K6 */
    check(daw_edit == 2 && daw_esel == DAW_KNOB0 + 5 && npk == n0, "K6 turned: picked, nothing sent");
    /* KNOB 1..3 of the FM-1 layout: K5, K6, K7 (calibration slots 3, 4, 5 = encoders 4, 5, 6) */
    enc8[4] = 1; step();
    check(daw_umap[DAW_KNOB0 + 5].type == DT_OFF, "KNOB 1 (K5): TYPE CC -> OFF");
    enc8[4] = -1; step();
    enc8[5] = 3; step();
    check(daw_umap[DAW_KNOB0 + 5].type == DT_CC && daw_umap[DAW_KNOB0 + 5].num == 78, "KNOB 2 (K6): NUMBER 75 -> 78");
    enc8[6] = 2; step();
    check(daw_umap[DAW_KNOB0 + 5].ch == 2, "KNOB 3 (K7): CHANNEL MAIN -> 2");
    shot("daw-edit-set");
    id_tap(BTN(B_OCTUP));
    check(daw_edit == 1, "OCT+: the next control");
    daw_touch_pad = (uint8_t)(DAW_PAD0 + 8 + 1); step();
    check(daw_edit == 2 && daw_esel == DAW_PAD0 + 8, "a pad hit: picked (PAD 9)");
    enc8[4] = -1; step();
    check(daw_umap[DAW_PAD0 + 8].type == DT_TOG, "its TYPE: NOTE -> CC TOGGLE");
    shot("daw-edit-pad");
    id_tap(BTN(B_REC));
    check(daw_esel == DAW_BTN0 + 3, "SEQ REC pressed: picked");
    fad[3] = 90; step();
    check(daw_esel == DAW_FADER0 + 3, "fader 4 moved: picked");
    daw_wheels(4000, 0); step();
    check(daw_esel == DAW_PITCH, "the pitch wheel bent: picked");
    daw_wheels(0, 0); step();
    id_tap(BTN(B_OCTDN));
    check(!daw_edit && settings_later && daw_user_changed() == 2, "OCT-: done; two entries changed, kept once quiet");
    settings_later = 0;
    enc8[5] = 1; fm1_ms += 100; step();
    check(last_pkt() == PKT(0x0B, 0xB1, 78, 1) || last_pkt() == PKT(0x0B, 0xB1, 78, 2), "K6 now sends CC 78 on channel 2");

    /* ---- the menu ---- */
    edges_btn |= 1u << BTN(B_HOME); fm1_in.buttons |= 1u << BTN(B_HOME);
    for (i = 0; i < 60u; i++) step();
    fm1_in.buttons &= ~(1u << BTN(B_HOME)); step();
    check(ui.menu == 1 && ui.menu_sel == MI_TRS, "PATCH held: the menu, at MIDI");
    shot("daw-menu-midi");
    check(MS_FIRST[MS_PLAY] == MI_VEL && MS_FIRST[MS_MIDI] == MI_TRS && MS_FIRST[MS_DAW] == MI_DMAP && MI_LOCAL - MI_TRS == 3 &&
          MI_DRESET - MI_DMAP == 3 && MI_BDN - MI_VEL == 2 && MI_ACCEL == MI_BDN + 1 && MS_FIRST[MS_MIDI] - MS_FIRST[MS_PLAY] == 4 &&
          MI_KLAY == MI_ZOOM + 1 && MI_LIMIT - MS_FIRST[MS_AUDIO] == 3,
          "PLAY: KEY VELOCITY, BEND UP, BEND DOWN, KNOB ACCEL (2.7); MIDI: TRS, MODE, CHANNEL, LOCAL; DAW: 4; KNOB LAYOUT, LIMITER on their knobs");
    for (i = 0; i < MS_COUNT; i++)
        if (MS_FIRST[i + 1] - MS_FIRST[i] > 4u || MS_FIRST[i + 1] <= MS_FIRST[i])
            check(0, "a section of 1..4 rows");
    check(!strcmp(MI_NAME[MI_MODE], "MODE") && !strcmp(MI_NAME[MI_DCH], "CHANNEL") && !strcmp(MI_NAME[MI_LOCAL], "LOCAL") &&
          !strcmp(MI_NAME[MI_DMAP], "CC MAP") && !strcmp(MI_NAME[MI_VEL], "KEY VELOCITY"), "the rows' names");
    mi_set(MI_DCH, 1);
    check(daw_ch == 3, "CHANNEL: right 3");
    for (i = 0; i < 20u; i++) mi_set(MI_DCH, 1);
    check(daw_ch == 16, "stops at 16");
    mi_set(MI_DCH, 0);
    check(daw_ch == 1, "OCT+: round to 1");
    mi_set(MI_LOCAL, 1); mi_set(MI_KREL, 1); mi_set(MI_DMAP, -1);
    check(daw_local && daw_rel && !daw_user, "LOCAL ON, KNOBS RELATIVE, CC MAP DEFAULT");
    {
        uint16_t c;
        check(!strcmp(mi_value(MI_DRESET, &c), "2 CHANGED") && !strcmp(mi_value(MI_MODE, &c), "DAW"), "RESET MAP shows 2 CHANGED");
        mi_set(MI_DRESET, 0);
        check(!strcmp(mi_value(MI_DRESET, &c), "OCT+ AGAIN") && daw_user_changed() == 2, "OCT+: armed, nothing yet");
        mi_set(MI_DRESET, 0);
        check(daw_user_changed() == 0 && !strcmp(mi_value(MI_DRESET, &c), "NONE CHANGED"), "OCT+ again: the USER map is the DEFAULT");
    }
    ui.menu_sel = MS_FIRST[MS_DAW]; shot("daw-menu-daw");
    ui.menu_sel = MS_FIRST[MS_PLAY]; shot("daw-menu-play");
    knob_layout = 1;
    ui.menu_sel = MS_FIRST[MS_MIDI]; shot("daw-menu-midi-smk");
    check(knob_digit(EN_K1) == '1', "KNOB LAYOUT SMK: the rows are K1..K4");
    knob_layout = 0;
    mi_set(MI_MODE, -1);
    check(!daw_on && !smk_master_hold, "MODE left: SYNTH (the menu stays up)");
    mi_set(MI_MODE, 0);
    check(daw_on, "OCT+: DAW");
    daw_local = 0; daw_rel = 0; daw_ch = 2;
    menu_close();

    /* ---- back to SYNTH ---- */
    daw_wheels(3000, 0); fm1_ms += 20; step();
    check(pk[(npk - 2u) & 511u] == PKT(0x0E, 0xE1, (8192 + 3000) & 127, (8192 + 3000) >> 7) && last_pkt() == PKT(0x0B, 0xB1, 1, 0),
          "the wheels: pitch bend on channel 2, the mod wheel CC 1 (as DAW mode starts: where they are)");
    id_press(BTN(B_SEQ));
    id_press(SMK_BTN_RIGHT); id_press(SMK_BTN_LEFT);
    check(!daw_on && daw_splash_t && !daw_splash_on, "> held + <: SYNTH, its screen");
    check(pk[(npk - 2u) & 511u] == PKT(0x0B, 0xB1, 112, 0) && last_pkt() == PKT(0x0E, 0xE1, 0, 64),
          "the button held lets go (CC 112 = 0), the bend back to the centre");
    shot("synth-splash");
    id_release(SMK_BTN_LEFT); id_release(SMK_BTN_RIGHT); id_release(BTN(B_SEQ));
    printf("daw_ui: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
