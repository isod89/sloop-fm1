/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.0 sequencer: the transport clock and what plays on it.
 *   timing   the kick and the click stay together for 64 bars at any tempo (no drift); a tempo or DIV
 *            change mid-step plays one step, not a burst; LEN changes keep the track in phase; an odd
 *            LEN with SWING stays in phase with a 16-step track
 *   ratchet  x2 / x3 / x4 hits evenly in their step (drums and synth)
 *   roll     ARP + key: hits on the grid at G_ROLL; recorded as ratchets (1/32 -> x2 on 1/16 steps)
 *   erase    EDIT + key: the lane goes as the playhead passes (playing), all of it (stopped); undo / redo
 *   levels   OCT- / OCT+ held on the drum track: ghost / hard hits, recorded as such
 *   chords   P_CHORD: one key plays the chord of the scale (white keys walk the degrees from C4)
 *   mute     P_MUTE / solo: no new notes, the output fades
 *   nudge    (2.4) a step fires 1/64ths of a step early or late; crossing nudges keep the order
 *   locks    (2.4) a parameter lock sets p[] for its step, the base returns at the next one
 *   fill     (2.4) FILL ONLY / NO FILL steps follow GLO + key 9 (held) and key 10 (the next bar); STOP clears
 *   chain    (2.4) SAVE + two section taps: the sections in turn, each for its pattern's bars, looped
 * Exit status: the number of failed checks. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"

static uint64_t blk;
typedef struct { uint64_t blk; uint8_t note, vel; } hit_t;
static hit_t hits[200000];
static uint32_t nhits;
static void run_block(void)
{
    int32_t out[CTL * 2];
    uint32_t a = drums.age, k, c = click_n;
    mix_block(out, CTL);
    if (click_n != c && nhits < 200000u) {             /* (2.4.1: the click is its own voice, drums.c click_on;
                                                        * logged as the GM wood blocks it used to play) */
        hits[nhits].blk = blk;
        hits[nhits].note = click_acc ? 77u : 76u;
        hits[nhits].vel = click_acc ? 120u : 72u;
        nhits++;
    }
    if (drums.age != a)
        for (k = 0; k < NDRUM; k++)
            if (drums.v[k].age > a && nhits < 200000u) {
                hits[nhits].blk = blk;
                hits[nhits].note = drums.v[k].note;
                hits[nhits].vel = drums.v[k].vel;
                nhits++;
            }
    blk++;
}
static void reset(uint32_t bpm)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NTRK; i++)
        steps_clear(&trk[i]);
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    nhits = 0;
    blk = 0;
    song.g[G_BPM] = (int16_t)bpm;
    song.rec = 0;
    rec_wait = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    kb_prev = 0;
    ly_bit[LY_ROLL] = 1u << 8;                       /* (the buttons: any bits) */
    ly_bit[LY_ERASE] = 1u << 4;
    dyn_bit[0] = 1u << 0;
    dyn_bit[1] = 1u << 1;
}
static int fails;
static void check(int ok, const char *what)
{
    printf("seq2: %-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t count_note(uint32_t note, uint64_t b0, uint64_t b1)
{
    uint32_t i, n = 0;
    for (i = 0; i < nhits; i++)
        n += hits[i].note == note && hits[i].blk >= b0 && hits[i].blk < b1;
    return n;
}

static void t_drift(void)
{
    static const uint32_t BPM[5] = {87, 90, 120, 128, 174};
    uint32_t bi, worst = 0;
    for (bi = 0; bi < 5u; bi++) {
        uint32_t i, nk = 0, nc = 0, bad = 0;
        uint64_t kb[80], cb[80];
        reset(BPM[bi]);
        song.g[G_CLOCK] = 2;                         /* the click on: its downbeat (77) */
        dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);  /* a kick on the 1 */
        transport_req = 1;
        while (blk < (uint64_t)(FS * 60.0 / BPM[bi] * 4 * 66 / CTL))
            run_block();
        for (i = 0; i < nhits; i++) {
            if (hits[i].note == 36 && nk < 80u)
                kb[nk++] = hits[i].blk;
            if (hits[i].note == 77 && nc < 80u)
                cb[nc++] = hits[i].blk;
        }
        for (i = 0; i < nk && i < nc && i < 64u; i++) {
            uint32_t d = kb[i] > cb[i] ? (uint32_t)(kb[i] - cb[i]) : (uint32_t)(cb[i] - kb[i]);
            bad += d != 0u;
            worst = d > worst ? d : worst;
        }
        if (bad || nk < 64u || nc < 64u)
            printf("seq2:   %u BPM: %u kicks, %u clicks, %u apart\n", BPM[bi], nk, nc, bad);
        worst += (nk < 64u || nc < 64u) * 1000u;
    }
    check(worst == 0u, "64 bars at 87..174 BPM: every kick in the block of its downbeat click");
}

static void t_burst(void)
{
    uint32_t i, n0;
    int ok1, ok2;
    reset(90);
    TDRUM->p[P_SDIV] = 0;                            /* 1/4 */
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    while (clk_beat < 2u || clk_pos < BEAT_U * 85u / 100u)
        run_block();
    TDRUM->p[P_SDIV] = 3;                            /* DIV 1/4 -> 1/32 at 85 % of a step */
    n0 = nhits;
    run_block();
    ok1 = nhits - n0 <= 1u;
    reset(40);
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    while (clk_beat < 1u || clk_pos < BEAT_U / 4u * 3u / 4u)
        run_block();
    song.g[G_BPM] = 240;                             /* 40 -> 240 in the middle of a step */
    n0 = nhits;
    run_block();
    ok2 = nhits - n0 <= 1u;
    check(ok1 && ok2, "DIV 1/4 -> 1/32 and 40 -> 240 BPM mid-step: one step a block, no burst");
}

static void t_swing_odd(void)
{
    uint32_t i, ok = 1, starts0 = 0, startsd = 0;
    uint64_t at0[64], atd[64];
    reset(120);
    song.g[G_SWING] = 50;
    trk[0].p[P_SLEN] = 3;
    TDRUM->p[P_SLEN] = 16;
    transport_req = 1;
    for (i = 0; i < 20u * FS / CTL; i++) {
        uint32_t b0 = trk[0].seq_idx, bd = TDRUM->seq_idx;
        uint32_t f0 = trk[0].seq_abs, fd = TDRUM->seq_abs;
        run_block();
        if (trk[0].seq_abs != f0 && trk[0].seq_idx == 0u && starts0 < 64u)
            at0[starts0++] = blk - 1u;
        if (TDRUM->seq_abs != fd && TDRUM->seq_idx == 0u && startsd < 64u)
            atd[startsd++] = blk - 1u;
        (void)b0;
        (void)bd;
    }
    /* step 48 = cycle 16 of 3 = bar 3 of 16; step 144 = cycle 48 = bar 9 */
    ok = starts0 > 48u && startsd > 9u && at0[16] == atd[3] && at0[48] == atd[9];
    if (!ok)
        printf("seq2:   %u %u starts; step 48: %llu %llu, step 144: %llu %llu\n", starts0, startsd,
               (unsigned long long)at0[16], (unsigned long long)atd[3], (unsigned long long)at0[48], (unsigned long long)atd[9]);
    check(ok, "SWING 75 %, LEN 3 against LEN 16: in phase after 15 bars (no drift)");
}

static void t_len_phase(void)
{
    reset(120);
    TDRUM->p[P_SLEN] = 32;
    trk[0].p[P_SLEN] = 16;
    transport_req = 1;
    while (TDRUM->seq_idx != 20u)
        run_block();
    TDRUM->p[P_SLEN] = 16;                           /* LEN 32 -> 16 at step 20 */
    while (TDRUM->seq_idx == 20u)
        run_block();
    check(TDRUM->seq_idx == trk[0].seq_idx, "LEN 32 -> 16 while playing: in phase with a 16-step track");
}

static void t_ratchet(void)
{
    uint32_t i, n = 0, gaps_ok = 1;
    uint64_t b[8];
    reset(120);
    dstep_set(&TDRUM->dstep[0], 4, LV_NORM, 3);      /* a hat x4 on step 0 */
    dstep_set(&TDRUM->dstep[1], 0, LV_NORM, 0);      /* a kick on step 1 */
    transport_req = 1;
    while (blk < div_samples(2) * 2u / CTL + 2u)
        run_block();
    for (i = 0; i < nhits; i++)
        if (hits[i].note == 42 && n < 8u)
            b[n++] = hits[i].blk;
    for (i = 1; i < n; i++) {
        int32_t g = (int32_t)(b[i] - b[i - 1]), want = (int32_t)(div_samples(2) / 4u / CTL);
        gaps_ok &= g >= want - 1 && g <= want + 1;
    }
    check(n == 4u && gaps_ok && count_note(36, 0, blk) == 1u, "drums: a hat x4 plays 4 even hits in its step, the next step plays");
    reset(120);
    host_preset(&trk[0], 0, 7);                      /* TRAP PLUCK (POLY) */
    put_step(&trk[0], 0, 1, (const uint8_t[]){60}, ST_NOTE, 0);
    trk[0].step[0].rat = 2;                          /* x3 */
    transport_req = 1;
    {
        uint32_t starts = 0, last = vage;
        while (blk < div_samples(2) / CTL) {
            run_block();
            if (vage != last)
                starts += vage - last;
            last = vage;
        }
        check(starts == 3u, "synth: a note x3 starts 3 times in its step");
    }
}

static void t_roll(void)
{
    uint32_t i, k = 6, n;                           /* key 6 = B3 -> lane 3 (clap) */
    reset(120);
    song.g[G_ROLL] = 2;                              /* 1/32 */
    song.sel = TRK_DRUM;
    song.rec = 1u << TRK_DRUM;
    transport_req = 1;
    run_block();
    fm1_in.buttons = ly_bit[LY_ROLL];                /* ARP held */
    fm1_in.notes = 1u << k;
    for (i = 0; i < div_samples(2) * 8u / CTL; i++)  /* 8 steps */
        run_block();
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    run_block();
    n = count_note(LANE_NOTE[3], 0, blk);
    {
        uint32_t s, rat_ok = 1, on = 0;
        for (s = 1; s < 7u; s++) {
            on += dstep_has(&TDRUM->dstep[s], 3);
            rat_ok &= !dstep_has(&TDRUM->dstep[s], 3) || dstep_rat(&TDRUM->dstep[s], 3) == 1u;
        }
        if (n < 15u || n > 17u)
            printf("seq2:   roll: %u hits\n", n);
        check(n >= 15u && n <= 17u, "roll 1/32 held 8 steps: ~16 hits");
        check(on >= 6u && rat_ok, "roll 1/32 recorded: one hit a step, ratchet x2");
    }
}

static void t_erase_undo(void)
{
    uint32_t i, left = 0, kept = 0;
    reset(120);
    for (i = 0; i < 16u; i++) {
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);  /* hats everywhere */
        if (i % 4u == 0u)
            dstep_set(&TDRUM->dstep[i], 0, LV_NORM, 0);
    }
    song.sel = TRK_DRUM;
    transport_req = 1;
    while (TDRUM->seq_idx != 4u)
        run_block();
    fm1_in.buttons = ly_bit[LY_ERASE];               /* EDIT held + the hat key (C4 = key 7) */
    fm1_in.notes = 1u << 7;
    while (TDRUM->seq_idx != 7u)
        run_block();
    for (i = 0; i < 20u; i++)                        /* (into step 7, not 8) */
        run_block();
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    run_block();
    for (i = 0; i < 16u; i++) {
        left += dstep_has(&TDRUM->dstep[i], 4);
        kept += dstep_has(&TDRUM->dstep[i], 0);
    }
    if (left != 12u)
        printf("seq2:   erase: %u hats left, %u kicks; %d %d\n", left, kept, dstep_has(&TDRUM->dstep[5], 4), dstep_has(&TDRUM->dstep[9], 4));
    check(left == 12u && kept == 4u && !dstep_has(&TDRUM->dstep[5], 4) && dstep_has(&TDRUM->dstep[9], 4),
          "EDIT + hat while playing steps 4..7: those hats gone, the rest and the kicks kept");
    {   /* undo (ui.c undo_swap): the hats back */
        step_t tmp[NSTEP];
        memcpy(tmp, TDRUM->step, sizeof tmp);
        memcpy(TDRUM->step, undo.st, sizeof tmp);
        memcpy(undo.st, tmp, sizeof tmp);
    }
    left = 0;
    for (i = 0; i < 16u; i++)
        left += dstep_has(&TDRUM->dstep[i], 4);
    check(left == 16u && undo.trk == TRK_DRUM, "undo: the erased hats are back");
    transport_req = 2;
    run_block();
    fm1_in.buttons = ly_bit[LY_ERASE];               /* stopped: every hat of the pattern */
    fm1_in.notes = 1u << 7;
    run_block();
    fm1_in.notes = fm1_in.buttons = 0;
    run_block();
    left = 0;
    for (i = 0; i < 16u; i++)
        left += dstep_has(&TDRUM->dstep[i], 4);
    check(left == 0u, "EDIT + hat while stopped: every hat gone");
}

