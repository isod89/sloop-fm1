/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Keyboard and its layers, scale and chords, arpeggiator, note repeat, sequencer, recording and
 * transport. Runs in the audio ISR, once per CTL-sample block (events_block), and ends in
 * trk_note_on / trk_note_off / drum_on: engines never see where a note came from.
 *
 * Time: the transport clock of fx.c (clk_beat, clk_pos, in units of a sample at 1 BPM). Every track
 * reads its step from it (trk_grid): the step is the clock divided by the track's DIV, swung, modulo
 * its LEN. So the tracks, the click, the arp, the rolls and the song arranger never drift apart, a
 * tempo or DIV change plays at most one step a block, and a polymeter (any LEN) stays in phase.
 *
 * Four tracks: tracks 1..3 are synth parts (steps of up to 4 notes), track 4 the drum track (steps
 * of 16 lanes, one per white key). Each step note / lane has a level (ghost .. hard) and a ratchet
 * (x1..x4 hits in its step). The keys play the selected track; MIDI channels 1..3 play parts 1..3,
 * the DRUMS channel (GLO > DRUMS, default 10) the drum track, any other channel the selected track.
 *
 * Layers: a function button held turns the keys into something else (TE style: hold + touch):
 *   FX   punch-in effects (punch.c)       EDIT  erase that note / sound (while held, as it plays)
 *   ARP  note repeat (roll) at G_ROLL     SEQ   steps 1..16 (the UI: ui_layers.c)
 *   SCL  the key of the song (the UI)     GLO   mute / solo / tap tempo (the UI)
 * On the drum track OCT- / OCT+ held play (and record) ghost / hard hits. */
static const uint16_t SCALE_MASK[] = {
    0xFFF,                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                          /* PEN */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         /* MPEN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HARM */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHRY */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MEL (ascending) */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),              /* BLUES (minor) */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),              /* WHOLE */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10), /* DIMHW */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 11), /* DIMWH */
};
#define NSCALES (sizeof SCALE_MASK / sizeof SCALE_MASK[0])
#define SEQ_NONE 0xFFFFFFFFu

#define KB_SILENT 255u
static uint32_t kb_prev;
/* per key: what its press started, so its release ends the same (whatever the layer or track is now) */
enum { KS_NONE, KS_NOTE, KS_DRUM, KS_ROLL, KS_ERASE, KS_FX, KS_UI };
static uint8_t kb_kind[27], kb_trk[27], kb_n[27], kb_nt[27][4];
static uint8_t last_note = 60;
static uint8_t pen_n = 1, pen_note[4] = {60};   /* the last chord / note played: the SEQ layer writes it */
static uint8_t pen_lane;                       /* the last drum lane played: the SEQ layer's lane */
static volatile uint8_t transport_req;   /* 1 start, 2 stop (from the UI) */
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

static uint32_t trk_midi_ch(uint32_t i)    /* MIDI channel 0..15 of track i (keys -> MIDI out) */
{
    if (i < NPART)
        return i;
    return song.g[G_DRCH] ? (uint32_t)song.g[G_DRCH] - 1u : 9u;
}

static uint32_t scale_mask(const track_t *t)
{
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, NSCALES - 1)];
}

/* ---------------------------------------------------------- layers --- */
enum { LY_PLAY, LY_FX, LY_ERASE, LY_ROLL, LY_STEP, LY_SCALE, LY_MIX, LY_SONG, LY_COUNT };
static uint32_t ly_bit[LY_COUNT];        /* the button (fm1_in.buttons bit) of each layer: the UI sets them */
static uint32_t dyn_bit[2];              /* OCT- / OCT+: ghost / hard on the drum track */
/* a layer locked open (its button held + HOME tapped: ui_input.c), LY_PLAY = none: the keys and knobs
 * stay in it with the button let go, as if it were held */
static volatile uint8_t ly_lock = LY_PLAY;
static uint32_t layer_buttons(void) { return fm1_in.buttons | (ly_lock != LY_PLAY ? ly_bit[ly_lock % LY_COUNT] : 0u); }
/* the layer the keys are in: the held function button (FX, EDIT, ARP, SEQ, SCL, GLO in that order),
 * else the locked one */
static uint32_t layer_now(void)
{
    uint32_t b = layer_buttons(), l;
    for (l = LY_FX; l < LY_COUNT; l++)
        if (b & ly_bit[l])
            return l;
    return LY_PLAY;
}
/* the keys of the layers the UI handles (steps, key, mix): key k down / up, in order */
#define LKQ 16u
static volatile uint16_t lk_q[LKQ];
static volatile uint32_t lk_w, lk_r;
static void lk_push(uint32_t layer, uint32_t k, uint32_t down)
{
    if (lk_w - lk_r < LKQ) {
        lk_q[lk_w % LKQ] = (uint16_t)(layer << 8 | down << 7 | k);
        RING_PUBLISH();
        lk_w++;
    }
}

/* ------------------------------------------------------------- keys --- */
/* key k -> note on a synth part (KB_SILENT: none). WHITE (and chord mode): the white keys walk the
 * scale from C4 = the root, the black keys are silent; SNAP: every key, rounded down into the scale */
static uint32_t kb_map(const track_t *t, uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t n = 53 + (int32_t)k;
    if (is_drum(t))
        return LANE_NOTE[lane_of_key(k)];
    if (ENGINES[t->eng_req % NENGINES] == &ENG_SAMPLE && drum_set() >= 0 &&   /* (the engine it switches to) */
        (uint32_t)t->p[P_E0] % SMP_NSETS == (uint32_t)drum_set())   /* GM KIT: lowest key = kick (C2), no scale */
        return (uint32_t)clamp(36 + 12 * song.octave + (int32_t)k, 0, 127);
#if FELUCCA_SLICE
    if (ENGINES[t->eng_req % NENGINES] == &ENG_SLICE)   /* SLICE: lowest key = slice 0 (C4 + ROOT), no scale */
        return (uint32_t)clamp(SLC_BASE + t->p[P_ROOT] + 12 * song.octave + (int32_t)k, 0, 127);
#endif
    if (t->p[P_QUANT] == 1 && !t->p[P_CHORD]) {  /* SNAP: every key, rounded down to the scale (the old ON) */
        uint32_t mask = scale_mask(t), guard = 12;
        n += 12 * song.octave + t->p[P_TRANS];
        while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    if (t->p[P_QUANT] == 2 || t->p[P_CHORD]) {   /* WHITE: white keys walk the scale, black keys are silent */
        uint32_t mask = t->p[P_CHORD] && !t->p[P_SCALE] ? SCALE_MASK[2] : scale_mask(t), i;
        int32_t count = 0, degree = DEGREE[n % 12], oct;
        if (degree < 0)
            return KB_SILENT;
        /* C4 is the root. Walk scale degrees on successive white keys, including
         * below C4; scales with 5, 6, 8 or 12 notes still have no duplicated degrees. */
        degree += (n / 12 - 5) * 7;
        for (i = 0; i < 12u; i++)
            count += (mask >> i) & 1u;
        oct = degree / count;
        degree %= count;
        if (degree < 0) {
            degree += count;
            oct--;
        }
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (!degree)
                    break;
                degree--;
            }
        n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i;
    }
    return (uint32_t)clamp(n + 12 * song.octave + t->p[P_TRANS], 0, 127);
}

/* chord mode (P_CHORD): the chord of the scale built on note n (in the scale; CHR: minor), into c[];
 * the notes it holds (<= 4, the most a step keeps) */
static const int8_t CHORD_DEG[6][4] = {
    {0, -1, -1, -1},                     /* OFF */
    {0, 2, 4, -1},                       /* TRIAD: 1 3 5 */
    {0, 2, 4, 6},                        /* 7TH: 1 3 5 7 */
    {0, 2, 6, 8},                        /* 9TH: 1 3 7 9 (the lo-fi / R&B voicing) */
    {0, 3, 4, -1},                       /* SUS4: 1 4 5 */
    {0, -1, -1, -1},                     /* POWER: 1 5 8 (semitones, below) */
};
static uint32_t chord_notes(const track_t *t, uint32_t n, uint8_t *c)
{
    uint32_t type = (uint32_t)clamp(t->p[P_CHORD], 0, 5), mask = t->p[P_SCALE] ? scale_mask(t) : SCALE_MASK[2];
    uint32_t k = 0, j;
    if (type == 5u) {
        static const uint8_t PW[3] = {0, 7, 12};
        for (j = 0; j < 3u; j++)
            if (n + PW[j] < 128u)
                c[k++] = (uint8_t)(n + PW[j]);
        return k;
    }
    for (j = 0; j < 4u && CHORD_DEG[type][j] >= 0; j++) {
        int32_t m = (int32_t)n, d = CHORD_DEG[type][j], guard = 48;
        while (d > 0 && guard--) {                       /* d scale degrees up */
            m++;
            if ((mask >> (uint32_t)((m - t->p[P_ROOT] + 120) % 12)) & 1u)
                d--;
        }
        if (m < 128)
            c[k++] = (uint8_t)m;
    }
    return k;
}

