/* SPDX-License-Identifier: GPL-3.0-only */
/* Chords on screen: what a synth track is playing, named. Its notes now (its voices with the gate
 * on, and the notes it started since the last frame, so a short sequencer note is not missed) and their chord:
 * C, C MIN, C7, C MAJ7, A MIN/C... The TRACKS screen shows it on the track's row while it plays; the CHORDS
 * visualiser (ui_vis.c) shows it big, with its notes, its degree in the track's scale and a keyboard.
 * It follows the selected track (the drum track selected: the synth track selected before it). Let go of the
 * keys and the chord stays (CHORDS: dimmed) until the next notes; letting go of them one by one does not rename
 * it on the way. The font has capitals only, so the qualities are spelt out (MIN, MAJ7) where lead sheets
 * write m and maj: CM7 would read as either. Called from pads_tick (ui_studio.c), once a frame. */

#define CHD_HOLD 120u                         /* frames (~2 s) the TRACKS row keeps a chord after the last note */
#define CHD_MAXN 16u                          /* notes kept (voices, keys held, the ones started this frame) */

typedef struct {
    uint16_t mask;                              /* the pitch classes above the root, bit 0 = the root */
    const char *q;                              /* the quality, after the root's name */
} chd_shape_t;
#define CB(a) (1u << (a))
/* in order of preference: an inversion takes the first shape that fits (a root position always wins) */
static const chd_shape_t CHD_SHAPES[] = {
    {CB(0) | CB(4) | CB(7), ""},
    {CB(0) | CB(3) | CB(7), "MIN"},
    {CB(0) | CB(4) | CB(7) | CB(10), "7"},
    {CB(0) | CB(4) | CB(7) | CB(11), "MAJ7"},
    {CB(0) | CB(3) | CB(7) | CB(10), "MIN7"},
    {CB(0) | CB(3) | CB(6), "DIM"},
    {CB(0) | CB(4) | CB(8), "AUG"},
    {CB(0) | CB(5) | CB(7), "SUS4"},
    {CB(0) | CB(2) | CB(7), "SUS2"},
    {CB(0) | CB(7), "5"},
    {CB(0) | CB(3) | CB(6) | CB(10), "MIN7B5"},
    {CB(0) | CB(3) | CB(6) | CB(9), "DIM7"},
    {CB(0) | CB(5) | CB(7) | CB(10), "7SUS4"},
    {CB(0) | CB(4) | CB(7) | CB(9), "6"},
    {CB(0) | CB(3) | CB(7) | CB(9), "MIN6"},
    {CB(0) | CB(2) | CB(4) | CB(7) | CB(10), "9"},
    {CB(0) | CB(2) | CB(4) | CB(7) | CB(11), "MAJ9"},
    {CB(0) | CB(2) | CB(3) | CB(7) | CB(10), "MIN9"},
    {CB(0) | CB(2) | CB(4) | CB(10), "9"},      /* (no 5th: SLOOP's own 9TH chords, 1 3 7 9) */
    {CB(0) | CB(2) | CB(4) | CB(11), "MAJ9"},
    {CB(0) | CB(2) | CB(3) | CB(10), "MIN9"},
    {CB(0) | CB(2) | CB(4) | CB(7), "ADD9"},
    {CB(0) | CB(2) | CB(3) | CB(7), "MIN ADD9"},
    {CB(0) | CB(3) | CB(7) | CB(11), "MIN MAJ7"},
    {CB(0) | CB(4) | CB(8) | CB(10), "7#5"},
    {CB(0) | CB(4) | CB(6) | CB(10), "7B5"},
    {CB(0) | CB(1) | CB(4) | CB(7) | CB(10), "7B9"},
    {CB(0) | CB(2) | CB(4) | CB(7) | CB(9), "6/9"},
    {CB(0) | CB(4) | CB(10), "7"},              /* (no 5th) */
    {CB(0) | CB(4) | CB(11), "MAJ7"},
    {CB(0) | CB(3) | CB(10), "MIN7"},
};
#undef CB
#define NCHD_SHAPES (sizeof CHD_SHAPES / sizeof CHD_SHAPES[0])
static const char *const CHD_ROMAN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};