static void t_levels(void)
{
    uint32_t i, gh = 0, hd = 0;
    reset(120);
    song.sel = TRK_DRUM;
    song.rec = 1u << TRK_DRUM;
    transport_req = 1;
    run_block();
    fm1_in.buttons = dyn_bit[0];                     /* OCT- held: a ghost snare (A3 = key 4) */
    fm1_in.notes = 1u << 4;
    run_block();
    fm1_in.notes = 0;
    run_block();
    while (TDRUM->seq_idx != 3u)
        run_block();
    fm1_in.buttons = dyn_bit[1];                     /* OCT+ held: a hard kick */
    fm1_in.notes = 1u << 0;
    run_block();
    fm1_in.notes = fm1_in.buttons = 0;
    run_block();
    for (i = 0; i < 16u; i++) {
        if (dstep_has(&TDRUM->dstep[i], 2))
            gh = dstep_lvl(&TDRUM->dstep[i], 2);
        if (dstep_has(&TDRUM->dstep[i], 0))
            hd = dstep_lvl(&TDRUM->dstep[i], 0);
    }
    check(gh == LV_GHOST && hd == LV_HARD, "OCT- / OCT+ held on the drums: a ghost snare and a hard kick recorded");
    {
        uint32_t vg = 0, vh = 0;
        for (i = 0; i < nhits; i++) {
            if (hits[i].note == 38)
                vg = hits[i].vel;
            if (hits[i].note == 36)
                vh = hits[i].vel;
        }
        check(vg == 42u && vh == 127u, "... and played at 42 / 127");
    }
}

static void t_chords(void)
{
    uint8_t c[4];
    uint32_t n;
    reset(120);
    trk[0].p[P_ROOT] = 0;
    trk[0].p[P_SCALE] = 2;                           /* C minor */
    trk[0].p[P_CHORD] = 3;                           /* 9TH: 1 3 7 9 */
    n = chord_notes(&trk[0], kb_map(&trk[0], 7), c); /* C4: i */
    check(n == 4u && c[0] == 60 && c[1] == 63 && c[2] == 70 && c[3] == 74, "chord 9TH on C4 in C minor: C Eb Bb D");
    trk[0].p[P_CHORD] = 1;
    n = chord_notes(&trk[0], kb_map(&trk[0], 9), c); /* D4: ii (dim) */
    check(n == 3u && c[0] == 62 && c[1] == 65 && c[2] == 68, "chord TRIAD on D4 in C minor: D F Ab");
    check(kb_map(&trk[0], 8) == KB_SILENT, "chord mode: the black keys are silent");
}

/* CHORD+ (2.4): the black keys modify the chord of a white key (major / minor, 7th, sus4, 9th, inversion), also
 * while it is held; STRUM spreads a chord's notes (keys and steps); VLEAD voices each chord nearest the last */
