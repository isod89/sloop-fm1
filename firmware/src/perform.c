/* SPDX-License-Identifier: GPL-3.0-only */
/* PERFORM (editor protocol v11, cmd 43): the live layers for a remote surface (a phone, a tablet).
 * What a held function button + a white key does on the panel, as one call each, through the same
 * code the keys run: punch-in FX (punch.c), sections A..D and the quick chain (seq.c live sections,
 * ui_layers.c LY_SONG), mute / solo / fill / tap tempo (LY_MIX), PLAY / STOP, song mode, SONG REC.
 * The UI main loop calls these (editor.c ed_service), as it runs layer_key.
 *
 * A remote hold (FX, FILL) is released when the remote goes quiet: no request for PF_QUIET_MS (the
 * editor PINGs every second while it watches), so a phone that drops off never leaves the mix
 * looping. A key on the panel takes over a remote FX at once, as one key takes over another. */
#define ED_PERFORM_CMD 43u                          /* its editor command (editor.c checks ED_PERFORM) */
#define PF_EXT 0x80000000u                          /* punch.keybit of a remote FX (no key is bit 31) */
#define PF_QUIET_MS 1500u
#define PF_NONE 127u                                /* "none" in a 7-bit byte */
enum { PF_STATE, PF_FX, PF_SECTION, PF_CHAIN, PF_MUTE, PF_SOLO, PF_TRANSPORT, PF_FILL, PF_TAP,
       PF_SONGMODE, PF_SONGREC, PF_STORE, PF_COUNT };
static uint8_t pf_fill_ext;                         /* FILL held by the remote */

/* 0 off, 1 on, 2 toggle */
static uint32_t pf_tri(uint32_t cur, uint32_t v) { return v == 2u ? !cur : v != 0u; }

/* a remote FX: 0..15, or none; a panel key holding one keeps it (its key-up ends it) */
static void pf_fx(uint32_t fx)
{
    if (fx < PUNCH_NFX) {
        if (punch.keybit && punch.keybit != PF_EXT)
            return;                                 /* (the panel has it) */
        punch.keybit = PF_EXT;
        punch.req = (int8_t)fx;
    } else if (punch.keybit == PF_EXT) {
        punch.keybit = 0;
        punch.req = -1;
    }
}

/* SAVE + key w: the panel's own code (sections A..D: w 0..3, store 4..7, song mode 12, SONG REC 13);
 * SAVE is not held, so a section is one jump (any chain stops), never a chain tap */
static void pf_song_key(uint32_t w)
{
    layer_key(LY_SONG, key_of_white(w), 1);
    layer_key(LY_SONG, key_of_white(w), 0);
}

#if FELUCCA_ARRANGER
/* the quick chain, as SAVE held + n section taps let go (ui_layers.c chain_release): the first is asked
 * for at once (the next bar), then each plays its pattern's bars, looped; rc 1: not playing, 2: a section
 * is empty, 3: the song plays */
static uint32_t pf_chain(const uint8_t *s, uint32_t n)
{
    uint32_t i, ready = arrangement_ready();
    if (n > CHAIN_MAX)
        n = CHAIN_MAX;
    if (!song.playing)
        return 1;
    if (arrangement_clock.running)
        return 3;
    for (i = 0; i < n; i++)
        if (!((ready >> (s[i] & 3u)) & 1u))
            return 2;
    fm1_irq_off();
    chain_n = 0;
    if (n) {
        live_req = (int8_t)(s[0] & 3u);
        for (i = 0; i < n; i++)
            chain_sec[i] = (uint8_t)(s[i] & 3u);
        chain_i = 0;
        chain_bars = (uint8_t)section_bars(s[0] & 3u);
        chain_n = (uint8_t)(n >= 2u ? n : 0u);      /* (one: a plain jump) */
    }
    fm1_irq_on();
    return 0;
}
#endif

/* PLAY / STOP as the PLAY button: 0 toggle, 1 play, 2 stop; rc 1: song mode with an empty section */
static uint32_t pf_transport(uint32_t v)
{
    uint32_t play = v == 0u ? !song.playing : v == 1u;
    if (play == (song.playing != 0))
        return 0;
#if FELUCCA_ARRANGER
    if (play && arrangement_enabled && !arr_valid(&arrangement, arrangement_ready())) {
        ui_message("EMPTY SECTION: REC");
        return 1;
    }
#endif
    transport_req = play ? 1 : 2;
    return 0;
}