static struct {
    uint8_t trk;                                /* the track followed, 0xFF: none yet */
    uint8_t n, note[CHD_MAXN];                /* its notes, low to high (n = 0: nothing played yet) */
    uint8_t live;                               /* sounding now */
    uint8_t hold;                               /* frames the TRACKS row still shows them */
    uint8_t ok, root, bass;                     /* a chord was named: its root and lowest note (pitch classes) */
    const char *q;                              /* its quality */
    uint32_t sig;                               /* changes with what is shown */
} chd = {0xFFu, 0, {0}, 0, 0, 0, 0, 0, "", 0};

/* the chord of the pitch classes pcs (bit 0 = C) with bass the lowest: 1 and its root and quality, 0 if none
 * fits. A shape whose root is the bass wins (C6, not AM7/C); else the first that fits, over the bass (C/E) */
static int chd_find(uint32_t pcs, uint32_t bass, uint32_t *root, const char **q)
{
    uint32_t pass, s, r;
    for (pass = 0; pass < 2u; pass++)
        for (s = 0; s < NCHD_SHAPES; s++)
            for (r = 0; r < 12u; r++) {
                uint32_t rot;
                if (!((pcs >> r) & 1u) || (!pass && r != bass))
                    continue;
                rot = ((pcs >> r) | (pcs << (12u - r))) & 0xFFFu;
                if (rot == CHD_SHAPES[s].mask) {
                    *root = r;
                    *q = CHD_SHAPES[s].q;
                    return 1;
                }
            }
    return 0;
}

static uint32_t chd_bits(uint32_t m)
{
    uint32_t n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}

/* the chord's degree in track t's scale (a seven-note scale, the root in it): "IV", else "" */
UI_SMALL static const char *chd_degree(const track_t *t)
{
    uint32_t mask, rel;
    if (!chd.ok || !t->p[P_SCALE])
        return "";
    mask = SCALE_MASK[clamp(t->p[P_SCALE], 0, (int32_t)NSCALES - 1)];
    rel = (chd.root + 120u - (uint32_t)t->p[P_ROOT]) % 12u;
    if (chd_bits(mask) != 7u || !((mask >> rel) & 1u))
        return "";
    return CHD_ROMAN[chd_bits(mask & ((1u << rel) - 1u))];
}

/* a chord's name: root, quality (a space before a word), "/bass" when the bass is another note */
UI_SMALL static void chd_label_of(char *b, uint32_t size, uint32_t root, const char *q, uint32_t bass)
{
    str_cpy(b, N_NOTE[root % 12u], size);
    if (q[0] >= 'A' && q[0] <= 'Z')
        str_cpy(b + str_len(b), " ", size - str_len(b));
    str_cpy(b + str_len(b), q, size - str_len(b));
    if (bass != root) {
        str_cpy(b + str_len(b), "/", size - str_len(b));
        str_cpy(b + str_len(b), N_NOTE[bass % 12u], size - str_len(b));
    }
}

/* the chord as one line (the TRACKS row): "C# MIN7/E", "G7", a note alone "E3", else the notes "C D" */
UI_SMALL static void chd_label(char *b, uint32_t size)
{
    uint32_t i;
    b[0] = 0;
    if (!chd.n)
        return;
    if (chd.n == 1u) {
        note_name(b, chd.note[0]);
        return;
    }
    if (chd.ok) {
        chd_label_of(b, size, chd.root, chd.q, chd.bass);
        return;
    }
    for (i = 0; i < chd.n; i++) {                      /* no chord: the pitch classes, once each */
        uint32_t pc = chd.note[i] % 12u, j, seen = 0;
        for (j = 0; j < i; j++)
            seen |= chd.note[j] % 12u == pc;
        if (seen || str_len(b) + 4u >= size)
            continue;
        if (b[0])
            str_cpy(b + str_len(b), " ", size - str_len(b));
        str_cpy(b + str_len(b), N_NOTE[pc], size - str_len(b));
    }
}