static uint32_t gated(const track_t *t, uint8_t *out)   /* the notes held by gated voices, sorted */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            out[n++] = (uint8_t)t->v[i].note;
    sort_notes(out, n);
    return n;
}
static int notes_are(const uint8_t *c, uint32_t n, int a, int b, int d, int e)
{
    int want[4] = {a, b, d, e}, k, m = 0;
    for (k = 0; k < 4; k++)
        if (want[k] >= 0) {
            if ((uint32_t)m >= n || c[m] != want[k])
                return 0;
            m++;
        }
    return (uint32_t)m == n;
}
static void keys(uint32_t mask) { fm1_in.notes = mask; run_block(); run_block(); }
static void t_chordplus(void)
{
    uint8_t c[8];
    uint32_t n, k;
    reset(120);
    song.sel = 0;
    song.solo = 0;
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_MUTE] = 0;
        steps_clear(&trk[k]);
        trk_all_off(&trk[k]);                        /* (what the tests before left sounding) */
    }
    if (song.playing) { transport_req = 2; run_block(); }
    trk[0].p[P_VOICE] = 0;                           /* POLY */
    trk[0].p[P_ROOT] = 0;
    trk[0].p[P_SCALE] = 1;                           /* C major */
    trk[0].p[P_CHORD] = 1;                           /* TRIAD */
    trk[0].p[P_STRUM] = 0;
    trk[0].p[P_VLEAD] = 0;
    trk[0].p[P_AMODE] = 0;
    n = chord_play_notes(&trk[0], 60, 0, c);
    check(notes_are(c, n, 60, 64, 67, -1), "CHORD+: C4, no modifier: C E G");
    n = chord_play_notes(&trk[0], 60, CM_MINOR, c);
    check(notes_are(c, n, 60, 63, 67, -1), "CHORD+: F# held: the third flips, C Eb G");
    n = chord_play_notes(&trk[0], 62, CM_MINOR, c);
    check(notes_are(c, n, 62, 66, 69, -1), "CHORD+: ... and back the other way on a minor chord: D F# A");
    n = chord_play_notes(&trk[0], 60, CM_SEVEN, c);
    check(notes_are(c, n, 60, 64, 67, 71), "CHORD+: G#: the 7th, C E G B");
    n = chord_play_notes(&trk[0], 60, CM_SUS4, c);
    check(notes_are(c, n, 60, 65, 67, -1), "CHORD+: A#: sus4, C F G");
    n = chord_play_notes(&trk[0], 60, CM_NINE, c);
    check(notes_are(c, n, 60, 64, 67, 74), "CHORD+: C#: the 9th, C E G D");
    n = chord_play_notes(&trk[0], 60, CM_SEVEN | CM_NINE, c);
    check(notes_are(c, n, 60, 64, 71, 74), "CHORD+: G# + C#: 7th and 9th, four notes (the 5th gives way)");
    n = chord_play_notes(&trk[0], 60, CM_INV, c);
    check(notes_are(c, n, 64, 67, 72, -1), "CHORD+: D#: an inversion, E G C");
    n = chord_play_notes(&trk[0], 60, CM_MINOR | CM_SEVEN, c);
    check(notes_are(c, n, 60, 63, 67, 71), "CHORD+: modifiers combine (F# + G#: C Eb G B)");
    check(chord_mod_of_key(1) == CM_MINOR && chord_mod_of_key(3) == CM_SEVEN && chord_mod_of_key(5) == CM_SUS4 &&
          chord_mod_of_key(8) == CM_NINE && chord_mod_of_key(10) == CM_INV && chord_mod_of_key(13) == CM_MINOR &&
          !chord_mod_of_key(7), "CHORD+: the black keys F# G# A# C# D# (both octaves), white keys none");
    /* the keys: F#3 held, then C4: C minor; F#3 let go while C4 is held: the third goes back up, C and G ring on */
    keys(1u << 1);
    keys(1u << 1 | 1u << 7);
    n = gated(&trk[0], c);
    check(notes_are(c, n, 60, 63, 67, -1), "CHORD+ keys: F#3 held, C4: C Eb G sound");
    keys(1u << 7);
    n = gated(&trk[0], c);
    check(notes_are(c, n, 60, 64, 67, -1), "CHORD+ keys: F#3 let go with C4 held: C E G (the chord changes under the finger)");
    keys(1u << 7 | 1u << 3);
    n = gated(&trk[0], c);
    check(notes_are(c, n, 60, 64, 67, 71), "CHORD+ keys: G#3 pressed with C4 held: the 7th joins");
    keys(0);
    for (k = 0; k < 20u; k++) run_block();
    check(gated(&trk[0], c) == 0u, "CHORD+ keys: all let go: nothing held");
    check(kb_map(&trk[0], 1) == KB_SILENT, "CHORD+: a modifier key plays no note of its own");
    /* STRUM 20 ms down: the notes start low to high, 20 ms apart */
    trk[0].p[P_STRUM] = 20;
    fm1_in.notes = 1u << 7; run_block();
    n = gated(&trk[0], c);
    check(n == 1u && c[0] == 60, "STRUM 20: the lowest note at once");
    for (k = 0; k < (uint32_t)(0.020 * FS / CTL) + 1u; k++) run_block();
    n = gated(&trk[0], c);
    check(n == 2u && c[1] == 64, "STRUM 20: the next one 20 ms later");
    for (k = 0; k < (uint32_t)(0.020 * FS / CTL) + 1u; k++) run_block();
    check(gated(&trk[0], c) == 3u, "STRUM 20: the third 20 ms after that");
    keys(0);
    for (k = 0; k < 20u; k++) run_block();
    trk[0].p[P_STRUM] = -20;                          /* up: from the highest */
    fm1_in.notes = 1u << 7; run_block();
    n = gated(&trk[0], c);
    check(n == 1u && c[0] == 67, "STRUM -20 (up): the highest note first");
    fm1_in.notes = 0; run_block();
    for (k = 0; k < 80u; k++) run_block();
    check(gated(&trk[0], c) == 0u, "STRUM: a key let go before its notes start: they never start (no hanging note)");
    trk[0].p[P_STRUM] = 0;
    /* VLEAD: C then F, the F voiced nearest the C: C F A, not F A C above */
    trk[0].p[P_VLEAD] = 1;
    vl_n[0] = 0;
    n = chord_play_notes(&trk[0], 60, 0, c);
    n = chord_play_notes(&trk[0], 65, 0, c);
    check(notes_are(c, n, 60, 65, 69, -1), "VLEAD: C E G then F: C F A (the nearest inversion)");
    n = chord_play_notes(&trk[0], 67, 0, c);
    check(notes_are(c, n, 59, 62, 67, -1) || notes_are(c, n, 62, 67, 71, -1), "VLEAD: ... then G: B D G (or D G B), no jump up");
    trk[0].p[P_VLEAD] = 0;
    /* the sequencer: a chord step with STRUM 15 starts its notes one after the other */
    steps_clear(&trk[0]);
    trk[0].step[0].n = 3; trk[0].step[0].note[0] = 60; trk[0].step[0].note[1] = 64; trk[0].step[0].note[2] = 67;
    trk[0].step[0].time = ST_NOTE; trk[0].step[0].vel = 100;
    trk[0].p[P_STRUM] = 15;
    trk[0].seq_active = 1;
    transport_req = 1;
    for (k = 0; k < 3u; k++) run_block();
    n = gated(&trk[0], c);
    check(n >= 1u && n < 3u && c[0] == 60, "STRUM on a chord step: the low note first, the others after");
    for (k = 0; k < (uint32_t)(0.040 * FS / CTL); k++) run_block();
    check(gated(&trk[0], c) == 3u, "STRUM on a chord step: all three within 30 ms");
    transport_req = 2; run_block(); run_block();
    for (k = 0; k < 40u; k++) run_block();
    check(gated(&trk[0], c) == 0u, "STOP: no strummed note left waiting");
    trk[0].p[P_STRUM] = 0; trk[0].p[P_CHORD] = 0;
    steps_clear(&trk[0]);
    fm1_in.notes = 0;
}

static void t_mute(void)
{
    uint32_t i, n0;
    reset(120);
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 0, LV_NORM, 0);
    transport_req = 1;
    while (blk < 200u)
        run_block();
    TDRUM->p[P_MUTE] = 1;
    n0 = nhits;
    while (blk < 400u)
        run_block();
    check(nhits == n0, "drum track muted: no hits");
    TDRUM->p[P_MUTE] = 0;
    song.solo = 1u;                                  /* track 1 soloed: the drums silent */
    n0 = nhits;
    while (blk < 600u)
        run_block();
    check(nhits == n0 && TDRUM->att == 32767, "track 1 soloed: the drums silent (faded out)");
    song.solo = 0;
    while (blk < 800u)
        run_block();
    check(nhits > n0 && TDRUM->att == 0, "solo off: the drums back");
}

/* overload shedding (voice.c shed_voice): a releasing voice first, then the oldest held voice that is
 * neither a POLY part's lowest note nor a MONO part's lead; faded (stage 4), never cut */
static void t_shed(void)
{
    track_t *t = &trk[0], *m = &trk[1];
    uint32_t i, k, low_ok = 1, lead_ok = 1;
    reset(120);
    t->p[P_VOICE] = V_POLY;
    m->p[P_VOICE] = V_MONO;
    t->p[P_ATK] = 0;
    t->p[P_REL] = 100;                                /* a long release: the released voice still rings */
    trk_note_on(t, 48, 100);                          /* the bass, first and lowest */
    trk_note_on(t, 64, 100);
    trk_note_on(t, 67, 100);
    trk_note_on(t, 72, 100);
    trk_note_on(m, 40, 100);                          /* a MONO lead */
    trk_note_on(t, 76, 100);
    for (k = 0; k < 20u; k++)
        run_block();
    trk_note_off(t, 76);                              /* releasing */
    run_block();
    shed_voice();
    for (k = 0, i = 0; i < NVOICE; i++)
        k += t->v[i].note == 76 && t->v[i].stage == 4u;
    check(k == 1u, "overload: the releasing voice goes first, faded (stage 4)");
    for (k = 0; k < 6u; k++)
        shed_voice();
    for (i = 0; i < NVOICE; i++) {
        if (t->v[i].note == 48 && t->v[i].active && t->v[i].stage == 4u)
            low_ok = 0;
        if (m->v[i].note == 40 && i == 0u && m->v[i].stage == 4u)
            lead_ok = 0;
    }
    for (k = 0, i = 0; i < NVOICE; i++)
        k += t->v[i].active && t->v[i].gate && (t->v[i].note == 64 || t->v[i].note == 67 || t->v[i].note == 72);
    check(low_ok && lead_ok && k == 0u, "overload: the upper notes thin out, the bass and the MONO lead stay");
}