/* ------------------------------------------------------------- grid --- */
/* units an odd step starts late: the track's + the global SWING (MPC: 0 = 50 %, 100 = 75 %) */
static uint32_t swing_units(int32_t pct, uint32_t u)
{
    return (uint32_t)clamp(pct, 0, 100) * u / 200u;
}
/* the clock on a grid of den steps a beat, odd steps sw late: the step, units into it, its length */
static uint32_t grid_at(uint32_t den, uint32_t sw, uint32_t *into, uint32_t *len)
{
    uint32_t u = BEAT_U / den, abs = clk_beat * den + clk_pos / u, frac = clk_pos % u;
    if (abs & 1u) {                                      /* an odd step: sw late */
        if (frac < sw) {
            abs--;
            frac += u;
            *len = u + sw;
        } else {
            frac -= sw;
            *len = u - sw;
        }
    } else {
        *len = u + sw;
    }
    *into = frac;
    return abs;
}
static uint32_t trk_grid(const track_t *t, uint32_t *into, uint32_t *len)
{
    uint32_t den = DIV_DEN[(uint32_t)t->p[P_SDIV] % 6u];
    return grid_at(den, swing_units(t->p[P_SSWING] + song.g[G_SWING], BEAT_U / den), into, len);
}
static uint32_t trk_len(const track_t *t) { return t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u; }

/* ------------------------------------------------------------- undo --- */
/* One step back (and forward again) for the pattern of one track: what it was before the last
 * recording pass, erase, step edit, tool or clear (a session: one mark). EDIT + OCT- / OCT+. */
static struct {
    uint8_t valid, undone, trk;
    int16_t len;
    uint32_t sess;
    step_t st[NSTEP];
} undo;
static uint32_t undo_sess = 1;           /* UI sessions (seq.c: recording passes use the track's pass) */
static void undo_mark(const track_t *t, uint32_t sess)
{
    uint32_t i = trk_index(t);
    if (undo.valid && !undo.undone && undo.trk == i && undo.sess == sess)
        return;                                          /* (this session is marked already) */
    memcpy(undo.st, t->step, sizeof undo.st);
    undo.len = t->p[P_SLEN];
    undo.trk = (uint8_t)i;
    undo.sess = sess;
    undo.valid = 1;
    undo.undone = 0;
}
#define UNDO_REC(t) (((t)->pass << 2) | 1u)      /* a recording pass of track t */
static uint32_t undo_erase_sess;

/* ------------------------------------------------------- recording --- */
/* key to ear, in samples: the key's debounce (~3 ms) and the audio out buffer (HALF_FRAMES to
 * 2 x HALF_FRAMES, ~9 ms on average). A note played in time with what the player hears reaches
 * the sequencer this much later than the sound it was played to: recording takes it back. */
#define REC_LAT 512u

/* the step a note played now goes into (as heard: REC_LAT earlier): the one playing, or the next one
 * when it is past the middle of the playing one; *later: it has not played yet (it must not sound twice) */
static uint32_t rec_target(const track_t *t, uint32_t *later)
{
    uint32_t into, slen, abs = trk_grid(t, &into, &slen), half = slen / 2u, lat = REC_LAT * (uint32_t)song.g[G_BPM];
    if (into > half + (lat < half ? lat : half))
        abs++;
    *later = abs != t->seq_abs;
    return abs;
}

/* note into synth step idx (overdub: a step that holds notes gets this one added, a chord of up to 4;
 * when full, the last note is replaced; MONO / LEGATO / UNISON parts keep one note per step) */
static void step_add(track_t *t, uint32_t idx, uint32_t note, uint32_t vel, uint32_t lvl, uint32_t rat)
{
    step_t *s = &t->step[idx];
    uint32_t k;
    if (s->time != ST_NOTE || !s->n || t->p[P_VOICE] != V_POLY) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
        s->lvl = 0;
        s->rat = 0;
    }
    for (k = 0; k < s->n && s->note[k] != note; k++)
        ;
    if (k == s->n) {
        if (s->n < 4u)
            s->n++;
        k = s->n - 1u;
        s->note[k] = (uint8_t)note;
    }
    s->lvl = (uint8_t)((s->lvl & ~(3u << (2u * k))) | (lvl & 3u) << (2u * k));
    s->rat = (uint8_t)((s->rat & ~(3u << (2u * k))) | (rat & 3u) << (2u * k));
    s->time = ST_NOTE;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
}

/* live recording into a synth part: the nearest step (rec_target). Held on: each further step the
 * sequencer enters while the note is held becomes a TIE (rec_hold), up to the pattern length and
 * never over a step with notes (overdub keeps them); a release before the middle of the last one
 * puts that step back (rec_release), so a short note stays one step. A note recorded into another
 * step ends the hold before (the step model ties the notes of one step only). */