UI_SMALL static void chd_name(void)
{
    uint32_t pcs = 0, i, root = 0;
    for (i = 0; i < chd.n; i++)
        pcs |= 1u << (chd.note[i] % 12u);
    chd.bass = (uint8_t)(chd.note[0] % 12u);
    chd.ok = chd_bits(pcs) >= 2u && chd_find(pcs, chd.bass, &root, &chd.q);
    chd.root = (uint8_t)(chd.ok ? root : chd.bass);
    if (!chd.ok)
        chd.q = "";
}

static void chd_add(uint8_t *c, uint32_t *k, uint32_t note)
{
    uint32_t i;
    if (note > 127u || *k >= CHD_MAXN)
        return;
    for (i = 0; i < *k; i++)
        if (c[i] == note)
            return;
    for (i = *k; i > 0 && c[i - 1u] > note; i--)        /* kept low to high */
        c[i] = c[i - 1u];
    c[i] = (uint8_t)note;
    (*k)++;
}

/* ---- the progression (the PROGRESSION visualiser, ui_vis.c): the last CHD_HIST chords the followed track played,
 * oldest first. A chord counts once it has been the one shown for CHD_STABLE frames (the shapes passed on the way
 * while keys go down one by one do not), and not again right after itself. A new track followed: a new list */
#define CHD_HIST 6u
#define CHD_STABLE 9u                           /* frames (~0.15 s) */
typedef struct {
    uint8_t root, bass;
    const char *q;
} chd_ent_t;
static struct {
    chd_ent_t e[CHD_HIST];
    uint8_t len, cnt;                           /* entries; frames the chord shown has stayed */
    chd_ent_t cand;                             /* that chord */
    uint32_t gen;                               /* changes with the list (the screen's signature) */
} chh;
UI_SMALL static int chd_ent_eq(const chd_ent_t *a, const chd_ent_t *b) { return a->root == b->root && a->bass == b->bass && a->q == b->q; }
UI_SMALL static void chd_hist_tick(void)
{
    chd_ent_t c;
    if (!chd.ok || chd.n < 2u) {
        chh.cnt = 0;
        return;
    }
    c.root = chd.root, c.bass = chd.bass, c.q = chd.q;
    if (!chh.cnt || !chd_ent_eq(&c, &chh.cand)) {
        chh.cand = c;
        chh.cnt = 1;
        return;
    }
    if (chh.cnt < 255u)
        chh.cnt++;
    if (chh.cnt != CHD_STABLE || (chh.len && chd_ent_eq(&c, &chh.e[chh.len - 1u])))
        return;
    if (chh.len == CHD_HIST) {
        uint32_t i;
        for (i = 1; i < CHD_HIST; i++)                  /* (the oldest goes) */
            chh.e[i - 1u] = chh.e[i];
        chh.len--;
    }
    chh.e[chh.len++] = c;
    chh.gen++;
}

