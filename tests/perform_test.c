/* SPDX-License-Identifier: GPL-3.0-only */
/* PERFORM (editor protocol v11, firmware/src/perform.c) on the real UI and sequencer (the ui_pages_test
 * harness): every op as a remote sends it, its reply (op, rc, the state), and that it does what the
 * panel's layer keys do: punch-in FX held and let go (a panel key takes over), sections A..D (stopped:
 * loaded; playing: the next bar), the quick chain, store (twice over a used section), mute / solo, PLAY /
 * STOP, fill held / next bar, tap tempo, song mode, SONG REC; a quiet remote lets go of its holds. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include <stdarg.h>

/* the editor's reply writer (editor.c), captured */
static uint8_t rep[256];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_v(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    ed_b(u);
    ed_b(u >> 7);
}
#include "../firmware/src/perform.c"

/* one request: op + args; the reply decoded */
static struct { uint32_t op, rc, flags, bpm, beat, fx, mute, solo, armed, ready, sec, req, srec, cn, ci, cs[8], sel, bars[4]; } st;
static void pf(uint32_t n, ...)
{
    uint8_t a[16];
    uint32_t i;
    va_list ap;
    va_start(ap, n);
    for (i = 0; i < n && i < sizeof a; i++)
        a[i] = (uint8_t)va_arg(ap, int);
    va_end(ap);
    rep_n = 0;
    pf_handle(a, n);
    st.op = rep[0]; st.rc = rep[1]; st.flags = rep[2];
    st.bpm = (uint32_t)(rep[3] | rep[4] << 7) - 8192u;
    st.beat = rep[5]; st.fx = rep[6]; st.mute = rep[7]; st.solo = rep[8]; st.armed = rep[9];
    st.ready = rep[10]; st.sec = rep[11]; st.req = rep[12]; st.srec = rep[13]; st.cn = rep[14]; st.ci = rep[15];
    for (i = 0; i < st.cn && i < 8u; i++)
        st.cs[i] = rep[16 + i];
    st.sel = rep[16 + st.cn];
    for (i = 0; i < 4u; i++)
        st.bars[i] = rep[17 + st.cn + i];
    frame();                                        /* the UI and the audio go on */
}