static void rec_note(track_t *t, uint32_t note, uint32_t vel, uint32_t rat, int hold)
{
    uint32_t len = trk_len(t), later, abs = rec_target(t, &later), idx = abs % len, k;
    undo_mark(t, UNDO_REC(t));
    step_add(t, idx, note, vel, vel_lvl(vel), rat);
    t->seq_active = 1;
    if (later) {                                    /* it sounds now: the step must not trigger it again */
        if (t->rskip_abs != abs)
            t->rskip_n = 0;
        t->rskip_abs = abs;
        if (t->rskip_n < 4u)
            t->rskip[t->rskip_n++] = (uint8_t)note;
    }
    if (!hold)
        return;
    if (!t->rh_n || t->rh_start != idx) {           /* a new hold (one in another step ends) */
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;      /* a chord: held until its last key is up */
}

/* live recording into the drum track: lane, level, ratchet */
static void rec_hit(track_t *t, uint32_t lane, uint32_t lvl, uint32_t rat)
{
    uint32_t later, abs = rec_target(t, &later);
    undo_mark(t, UNDO_REC(t));
    dstep_set(&t->dstep[abs % trk_len(t)], lane, lvl, rat);
    t->seq_active = 1;
    if (later) {
        if (t->rskip_abs != abs)
            t->rskip_lanes = 0;
        t->rskip_abs = abs;
        t->rskip_lanes |= (uint16_t)(1u << lane);
    }
}

/* the sequencer enters step idx (before playing it): a recorded note still held ties into it */
static void rec_hold(track_t *t, uint32_t idx, uint32_t len, uint32_t abs)
{
    step_t *s;
    if (!t->rh_n)
        return;
    if (!((song.rec >> trk_index(t)) & 1u) || t->rh_ties + 1u >= len) {
        t->rh_n = 0;                                /* disarmed, or the whole pattern is this note */
        return;
    }
    if (idx == t->rh_start)
        return;                                     /* (recorded ahead into the step now starting) */
    s = &t->step[idx];
    if (s->time == ST_NOTE && s->n) {
        t->rh_n = 0;                                /* a step with notes: the hold ends before it */
        return;
    }
    t->rh_bak = *s;
    t->rh_last = (uint8_t)idx;
    t->rh_last_abs = abs;
    t->rh_ties++;
    memset(s, 0, sizeof *s);
    s->time = ST_TIE;
}

/* a key of a recorded note is up: the hold ends with the last one */
static void rec_release(track_t *t, uint32_t note)
{
    uint32_t i, k = 0, into, slen, half, lat;
    for (i = 0; i < t->rh_n; i++)
        if (t->rh_note[i] != note)
            t->rh_note[k++] = t->rh_note[i];
    if (k == t->rh_n || (t->rh_n = (uint8_t)k))
        return;                                     /* not one of them, or others still held */
    if (!t->rh_ties || trk_grid(t, &into, &slen) != t->rh_last_abs)
        return;
    half = slen / 2u;
    lat = REC_LAT * (uint32_t)song.g[G_BPM];
    if (into <= half + (lat < half ? lat : half) && t->step[t->rh_last].time == ST_TIE)
        t->step[t->rh_last] = t->rh_bak;            /* released early in it: not held into this step */
}

/* LIVE recording, no click needed (REC: ui_input.c rec_toggle).
 *  playing: REC records the selected track at once (song.rec), quantised, overdub.
 *  stopped: REC arms (rec_wait). The first note played on the selected track:
 *   - a project with notes: starts the transport, that note is step 1, recording on.
 *     PLAY while armed starts the transport and the recording together.
 *   - an empty project: a FREE TAKE (ft_on). Play freely, as long as you like: no tempo, no
 *     grid. REC on the next downbeat closes the loop: its length sets the tempo (1, 2 or 4
 *     bars, the nearest the current tempo; within 3 % of it the tempo is kept), the notes are
 *     quantised to 1/16 into it with their lengths, and the loop plays on. PLAY drops the take.
 *  The REC screen (ui_studio.c) sets how (settings of the FM-1, panel.c lights_word):
 *   KNOB 1 MODE (an empty project): FREE (the free take above) or TEMPO (record at the tempo set,
 *          as in a project with notes);
 *   KNOB 3 START (TEMPO, or a project with notes): NOTE (the first note starts the loop, as above)
 *          or COUNT (PLAY clicks one bar, 4 beats, then the loop and the recording start; notes
 *          played meanwhile only sound). */
static volatile uint8_t rec_wait;             /* 1: armed, waits for a note */
static uint8_t rec_tempo;                     /* REC screen MODE: 0 FREE, 1 TEMPO (an empty project) */
static uint8_t rec_count;                     /* REC screen START: 0 NOTE, 1 COUNT (one bar of clicks) */
static volatile uint8_t ci_on;                /* the count-in runs (armed, COUNT, PLAY) */
static volatile uint8_t ci_beat;              /* its beats clicked so far - 1 (the UI shows 4 - ci_beat) */
static uint32_t ci_u;                         /* clock units since it started */
static volatile uint8_t rec_go;               /* recording just started (the UI says so) */
static void seq_start(void);
static void rec_begin(void)
{
    song.rec = (uint8_t)(1u << (song.sel % NTRK));
    rec_wait = 0;
    rec_go = 1;
}

#define FT_MAX 192u
#define FT_BLOCKS (24u * FS / CTL)            /* 24 s: 4 bars at 40 BPM, the longest loop */
#define FT_OPEN 0xFFFFu                       /* the key is still down */
typedef struct { uint16_t t, d; uint8_t note, vel; } ft_ev_t;   /* in blocks from the first note; drums: lane, level */
static ft_ev_t ft_ev[FT_MAX];
static uint32_t ft_n;
static volatile uint8_t ft_on;                /* a free take runs */
static uint8_t ft_trk;
static volatile uint32_t ft_t;                /* blocks since its first note */
static volatile uint32_t ft_btn_mask;         /* the REC button (the UI sets it): closes the take */
static volatile uint32_t ft_drop_mask;        /* the PLAY button: drops it */
static uint32_t ft_btn_prev;
static volatile uint8_t ft_bars;              /* the loop just closed: bars (the UI says so), 0xFF dropped */
static volatile uint32_t ft_close_ms;         /* when (the UI drops the press that closed it) */
static volatile uint8_t ft_closed;            /* (ft_close_ms is set) */

static int track_empty(const track_t *t)
{
    uint32_t k;
    for (k = 0; k < NSTEP; k++)
        if (is_drum(t) ? dstep_mask(&t->dstep[k]) != 0u : t->step[k].time == ST_NOTE && t->step[k].n)
            return 0;
    return 1;
}
static int project_empty(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (!track_empty(&trk[i]))
            return 0;
    return 1;
}
static void steps_clear(track_t *t)           /* an empty pattern (synth: REST steps, drums: no lane) */
{
    uint32_t k;
    memset(t->step, 0, sizeof t->step);
    if (!is_drum(t))
        for (k = 0; k < NSTEP; k++)
            t->step[k].time = ST_REST;
}

/* tempo x 10 of a loop of T blocks holding n bars of 4/4 */
static uint32_t ft_bpm10(uint32_t T, uint32_t n)
{
    uint32_t den = T * CTL;                       /* (n * 2400 * FS < 2^32 up to n = 4) */
    return den ? (n * 2400u * FS + den / 2u) / den : 0u;
}

/* the bars a loop of T blocks holds (1, 2 or 4: the tempo the nearest the current one, in
 * 40..240), 0 none; *bpm its tempo */
static uint32_t ft_fit(uint32_t T, uint32_t *bpm)
{
    static const uint8_t BARS[3] = {1, 2, 4};
    uint32_t i, best = 0, cur = (uint32_t)song.g[G_BPM] * 10u, err = 0xFFFFFFFFu;
    for (i = 0; i < 3u; i++) {
        uint32_t b = ft_bpm10(T, BARS[i]), e;
        if (b < 400u || b > 2400u)
            continue;
        e = b > cur ? b * 1000u / cur : cur * 1000u / b;   /* the ratio, x 1000 */
        if (e < err) {
            err = e;
            best = BARS[i];
            *bpm = (b + 5u) / 10u;
            if (e <= 1030u)
                *bpm = cur / 10u;                           /* played to the tempo set: keep it */
        }
    }
    return best;
}

static void ft_start(track_t *t)
{
    ft_on = 1;
    ft_trk = (uint8_t)trk_index(t);
    ft_t = 0;
    ft_n = 0;
    rec_wait = 0;
}

static void ft_note_on(uint32_t note, uint32_t vel)
{
    if (ft_n < FT_MAX && ft_t < FT_BLOCKS) {
        ft_ev[ft_n].t = (uint16_t)ft_t;
        ft_ev[ft_n].d = FT_OPEN;
        ft_ev[ft_n].note = (uint8_t)note;
        ft_ev[ft_n].vel = (uint8_t)vel;
        ft_n++;
    }
}

static void ft_note_off(uint32_t note)
{
    uint32_t i = ft_n;
    while (i--)
        if (ft_ev[i].note == note && ft_ev[i].d == FT_OPEN) {
            ft_ev[i].d = (uint16_t)(ft_t - ft_ev[i].t);
            return;
        }
}

/* REC on the downbeat: the loop is the time from the first note to now */
static void ft_close(void)
{
    track_t *t = &trk[ft_trk % NTRK];
    uint32_t T = ft_t, bars, bpm = 0, len, i, k;
    ft_on = 0;
    ft_close_ms = fm1_ms;
    ft_closed = 1;
    bars = ft_n && T >= FS / CTL / 2u ? ft_fit(T, &bpm) : 0u;
    if (!bars) {
        ft_bars = 0xFF;                               /* nothing played, or no tempo fits */
        return;
    }
    len = 16u * bars;
    undo_mark(t, (undo_sess += 4u) | 3u);             /* (undo: back to the empty project) */
    steps_clear(t);                                   /* (no tie left over from an old pattern) */
    for (i = 0; i < NTRK; i++) {                      /* the project is empty: one loop length */
        trk[i].p[P_SLEN] = (int16_t)len;
        trk[i].p[P_SDIV] = 2;                         /* 1/16 */
    }
    for (i = 0; i < ft_n; i++) {                      /* the notes, to the nearest step */
        uint32_t idx = (ft_ev[i].t * len * 2u + T) / (2u * T) % len;
        if (is_drum(t))
            dstep_set(&t->dstep[idx], ft_ev[i].note & 15u, ft_ev[i].vel & 3u, 0);
        else
            step_add(t, idx, ft_ev[i].note, ft_ev[i].vel, vel_lvl(ft_ev[i].vel), 0);
    }
    if (!is_drum(t))
        for (i = 0; i < ft_n; i++) {                  /* their lengths: TIE steps, up to the next note */
            uint32_t d = ft_ev[i].d == FT_OPEN || ft_ev[i].t + ft_ev[i].d > T ? T - ft_ev[i].t : ft_ev[i].d;
            uint32_t idx = (ft_ev[i].t * len * 2u + T) / (2u * T) % len;
            uint32_t steps = (d * len * 2u + T) / (2u * T);
            for (k = 1; k < steps && k < len; k++) {
                step_t *s = &t->step[(idx + k) % len];
                if (s->time == ST_NOTE && s->n)
                    break;
                memset(s, 0, sizeof *s);
                s->time = ST_TIE;
            }
        }
    t->seq_active = 1;
    song.g[G_BPM] = (int16_t)bpm;
    ft_bars = (uint8_t)bars;
#if FELUCCA_ARRANGER
    arrangement_enabled = 0;
#endif
    seq_start();                                      /* this is the downbeat: the loop plays */
}

/* the UI: a REC / PLAY press is this one's (it runs the take, or just closed it): not the UI's */
static int ft_owns_press(void)
{
    return ft_on || (ft_closed && (uint32_t)(fm1_ms - ft_close_ms) < 300u);
}

/* a free take, once per block: REC closes it, PLAY drops it (their press, timed here, not by the UI) */
static void ft_block(void)
{
    uint32_t b = fm1_in.buttons & (ft_btn_mask | ft_drop_mask), press = b & ~ft_btn_prev;
    ft_btn_prev = b;
    if (!ft_on)
        return;
    ft_t++;
    if (press & ft_drop_mask) {
        ft_on = 0;                                    /* PLAY: dropped, nothing changes */
        ft_bars = 0xFF;
        ft_close_ms = fm1_ms;
        ft_closed = 1;
    } else if ((press & ft_btn_mask) || ft_t >= FT_BLOCKS) {
        ft_close();
    }
}

/* ------------------------------------------------------------ erase --- */
/* EDIT + key: that note / sound goes from the selected pattern. Playing: from the step playing and
 * every step the sequencer enters while the key is held (MPC style); stopped: from every step. */
static uint8_t er_trk;
static uint16_t er_lanes;                     /* drums: lanes held */
static uint32_t er_notes[4];                  /* synth: notes held (bit per MIDI note) */
static volatile uint8_t er_flash;             /* something was erased (the UI flashes) */

static int er_has(uint32_t note) { return (er_notes[(note >> 5) & 3u] >> (note & 31u)) & 1u; }
static void erase_step(track_t *t, uint32_t idx)
{
    if (is_drum(t)) {
        dstep_t *s = &t->dstep[idx];
        uint32_t l, m = dstep_mask(s) & er_lanes;
        for (l = 0; m; l++, m >>= 1)
            if (m & 1u) {
                dstep_clr(s, l);
                er_flash = 1;
            }
    } else {
        step_t *s = &t->step[idx];
        uint32_t i, k = 0, lv = 0, rt = 0;
        if (s->time != ST_NOTE)
            return;
        for (i = 0; i < s->n; i++)
            if (!er_has(s->note[i])) {
                lv |= ((s->lvl >> (2u * i)) & 3u) << (2u * k);
                rt |= ((s->rat >> (2u * i)) & 3u) << (2u * k);
                s->note[k++] = s->note[i];
            }
        if (k == s->n)
            return;
        er_flash = 1;
        s->n = (uint8_t)k;
        s->lvl = (uint8_t)lv;
        s->rat = (uint8_t)rt;
        if (!k) {
            s->time = ST_REST;
            s->flags = 0;
            s->vel = 0;
        }
    }
}
static void erase_now(track_t *t)             /* a key just went down */
{
    uint32_t i, len = trk_len(t);
    undo_mark(t, undo_erase_sess);
    if (song.playing && t->seq_abs != SEQ_NONE) {
        erase_step(t, t->seq_idx % len);
    } else {
        for (i = 0; i < len; i++)
            erase_step(t, i);
    }
}
static int erasing(const track_t *t) { return trk_index(t) == er_trk && (er_lanes || er_notes[0] || er_notes[1] || er_notes[2] || er_notes[3]); }

/* ------------------------------------------------------------- arp --- */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (t->p[P_AHOLD] && t->arp_phys == 0u)
        t->nheld = 0;                               /* new chord replaces the latched one */
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u)
        t->held[t->nheld++] = (uint8_t)note;
    if (t->nheld == 1u) {
        t->arp_new = 1;                             /* the first note: now (or on the grid just ahead) */
        t->arp_idx = 0xFFFFFFFFu;
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (t->arp_phys)
        t->arp_phys--;
    if (t->p[P_AHOLD])
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note)
            t->held[k++] = t->held[i];
    t->nheld = (uint8_t)k;
}