/* MIDI clock in (SYNC USB / TRS): START, 24 pulses a beat, tempo changes, STOP; the other source ignored */
static void mclk_push(uint32_t pkt) { midi_in_q[mi_w % MQ] = pkt; mi_w++; }
static void mclk_run(double *t, double *next, double per, double until, uint32_t src)
{
    while (*t < until) {
        while (*next <= *t) {
            mclk_push(0xF80Fu | src << 4);
            *next += per;
        }
        fm1_ms = (uint32_t)*t;
        run_block();
        *t += (double)CTL * 1000.0 / FS;
    }
}
static void t_mclk(void)
{
    double t = 0, next = 0, per = 60000.0 / 120 / 24;
    reset(90);                                        /* the internal tempo: 90 */
    mi_r = mi_w;
    song.g[G_SYNC] = 1;                               /* USB */
    mclk_run(&t, &next, per, 700, 0);                 /* the clock runs before START (the tempo shows) */
    check(!song.playing && song.g[G_BPM] == 120, "MIDI clock: stopped, BPM follows the master (120)");
    mclk_push(0xFA0Fu);                               /* START, then the downbeat pulse */
    next = t;
    mclk_run(&t, &next, per, t + 4000.0 + per / 2, 0);   /* two bars of 120 */
    check(song.playing && clk_beat == 8u, "MIDI clock: START, 2 bars at 120 -> beat 8 exactly");
    per = 60000.0 / 100 / 24;                          /* the master slows to 100 */
    mclk_run(&t, &next, per, t + 4800.0, 0);
    check(clk_beat == 16u && song.g[G_BPM] == 100, "MIDI clock: the master at 100 -> 2 more bars, BPM 100");
    mclk_push(0xFC1Fu);                               /* STOP from the TRS jack: not the source */
    mclk_run(&t, &next, per, t + 50.0, 0);
    check(song.playing, "MIDI clock: SYNC USB ignores the TRS jack");
    mclk_push(0xFC0Fu);
    mclk_run(&t, &next, per, t + 10.0, 0);
    check(!song.playing, "MIDI clock: STOP");
    t += 1000.0;                                      /* the master is gone: PLAY on the FM-1 plays */
    fm1_ms = (uint32_t)t;
    transport_req = 1;
    run_block();
    {
        uint32_t b0 = clk_beat, k;
        for (k = 0; k < (uint32_t)(FS / CTL); k++)
            run_block();
        check(song.playing && clk_beat >= b0 + 1u, "MIDI clock: no pulse for 0.5 s -> the internal tempo plays");
    }
    transport_req = 2;
    run_block();
    song.g[G_SYNC] = 0;
    mi_r = mi_w;
}

/* the REC screen's MODE and START (SLOOP 2.3): an empty project records at the tempo set (TEMPO) or
 * takes it from the playing (FREE); COUNT: PLAY clicks one bar, then the loop and the recording start */
static void t_recmode(void)
{
    uint32_t k, bpb = (uint32_t)((double)FS * 60.0 / 100.0 / CTL + 0.5), c0;
    reset(100);
    song.sel = 0;
    song.playing = 0;
    rec_tempo = 0, rec_count = 0;
    rec_wait = 1;
    input_on(TSEL, 60, 100);
    run_block();
    check(ft_on && !song.playing, "REC mode FREE, empty project: the first note starts a free take");
    transport_req = 2; run_block(); ft_bars = 0;
    for (k = 0; k < 4u; k++) trk_note_off(&trk[k % NPART], 60);

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 0;
    rec_wait = 1;
    input_on(TSEL, 60, 100);
    run_block();
    check(!ft_on && song.playing && song.rec == 1u && song.g[G_BPM] == 100,
          "REC mode TEMPO, empty project: the first note starts the loop at 100 BPM, recording");
    transport_req = 2; run_block();

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 1;
    rec_wait = 1;
    input_on(TSEL, 62, 100);
    run_block();
    check(!song.playing && !ci_on && !ft_on && rec_wait, "REC START COUNT: a note only sounds, nothing starts");
    c0 = nhits;
    transport_req = 1;                                /* PLAY: the count-in */
    run_block();
    for (k = 0; k + 2u < 4u * bpb; k++) run_block();
    check(ci_on && !song.playing, "REC START COUNT: PLAY -> one bar of clicks, not playing yet");
    {
        uint32_t n = 0, i;
        for (i = c0; i < nhits; i++) n += hits[i].note == 76 || hits[i].note == 77;
        check(n == 4u, "REC START COUNT: 4 clicks (one bar of 4/4)");
    }
    for (k = 0; k < 4u; k++) run_block();
    check(!ci_on && song.playing && song.rec == 1u && !rec_wait,
          "REC START COUNT: after the bar the loop starts and records");
    transport_req = 2; run_block();

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 1;
    rec_wait = 1;
    transport_req = 1; run_block();
    for (k = 0; k < bpb; k++) run_block();
    rec_wait = 0;                                     /* REC again: cancelled */
    for (k = 0; k < 4u * bpb; k++) run_block();
    check(!ci_on && !song.playing && !song.rec, "REC START COUNT: REC during the count-in cancels it");
    rec_wait = 1;
    transport_req = 1; run_block();
    transport_req = 1; run_block();                   /* PLAY again: back to armed */
    check(!ci_on && rec_wait && !song.playing, "REC START COUNT: PLAY again during the count-in: back to armed");
    rec_wait = 0;
    rec_tempo = 0, rec_count = 0;
}

/* SLOOP 2.4 fixes: a MIDI START cuts a count-in short (the master counts); swing is off on the
 * triplet grids; a DIV change in the first beat after PLAY does not lose a step */
