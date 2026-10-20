/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded undo/redo of FM-1 pattern edits. Included by seq.c.
 * Each stored pre-edit state swaps with the track to become the redo state.
 * Performance state, sound settings and mixer parameters are out of scope. */
#define UNDO_DEPTH 8u
typedef struct {
    uint32_t sess;
    uint8_t trk;
    int16_t len;
    step_t st[NSTEP];
    int8_t micro[NSTEP];
    plock_t lock[NLOCK];
    uint8_t fill[NSTEP / 4];
} undo_state_t;
_Static_assert(sizeof(undo_state_t) <= 832u, "FM-1 undo RAM budget");
static struct {
    uint8_t head, count, pos;
    undo_state_t entry[UNDO_DEPTH];
} undo;
static uint32_t undo_sess = 1u;
static undo_state_t *undo_at(uint32_t i) { return &undo.entry[(undo.head + i) % UNDO_DEPTH]; }
static int undo_can(int redo) { return redo ? undo.pos < undo.count : undo.pos > 0u; }
static uint32_t undo_last_sess(void)
{
    return undo.pos && undo.pos == undo.count ? undo_at(undo.pos - 1u)->sess : 0u;
}
static void undo_clear(void) { undo.head = undo.count = undo.pos = 0u; }
static void undo_mark(const track_t *t, uint32_t sess)
{
    uint32_t ti = trk_index(t);
    undo_state_t *s;
    if (undo.pos && undo.pos == undo.count) {
        s = undo_at(undo.pos - 1u);
        if (s->trk == ti && s->sess == sess)
            return;                                   /* same recording/knob session */
    }
    undo.count = undo.pos;                            /* a new edit discards redo */
    if (undo.count == UNDO_DEPTH) {
        undo.head = (uint8_t)((undo.head + 1u) % UNDO_DEPTH);
        undo.count--;
        undo.pos--;
    }
    s = undo_at(undo.count);
    memcpy(s->st, t->step, sizeof s->st);
    memcpy(s->micro, t->micro, sizeof s->micro);
    memcpy(s->lock, t->lock, sizeof s->lock);
    memcpy(s->fill, t->fill, sizeof s->fill);
    s->len = t->p[P_SLEN];
    s->trk = (uint8_t)ti;
    s->sess = sess;
    undo.count++;
    undo.pos = undo.count;
}
/* UI disables audio IRQ around swaps; sequencer host tests may call directly. */
static int undo_apply(int redo)
{
    undo_state_t *s;
    track_t *t;
    uint32_t i;
    int16_t len;
    if (!undo_can(redo)) return 0;
    s = undo_at(redo ? undo.pos : undo.pos - 1u);
    t = &trk[s->trk % NTRK];
    for (i = 0; i < NSTEP; i++) {
        step_t tmp = t->step[i];
        int8_t m = t->micro[i];
        t->step[i] = s->st[i]; s->st[i] = tmp;
        t->micro[i] = s->micro[i]; s->micro[i] = m;
    }
    for (i = 0; i < NLOCK; i++) {
        plock_t l = t->lock[i];
        t->lock[i] = s->lock[i]; s->lock[i] = l;
    }
    for (i = 0; i < NSTEP / 4u; i++) {
        uint8_t x = t->fill[i];
        t->fill[i] = s->fill[i]; s->fill[i] = x;
    }
    len = t->p[P_SLEN]; t->p[P_SLEN] = s->len; s->len = len;
    if (redo) undo.pos++;
    else undo.pos--;
    return 1;
}