/* the next note of the arp: the held notes (sorted or as played) over OCT octaves, by MODE */
static uint32_t arp_next(track_t *t)
{
    uint32_t cnt, list[64], len = 0, i, j, o;
    for (i = 0; i < t->nheld; i++)
        list[i] = t->held[i];
    cnt = t->nheld;
    if (!t->p[P_AORDER])
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1];
                list[j - 1] = x;
            }
    for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    t->arp_idx++;
    switch (t->p[P_AMODE]) {
    case 2:
        j = len - 1u - t->arp_idx % len;
        break;
    case 3: {
        uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = t->arp_idx % cyc;
        j = k < len ? k : cyc - k;
        break;
    }
    case 4:
        j = rng() % len;
        break;
    default:
        j = t->arp_idx % len;
        break;
    }
    return list[j];
}

/* the arp, once per block: on the transport's grid of RATE (with its SWING) while playing, from the
 * first key while stopped. A new chord starts at once unless the grid is just ahead. While recording,
 * each note it plays is recorded (what you hear) */
static void arp_tick(track_t *t, uint32_t adv)
{
    uint32_t den = DIV_DEN[(uint32_t)t->p[P_ARATE] % 6u], u = BEAT_U / den, into, slen, abs = 0, fire = 0;
    if (t->arp_note) {
        if (t->arp_off <= adv) {
            trk_note_off(t, t->arp_note);
            t->arp_note = 0;
        } else {
            t->arp_off -= adv;
        }
    }
    if (!t->p[P_AMODE] || !t->nheld) {
        if (!t->nheld && t->arp_note) {
            trk_note_off(t, t->arp_note);
            t->arp_note = 0;
        }
        t->arp_new = 0;
        return;
    }
    if (song.playing) {
        abs = grid_at(den, swing_units(t->p[P_ASWING], u), &into, &slen);
        if (t->arp_new) {
            t->arp_new = 0;
            t->arp_abs = into * 4u >= slen * 3u ? abs : abs - 1u;   /* the last quarter: the grid plays it */
        }
        if (abs + 1u == t->arp_abs)
            abs = t->arp_abs;                       /* ARP SWG turned up inside an odd step: no replay */
        fire = abs != t->arp_abs;
        t->arp_abs = abs;
    } else {
        if (t->arp_new) {
            t->arp_new = 0;
            t->arp_pos = u;
        }
        slen = u;
        t->arp_pos += adv;
        if (t->arp_pos >= u) {
            t->arp_pos = t->arp_pos - u < u ? t->arp_pos - u : 0;
            fire = 1;
        }
    }
    if (!fire)
        return;
    if (t->arp_note)
        trk_note_off(t, t->arp_note);
    t->arp_note = 0;
    if ((uint32_t)(rng() & 127u) <= (uint32_t)t->p[P_APROB]) {
        uint32_t n = arp_next(t);
        t->arp_note = (uint8_t)n;
        t->arp_off = slen * (uint32_t)t->p[P_AGATE] / 128u;
        trk_note_on(t, n, 100);
        if (((song.rec >> trk_index(t)) & 1u) && song.playing)
            rec_note(t, n, 100, 0, 0);
    }
}

/* ------------------------------------------------------- note input --- */
/* armed and stopped, a note on the selected track: an empty project starts a free take, else the
 * note is the downbeat (the transport starts, recording on) */
static void arm_start(track_t *t)
{
    if (!rec_wait || t != TSEL || song.playing || ci_on)
        return;
    if (project_empty() && !rec_tempo) {
        ft_start(t);                              /* the first take sets the loop and the tempo */
    } else if (rec_count) {
        return;                                   /* COUNT: PLAY counts in; a note only sounds */
    } else {
#if FELUCCA_ARRANGER
        arrangement_enabled = 0;
#endif
        seq_start();                              /* the note is the downbeat */
        if (song.playing)
            rec_begin();
    }
}

static void drum_input(uint32_t lane, uint32_t lvl, uint32_t rat, int rec);
static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    if (is_drum(t)) {                             /* (a GM note on the drum track: its lane) */
        drum_input(lane_of_note(note), vel_lvl(vel), 0, 1);
        return;
    }
    last_note = (uint8_t)note;
    arm_start(t);
    if (ft_on && t == &trk[ft_trk % NTRK])
        ft_note_on(note, vel);
    if (t->p[P_AMODE]) {
        arp_add(t, note);                         /* (the arp records the notes it plays) */
        return;
    }
    if (((song.rec >> trk_index(t)) & 1u) && song.playing)
        rec_note(t, note, vel, 0, 1);
    trk_note_on(t, note, vel);
}

static void input_off(track_t *t, uint32_t note)
{
    if (is_drum(t)) {
        if (ft_on && ft_trk == TRK_DRUM)
            ft_note_off(lane_of_note(note));
        return;
    }
    if (ft_on && t == &trk[ft_trk % NTRK])
        ft_note_off(note);
    rec_release(t, note);
    arp_remove(t, note);                            /* both: the note may have started in the */
    trk_note_off(t, note);                          /* other mode (ARP switched while held) */
}

/* a drum hit from a key, MIDI or a roll: lane, level; rat: its ratchet when recorded (rolls); rec: it
 * may be recorded (a roll records one hit a step) */
static void drum_input(uint32_t lane, uint32_t lvl, uint32_t rat, int rec)
{
    track_t *t = TDRUM;
    lane &= 15u;
    pen_lane = (uint8_t)lane;
    arm_start(t);
    if (ft_on && ft_trk == TRK_DRUM)
        ft_note_on(lane, lvl);
    if (rec && ((song.rec >> TRK_DRUM) & 1u) && song.playing)
        rec_hit(t, lane, lvl, rat);
    trk_note_on(t, LANE_NOTE[lane], lvl_vel(lvl, 100));
}

/* -------------------------------------------------------------- roll --- */
/* ARP + key: note repeat. The key plays at G_ROLL (1/8, 1/16, 1/32, 32T, 1/64) on the transport's grid
 * (stopped: from the press) until it or ARP is up. Recorded, a roll faster than the track's steps
 * becomes ratchets (1/32 on 1/16 steps: x2, 32T: x3, 1/64: x4). */