static void t_fixes24(void)
{
    uint32_t k, i, bpb = (uint32_t)((double)FS * 60.0 / 100.0 / CTL + 0.5), n0;
    /* MIDI START during the count-in */
    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 1;
    rec_wait = 1;
    song.g[G_SYNC] = 1;
    mi_r = mi_w;
    transport_req = 1; run_block();                   /* PLAY: the count-in clicks */
    for (k = 0; k < bpb; k++) run_block();
    check(ci_on && !song.playing, "2.4: count-in running before the master starts");
    mclk_push(0xFA0Fu);                               /* the DAW starts: START */
    run_block();
    run_block();
    check(!ci_on && song.playing && song.rec == 1u, "2.4: a MIDI START during the count-in starts and records at once");
    transport_req = 2; run_block();
    song.g[G_SYNC] = 0;
    rec_wait = 0; rec_tempo = 0; rec_count = 0;
    mi_r = mi_w;

    /* swing on 8T: every beat the same */
    reset(120);
    song.g[G_SWING] = 80;
    TDRUM->p[P_SDIV] = 4;                             /* 8T: 3 a beat */
    TDRUM->p[P_SLEN] = 12;
    for (i = 0; i < 12u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    for (k = 0; k < 8u * (uint32_t)(FS / CTL) / 2u; k++) run_block();   /* 4 s = 8 beats */
    {
        uint64_t d[24]; uint32_t n = 0, ok = 1;
        for (i = 0; i < nhits && n < 24u; i++)
            if (hits[i].note == 36 || hits[i].note == 35 || hits[i].vel) d[n++] = hits[i].blk;
        /* 3 hits a beat, evenly spaced: consecutive gaps within one block of each other */
        for (i = 2; i < n; i++)
            if (d[i] - d[i - 1] > d[i - 1] - d[i - 2] + 1u || d[i - 1] - d[i - 2] > d[i] - d[i - 1] + 1u) ok = 0;
        check(n >= 20u && ok, "2.4: SWING on the 8T grid does nothing (triplets stay even)");
    }
    song.g[G_SWING] = 0;
    transport_req = 2; run_block();

    /* DIV 1/8 -> 1/4 in the first beat: beat 2 still plays */
    reset(120);
    TDRUM->p[P_SDIV] = 1;                             /* 1/8 */
    TDRUM->p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    run_block();                                      /* (the clock is at 0 from here) */
    while (clk_beat < 1u && clk_pos < BEAT_U * 6u / 10u) run_block();   /* 60 % into beat 1: abs 1 of 1/8 */
    TDRUM->p[P_SDIV] = 0;                             /* 1/4 */
    n0 = nhits;
    while (clk_beat < 1u) run_block();                /* to the start of beat 2 */
    for (k = 0; k < 3u; k++) run_block();
    check(nhits > n0, "2.4: DIV 1/8 -> 1/4 in the first beat: the step on beat 2 plays");
    transport_req = 2; run_block();
}

/* SLOOP 2.4: steps of whole beats (DIV 1/2, 1BAR, 2BAR) and the dotted delay times */
static void t_longdiv(void)
{
    uint32_t i, k, n0, bpb = (uint32_t)((double)FS * 60.0 / 120.0 / CTL + 0.5);
    reset(120);
    TDRUM->p[P_SDIV] = 8;                             /* 2BAR: a step every 8 beats */
    TDRUM->p[P_SLEN] = 2;
    dstep_set(&TDRUM->dstep[0], 4, LV_NORM, 0);
    dstep_set(&TDRUM->dstep[1], 5, LV_NORM, 0);
    transport_req = 1;
    run_block();
    n0 = nhits;
    for (k = 0; k < 16u * bpb + 4u; k++) run_block();   /* 16 beats: steps at beats 0 and 8, step 0 again at 16 */
    {
        uint32_t a = 0, b = 0;
        for (i = 0; i < nhits; i++) a += hits[i].note == 36 || hits[i].note == 35, b += hits[i].note == 38 || hits[i].note == 37;
        check(nhits - n0 + 1u == 3u && clk_beat == 16u, "2.4: DIV 2BAR: one step every 8 beats (3 hits in 16 beats)");
        (void)a; (void)b;
    }
    transport_req = 2; run_block();
    reset(120);
    TDRUM->p[P_SDIV] = 6;                             /* 1/2: every 2 beats */
    TDRUM->p[P_SLEN] = 4;
    for (i = 0; i < 4u; i++) dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    run_block();
    n0 = nhits;
    for (k = 0; k < 8u * bpb + 4u; k++) run_block();
    check(nhits - n0 + 1u == 5u, "2.4: DIV 1/2: one step every 2 beats (5 hits in 8 beats)");
    transport_req = 2; run_block();
    song.g[G_BPM] = 120;
    check(dly_samples(6) == (uint32_t)FS * 60u / 120u * 3u / 4u && dly_samples(7) == (uint32_t)FS * 60u / 120u * 3u / 8u,
          "2.4: delay TIME 1/8D = 3/4 beat, 1/16D = 3/8 beat");
    check(div_units(7) == BEAT_U * 4u && div_units(8) == BEAT_U * 8u && div_units(2) == BEAT_U / 4u,
          "2.4: div_units: 1BAR = 4 beats, 2BAR = 8, 1/16 = a quarter beat");
}

/* SLOOP 2.4: GLO > SYSTEM > MIDI = SEQ sends what the sequencer plays to MIDI OUT (note on, then off),
 * with the keys' channels; KEYS (the default) sends nothing of it; STOP ends every note sent */
static void mo_drain(uint32_t *on, uint32_t *off, uint32_t *onDrum, uint32_t *last_note)
{
    while (mo_r != mo_w) {
        uint32_t p = midi_out_q[mo_r % MQ], st = (p >> 8) & 0xF0u, ch = (p >> 8) & 15u;
        mo_r++;
        if (st == 0x90u && (p >> 24)) { (*on)++; if (ch == 9u) (*onDrum)++; else if (ch == 0u) *last_note = (p >> 16) & 127u; }
        else if (st == 0x80u || st == 0x90u) (*off)++;
    }
}
static void t_midiout(void)
{
    uint32_t k, i, on = 0, off = 0, od = 0, ln = 0, bpb = (uint32_t)((double)FS * 60.0 / 120.0 / CTL + 0.5);
    reset(120);
    usb.config = 1;
    mo_r = mo_w;
    song.g[G_MIDI] = 0;
    trk[0].p[P_SDIV] = 0;                             /* 1/4: a note a beat */
    trk[0].p[P_SLEN] = 4;
    for (i = 0; i < 4u; i++) { trk[0].step[i].time = ST_NOTE; trk[0].step[i].n = 1; trk[0].step[i].note[0] = 60 + i; trk[0].step[i].vel = 100; }
    TDRUM->p[P_SDIV] = 0;
    TDRUM->p[P_SLEN] = 4;
    dstep_set(&TDRUM->dstep[0], 4, LV_NORM, 0);
    transport_req = 1;
    for (k = 0; k < 4u * bpb + 2u; k++) run_block();
    mo_drain(&on, &off, &od, &ln);
    check(on == 0 && off == 0, "2.4: MIDI = KEYS: the sequencer sends nothing to MIDI OUT");
    song.g[G_MIDI] = 1;
    for (k = 0; k < 4u * bpb; k++) run_block();
    mo_drain(&on, &off, &od, &ln);
    if (!(on >= 4u && od >= 1u && ln >= 60u && ln < 64u)) printf("seq2:   on %u off %u drum %u last %u\n", on, off, od, ln);
    check(on >= 4u && od >= 1u && ln >= 60u && ln < 64u, "2.4: MIDI = SEQ: the steps go out on their track's channel (drums on 10)");
    transport_req = 2; run_block();
    mo_drain(&on, &off, &od, &ln);
    check(on == off, "2.4: MIDI = SEQ: every note sent on was ended (STOP ends the rest)");
    song.g[G_MIDI] = 0;
    usb.config = 0;
    for (k = 0; k < (uint32_t)(FS / CTL); k++)
        run_block();                                  /* (the released voices die down for the next test) */
}

/* SLOOP 2.4: per-step nudge (micro timing). A step nudged +16 fires a quarter step after its grid time,
 * -16 a quarter step before it (inside the previous grid step); over 8 bars, with two neighbours whose
 * nudges cross, no step is lost or played twice */
static void t_micro(void)
{
    uint32_t i, k, nk = 0, ns = 0, nc = 0, bad = 0, bpb = (uint32_t)((double)FS * 60.0 / 120.0 / CTL + 0.5);
    double sb = (double)FS * 60.0 / 120.0 / 4.0 / CTL, worst = 0;   /* blocks a 1/16 step */
    uint64_t kb[16], sbk[16], cb[16];
    reset(120);
    dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);      /* a kick on step 1: the grid reference */
    dstep_set(&TDRUM->dstep[4], 2, LV_NORM, 0);      /* a snare on step 5, nudged +16 (a quarter step late) */
    dstep_set(&TDRUM->dstep[8], 3, LV_NORM, 0);      /* a clap on step 9, nudged -16 (a quarter step early) */
    TDRUM->micro[4] = 16;
    TDRUM->micro[8] = -16;
    transport_req = 1;
    for (k = 0; k < 8u * 4u * bpb - 10u; k++)        /* 8 bars (short of bar 9's kick) */
        run_block();
    for (i = 0; i < nhits; i++) {
        if (hits[i].note == 36 && nk < 16u) kb[nk++] = hits[i].blk;
        if (hits[i].note == 38 && ns < 16u) sbk[ns++] = hits[i].blk;
        if (hits[i].note == 39 && nc < 16u) cb[nc++] = hits[i].blk;
    }
    for (i = 0; i < 8u && i < nk && i < ns && i < nc; i++) {
        double ds = (double)sbk[i] - (double)kb[i] - 4.25 * sb, dc = (double)cb[i] - (double)kb[i] - 7.75 * sb;
        if (ds < 0) ds = -ds;
        if (dc < 0) dc = -dc;
        worst = ds > worst ? ds : worst;
        worst = dc > worst ? dc : worst;
    }
    if (nk != 8u || ns != 8u || nc != 8u || worst > 1.0)
        printf("seq2:   micro: %u kicks, %u snares, %u claps, worst %.2f blocks\n", nk, ns, nc, worst);
    check(nk == 8u && ns == 8u && nc == 8u && worst <= 1.0, "2.4: nudge +16 fires a quarter step late, -16 a quarter step early (within a block)");
    transport_req = 2; run_block();

    reset(120);
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);  /* a hat on every step, nudged all over the place */
    for (i = 0; i < 16u; i++)
        TDRUM->micro[i] = (int8_t)((int32_t)(i * 23u % 64u) - 32);
    TDRUM->micro[5] = 31;                            /* two neighbours whose nudges cross */
    TDRUM->micro[6] = -32;
    TDRUM->micro[0] = -32;                           /* the first step as early as it goes (across the loop) */
    transport_req = 1;
    {
        uint32_t prev = TDRUM->seq_idx, order_ok = 1, fired = 0;
        for (k = 0; k < 8u * 4u * bpb - 100u; k++) { /* 8 bars, short of bar 9's step 1 (half a step early) */
            run_block();
            if (TDRUM->seq_idx != prev) {            /* the steps in order, each once */
                order_ok &= TDRUM->seq_idx == (prev + 1u) % 16u;
                prev = TDRUM->seq_idx;
            }
        }
        for (i = 0; i < nhits; i++)
            if (hits[i].note == 42)
                fired++;
        if (fired != 128u || !order_ok)
            printf("seq2:   micro: %u hats in 8 bars (want 128), order %s\n", fired, order_ok ? "ok" : "broken");
        bad = fired != 128u || !order_ok;
        check(!bad, "2.4: 16 nudged steps (two crossing) over 8 bars: every step once, in order, none doubled");
    }
    transport_req = 2; run_block();
    for (i = 0; i < 16u; i++) TDRUM->micro[i] = 0;
}

