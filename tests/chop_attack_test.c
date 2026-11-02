/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.8.3: a sample (SAMPLE, SLICE) played again on a voice still sounding starts over from silence, with its ATK
 * (voice.c voice_start, VP_ON). A user's chopped breaks with ATK set to soften the chops' starts: the attack worked
 * only on different notes in POLY; the same note again in POLY, and every note in MONO / LEGATO / UNISON, started at
 * the level the voice had (a click, ATK ignored). Checks, per voice mode: the second chop's level over its first
 * blocks (it starts low and rises with ATK), and the largest step between two output samples around its start
 * against the largest within the chop (no click). LEGATO with the first chop still held glides on by design.
 * Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
static void check(int ok, const char *what) { printf("chop: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static int32_t outb[CTL * 2];
static int32_t jump_max, last;
static void block(void)
{
    uint32_t i;
    mix_block(outb, CTL);
    for (i = 0; i < CTL; i++) {
        int32_t d = outb[2 * i] - last;
        d = d < 0 ? -d : d;
        if (d > jump_max) jump_max = d;
        last = outb[2 * i];
    }
}
static voice_t *voice_of(track_t *t, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if ((t->v[i].active || t->v[i].pend) && t->v[i].note == n)
            return &t->v[i];
    return 0;
}
static void run(int mode, uint32_t n1, uint32_t n2, const char *what)
{
    track_t *t = &trk[1];
    uint32_t k;
    double lv[8];
    int32_t steady, start;
    char b[200];
    host_tracks_init();
    host_preset(t, 4, 0);                               /* SAMPLE */
    for (k = 0; k < NVOICE; k++) memset(&t->v[k], 0, sizeof t->v[k]);
    t->nmono = 0; t->mono_note = 0;
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_ATK] = 60; t->p[P_SUS] = 127; t->p[P_REL] = 90;
    for (k = 0; k < 8; k++) block();
    trk_note_on(t, n1, 100);
    for (k = 0; k < 40; k++) block();
    jump_max = 0;
    for (k = 0; k < 80; k++) block();                   /* the chop as it plays: its own largest step */
    steady = jump_max;
    trk_note_off(t, n1);
    for (k = 0; k < 3; k++) block();                    /* its release still sounding */
    jump_max = 0;
    trk_note_on(t, n2, 100);
    for (k = 0; k < 8; k++) {
        voice_t *v;
        block();
        v = voice_of(t, n2);
        lv[k] = v && v->active && v->stage != 4u ? v->env * 100.0 / (1 << 24) : -1.0;
    }
    start = jump_max;
    snprintf(b, sizeof b, "%s: the 2nd chop starts from silence with its ATK (%.1f %% .. %.1f %%)", what, lv[2], lv[7]);
    check(lv[2] >= 0 && lv[2] < 5.0 && lv[7] > lv[2], b);
    snprintf(b, sizeof b, "%s: no click at its start (largest step %d, in the chop %d)", what, start, steady);
    check(start <= steady + steady / 4, b);
}
int main(void)
{
    run(V_POLY, 60, 60, "POLY, the same chop again");
    run(V_POLY, 60, 62, "POLY, another chop");
    run(V_MONO, 60, 62, "MONO, another chop");
    run(V_LEGATO, 60, 62, "LEGATO, another chop");
    run(V_UNISON, 60, 62, "UNISON, another chop");
    printf("chop: %d failed\n", fails);
    return fails;
}