static const uint8_t ROLL_DEN[5] = {2, 4, 8, 12, 16};
#define NROLL 4u
static struct {
    uint8_t on, key, trk, note, lvl;    /* note: the synth note, or the lane */
    uint32_t last;                      /* playing: the roll step last played; stopped: units since */
    uint32_t off;                       /* synth: units to its note-off, 0 = not sounding */
    uint32_t rec_abs;                   /* the step its last recorded hit went into */
} roll[NROLL];
static uint32_t roll_den(void) { return ROLL_DEN[(uint32_t)clamp(song.g[G_ROLL], 0, 4)]; }
/* the lanes / notes a roll plays live on track t: the pattern does not play them meanwhile (no flam
 * with what was recorded, nor with what this roll is recording) */
static uint32_t roll_lanes(const track_t *t)
{
    uint32_t r, m = 0;
    for (r = 0; r < NROLL; r++)
        if (roll[r].on && roll[r].trk == trk_index(t))
            m |= 1u << (roll[r].note & 15u);
    return m;
}
static int roll_has(const track_t *t, uint32_t note)
{
    uint32_t r;
    for (r = 0; r < NROLL; r++)
        if (roll[r].on && roll[r].trk == trk_index(t) && roll[r].note == note)
            return 1;
    return 0;
}

static void roll_hit(uint32_t r)
{
    track_t *t = &trk[roll[r].trk % NTRK];
    uint32_t u = BEAT_U / roll_den(), su = div_units((uint32_t)t->p[P_SDIV]);
    uint32_t hits = su / u, rat = hits > 4u ? 3u : hits > 1u ? hits - 1u : 0u;
    uint32_t armed = ((song.rec >> trk_index(t)) & 1u) && song.playing, later, abs = 0, rec = 0;
    if (armed) {                                     /* one recorded hit a step: the ratchet does the rest */
        abs = rec_target(t, &later);
        rec = abs != roll[r].rec_abs;
        roll[r].rec_abs = abs;
    }
    if (is_drum(t)) {
        drum_input(roll[r].note, roll[r].lvl, rat, rec || !armed);
        return;
    }
    arm_start(t);
    if (roll[r].off)
        trk_note_off(t, roll[r].note);
    trk_note_on(t, roll[r].note, lvl_vel(roll[r].lvl, 100));
    roll[r].off = u / 2u;
    if (rec)
        rec_note(t, roll[r].note, 100, rat, 0);
    if (ft_on && t == &trk[ft_trk % NTRK])
        ft_note_on(roll[r].note, 100), ft_note_off(roll[r].note);
}

static void roll_start(uint32_t k, track_t *t, uint32_t note, uint32_t lvl)
{
    uint32_t r, den = roll_den(), into, slen, abs;
    for (r = 0; r < NROLL && roll[r].on; r++)
        ;
    if (r == NROLL)
        return;
    roll[r].on = 1;
    roll[r].key = (uint8_t)k;
    roll[r].trk = (uint8_t)trk_index(t);
    roll[r].note = (uint8_t)note;
    roll[r].lvl = (uint8_t)lvl;
    roll[r].off = 0;
    roll[r].rec_abs = SEQ_NONE;
    if (song.playing) {
        abs = grid_at(den, 0, &into, &slen);
        roll[r].last = abs;
        if (into * 4u >= slen * 3u)
            return;                                  /* the grid is just ahead: it starts there */
    } else {
        roll[r].last = 0;
    }
    roll_hit(r);
}

static void roll_end(uint32_t r)
{
    if (roll[r].on && roll[r].off && roll[r].trk != TRK_DRUM)
        trk_note_off(&trk[roll[r].trk % NTRK], roll[r].note);
    roll[r].on = 0;
}

static void roll_block(uint32_t adv)
{
    uint32_t r, den = roll_den(), u = BEAT_U / den, into, slen;
    for (r = 0; r < NROLL; r++) {
        if (!roll[r].on)
            continue;
        if (roll[r].off) {
            if (roll[r].off <= adv) {
                trk_note_off(&trk[roll[r].trk % NTRK], roll[r].note);
                roll[r].off = 0;
            } else {
                roll[r].off -= adv;
            }
        }
        if (song.playing) {
            uint32_t abs = grid_at(den, 0, &into, &slen);
            if (abs != roll[r].last) {
                roll[r].last = abs;
                roll_hit(r);
            }
        } else {
            roll[r].last += adv;
            if (roll[r].last >= u) {
                roll[r].last = roll[r].last - u < u ? roll[r].last - u : 0;
                roll_hit(r);
            }
        }
    }
}

/* ---------------------------------------------------------- keyboard --- */
/* the level of a key on the drum track: OCT- held ghost, OCT+ held hard */
static uint32_t key_lvl(void)
{
    uint32_t b = fm1_in.buttons;
    return (b & dyn_bit[0]) ? LV_GHOST : (b & dyn_bit[1]) ? LV_HARD : LV_NORM;
}

static void key_down(uint32_t k)
{
    uint32_t layer = layer_now(), sel = song.sel % NTRK, i, mc;
    track_t *t = &trk[sel];
    kb_kind[k] = KS_NONE;
    kb_trk[k] = (uint8_t)sel;
    kb_n[k] = 0;
    switch (layer) {
    case LY_FX: {                                     /* FX held: the white keys pick a punch-in effect */
        int32_t fx = punch_key(k);
        kb_kind[k] = KS_FX;
        if (fx >= 0 && fx < (int32_t)PUNCH_NFX) {
            if (punch_latch) {                        /* LATCH: the key switches it on, the same key off */
                punch.req = punch.req == (int8_t)fx ? (int8_t)-1 : (int8_t)fx;
                punch.keybit = 0;                     /* (no key-up ends it) */
            } else {
                punch.req = (int8_t)fx;
                punch.keybit = 1u << k;
            }
        }
        return;
    }
    case LY_STEP:
    case LY_SCALE:
    case LY_MIX:
    case LY_SONG:                                     /* the UI's: steps, the key, the mix, the sections */
        kb_kind[k] = KS_UI;
        kb_nt[k][0] = (uint8_t)layer;                 /* (its key-up goes to the same layer) */
        lk_push(layer, k, 1);
        return;
    case LY_ERASE:
        kb_kind[k] = KS_ERASE;
        if (!erasing(t) || er_trk != sel)
            undo_erase_sess = (undo_sess += 4u) | 2u;  /* a new erase: one undo */
        if (er_trk != sel) {
            er_lanes = 0;
            er_notes[0] = er_notes[1] = er_notes[2] = er_notes[3] = 0;
            er_trk = (uint8_t)sel;
        }
        if (is_drum(t)) {
            kb_nt[k][0] = (uint8_t)lane_of_key(k);
            kb_n[k] = 1;
            er_lanes |= (uint16_t)(1u << kb_nt[k][0]);
        } else {
            uint32_t n = kb_map(t, k);
            if (n == KB_SILENT)
                return;
            kb_n[k] = (uint8_t)(t->p[P_CHORD] ? chord_notes(t, n, kb_nt[k]) : 1u);
            if (!t->p[P_CHORD])
                kb_nt[k][0] = (uint8_t)n;
            for (i = 0; i < kb_n[k]; i++)
                er_notes[(kb_nt[k][i] >> 5) & 3u] |= 1u << (kb_nt[k][i] & 31u);
        }
        erase_now(t);
        return;
    default:
        break;
    }
    if (is_drum(t)) {                                 /* the drum track: the key's lane */
        uint32_t lane = lane_of_key(k), lvl = key_lvl();
        kb_nt[k][0] = (uint8_t)lane;
        kb_n[k] = 1;
        if (layer == LY_ROLL) {
            kb_kind[k] = KS_ROLL;
            roll_start(k, t, lane, lvl);
            return;
        }
        kb_kind[k] = KS_DRUM;
        drum_input(lane, lvl, 0, 1);
        mc = trk_midi_ch(sel);
        midi_out_event(0x09u | (0x90u | mc) << 8 | (uint32_t)LANE_NOTE[lane] << 16 | lvl_vel(lvl, 100) << 24);
        return;
    }
    {
        uint32_t n = kb_map(t, k);
        if (n == KB_SILENT)
            return;
        if (layer == LY_ROLL) {
            kb_kind[k] = KS_ROLL;
            kb_nt[k][0] = (uint8_t)n;
            kb_n[k] = 1;
            roll_start(k, t, n, LV_NORM);
            return;
        }
        kb_kind[k] = KS_NOTE;
        if (t->p[P_CHORD]) {
            kb_n[k] = (uint8_t)chord_notes(t, n, kb_nt[k]);
        } else {
            kb_nt[k][0] = (uint8_t)n;
            kb_n[k] = 1;
        }
        mc = trk_midi_ch(sel);
        for (i = 0; i < kb_n[k]; i++) {
            input_on(t, kb_nt[k][i], 100);
            midi_out_event(0x09u | (0x90u | mc) << 8 | (uint32_t)kb_nt[k][i] << 16 | 100u << 24);
        }
        /* the pen of the SEQ layer: the keys down now (a chord), else this note */
        if (!(kb_prev & ~(1u << k)) || pen_n >= 4u)
            pen_n = 0;
        for (i = 0; i < kb_n[k] && pen_n < 4u; i++)
            pen_note[pen_n++] = kb_nt[k][i];
    }
}