/* SLOOP 2.4: parameter locks. A lock on P_E0 at step 3 sets t->p[P_E0] for that step, the base is back at
 * step 4; a knob turned during the locked step is kept as the new base; STOP restores; a lock on a parameter
 * that is not lockable is refused */
static void t_plock(void)
{
    track_t *t = &trk[0];
    uint32_t i, bpb = (uint32_t)((double)FS * 60.0 / 120.0 / CTL + 0.5), ok1, ok2, ok3, ok4;
    int16_t at2, at3, dist0;
    reset(120);
    host_preset(t, 0, 7);                             /* ANALOG TRAP PLUCK: P_E0 = WAVE (0..4) */
    for (i = 0; i < 16u; i++)
        put_step(t, i, 1, (const uint8_t[]){60}, ST_NOTE, 0);
    t->p[P_E0] = 3;
    dist0 = t->p[P_DIST];
    check(lock_set(t, 2, P_E0, 1) && lock_set(t, 2, P_DIST, 90) && !lock_set(t, 2, P_SLEN, 8) && !lock_set(t, 2, P_AMODE, 1),
          "2.4: lock_set takes P_E0 and P_DIST, refuses P_SLEN and P_AMODE (not lockable)");
    check(lock_set(t, 70, P_E0, 1) == 0 && lock_set(t, 2, P_COUNT, 1) == 0, "2.4: lock_set bounds the step and the parameter");
    transport_req = 1;
    while (t->seq_idx != 2u) run_block();
    at2 = t->p[P_E0];
    ok1 = t->p[P_DIST] == 90;
    while (t->seq_idx != 3u) run_block();
    at3 = t->p[P_E0];
    check(at2 == 1 && at3 == 3 && ok1 && t->p[P_DIST] == dist0, "2.4: a lock on P_E0 / P_DIST at step 3: set for that step, the base back at step 4");
    while (t->seq_idx != 2u) run_block();            /* the next pass: the knob is turned while the lock is on */
    ok2 = t->p[P_E0] == 1;
    t->p[P_E0] = 4;
    while (t->seq_idx != 3u) run_block();
    check(ok2 && t->p[P_E0] == 4, "2.4: a knob turned during the locked step: kept as the new base");
    while (t->seq_idx != 2u) run_block();
    ok3 = t->p[P_E0] == 1;
    transport_req = 2; run_block();
    check(ok3 && t->p[P_E0] == 4 && t->lk_n == 0, "2.4: STOP during the locked step: the base (4) is back");
    /* the lock's value is clamped to the parameter's range; a lock on every step of two params fits NLOCK */
    ok4 = lock_set(t, 5, P_E0, 99) && t->lock[lock_find(t, 5, P_E0, 0)].val == 4;
    lock_del(t, 2, P_COUNT);
    check(ok4 && lock_find(t, 2, P_E0, 0) < 0 && lock_find(t, 2, P_DIST, 0) < 0 && lock_find(t, 5, P_E0, 0) >= 0,
          "2.4: a lock's value is clamped (WAVE 99 -> 4); lock_del clears a step's locks");
    steps_clear(t);
    for (i = 0, ok4 = 1; i < NLOCK; i++) ok4 &= t->lock[i].step == LOCK_FREE;
    check(ok4 && !step_locked(t, 5), "2.4: steps_clear frees every lock and nudge");
    for (i = 0; i < (uint32_t)(FS / CTL); i++) run_block();
}

/* SLOOP 2.4: fill conditions. A FILL ONLY step is silent until GLO + key 9 is held (fill_held) and plays while
 * it is, a NO FILL step the other way round; its ratchets go with it; a synth step too. FILL NEXT BAR (fill_arm)
 * makes exactly the next bar a fill; STOP clears a held and an armed fill */
static uint32_t bar_run(uint32_t to_beat)         /* run to the start of beat to_beat; the first block in it */
{
    while (clk_beat < to_beat)
        run_block();
    return (uint32_t)blk;
}
static void t_fill(void)
{
    uint32_t b[8], i, ok, starts0, starts1;
    track_t *t = &trk[0];
    reset(120);
    dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);      /* a kick on step 1: always */
    dstep_set(&TDRUM->dstep[4], 2, LV_NORM, 3);      /* a snare x4 on step 5: FILL ONLY */
    dstep_set(&TDRUM->dstep[8], 4, LV_NORM, 0);      /* a hat on step 9: NO FILL */
    step_fill_set(TDRUM, 4, FC_FILL);
    step_fill_set(TDRUM, 8, FC_NOFILL);
    step_fill_set(TDRUM, 12, FC_FILL);               /* (an empty step: nothing either way) */
    host_preset(t, 0, 7);                            /* a synth part: C on step 1, E on step 5 FILL ONLY */
    put_step(t, 0, 1, (const uint8_t[]){60}, ST_NOTE, 0);
    put_step(t, 4, 1, (const uint8_t[]){64}, ST_NOTE, 0);
    step_fill_set(t, 4, FC_FILL);
    check(step_fill(TDRUM, 4) == FC_FILL && step_fill(TDRUM, 8) == FC_NOFILL && step_fill(TDRUM, 5) == FC_NORM &&
          step_fill(TDRUM, 70) == step_fill(TDRUM, 6), "2.4: step_fill / step_fill_set: 2 bits a step, the index bounded");
    transport_req = 1;
    b[0] = (uint32_t)blk;
    starts0 = vage;
    run_block();
    b[1] = bar_run(4);                               /* bar 1: no fill */
    starts0 = vage - starts0;
    fill_held = 1;                                   /* GLO + key 9 held through bar 2 */
    starts1 = vage;
    b[2] = bar_run(8);
    starts1 = vage - starts1;
    fill_held = 0;
    b[3] = bar_run(12);                              /* bar 3: no fill again */
    ok = count_note(36, b[0], b[1]) == 1u && count_note(38, b[0], b[1]) == 0u && count_note(42, b[0], b[1]) == 1u;
    check(ok, "2.4: no fill: the kick and the NO FILL hat play, the FILL ONLY snare (x4) is silent");
    ok = count_note(36, b[1], b[2]) == 1u && count_note(38, b[1], b[2]) == 4u && count_note(42, b[1], b[2]) == 0u;
    if (!ok)
        printf("seq2:   fill held: %u kicks, %u snares, %u hats\n", count_note(36, b[1], b[2]), count_note(38, b[1], b[2]), count_note(42, b[1], b[2]));
    check(ok, "2.4: fill held: the snare plays with its 4 ratchet hits, the NO FILL hat is silent");
    check(count_note(38, b[2], b[3]) == 0u && count_note(42, b[2], b[3]) == 1u, "2.4: fill let go: back to normal");
    check(starts0 == 1u && starts1 == 2u, "2.4: a synth FILL ONLY step: one note a bar without the fill, two with it");
    /* FILL NEXT BAR: armed in bar 4, bar 5 is the fill, bar 6 is not */
    bar_run(13);
    fill_arm = 1;
    run_block();
    ok = fill_arm && !fill_bar_on && !fill_now;
    b[4] = bar_run(16);
    run_block();
    ok &= !fill_arm && fill_bar_on && fill_now;
    b[5] = bar_run(20);
    run_block();
    ok &= !fill_bar_on && !fill_now;
    b[6] = bar_run(24);
    check(ok && count_note(38, b[4], b[5]) == 4u && count_note(42, b[4], b[5]) == 0u,
          "2.4: FILL NEXT BAR: the whole next bar is a fill (the snare plays, the hat not)...");
    check(count_note(38, b[5], b[6]) == 0u && count_note(42, b[5], b[6]) == 1u && count_note(38, b[3], b[4]) == 0u,
          "2.4: ... and only that bar");
    fill_arm = 1;                                    /* armed, then cancelled (the key again): no fill bar */
    fill_arm = 0;
    b[6] = bar_run(28);
    b[7] = bar_run(32);
    check(count_note(38, b[6], b[7]) == 0u && count_note(42, b[6], b[7]) == 1u, "2.4: FILL NEXT BAR pressed again before the bar: cancelled");
    fill_held = 1;
    fill_arm = 1;
    transport_req = 2;
    run_block();
    check(!fill_held && !fill_arm && !fill_bar_on && !song.playing, "2.4: STOP clears the held and the armed fill");
    for (i = 0; i < (uint32_t)(FS / CTL); i++)
        run_block();
}

