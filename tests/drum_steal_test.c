/* SLOOP 2.8.3: drum voices on a busy beat (kick on the beat, 16th hats, open hats, snare + clap, crash, ride, a tom
 * fill), every kit: no kick is cut short by another sound taking its voice (NDRUM voices; up to 2.8.2 the oldest went,
 * kick or not: 11 to 13 kicks of 16 cut on the 808, GM and AMBIENT kits; a tester's report). A kick re-hit while it
 * still sounds may take its own voice (as on a drum machine). Exit status: the kits that fail */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"
static void block(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
typedef struct { uint32_t age, start, alive; } kk_t;
static uint32_t early, hits, lost, lostk;
static uint32_t slot_age[16], slot_t0[16];
static uint32_t run_kit(uint32_t kit, int dense, uint32_t *kicks, uint32_t *natural_ms)
{
    uint32_t step, b, i, cut = 0, nk = 0, SB = FS / 8u / CTL, now = 0;
    kk_t k[64];
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    TDRUM->p[P_E0] = (int16_t)kit;
    TDRUM->p[P_MUTE] = 0;
    for (i = 0; i < 20; i++) block();
    /* the kick alone: how long it sounds */
    drum_on(36, 110);
    for (b = 0; b < FS * 2u / CTL; b++) { int a = 0; for (i = 0; i < NDRUM; i++) a |= drums.v[i].active && drums.v[i].note == 36; if (!a) break; block(); }
    *natural_ms = b * CTL * 1000u / FS;
    for (i = 0; i < 40; i++) block();
    for (step = 0; step < 64; step++) {
        uint32_t s = step % 16;
        uint8_t notes[10]; uint32_t n = 0;
        if (s % 4 == 0) notes[n++] = 36;                     /* kick */
        if (dense) {
            if (s == 0) notes[n++] = 49;                     /* crash, each bar */
            if (s == 4 || s == 12) { notes[n++] = 38; notes[n++] = 39; }   /* snare + clap */
            notes[n++] = (s % 4 == 2) ? 46 : 42;             /* hats: open on the off-beat */
            if (s % 2 == 0) notes[n++] = 51;                 /* ride, 8ths */
            if (step >= 56 && s % 2 == 1) notes[n++] = s < 12 ? 48 : 43;   /* a tom fill */
        } else {
            notes[n++] = 42;
            if (s == 4 || s == 12) notes[n++] = 38;
        }
        uint32_t ages[10];
        for (i = 0; i < n; i++) {
            drum_on(notes[i], 100);
            ages[i] = drums.age;
            if (notes[i] == 36) {
                uint32_t v;
                for (v = 0; v < NDRUM; v++) if (drums.v[v].active && drums.v[v].note == 36 && drums.v[v].age == drums.age) break;
                k[nk].age = drums.age; k[nk].start = now; k[nk].alive = 1; nk++;
            }
        }
        for (i = 0; i < n && dense; i++) {             /* a hit of this step already gone: never heard */
            uint32_t v, f = 0;
            for (v = 0; v < NDRUM; v++) f |= drums.v[v].age == ages[i] && drums.v[v].active;
            if (!f && notes[i] != 46) { lost++; if (notes[i] == 36) lostk++; }
        }
        for (b = 0; b < SB; b++) {
            block(); now++;
            { uint32_t v; for (v = 0; v < NDRUM; v++) { if (drums.v[v].age != slot_age[v]) { if (slot_age[v] && dense && (now - slot_t0[v]) * CTL * 1000u / FS < 120u && drums.v[v].note != 0) early++; slot_age[v] = drums.v[v].age; slot_t0[v] = now; if (dense) hits++; } } }
            for (i = 0; i < nk; i++) {
                uint32_t v, found = 0, ended = 1;
                if (!k[i].alive) continue;
                for (v = 0; v < NDRUM; v++) if (drums.v[v].age == k[i].age) { found = 1; ended = !drums.v[v].active; }
                if (!found) {      /* its voice went to another hit */
                    uint32_t self = 0;
                    for (v = 0; v < NDRUM; v++) if (drums.v[v].active && drums.v[v].note == 36 && drums.v[v].age > k[i].age) self = 1;
                    if (self) { k[i].alive = 0; continue; }
                    k[i].alive = 0;
                    if ((now - k[i].start) * CTL * 1000u / FS < *natural_ms * 9u / 10u) cut++;
                    k[i].alive = 0;
                } else if (ended) k[i].alive = 0;
            }
        }
    }
    *kicks = nk;
    return cut;
}
int main(void)
{
    uint32_t kit, kicks, nat, c1, c2, fails = 0;
    host_tracks_init();
    printf("NDRUM %u\n", NDRUM);
    for (kit = 0; kit < DRUM_USR; kit++) {
        c1 = run_kit(kit, 0, &kicks, &nat);
        c2 = run_kit(kit, 1, &kicks, &nat);
        lost = lostk = early = hits = 0;
        printf("drums: kit %2u (%-8s) kick %4u ms; kicks cut by another sound: simple beat %u/%u, busy beat %u/%u %s\n", kit,
               kit >= DRUM_SAMPLED ? DS_KITS[kit - DRUM_SAMPLED].name : "GM", nat, c1, kicks, c2, kicks, c1 || c2 ? "FAIL" : "ok");
        fails += c1 || c2;
    }
    printf("drums: %u kits failed\n", fails);
    return (int)fails;
}