static void key_up(uint32_t k)
{
    uint32_t i, mc, kind = kb_kind[k];
    track_t *t = &trk[kb_trk[k] % NTRK];
    kb_kind[k] = KS_NONE;
    switch (kind) {
    case KS_FX:
        if (punch.keybit == 1u << k) {                /* its key is up: the mix comes back */
            punch.keybit = 0;
            punch.req = -1;
        }
        return;
    case KS_UI:
        lk_push(kb_nt[k][0], k, 0);
        return;
    case KS_ERASE:
        if (is_drum(t))
            er_lanes &= (uint16_t)~(1u << kb_nt[k][0]);
        else
            for (i = 0; i < kb_n[k]; i++)
                er_notes[(kb_nt[k][i] >> 5) & 3u] &= ~(1u << (kb_nt[k][i] & 31u));
        return;
    case KS_ROLL:
        for (i = 0; i < NROLL; i++)
            if (roll[i].on && roll[i].key == k)
                roll_end(i);
        return;
    case KS_NOTE:
        mc = trk_midi_ch(kb_trk[k] % NTRK);
        for (i = 0; i < kb_n[k]; i++) {
            input_off(t, kb_nt[k][i]);
            midi_out_event(0x08u | (0x80u | mc) << 8 | (uint32_t)kb_nt[k][i] << 16);
        }
        return;
    case KS_DRUM:
        if (ft_on && ft_trk == TRK_DRUM)
            ft_note_off(kb_nt[k][0]);
        mc = trk_midi_ch(TRK_DRUM);
        midi_out_event(0x08u | (0x80u | mc) << 8 | (uint32_t)LANE_NOTE[kb_nt[k][0] & 15u] << 16);
        return;
    default:
        return;
    }
}

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes, ch = cur ^ kb_prev, k, r;
    if (!(layer_buttons() & ly_bit[LY_ROLL]))         /* ARP up (and not locked): the rolls end (the keys stay silent) */
        for (r = 0; r < NROLL; r++)
            if (roll[r].on)
                roll_end(r);
    if (!ch)
        return;
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u)
            key_down(k);
        else
            key_up(k);
        kb_prev ^= 1u << k;                           /* (key_down sees the keys down before it) */
    }
}

/* LIVE: one record arm at most, on the selected track; selecting another track moves it there
 * (an arm follows too: it waits for a note on the track selected; a free take stays on its track) */
static void rec_follow(uint32_t sel)
{
    if (song.rec)
        song.rec = (uint8_t)(1u << (sel % NTRK));
}

/* -------------------------------------------------------- metronome --- */
/* A wood block on every beat, louder on the first of the bar, from the drum kit's voices
 * (not recorded, not muted with the drum track; GLO > DRUMS LVL sets its level).
 * GLO > GLOBAL CLICK: OFF (0, default: LIVE needs none), REC = while a track records,
 * ON = while playing. */
#define CLICK_REC 1
#define CLICK_ON 2
static uint32_t click_last = SEQ_NONE;          /* the beat it last played */
static void click_tick(void)
{
    uint32_t mode = (uint32_t)song.g[G_CLOCK];
    if (!song.playing || clk_beat == click_last)
        return;
    click_last = clk_beat;
    if (mode == CLICK_ON || (mode == CLICK_REC && song.rec))
        drum_on(clk_beat % 4u ? 76u : 77u, clk_beat % 4u ? 72u : 120u);
}

/* -------------------------------------------------------- sequencer --- */
#if FELUCCA_ARRANGER
/* ---- LIVE SECTIONS (SAVE held + key, ui_layers.c): a section asked for while playing starts on the
 * next bar, every track from its step 0 (as the song does). SONG REC writes the order you play into the
 * song chain (arrangement): each section with the bars it played, from the bar after the arm. */
static volatile int8_t live_req = -1;              /* section asked for (UI), applied on the next bar */
static volatile int8_t live_sec = -1;              /* the section playing: last jumped to, loaded or stored */
static uint32_t live_bar = 0xFFFFFFFFu;            /* clk_beat / 4 of the last bar seen */
static volatile uint8_t srec;                      /* SONG REC: 0 off, 1 armed (from the next bar), 2 recording */
static arr_entry_t srec_e[ARR_STEPS];
static volatile uint8_t srec_n;                    /* entries so far (the last one still growing) */
static volatile uint8_t srec_done;                 /* the chain was written: n parts (0xFF: nothing played) */

static void srec_finish(void)                      /* (audio ISR, or the UI with the IRQ off) */
{
    uint32_t i, n = 0;
    for (i = 0; i < srec_n; i++)
        if (srec_e[i].bars)
            srec_e[n++] = srec_e[i];
    if (n) {
        arrangement.count = (uint8_t)n;
        arrangement.loop = 0;
        for (i = 0; i < n; i++)
            arrangement.entry[i] = srec_e[i];
    }
    srec_done = (uint8_t)(n ? n : 0xFFu);
    srec = 0;
    srec_n = 0;
}
static void srec_add(uint32_t s)
{
    if (srec_n >= ARR_STEPS) {
        srec_finish();                              /* the chain is full: what was played so far */
        return;
    }
    srec_e[srec_n].scene = (uint8_t)s;
    srec_e[srec_n].bars = 0;
    srec_n++;
}
/* STOP (or SONG REC pressed again): the bar playing counts if it had begun */
static void srec_stop(void)
{
    if (srec == 2u && srec_n && srec_e[srec_n - 1u].bars < 64u &&
        ((!(clk_beat & 3u) && (clk_beat >> 2) != live_bar) || (clk_beat & 3u)))
        srec_e[srec_n - 1u].bars++;
    if (srec == 2u)
        srec_finish();
    srec = 0;
}
static void seq_reset_tracks(uint32_t pos);
static void live_block(void)                       /* once a block while playing a loop (not the song) */
{
    if ((clk_beat & 3u) || (clk_beat >> 2) == live_bar)
        return;
    live_bar = clk_beat >> 2;                       /* a new bar */
    if (srec == 2u && srec_n) {
        arr_entry_t *e = &srec_e[srec_n - 1u];
        if (e->bars < 64u) {
            e->bars++;
        } else {                                    /* (64 bars of one section: it goes on in the next entry) */
            srec_add(e->scene);
            if (srec == 2u)
                srec_e[srec_n - 1u].bars = 1;
        }
    }
    if (live_req >= 0) {
        uint32_t s = (uint32_t)live_req;
        live_req = -1;
        {                                           /* (the UI asked for a section it checked: no hash here) */
            arrangement_apply(s);
            song.rec = 0;                           /* (a take does not run on into another section) */
            live_sec = (int8_t)s;
            seq_reset_tracks(clk_pos);              /* on the bar: every track from its step 0 */
            live_bar = 0;
            if (srec == 2u)
                srec_add(s);
        }
    }
    if (srec == 1u && live_sec >= 0) {
        srec = 2;
        srec_n = 0;
        srec_add((uint32_t)live_sec);
    }
}
#endif

/* every track from its step 0, together, at pos units into beat 0 (a song section: the arranger's
 * remainder, so the new section starts exactly on its bar) */
static void seq_reset_tracks(uint32_t pos)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        t->seq_abs = SEQ_NONE;
        t->seq_idx = 0;
        t->rskip_n = 0;
        t->rskip_lanes = 0;
        t->rskip_abs = SEQ_NONE;
        t->rh_n = 0;
        t->arp_new = t->nheld != 0;
    }
    for (i = 0; i < NROLL; i++)
        roll[i].last = SEQ_NONE - 1u;               /* (a roll held over the start: on the grid from here) */
    clk_beat = 0;
    clk_pos = pos;
#if FELUCCA_ARRANGER
    live_bar = 0xFFFFFFFFu;                        /* (bar 0 is a new bar: SONG REC can start on it) */
#endif
    click_last = SEQ_NONE;
    song.tick = 0;
    song.playing = 1;
    slicer_start(pos);                             /* slicer.c: its step 0 with the sequencer's */
}


static void song_backup(void);                     /* project.c: song mode keeps the loop you made */
static void song_restore(void);

static void seq_start(void)
{
#if FELUCCA_ARRANGER
    if (arrangement_enabled && !song.playing) {
        rec_wait = 0;                              /* song mode plays, it does not record */
        song_backup();
    }
    if (!arrangement_start()) return;
#endif
    seq_reset_tracks(0);
}

static void seq_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        trk_note_off(t, t->seq_notes[i]);
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;                             /* live MONO / LEG keys must not glide after it */
}