/* SLOOP 2.4: the quick chain (SAVE held + two or more section keys, ui_layers.c chain_release). Sections A (1 bar:
 * 16 steps of 1/16) and B (2 bars: the drum track 32 steps) in proj_slot[0..1]; the chain A B asked for while
 * playing: A on the next bar, B a bar later, A two bars after that, round again; a single tap ends the chain */
static void t_chain(void)
{
    uint32_t i, bar = (uint32_t)((double)FS * 60.0 / 120.0 * 4.0 / CTL + 0.5), t0 = 0, n = 0, ok = 1, bars_a, bars_b;
    struct { uint32_t blk; int8_t sec; } ev[8];
    int8_t last;
    reset(120);
    for (i = 0; i < NTRK; i++) {
        trk[i].p[P_SLEN] = 16;
        trk[i].p[P_SDIV] = 2;
    }
    dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);
    proj_capture(&proj_slot[0]);                     /* A: one bar */
    TDRUM->p[P_SLEN] = 32;
    dstep_set(&TDRUM->dstep[16], 2, LV_NORM, 0);
    proj_capture(&proj_slot[1]);                     /* B: two bars (the drum track) */
    trk[1].p[P_SLEN] = 3;
    trk[1].p[P_SDIV] = 7;                            /* 1BAR x 3: three bars */
    proj_capture(&proj_slot[2]);                     /* C: three bars */
    proj_slot[3].magic = 0;
    bars_a = section_bars(0);
    bars_b = section_bars(1);
    check(bars_a == 1u && bars_b == 2u && section_bars(2) == 3u && section_bars(3) == 1u && arrangement_ready() == 7u,
          "2.4: section_bars: A 1 bar, B 2 bars (32 x 1/16), C 3 bars (3 x 1BAR); an empty slot 1");
    proj_apply(&proj_slot[2], 1);                    /* C is the loop */
    live_sec = 2;
    arrangement_enabled = 0;
    transport_req = 1;
    run_block();
    bar_run(2);                                      /* mid bar 1: SAVE held, A then B tapped, SAVE let go */
    chain_n = 0;
    live_req = 0;                                    /* (the first tap, as ever) */
    chain_sec[0] = 0;
    chain_sec[1] = 1;
    chain_i = 0;
    chain_bars = (uint8_t)bars_a;
    chain_n = 2;
    last = live_sec;
    while (n < 6u && blk < 40000u) {
        run_block();
        if (live_sec != last) {
            last = live_sec;
            ev[n].blk = (uint32_t)blk - 1u;
            ev[n].sec = live_sec;
            n++;
        }
    }
    /* the section changes seen: A (the tap, bar 2) B (bar 3) A (bar 5) B (bar 6) A (bar 8) B (bar 9) */
    ok = n == 6u && ev[0].sec == 0 && ev[1].sec == 1 && ev[2].sec == 0 && ev[3].sec == 1 && ev[4].sec == 0 && ev[5].sec == 1;
    t0 = n ? ev[0].blk : 0;
    for (i = 1; i < n && i < 6u; i++) {
        static const uint32_t AT[6] = {0, 1, 3, 4, 6, 7};
        uint32_t want = t0 + AT[i] * bar, got = ev[i].blk;
        ok &= got + 1u >= want && got <= want + 1u;
    }
    if (!ok) {
        printf("seq2:   chain: %u changes:", n);
        for (i = 0; i < n; i++) printf(" %c@%u", 'A' + ev[i].sec, ev[i].blk);
        printf(" (bar %u blocks)\n", bar);
    }
    check(ok, "2.4: chain A B: A on the next bar, B a bar later, A two bars after that, round again (bar exact)");
    check(chain_n == 2u && chain_i == 1u && chain_bars == 2u, "2.4: the chain state follows: entry 2 (B), 2 bars");
    chain_n = 0;                                     /* a single tap of A: the chain ends, A plays on */
    live_req = 0;
    {
        uint32_t changes = 0;
        last = live_sec;
        for (i = 0; i < 6u * bar; i++) {
            run_block();
            if (live_sec != last) {
                changes++;
                last = live_sec;
            }
        }
        check(changes == 1u && live_sec == 0 && chain_n == 0u, "2.4: a single section tap cancels the chain: A, and it stays");
    }
    transport_req = 2;
    run_block();
    check(!song.playing && live_req < 0 && chain_n == 0u, "2.4: STOP: no chain, no request");
    for (i = 0; i < (uint32_t)(FS / CTL); i++)
        run_block();
    reset(120);
    for (i = 0; i < 4u; i++)
        proj_slot[i].magic = 0;
    live_sec = -1;
}

/* SLOOP 2.4: GLO > SYSTEM > IN = CLOCK: MIDI note-ons are ignored (the clock is not); a note-off still ends a
 * note held when IN was switched */
static void t_midiin(void)
{
    uint32_t k, on = 0;
    reset(120);
    mi_r = mi_w;
    song.g[G_ROUTE] = 0;
    mclk_push(0x643C9009u);                             /* note on C4, vel 100, ch 1 (cable 0, CIN 9) */
    run_block(); run_block();
    for (k = 0; k < NVOICE; k++) on += trk[0].v[k].active && trk[0].v[k].gate;
    check(on == 1u, "2.4: IN = NOTES: a MIDI note plays");
    song.g[G_ROUTE] = 1;                                /* CLOCK only: C4 still held */
    mclk_push(0x643E9009u);                             /* D4 on: ignored */
    run_block(); run_block();
    for (k = 0, on = 0; k < NVOICE; k++) on += trk[0].v[k].active && trk[0].v[k].gate && trk[0].v[k].note == 62;
    check(on == 0u, "2.4: IN = CLOCK: a MIDI note-on is ignored");
    mclk_push(0x003C8008u);                             /* C4 off */
    run_block(); run_block();
    for (k = 0, on = 0; k < NVOICE; k++) on += trk[0].v[k].active && trk[0].v[k].gate;
    check(on == 0u, "2.4: IN = CLOCK: the note-off still ends the note held before");
    {
        double t = 0, next = 0, per = 60000.0 / 120 / 24;
        song.g[G_SYNC] = 1;
        mclk_push(0xFA0Fu);
        mclk_run(&t, &next, per, 2000.0 + per / 2, 0);
        check(song.playing && clk_beat == 4u, "2.4: IN = CLOCK: the clock still runs SLOOP (START, one bar)");
        mclk_push(0xFC0Fu); mclk_run(&t, &next, per, t + 10.0, 0);
        song.g[G_SYNC] = 0;
    }
    song.g[G_ROUTE] = 0;
    mi_r = mi_w;
}