int main(void)
{
    uint32_t i;
    outdir = "build/host";
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    song.g[G_BPM] = 120;
    check(ED_PERFORM_CMD == 43, "PERFORM is cmd 43");
    go_home(); frame();

    /* STATE */
    song.playing = 0; live_sec = -1; live_req = -1; srec = 0; arrangement_enabled = 0; chain_n = 0;
    pf(1, PF_STATE);
    check(st.op == PF_STATE && st.rc == 0 && !(st.flags & 1u) && st.fx == PF_NONE && st.sec == PF_NONE &&
          st.ready == 3u && st.bpm == (uint32_t)song.g[G_BPM] && rep_n == 21u, "STATE: stopped, no FX, A B ready, the tempo");
    check(st.bars[0] == 1u && st.bars[1] == 2u && st.bars[2] == 0u && st.bars[3] == 0u, "STATE: section lengths A 1 bar, B 2 bars, C D empty (0)");
    song.sel = 2; pf(1, PF_STATE);
    check(st.sel == 2u, "STATE: the selected track");
    song.sel = 0;

    /* FX: held from the remote, let go; the panel takes over */
    pf(2, PF_FX, 7);
    check(punch.req == 7 && punch.keybit == PF_EXT && st.fx == 7u && (st.flags & 32u), "FX 7 (STOP) held from the remote");
    frames(5);
    check(punch.cur == 7, "the mix goes through it");
    pf(2, PF_FX, 127);
    check(punch.req == -1 && punch.keybit == 0 && st.fx == PF_NONE, "FX off: the mix comes back");
    pf(2, PF_FX, 2);
    press(B_FX); frames(15);
    fm1_in.notes |= 1u << key_of_white(5); frame();
    check(punch.req == 5 && punch.keybit != PF_EXT, "a panel key takes over a remote FX");
    pf(2, PF_FX, 9);
    check(punch.req == 5, "the remote does not take an FX the panel holds");
    pf(2, PF_FX, 127);
    check(punch.req == 5, "nor let go of it");
    fm1_in.notes &= ~(1u << key_of_white(5)); frame(); release(B_FX); frames(5);
    check(punch.req == -1, "its key up: off");
    pf(2, PF_FX, 16);
    check(punch.req == -1 && st.rc == 0, "FX 16 (no such effect): off");

    /* a quiet remote lets go of its holds */
    pf(2, PF_FX, 3);
    pf(2, PF_FILL, 1);
    check(fill_held && punch.req == 3, "FX and FILL held from the remote");
    pf_watchdog(fm1_ms - 200u);
    check(fill_held && punch.req == 3, "a request 200 ms ago: still held");
    pf_watchdog(fm1_ms - 2000u);
    check(!fill_held && punch.req == -1, "quiet for 2 s: FX and FILL let go");
    fill_held = 1;
    pf_watchdog(fm1_ms - 5000u);
    check(fill_held, "a fill held on the panel is not the remote's");
    fill_held = 0;

    /* sections: stopped, loaded; playing, the next bar; empty: not */
    pf(2, PF_SECTION, 1);
    check(sec_loads == 1 && live_sec == 1 && st.sec == 1u, "stopped: B loaded as the loop");
    pf(2, PF_SECTION, 3);
    check(sec_loads == 1, "an empty D: nothing");
    pf(2, PF_SECTION, 4);
    check(st.rc == 1, "section 4: rc 1");
    song.playing = 1; clk_beat = 1; clk_pos = 0;
    pf(2, PF_SECTION, 0);
    check(live_req == 0 && st.req == 0u && chain_n == 0u && chain_taps == 0u, "playing: A asked for the next bar, no chain tap");

    /* the quick chain */
    live_req = -1;
    pf(5, PF_CHAIN, 3, 0, 1, 1);
    check(st.rc == 0 && chain_n == 3u && chain_sec[0] == 0 && chain_sec[1] == 1 && chain_sec[2] == 1 && live_req == 0 &&
          chain_bars == 1u && st.cn == 3u && st.cs[2] == 1u && st.bars[1] == 2u, "CHAIN A B B: plays as SAVE + taps would (A next, 1 bar)");
    pf(4, PF_CHAIN, 2, 0, 3);
    check(st.rc == 2 && chain_n == 3u, "a chain with an empty D: rc 2, the chain stays");
    pf(3, PF_CHAIN, 3, 0);
    check(st.rc == 1, "a short frame: rc 1");
    pf(2, PF_SECTION, 1);
    check(chain_n == 0u && live_req == 1, "a single section: the chain ends, B asked for");
    song.playing = 0; live_req = -1;
    pf(3, PF_CHAIN, 1, 0);
    check(st.rc == 1, "stopped: no chain (rc 1)");

    /* store: over a used section twice within 3 s, an empty one at once */
    sec_stores = 0;
    pf(2, PF_STORE, 0);
    check(sec_stores == 0, "store over a used A: asks again");
    pf(2, PF_STORE, 0);
    check(sec_stores == 1 && live_sec == 0, "again: stored in A");
    pf(2, PF_STORE, 2);
    check(sec_stores == 2, "an empty C: stored at once");

    /* mute / solo */
    pf(3, PF_MUTE, 2, 1);
    check(trk[2].p[P_MUTE] && st.mute == 4u, "mute track 3");
    pf(3, PF_MUTE, 2, 2);
    check(!trk[2].p[P_MUTE] && st.mute == 0u, "toggle: unmuted");
    pf(3, PF_SOLO, 0, 2);
    pf(3, PF_SOLO, 3, 1);
    check(song.solo == 9u && st.solo == 9u, "solo tracks 1 and 4");
    pf(3, PF_SOLO, 0, 0);
    pf(3, PF_SOLO, 3, 2);
    check(song.solo == 0u, "both off");
    pf(3, PF_MUTE, 4, 1);
    check(st.rc == 1, "track 5: rc 1");

    /* transport */
    song.playing = 0; transport_req = 0;
    pf(2, PF_TRANSPORT, 1);
    frames(2);
    check(song.playing, "PLAY");
    pf(2, PF_TRANSPORT, 1);
    check(song.playing && st.rc == 0, "PLAY while playing: nothing");
    pf(2, PF_TRANSPORT, 0);
    frames(2);
    check(!song.playing, "toggle: STOP");

    /* fill */
    pf(2, PF_FILL, 2);
    check(fill_arm && (st.flags & 16u), "FILL next bar armed");
    pf(2, PF_FILL, 2);
    check(!fill_arm, "again: off");
    pf(2, PF_FILL, 1);
    check(fill_held && (st.flags & 8u), "FILL held");
    pf(2, PF_FILL, 0);
    check(!fill_held, "FILL let go");

    /* tap tempo: four taps 500 ms apart = 120 BPM */
    {
        song.g[G_BPM] = 90;
        for (i = 0; i < 4u; i++) {
            pf(1, PF_TAP);
            fm1_ms += 500u - 16u;
        }
        check(song.g[G_BPM] == 120, "tap tempo: 4 taps at 500 ms = 120 BPM");
    }

    /* song mode, SONG REC */
    pf(2, PF_SONGMODE, 1);
    check(arrangement_enabled && (st.flags & 2u), "song mode on");
    pf(2, PF_SONGMODE, 1);
    check(arrangement_enabled, "on again: stays on");
    pf(2, PF_SONGMODE, 2);
    check(!arrangement_enabled, "toggle: loop mode");
    pf(1, PF_SONGREC);
    check(srec == 1u && st.srec == 1u, "SONG REC armed");
    pf(1, PF_SONGREC);
    check(srec == 0u, "again: off");

    pf(1, 99);
    check(st.rc == 1 && st.op == 99u, "an unknown op: rc 1, the state still");
    pf(0);
    check(st.op == PF_STATE && st.rc == 0, "no op byte: STATE");

    printf("perform: %s\n", fails ? "FAIL" : "all ok");
    return fails != 0;
}