static void seq_stop(void)
{
    uint32_t i;
#if FELUCCA_ARRANGER
    if (song.playing)
        srec_stop();                                /* SONG REC: the order played so far is the song */
    live_req = -1;
#endif
    song.playing = 0;
    if (punch_latch && !punch.keybit)
        punch.req = -1;                            /* STOP ends a latched punch-in effect */
    for (i = 0; i < NTRK; i++) {
        seq_release(&trk[i]);
        trk[i].rh_n = 0;                           /* a recorded note held over the stop: as far as it got */
    }
#if FELUCCA_ARRANGER
    if (arrangement_clock.running) {
        arrangement_clock.running = 0;
        song_restore();                            /* back to the loop you were making */
    }
#endif
}

/* the velocity of note i of synth step s */
static uint32_t step_vel(const step_t *s, uint32_t i)
{
    uint32_t base = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    return lvl_vel((s->lvl >> (2u * i)) & 3u, base);
}

/* play one synth step: TIE extends, REST releases, NOTE (re)triggers; a SLIDE on the previous
 * step makes this one legato with a glide (acid style). skip: bit k = note k already sounds
 * from live recording (not triggered, not released here). len: the step's length (units). */
static void seq_step(track_t *t, const step_t *s, uint32_t slen, uint32_t skip)
{
    uint32_t i, j, gate = slen * (uint32_t)t->p[P_SGATE] / 128u;
    uint32_t slide_in = t->seq_hold && t->seq_n;
    uint32_t next_tie = t->step[(t->seq_idx + 1u) % trk_len(t)].time == ST_TIE;
    if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = gate + slen / 2u;
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;   /* chains hold at any GATE / swing */
        }
        return;
    }
    if (s->time == ST_REST || !s->n) {
        seq_release(t);
        return;
    }
    if (s->rat) {                                   /* ratchets: each hit its share of the step */
        uint32_t hits = 1u + ((s->rat >> 0) & 3u);
        for (i = 1; i < s->n; i++)
            if (1u + ((s->rat >> (2u * i)) & 3u) > hits)
                hits = 1u + ((s->rat >> (2u * i)) & 3u);
        gate /= hits;
    }
    t->slide_glide = (uint8_t)slide_in;
    if (!slide_in)
        seq_release(t);
    for (i = 0; i < s->n; i++)
        if (roll_has(t, s->note[i]))
            skip |= 1u << i;                        /* (a roll plays it) */
    for (i = 0; i < s->n; i++)
        if (!((skip >> i) & 1u))
            trk_note_on(t, s->note[i], step_vel(s, i));
    if (slide_in)                                   /* release what is not held over */
        for (i = 0; i < t->seq_n; i++) {
            for (j = 0; j < s->n && s->note[j] != t->seq_notes[i]; j++)
                ;
            if (j == s->n)
                trk_note_off(t, t->seq_notes[i]);
        }
    t->seq_n = 0;
    for (i = 0; i < s->n; i++)
        if (!((skip >> i) & 1u))
            t->seq_notes[t->seq_n++] = s->note[i];
    t->seq_off = gate;
    t->seq_hold = !s->rat && ((s->flags & SF_SLIDE) != 0 || next_tie);   /* next step a TIE: keep the notes to it */
}

/* play one drum step: each lane a hit (skip: lanes already played by live recording, or rolling) */
static void drum_step(track_t *t, const dstep_t *s, uint32_t skip)
{
    uint32_t l, m = dstep_mask(s) & ~skip & ~roll_lanes(t);
    for (l = 0; m; l++, m >>= 1)
        if (m & 1u)
            trk_note_on(t, LANE_NOTE[l], lvl_vel(dstep_lvl(s, l), 100));
}

/* ratchets: the further hits of the playing step's notes / lanes, each at its share of the step */
static void seq_ratchets(track_t *t, uint32_t into, uint32_t slen)
{
    uint32_t i;
    if (is_drum(t)) {
        const dstep_t *s = &t->dstep[t->seq_idx % NSTEP];
        uint32_t m = dstep_mask(s) & ~roll_lanes(t);
        for (i = 0; m; i++, m >>= 1) {
            uint32_t hits = 1u + dstep_rat(s, i), h, done;
            if (!(m & 1u) || hits == 1u)
                continue;
            h = into * hits / slen;
            done = (t->rat_lanes >> (2u * i)) & 3u;
            if (h > done && h < hits) {
                t->rat_lanes = (t->rat_lanes & ~(3u << (2u * i))) | h << (2u * i);
                trk_note_on(t, LANE_NOTE[i], lvl_vel(dstep_lvl(s, i), 100));
            }
        }
        return;
    }
    {
        const step_t *s = &t->step[t->seq_idx % NSTEP];
        if (s->time != ST_NOTE || !s->rat)
            return;
        for (i = 0; i < s->n; i++) {
            uint32_t hits = 1u + ((s->rat >> (2u * i)) & 3u), h;
            if (hits == 1u || roll_has(t, s->note[i]))
                continue;
            h = into * hits / slen;
            if (h > t->rat_done[i] && h < hits) {
                uint32_t j;
                t->rat_done[i] = (uint8_t)h;
                trk_note_off(t, s->note[i]);
                trk_note_on(t, s->note[i], step_vel(s, i));
                t->seq_off = slen / hits * (uint32_t)t->p[P_SGATE] / 128u;
                for (j = 0; j < t->seq_n && t->seq_notes[j] != s->note[i]; j++)
                    ;
                if (j == t->seq_n && t->seq_n < 4u)
                    t->seq_notes[t->seq_n++] = s->note[i];   /* (its gate ends it) */
            }
        }
    }
}

static void seq_tick(track_t *t, uint32_t adv)
{
    uint32_t len = trk_len(t), into, slen, abs, idx;
    if (t->seq_n && !t->seq_hold) {
        if (t->seq_off <= adv)
            seq_release(t);
        else
            t->seq_off -= adv;
    }
    if (!song.playing)
        return;
    abs = trk_grid(t, &into, &slen);
    if (t->seq_abs != SEQ_NONE && abs + 1u == t->seq_abs)
        abs = t->seq_abs;                            /* SWING turned up inside a played odd step */
    if (abs != t->seq_abs) {                         /* a new step: one a block at most */
        t->seq_abs = abs;
        idx = abs % len;
        t->seq_idx = (uint16_t)idx;
        t->rat_done[0] = t->rat_done[1] = t->rat_done[2] = t->rat_done[3] = 0;
        t->rat_lanes = 0;
        if (!idx)
            t->pass++;                               /* a new pass of the loop (recording: one undo) */
        if (erasing(t))
            erase_step(t, idx);                      /* EDIT + key held: gone as it passes */
        if (is_drum(t)) {
            uint32_t skip = t->rskip_abs == abs ? t->rskip_lanes : 0u;
            t->rskip_lanes = 0;
            drum_step(t, &t->dstep[idx], skip);
        } else {
            const step_t *s = &t->step[idx];
            uint32_t skip = 0, i, k;
            rec_hold(t, idx, len, abs);
            if (t->rskip_n && t->rskip_abs == abs)
                for (i = 0; i < s->n; i++)
                    for (k = 0; k < t->rskip_n; k++)
                        if (s->note[i] == t->rskip[k])
                            skip |= 1u << i;
            t->rskip_n = 0;
            seq_step(t, s, slen, skip);
        }
    }
    seq_ratchets(t, into, slen);
}

/* MIDI in: the track a channel plays (0..15) */
static track_t *midi_track(uint32_t ch)
{
    if (song.g[G_DRCH] && ch + 1u == (uint32_t)song.g[G_DRCH])
        return TDRUM;
    return ch < NPART ? &trk[ch] : TSEL;
}

/* a channel that plays the selected track: its note-off goes to the track its note-on went to,
 * even when another track was selected in between (else that note would hang) */
static uint8_t midi_sel_on[16][128];                  /* per channel and note: track + 1, 0 = none */
static track_t *midi_route(uint32_t ch, uint32_t note, int on)
{
    track_t *t = midi_track(ch);
    if (ch < NPART || (song.g[G_DRCH] && ch + 1u == (uint32_t)song.g[G_DRCH]))
        return t;                                     /* a part's own channel, or the drum channel */
    if (on)
        midi_sel_on[ch & 15u][note & 127u] = (uint8_t)(song.sel + 1u);
    else if (midi_sel_on[ch & 15u][note & 127u]) {
        t = &trk[(midi_sel_on[ch & 15u][note & 127u] - 1u) % NTRK];
        midi_sel_on[ch & 15u][note & 127u] = 0;
    }
    return t;
}

/* MIDI clock in (GLO > SYSTEM > SYNC = USB or TRS; after Felucca 1.0's midi_clock.c, from contributions by
 * ChanceTheMaker and keremimo): 24 pulses a beat. While the clock runs, the sequencer advances by the
 * pulses (a pulse = BEAT_U / 24 units), interpolated up to the next one from the last interval but never
 * past it, so it follows the master's tempo changes and cannot drift; BPM shows the master's tempo
 * (the slicer, delay and arp follow it). START restarts from the top, CONTINUE carries on where it
 * stopped, STOP stops. With no pulse for 0.5 s, the internal tempo takes over (PLAY works as ever). */