/* SLOOP 2.5: MIDI CCs set track parameters (Felucca 1.1.5's CC map), on the track the channel plays */
static void cc_push(uint32_t ch, uint32_t cc, uint32_t v) { mclk_push(0x0Bu | (0xB0u | ch) << 8 | cc << 16 | v << 24); }
static void t_midicc(void)
{
    track_t *a, *b;
    int16_t before[P_COUNT];
    uint32_t i, res = 99, same = 1, drch;
    reset(120);
    mi_r = mi_w;
    song.g[G_ROUTE] = 0;
    a = &trk[0]; b = &trk[1];
    a->eng_req = a->engine = 0;                         /* ANALOG: has RES */
    b->eng_req = b->engine = 1;                         /* DIGITAL: none */
    for (i = 0; i < 8u; i++)
        if (str_eq(ENGINES[0]->edit[i].label, "RES"))
            res = i;
    cc_push(0, 7, 0); cc_push(0, 10, 127); cc_push(0, 74, 64); cc_push(0, 72, 127); cc_push(0, 73, 0);
    cc_push(0, 75, 64); cc_push(0, 5, 127); cc_push(0, 91, 127); cc_push(0, 93, 64); cc_push(0, 94, 1);
    run_block(); run_block();
    check(a->p[P_LEVEL] == 0 && a->p[P_PAN] == 63 && a->p[P_E4] == 64 && a->p[P_REL] == 127 && a->p[P_ATK] == 0
          && a->p[P_DEC] == 64 && a->p[P_GLIDE] == 127 && a->p[P_REV] == 127 && a->p[P_CHOR] == 64 && a->p[P_DLY] == 1,
          "2.5: CC 7 10 74 72 73 75 5 91 93 94 on ch 1 set track 1 (CC 74 = engine cutoff/timbre)");
    cc_push(0, 10, 0); cc_push(0, 74, 0); cc_push(0, 7, 127);
    run_block(); run_block();
    check(a->p[P_PAN] == -64 && a->p[P_E4] == 0 && a->p[P_LEVEL] == 127, "2.5: CC 0 / 127: the ends of the range");
    cc_push(0, 71, 127);
    for (i = 0; i < P_COUNT; i++) before[i] = b->p[i];
    cc_push(1, 71, 127); cc_push(1, 1, 99); cc_push(1, 64, 127);   /* DIGITAL has no RES; mod wheel, sustain: not mapped */
    run_block(); run_block();
    for (i = 0; i < P_COUNT; i++) same &= b->p[i] == before[i];
    check(res < 8u && a->p[P_E0 + res] == 127 && same, "2.5: CC71 = the engine's RES; an engine without one, and unmapped CCs, change nothing");
    song.sel = 2;
    cc_push(6, 93, 127);                                /* ch 7: the selected track */
    run_block(); run_block();
    check(trk[2].p[P_CHOR] == 127, "2.5: a CC on ch 4-16 sets the selected track");
    drch = song.g[G_DRCH] ? (uint32_t)song.g[G_DRCH] - 1u : 9u;
    song.g[G_DRCH] = (int16_t)(drch + 1u);
    for (i = 0; i < P_COUNT; i++) before[i] = TDRUM->p[i];
    cc_push(drch, 7, 0); cc_push(drch, 91, 127); cc_push(drch, 10, 0); cc_push(drch, 74, 127); cc_push(drch, 73, 127);
    cc_push(drch, 94, 64);
    run_block(); run_block();
    check(song.g[G_DRLVL] == 0 && song.g[G_DRREV] == 127 && song.g[G_DRDLY] == 64 && TDRUM->p[P_PAN] == -64 && TDRUM->p[P_TFLT] == 63
          && TDRUM->p[P_ATK] == before[P_ATK] && TDRUM->p[P_DLY] == before[P_DLY],
          "2.5: the drum channel: 7 / 91 / 94 = DRUMS LVL / REV / DLY, 10 PAN, 74 FILTER; 73 ignored");
    song.g[G_DRDLY] = 0;
    song.g[G_ROUTE] = 1;
    cc_push(0, 7, 55);
    run_block(); run_block();
    check(a->p[P_LEVEL] == 127, "2.5: IN = CLOCK: CCs ignored");
    song.g[G_ROUTE] = 0;
    mi_r = mi_w;
}

/* menu USB AUDIO = FULL (2.3): the USB input at the level of MASTER all the way up, whatever the knob;
 * the DAC path keeps following the knob */
static void t_usbfull(void)
{
    uint32_t k, i;
    int32_t out[CTL * 2], pk_dac = 0, pk_usb = 0;
    reset(120);
    song.master_q12 = 512;                            /* MASTER low */
    master_cur = -1;
    usb_full_now = 1;
    for (k = 0; k < 8u; k++) trk_note_on(&trk[0], 48u + k * 3u, 110);
    for (k = 0; k < 600u; k++) {
        mix_block(out, CTL);
        if (k > 100u)
            for (i = 0; i < 2u * CTL; i++) {
                int32_t a = out[i] < 0 ? -out[i] : out[i], u = usb_out[i] < 0 ? -usb_out[i] : usb_out[i];
                if (a > pk_dac) pk_dac = a;
                if (u > pk_usb) pk_usb = u;
            }
    }
    usb_full_now = 0;
    for (k = 0; k < 8u; k++) trk_note_off(&trk[0], 48u + k * 3u);
    song.master_q12 = 2048;
    check(pk_usb > pk_dac * 3 && pk_usb <= 32767, "USB AUDIO FULL: the USB level stays up when MASTER is low, no clipping");
}

/* the track FILTER (2.4, P_TFLT): low-pass darkens, high-pass thins a part and the drum track; 0 is
 * bypassed again once open; it can be locked on a step */
static double tf_hf, tf_rms;
static void tf_measure(uint32_t blocks)
{
    int32_t out[CTL * 2];
    double e = 0, d = 0, prev = 0;
    uint32_t k, i;
    for (k = 0; k < blocks; k++) {
        mix_block(out, CTL);
        for (i = 0; i < CTL; i++) {
            double x = out[2u * i];
            e += x * x;
            d += (x - prev) * (x - prev);
            prev = x;
        }
    }
    tf_hf = e > 0 ? d / e : 0;
    tf_rms = e / (blocks * CTL);
}
static void t_tflt(void)
{
    double hf0, rms0, hf_lp, rms_hp, dhf0, dhf_lp;
    uint32_t k;
    reset(120);
    trk[0].p[P_TFLT] = 0;
    for (k = 0; k < 4u; k++) trk_note_on(&trk[0], 36u + k * 7u, 110);
    tf_measure(60); tf_measure(200);
    hf0 = tf_hf; rms0 = tf_rms;
    trk[0].p[P_TFLT] = -50; tf_measure(150); tf_measure(200);
    hf_lp = tf_hf;
    trk[0].p[P_TFLT] = 50; tf_measure(150); tf_measure(200);
    rms_hp = tf_rms;
    check(hf_lp < hf0 * 0.5 && rms_hp < rms0 * 0.5, "track FILTER: LP -50 darkens a part (HF / 2), HP +50 thins it (level / 2)");
    trk[0].p[P_TFLT] = 0; tf_measure(200);
    check(tflt[0].mode == 0, "track FILTER: back to 0, it opens and is bypassed again");
    for (k = 0; k < 4u; k++) trk_note_off(&trk[0], 36u + k * 7u);
    tf_measure(400);
    {   /* the drum track: hats and snares through the filter */
        {
            double e0 = 0, e1 = 0;
            TDRUM->p[P_TFLT] = 0;
            for (k = 0; k < 240u; k++) { if (!(k % 12u)) trk_note_on(TDRUM, 42u, 110); tf_measure(1); e0 += tf_hf; }
            TDRUM->p[P_TFLT] = -60;
            for (k = 0; k < 120u; k++) { if (!(k % 12u)) trk_note_on(TDRUM, 42u, 110); tf_measure(1); }
            for (k = 0; k < 240u; k++) { if (!(k % 12u)) trk_note_on(TDRUM, 42u, 110); tf_measure(1); e1 += tf_hf; }
            dhf0 = e0; dhf_lp = e1;
            check(dhf_lp < dhf0 * 0.5 && tflt[TRK_DRUM].mode == -1, "track FILTER on the drum track: LP darkens the hats");
            TDRUM->p[P_TFLT] = 0;
            tf_measure(300);
            check(tflt[TRK_DRUM].mode == 0, "track FILTER on the drum track: open again, bypassed");
        }
    }
    check(p_lockable(P_TFLT) && lock_set(&trk[1], 3, P_TFLT, -40) && lock_find(&trk[1], 3, P_TFLT, 0) >= 0,
          "track FILTER: a step can lock it");
    lock_del(&trk[1], 3, P_COUNT);
}

/* SLOOP 2.5: GLO > DRUMS > DLY sends the drums to the tempo delay: echoes after a hit has died; 0 none
 * (the drums' path bit for bit as 2.4); through the drum track's FILTER too */
static double drdly_echo(int32_t dly, int32_t flt)
{
    double e;
    reset(120);
    song.g[G_DRREV] = 0;
    song.g[G_DRDLY] = (int16_t)dly;
    TDRUM->p[P_TFLT] = (int16_t)flt;
    tf_measure(40);
    trk_note_on(TDRUM, 38u, 120);
    tf_measure(120);                                     /* the hit */
    tf_measure(1600);                                    /* then its echoes (or silence) */
    e = tf_rms;
    song.g[G_DRDLY] = 0;
    TDRUM->p[P_TFLT] = 0;
    return e;
}
static void t_drdly(void)
{
    double off = drdly_echo(0, 0), on = drdly_echo(127, 0), onf = drdly_echo(127, -30), half = drdly_echo(64, 0);
    printf("  drum DLY: echo rms off %.0f, 64 %.0f, 127 %.0f, 127 + filter %.0f\n", off, half, on, onf);
    check(on > off * 10.0 + 1.0 && half < on && half > off * 3.0 && onf > off * 10.0 + 1.0,
          "2.5: GLO > DRUMS > DLY: the drums echo in the tempo delay (more with more DLY, through the FILTER too); 0: none");
}

int main(void)
{
    t_drdly();
    t_tflt();
    t_usbfull();
    t_micro();
    t_plock();
    t_fill();
    t_chain();
    t_recmode();
    t_mclk();
    t_fixes24();
    t_longdiv();
    t_midiout();
    t_midiin();
    t_midicc();
    t_shed();
    t_drift();
    t_burst();
    t_swing_odd();
    t_len_phase();
    t_ratchet();
    t_roll();
    t_erase_undo();
    t_levels();
    t_chords();
    t_mute();
    t_chordplus();
    printf("seq2: %s\n", fails ? "FAILED" : "all checks ok");
    return fails;
}
