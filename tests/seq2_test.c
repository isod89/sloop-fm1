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
 * Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint64_t blk;
typedef struct { uint64_t blk; uint8_t note, vel; } hit_t;
static hit_t hits[200000];
static uint32_t nhits;
static void run_block(void)
{
    int32_t out[CTL * 2];
    uint32_t a = drums.age, k;
    mix_block(out, CTL);
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
    for (k = 0; k < 4u; k++) trk_note_off(&trk[k % NP3], 60);

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

int main(void)
{
    t_usbfull();
    t_recmode();
    t_mclk();
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
    printf("seq2: %s\n", fails ? "FAILED" : "all checks ok");
    return fails;
}