/* (a seven-note scale on track t) the degree 0..6 of pitch class pc in it, else -1 */
UI_SMALL static int32_t chd_deg_of(const track_t *t, uint32_t pc)
{
    uint32_t mask, rel;
    if (!t->p[P_SCALE])
        return -1;
    mask = SCALE_MASK[clamp(t->p[P_SCALE], 0, (int32_t)NSCALES - 1)];
    rel = (pc + 120u - (uint32_t)t->p[P_ROOT]) % 12u;
    if (chd_bits(mask) != 7u || !((mask >> rel) & 1u))
        return -1;
    return (int32_t)chd_bits(mask & ((1u << rel) - 1u));
}
/* the triad on degree d of track t's seven-note scale (the scale's own thirds): 1 and its root and quality */
UI_SMALL static int chd_triad(const track_t *t, uint32_t d, chd_ent_t *e)
{
    uint32_t mask = SCALE_MASK[clamp(t->p[P_SCALE], 0, (int32_t)NSCALES - 1)], pc[7], n = 0, i, set = 0, root;
    const char *q;
    for (i = 0; i < 12u && n < 7u; i++)
        if ((mask >> i) & 1u)
            pc[n++] = ((uint32_t)t->p[P_ROOT] + i) % 12u;
    if (n != 7u)
        return 0;
    for (i = 0; i < 3u; i++)
        set |= 1u << pc[(d + 2u * i) % 7u];
    if (!chd_find(set, pc[d % 7u], &root, &q))
        return 0;
    e->root = (uint8_t)root, e->bass = (uint8_t)root, e->q = q;
    return 1;
}
/* where a chord on degree d tends to go next (I: IV V VI, II: V VII IV, ... ; the common moves of the key): up to 3 */
static const uint8_t CHD_NEXT[7][3] = {{3, 4, 5}, {4, 6, 3}, {5, 3, 1}, {4, 0, 1}, {0, 5, 3}, {3, 1, 4}, {0, 2, 5}};
/* the suggestions after the chord shown (n of them into e[3]); 0: no seven-note scale or a chord outside it */
UI_SMALL static uint32_t chd_suggest(const track_t *t, chd_ent_t *e)
{
    int32_t d;
    uint32_t i, n = 0;
    if (!chd.ok || chd.n < 2u || (d = chd_deg_of(t, chd.root)) < 0)
        return 0;
    for (i = 0; i < 3u; i++)
        n += (uint32_t)chd_triad(t, CHD_NEXT[d][i], &e[n]);
    return n;
}

/* once a frame: hits, the notes each synth part started since the last frame (pads_tick took them) */
UI_SMALL static void chd_tick(uint32_t (*hits)[4])
{
    uint32_t tr = song.sel, k = 0, i, j, fresh = 0, sub = 1, sig;
    uint8_t cur[CHD_MAXN];
    const track_t *t;
    if (tr >= NPART || is_drum(&trk[tr]))
        tr = chd.trk;                                   /* the drums: the synth track followed before */
    if (tr >= NPART)
        return;
    if (tr != chd.trk) {
        chd.trk = (uint8_t)tr;
        chd.n = chd.live = chd.hold = chd.ok = 0;
        chh.len = chh.cnt = 0;                          /* (another track: its own progression) */
        chh.gen++;
    }
    t = &trk[tr];
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u)
            chd_add(cur, &k, t->v[i].note);
    for (i = 0; i < t->nmono && i < 8u; i++)            /* MONO, LEGATO, UNISON: every key held, not only the one */
        chd_add(cur, &k, t->mono_stack[i]);             /* that sounds (a chord played one key after another) */
    if (t->p[P_AMODE])
        for (i = 0; i < t->nheld && i < 16u; i++)       /* the arp: the chord it holds, not its one note now */
            chd_add(cur, &k, t->held[i]);
    for (i = 0; i < 128u; i++)
        if ((hits[tr][i >> 5] >> (i & 31u)) & 1u) {
            chd_add(cur, &k, i);
            fresh = 1;
        }
    if (k) {
        for (i = 0; i < k && sub; i++) {               /* only letting go: the chord stays as it was */
            for (j = 0; j < chd.n && chd.note[j] != cur[i]; j++)
                ;
            sub = j < chd.n;
        }
        if (fresh || !chd.live || !sub) {
            memcpy(chd.note, cur, k);
            chd.n = (uint8_t)k;
            chd_name();
        }
        chd.live = 1;
        chd.hold = CHD_HOLD;
    } else {
        chd.live = 0;
        if (chd.hold)
            chd.hold--;
    }
    chd_hist_tick();
    sig = 1u + tr + chd.live * 4u + chd.n * 8u + (chd.hold > 0u) * 1024u;
    for (i = 0; i < chd.n; i++)
        sig = sig * 131u + chd.note[i];
    chd.sig = sig;
}