#define MCLK_PULSE_U (BEAT_U / 24u)
static struct {
    uint32_t pos, done;          /* units: the master's position (pulses since START), ours */
    uint32_t last_ms, iv_ms;     /* the last pulse, the interval between pulses (smoothed) */
    uint32_t beat_ms;            /* when pulse 0 of the last 24 came: the tempo */
    uint8_t have, n24;           /* a pulse since START; pulses towards the next tempo reading */
    uint8_t alive;               /* pulses are coming (from the SYNC source) */
} mclk;

static int mclk_on(void)                          /* the clock drives the sequencer */
{
    return song.g[G_SYNC] && mclk.alive && fm1_ms - mclk.last_ms < 500u;
}

static void mclk_event(uint32_t st, uint32_t src)  /* a realtime message; src 1 USB, 2 TRS */
{
    uint32_t now = fm1_ms;
    if (!song.g[G_SYNC] || src != (uint32_t)song.g[G_SYNC])
        return;
    if (st == 0xFAu || st == 0xFBu) {              /* START: from the top; CONTINUE: on from where it stopped */
        mclk.pos = mclk.done = 0;
        mclk.have = 0;
        if (st == 0xFAu)
            transport_req = 1;
        else if (!song.playing)
            song.playing = 1;
        return;
    }
    if (st == 0xFCu) {                             /* STOP */
        transport_req = 2;
        return;
    }
    if (st != 0xF8u)
        return;
    if (mclk.alive && now - mclk.last_ms < 200u) { /* the interval, smoothed (a gap is not a tempo) */
        uint32_t iv = now - mclk.last_ms;
        mclk.iv_ms = mclk.iv_ms ? (mclk.iv_ms * 3u + iv + 2u) / 4u : iv;
    }
    if (!mclk.alive || now - mclk.last_ms >= 500u) {   /* (re)started: count a fresh beat */
        mclk.n24 = 0;
        mclk.beat_ms = now;
    } else if (++mclk.n24 == 24u) {                /* a beat: the tempo */
        uint32_t dt = now - mclk.beat_ms;
        mclk.n24 = 0;
        mclk.beat_ms = now;
        if (dt >= 250u && dt <= 1500u)             /* 40..240 BPM */
            song.g[G_BPM] = (int16_t)clamp((int32_t)((60000u + dt / 2u) / dt), 40, 240);
    }
    mclk.alive = 1;
    mclk.last_ms = now;
    if (song.playing || transport_req == 1u) {     /* (a START queued with it: the next block starts) */
        if (mclk.have)
            mclk.pos += MCLK_PULSE_U;
        mclk.have = 1;                             /* the first pulse after START is the downbeat */
    }
}

static uint32_t mclk_adv(uint32_t n)               /* units to advance this block (mclk_on) */
{
    uint32_t el, off = 0, tgt, adv, cap = n * 2u * (uint32_t)song.g[G_BPM];
    if (!mclk.have)
        return 0;                                  /* START seen: wait for the downbeat */
    el = fm1_ms - mclk.last_ms;
    if (mclk.iv_ms) {
        if (el > mclk.iv_ms)
            el = mclk.iv_ms;
        off = el * MCLK_PULSE_U / mclk.iv_ms;      /* (<= 200 x 110250: fits 32 bits) */
        if (off >= MCLK_PULSE_U)
            off = MCLK_PULSE_U - 1u;
    }
    tgt = mclk.pos + off;
    adv = (int32_t)(tgt - mclk.done) > 0 ? tgt - mclk.done : 0u;
    if (adv > cap)
        adv = cap;                                 /* behind: catch up at twice the tempo, no burst */
    mclk.done += adv;
    return adv;
}

/* everything that happens between two rendered blocks: transport, input, the steps of every
 * track at the clock, the click, the rolls and the arps; then the clock moves on by n samples */
static void events_block(uint32_t n)
{
    uint32_t i, pr, adv;
    if (transport_req == 1u) {
        transport_req = 0;
        if (ft_on) {
            ft_close();                             /* (PLAY from elsewhere: the editor) */
        } else if (ci_on) {
            ci_on = 0;                              /* PLAY again during the count-in: back to armed */
        } else if (rec_wait && rec_count && !song.playing && !mclk_on() &&
                   !(project_empty() && !rec_tempo)) {
            ci_on = 1;                              /* COUNT: one bar of clicks first (below) */
            ci_u = 0;
            ci_beat = 0;
            drum_on(77u, 120u);
        } else {
            seq_start();
            if (rec_wait && song.playing)
                rec_begin();                        /* PLAY while armed: record from the top */
        }
    } else if (transport_req == 2u) {
        seq_stop();
        transport_req = 0;
        song.rec = 0;                               /* STOP ends the take (and the wait) */
        rec_wait = 0;
        if (ft_on) {
            ft_on = 0;                              /* a free take: dropped */
            ft_bars = 0xFF;
        }
    }
    if (ci_on) {                                    /* the count-in: 4 beats at the tempo, then go */
        if (!rec_wait || song.playing) {
            ci_on = 0;                              /* (REC cancelled it, or it started some other way) */
        } else {
            uint32_t b;
            ci_u += n * (uint32_t)song.g[G_BPM];
            b = ci_u / BEAT_U;
            if (b >= 4u) {
                ci_on = 0;
                seq_start();
                if (song.playing)
                    rec_begin();
            } else if (b != ci_beat) {
                ci_beat = (uint8_t)b;
                drum_on(76u, 72u);
            }
        }
    }
    if (rec_wait && song.playing)
        rec_begin();                                /* started some other way: record now */
    adv = mclk_on() && song.playing ? mclk_adv(n) : n * (uint32_t)song.g[G_BPM];   /* (after a START) */
    ft_block();
#if FELUCCA_ARRANGER
    if (song.playing && arrangement_clock.running) {
        int scene = arr_next(&arrangement_clock, &arrangement, FS);
        if (scene == ARR_DONE) {
            arrangement_clock.running = 1;          /* (arr_next cleared it: seq_stop brings the loop back) */
            seq_stop();
        }
        else if (scene >= 0) {
            arrangement_apply((uint32_t)scene);
            seq_reset_tracks(arrangement_clock.phase);   /* (the remainder: exactly on the bar) */
        }
    } else if (song.playing) {
        live_block();
    }
#endif
    pr = panic_req;
    panic_req = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if ((pr >> i) & 1u) {
            trk_all_off(t);
            t->nheld = 0;
            t->arp_phys = 0;
            t->arp_note = 0;
        }
        if (i < NPART)
            engine_block(t);                          /* engine switch: fade, then switch (voice.c) */
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !t->p[P_AMODE]) || (t->aholdp && !t->p[P_AHOLD] && !t->arp_phys)) {
            t->nheld = 0;
            if (!t->p[P_AMODE])
                t->arp_phys = 0;
            if (t->arp_note) {
                trk_note_off(t, t->arp_note);
                t->arp_note = 0;
            }
        }
        t->armp = t->p[P_AMODE];
        t->aholdp = t->p[P_AHOLD];
    }
    keyboard_block();
    while (mi_r != mi_w) {                            /* USB-MIDI (and TRS) in */
        uint32_t pkt = midi_in_q[mi_r % MQ], st = (pkt >> 8) & 0xF0u, ch = (pkt >> 8) & 0x0Fu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        track_t *t;
        mi_r++;
        if ((pkt & 15u) == 0xFu) {                    /* clock / transport: cable 0 USB, 1 TRS */
            mclk_event((pkt >> 8) & 0xFFu, ((pkt >> 4) & 15u) ? 2u : 1u);
            continue;
        }
        if (st != 0x90u && st != 0x80u)
            continue;
        t = midi_route(ch, d1, st == 0x90u && d2);
        if (is_drum(t)) {
            if (st == 0x90u && d2)
                drum_input(lane_of_note(d1), vel_lvl(d2), 0, 1);
        } else if (st == 0x90u && d2) {
            input_on(t, d1, d2);
        } else {
            input_off(t, d1);
        }
    }
    for (i = 0; i < NTRK; i++)
        seq_tick(&trk[i], adv);
    click_tick();
    roll_block(adv);
    for (i = 0; i < NPART; i++)
        arp_tick(&trk[i], adv);
    if (song.playing) {
        song.tick++;
        clk_pos += adv;
        while (clk_pos >= BEAT_U) {
            clk_pos -= BEAT_U;
            clk_beat++;
        }
#if FELUCCA_ARRANGER
        arr_elapse(&arrangement_clock, adv, 1u);   /* (units: n x BPM, or the MIDI clock) */
#endif
    }
}