/* the state a remote shows: see web/EDITOR_PROTOCOL.md v11 */
static void pf_state(void)
{
    uint32_t i, mute = 0, flags;
    for (i = 0; i < NTRK; i++)
        mute |= (uint32_t)(trk[i].p[P_MUTE] != 0) << i;
    flags = (song.playing != 0) | (uint32_t)(fill_held != 0) << 3 | (uint32_t)(fill_arm != 0) << 4 |
            (uint32_t)(punch.keybit == PF_EXT) << 5 | (uint32_t)(rec_wait != 0 || ci_on) << 6;
#if FELUCCA_ARRANGER
    flags |= (uint32_t)(arrangement_enabled != 0) << 1 | (uint32_t)(arrangement_clock.running != 0) << 2;
#endif
    ed_b(flags);
    ed_v(song.g[G_BPM]);
    ed_b(clk_beat & 127u);                          /* beats since PLAY or the section's bar 0 (wraps) */
    ed_b(punch.req >= 0 ? (uint32_t)punch.req : PF_NONE);
    ed_b(mute);
    ed_b(song.solo & 15u);
    ed_b(song.rec & 15u);
#if FELUCCA_ARRANGER
    ed_b(arrangement_ready() & 15u);
    ed_b(live_sec >= 0 ? (uint32_t)live_sec : PF_NONE);
    ed_b(live_req >= 0 ? (uint32_t)live_req : PF_NONE);
    ed_b(srec);
    ed_b(chain_n);
    ed_b(chain_i);
    for (i = 0; i < chain_n && i < CHAIN_MAX; i++)
        ed_b(chain_sec[i]);
#else
    ed_b(0);
    ed_b(PF_NONE);
    ed_b(PF_NONE);
    ed_b(0);
    ed_b(0);
    ed_b(0);
#endif
    ed_b(song.sel % NTRK);                          /* the selected track (a remote's sound page follows it) */
    for (i = 0; i < 4u; i++) {                      /* each section's length in bars (0: empty), as a chain plays it */
#if FELUCCA_ARRANGER
        uint32_t b = (arrangement_ready() >> i) & 1u ? section_bars(i) : 0u;
        ed_b(b > 127u ? 127u : b);
#else
        ed_b(0);
#endif
    }
}

/* PERFORM: op, args -> op, rc, then the state (every op, so a remote follows with one request) */
static void pf_handle(const uint8_t *a, uint32_t na)
{
    uint32_t op = na ? a[0] : PF_STATE, rc = 0;
    const uint8_t *x = a + 1;
    uint32_t nx = na ? na - 1u : 0u;
    switch (op) {
    case PF_STATE:
        break;
    case PF_FX:                                     /* fx 0..15, 127 = off */
        if (nx < 1u) { rc = 1; break; }
        pf_fx(x[0]);
        break;
    case PF_SECTION:                                /* section 0..3: stopped, loaded; playing, the next bar */
    case PF_STORE:                                  /* the loop into section 0..3 (over a used one: twice in 3 s) */
        if (nx < 1u || x[0] > 3u) { rc = 1; break; }
        pf_song_key(x[0] + (op == PF_STORE ? 4u : 0u));
        break;
#if FELUCCA_ARRANGER
    case PF_CHAIN:                                  /* n, n x section */
        if (nx < 1u || nx - 1u < x[0]) { rc = 1; break; }
        rc = pf_chain(x + 1, x[0]);
        break;
    case PF_SONGMODE:                               /* 0 loop, 1 song, 2 toggle */
        if (nx < 1u) { rc = 1; break; }
        if (pf_tri(arrangement_enabled, x[0]) != arrangement_enabled)
            pf_song_key(12);
        break;
    case PF_SONGREC:                                /* toggle, as SAVE + key 14 */
        pf_song_key(13);
        break;
#endif
    case PF_MUTE:                                   /* track, 0/1/2 */
        if (nx < 2u || x[0] >= NTRK) { rc = 1; break; }
        trk[x[0]].p[P_MUTE] = (int16_t)pf_tri(trk[x[0]].p[P_MUTE] != 0, x[1]);
        ui.force = 1;
        break;
    case PF_SOLO:                                   /* track, 0/1/2 */
        if (nx < 2u || x[0] >= NTRK) { rc = 1; break; }
        if (pf_tri((song.solo >> x[0]) & 1u, x[1]))
            song.solo |= (uint8_t)(1u << x[0]);
        else
            song.solo &= (uint8_t)~(1u << x[0]);
        ui.force = 1;
        break;
    case PF_TRANSPORT:                              /* 0 toggle, 1 play, 2 stop */
        rc = pf_transport(nx ? x[0] : 0u);
        break;
    case PF_FILL:                                   /* 0 let go, 1 hold, 2 the next bar (again: off) */
        if (nx < 1u) { rc = 1; break; }
        if (x[0] == 2u) {
            fill_arm = (uint8_t)!fill_arm;
            ui_message(fill_arm ? "FILL: NEXT BAR" : "FILL BAR OFF");
        } else {
            fill_held = (uint8_t)(x[0] == 1u);
            pf_fill_ext = fill_held;
        }
        break;
    case PF_TAP:
        tap_tempo();
        break;
    default:
        rc = 1;
        break;
    }
    ed_b(op & 127u);
    ed_b(rc);
    pf_state();
}

/* main loop: a remote hold ends when the remote goes quiet */
static void pf_watchdog(uint32_t last_ms)
{
    if (fm1_ms - last_ms < PF_QUIET_MS)
        return;
    if (punch.keybit == PF_EXT)
        pf_fx(PF_NONE);
    if (pf_fill_ext) {
        pf_fill_ext = 0;
        fill_held = 0;
    }
}
