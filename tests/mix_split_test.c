/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* The render split over the two cores (fx.c mix_units): the same output, bit for bit, whatever the split.
 * Each scene renders about two seconds with cpu1_units = 0 (every unit on this core, as SLOOP 2.5 renders) and
 * again with every other split: each unit alone, pairs, all four, the firmware's own choice (-1); each render
 * in a fresh fork()ed child of a process that never touched the DSP state, as tests/regress.c does. The FNV-1a
 * hash of the output must be the one of 0 every time. The scenes are the ones whose units share state:
 *   formant3   three FORMANT parts (one breath-noise xorshift for all their voices: they all stay on core 0,
 *              only the drums go), chords, the drums through the drum track's FILTER, the metronome
 *   fm6x3      three FM6 parts (the operator buses), chords
 *   phys_lfo   three PHYS parts with MODEL changes under sounding notes, S&H LFOs on every part (rng() draws
 *              inside the render), SYMP and the small layout
 *   mix        ANALOG, GRAIN, DRAWBAR with DIST and the sends; mutes and a solo coming and going; the drums
 *              through the SLICER (GATE, then STUT) and the drum track's FILTER (low-pass, then high-pass);
 *              the metronome
 *   fx26       SLOOP 2.6: an INSERT on every part (SOFT, CRUSH, FLANG) and on the drum track (FUZZ: first with its
 *              FILTER, then without), LFO 2 in SYNC with S&H waves (rng() draws at the wraps, timed by the tempo),
 *              UNI and FREE, ENV SYNC on every part (the envelope steps are per part), ANALOG's filter as BP then
 *              HP, a preset loaded under the sounding notes of two parts in one block (both fade with their old
 *              sound at once; after 2.7.1 a fading part stays on core 0: fx.c units_split, tests/cpu1_hold_test.c),
 *              TEXTURE (VINYL, ducked), REV THROW then ECHO THROW, the punch-in
 *              FREEZE, a pitch bend, live drum mutes, the metronome
 * (after 2.7.1) the engines, kits and fades the scenes above never put on core 1, each part with a slow LFO (a wrap
 * keeps a part on core 0 for its block: fx.c units_split), the notes long and close (the parts sound in most blocks):
 *   dpl        DIGITAL, PHASE, LOFI; the SLICER on a part (GATE, then STUT), a part's FILTER (low-pass, then
 *              high-pass); the drums on the GM sample kit's treatments (DUST, then DEEP, then ACOUSTIC)
 *   stn        SAMPLE (a factory set), TRIO, NOISE (its MODE turned under the notes); the SLICER on a part (STUT,
 *              then GATE), a part's FILTER (high-pass, then low-pass); the drums on SYN2, then SYN4 (your synthesised
 *              kits; never stored: copies of 909 and TECHNO)
 *   formant1   one FORMANT part alone (it goes: only two or more stay on core 0), ANALOG, SAMPLE on a user slot
 *              (USR2); the drums on a user kit (USR1, then USR3+4); the slots a RAM image, as tests/userkit_test.c
 *   swap_a     engines switched under sounding notes (ui.c set_engine_of / apply_preset_to: each part fades out on its
 *              old engine, on core 0, while the other units render on core 1): ANALOG -> DRAWBAR; FM6 -> PHYS, the
 *              MODEL turned in the middle of the fade; PHYS SYMP (the big layout) -> NOISE, NOISE's MODE turned in the
 *              middle of the fade (the fade renders PHYS with pe_old); PHYS -> PHYS SYMP (a preset), the MODEL turned
 *              in the middle of the fade; NOISE -> FM6 (its patch after the fade: fm6_poll, the main loop's, every
 *              8 blocks: the part waits faded a few blocks); DRAWBAR -> PHYS. The notes of the next step come in the
 *              middle of each fade (they wait for the new sound: voice.c xp_note)
 *   swap_b     SAMPLE (USR2) -> GRAIN on USR2 -> SAMPLE; PHYS MEMB -> SYMP with the MODEL turned to STRNG in the
 *              middle of the fade, then -> MODAL with it turned to SYMP in the middle (the layouts both ways);
 *              FORMANT -> PHYS MEMB, its MODEL turned to SYMP under three notes, then a MODAL preset (the fade renders
 *              SYMP's layout with a third voice sounding: 2.7.1's phys_render_draws read past the part's arena there;
 *              a fading part no longer reaches it, the ASan run guards the bound if one ever does),
 *              then -> FORMANT; PHYS -> GRAIN (the part's engine arena handed over); the drums on SYN1
 * Runs under every mode of the host double of core 1 (tests/cpu1_host.c: CORES, CHAOS, CPU1_TIMEOUT_AT), under
 * ASan + UBSan and under ThreadSanitizer (tests/cpu1_modes.sh). (after 2.7.1) Every render also fails if core 1 drew
 * from rng() (CORES=1: the draws are core 0's) or rendered a fading part. What each of units_split's rules is guarded
 * by: a part whose LFO 2 wraps (phys_lfo, mix, fx26), a PHYS model drawing (phys_lfo), two FORMANT parts (formant3; a
 * lone one goes: formant1), a fading part (fx26, swap_a, swap_b). Prints the jobs handed to core 1 and, for each unit (the parts, the
 * drums), the blocks it sounded on core 1 (the units of each job, the double's c1h.arg), to show the split happened.
 * (after 2.7.1) With every unit asked for (15) and no job lost, each scene from dpl on must show what it is there for:
 * each engine and kit it names sounded on core 1 in most of the blocks it sounded, each fade (8 blocks a switch)
 * rendered on core 0 with core 1 rendering the other units, and the MODEL turns landed in the middle of a fade.
 *   build/host/mix_split_test [QUICK]       QUICK: the splits 0, 15 and -1 only (the TSan run)
 *   SPLIT_DETAIL=1                          also the blocks by engine, by kit and of the fades (splits 15 and -1) */
#include <stdint.h>
static uint32_t host_slots[4u * 0x14000u / 4u];         /* (after 2.7.1) USR1..4 as a RAM image (tests/userkit_test.c) */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main

#define BLOCKS 1500u                                     /* ~2.2 s at 44.1 kHz */
static const int32_t MASKS[] = {0, -1, 1, 2, 4, 8, 3, 5, 12, 7, 15};
static const int32_t MASKS_QUICK[] = {0, 15, -1};
static const char *const SCENE_NAME[] = {"formant3", "fm6x3", "phys_lfo", "mix", "fx26",
                                          "dpl", "stn", "formant1", "swap_a", "swap_b"};
#define NSCENE 10u
#define S_COVER 5u                                       /* (after 2.7.1) the scenes from here on: what must go, below */

/* (after 2.7.1) what a scene from S_COVER on is there for: the engines (bit e of ENGINES[]) and the drum kits (bit k,
 * drums.c drum_kit) that must sound on core 1 in most of their blocks, the fades (blocks) and the MODEL turns in the
 * middle of one */
#define EB(e) (1u << (e))
#define KB(k) (1ull << (k))
static const struct { uint32_t eng; uint64_t kits; uint32_t fades, midfade; } WANT[NSCENE] = {
    [5] = {EB(1) | EB(2) | EB(3), KB(4) | KB(1) | KB(0), 0, 0},             /* DIGITAL PHASE LOFI; DUST DEEP ACOUSTIC */
    [6] = {EB(4) | EB(6) | EB(ENGI_NOISE), KB(DRUM_SYN + 1u) | KB(DRUM_SYN + 3u), 0, 0},   /* SAMPLE TRIO NOISE; SYN2 SYN4 */
    [7] = {EB(0) | EB(4) | EB(5), KB(DRUM_USR) | KB(DRUM_PAIR), 0, 0},             /* ANALOG SAMPLE FORMANT; USR1 USR3+4 */
    [8] = {EB(0) | EB(7) | EB(ENGI_FM6) | EB(ENGI_PHYS) | EB(ENGI_NOISE), KB(DRUM_SAMPLED), 6u * 8u, 3},   /* 6 switches */
    [9] = {EB(4) | EB(5) | EB(ENGI_GRAIN) | EB(ENGI_PHYS), KB(DRUM_SYN), 8u * 8u, 2},   /* SYN1; 8 switches */
};
static uint32_t midfade;                                 /* MODEL turns that landed in the middle of a fade (scene_block) */

/* (after 2.7.1) a user sample slot k in the RAM image, as the web editor writes one (tests/userkit_test.c): a zone per
 * entry, lo..hi, its root in the middle; ADPCM nibbles up a while, then down. looped: 1 loops it whole (from its
 * start: the ADPCM state there is the zone's) */
static void slot_build(uint32_t k, const uint8_t *lo, const uint8_t *hi, const uint32_t *lens, uint32_t nz, uint32_t looped)
{
    uint8_t *img = (uint8_t *)host_slots + k * SMP_USER_SIZE, *d = img + SMP_USER_DATA;
    smp_user_hdr_t *h = (smp_user_hdr_t *)img;
    uint32_t i, j, off = 0;
    memset(img, 0xFF, SMP_USER_SIZE);
    memset(h, 0, sizeof *h);
    h->magic = SMP_USER_MAGIC;
    h->version = 1;
    h->nz = (uint8_t)nz;
    memcpy(h->name, "SPLIT", 6);
    for (i = 0; i < nz; i++) {
        smp_zone_t *z = &h->zone[i];
        uint32_t bytes = (lens[i] + 1u) / 2u;
        for (j = 0; j < bytes; j++)
            d[off + j] = (j / (8u + 3u * i)) & 1u ? 0xFFu : 0x77u;   /* (big steps: a loud square-ish wave) */
        z->off = off;
        z->n = lens[i];
        z->ls = 0;
        z->le = lens[i] - 1u;
        z->rate = 0x8000u;                               /* 22050 / 44100 */
        z->root16 = (int16_t)((lo[i] + hi[i]) / 2u * 16u);
        z->pred = 0;
        z->idx = 0;
        z->lo = lo[i];
        z->hi = hi[i];
        z->looped = (uint8_t)looped;
        off += bytes;
    }
    h->data_len = off;
    smp_user_scan(k);
}
/* USR1: a kit (KICK, SNARE, CLAP, HAT, OPEN HAT at their lanes' notes); USR2: two looped zones over the keys (a part's
 * SAMPLE or GRAIN); USR3 and USR4: one kit over both (USR3+4: KICK and HAT in USR3, SNARE, CLAP and OPEN HAT in USR4) */
static void user_slots(void)
{
    static const uint8_t k1[5] = {36, 38, 39, 42, 46}, l2[2] = {24, 60}, h2[2] = {59, 108}, k3[2] = {36, 42},
                         k4[3] = {38, 39, 46};
    static const uint32_t n1[5] = {9000, 4000, 5000, 2000, 14000}, n2[2] = {12000, 9000}, n3[2] = {11000, 3000},
                          n4[3] = {5000, 6000, 16000};
    slot_build(0, k1, k1, n1, 5, 0);
    slot_build(1, l2, h2, n2, 2, 1);
    slot_build(2, k3, k3, n3, 2, 0);
    slot_build(3, k4, k4, n4, 3, 0);
}

static void drums_pattern(void)                          /* kick 4 on the floor, snare 4 / 12, hats, a clap */
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        if (i % 2u == 0u)
            n[k++] = i == 14u ? 46 : 42;
        if (i == 7u)
            n[k++] = 39;
        put_step(TDRUM, i, k, n, k ? ST_NOTE : ST_REST, i % 4u == 0u ? SF_ACCENT : 0u);
    }
}

static void chords(track_t *t, uint32_t first, uint32_t every, uint32_t base)   /* 3-note chords on a 16-step pattern */
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t n[3] = {(uint8_t)(base + i), (uint8_t)(base + i + 4u), (uint8_t)(base + i + 7u)};
        int on = i >= first && (i - first) % every == 0u;
        put_step(t, i, on ? 3u : 0u, n, on ? ST_NOTE : ST_REST, i == first ? SF_ACCENT : 0u);
    }
}

static void slow_lfo(track_t *t)                         /* (after 2.7.1) no LFO wrap in a scene: the part may go */
{
    t->p[P_LRATE] = 4;
    t->p[P_LSYNC] = 0;
}
static void long_chords(uint32_t base, uint32_t every)   /* (after 2.7.1) a chord every `every` steps, held long */
{
    uint32_t k;
    for (k = 0; k < NPART; k++) {
        slow_lfo(&trk[k]);
        trk[k].p[P_SGATE] = (int16_t)(every > 1u ? 110 : 120);   /* (one a step: still sounding 3 blocks before the next) */
        chords(&trk[k], k, every, base + 5u * k);
    }
}
/* (after 2.7.1) engine e's preset pi on part k under its sounding notes, as ui.c set_engine_of / apply_preset_to: the
 * sound kept for the fade, the notes released, the new values with eng_req (one write between two blocks) */
static void swap_to(uint32_t k, uint32_t e, uint32_t pi)
{
    sound_change(&trk[k]);
    panic_req |= (uint8_t)(1u << k);
    host_preset_req(&trk[k], e, pi);
    slow_lfo(&trk[k]);
}
static void turn_e0(uint32_t k, int32_t v)               /* (after 2.7.1) EDIT 1 (PHYS's MODEL, NOISE's MODE) turned */
{
    midfade += trk[k].xf_on && trk[k].xf;                /* (in the middle of a fade: the render keeps pe_old) */
    trk[k].p[P_E0] = (int16_t)v;
}

static void scene_setup(uint32_t s)
{
    uint32_t k;
    host_tracks_init();
    song.g[G_BPM] = 140;
    song.g[G_DRLVL] = 90;
    song.g[G_DRREV] = 40;
    song.g[G_DRDLY] = 30;
    drums_pattern();
    switch (s) {
    case 0:
        for (k = 0; k < NPART; k++) {
            host_preset(&trk[k], 5, k);                  /* FORMANT */
            chords(&trk[k], k, 4, 48 + 5 * k);
        }
        song.g[G_CLOCK] = CLICK_ON;
        TDRUM->p[P_TFLT] = -50;
        break;
    case 1:
        for (k = 0; k < NPART; k++) {
            host_preset(&trk[k], ENGI_FM6, 2 * k);
            chords(&trk[k], k, 4, 52 + 3 * k);
        }
        break;
    case 2:
        for (k = 0; k < NPART; k++) {
            host_preset(&trk[k], ENGI_PHYS, 3 * k);
            trk[k].p[P_E0] = (int16_t)(k == 1u ? 3 : k);   /* MODAL, SYMP, MEMB */
            trk[k].p[P_LWAVE] = 4;                       /* S&H */
            trk[k].p[P_LRATE] = (int16_t)(110 + 5 * k);
            trk[k].p[P_LD_PIT] = 40;
            chords(&trk[k], k, 4, 50 + 4 * k);
        }
        break;
    case 4:
        host_preset(&trk[0], 0, 4);                      /* ANALOG */
        host_preset(&trk[1], ENGI_FM6, 1);
        host_preset(&trk[2], 7, 2);                      /* DRAWBAR */
        for (k = 0; k < NPART; k++) {
            trk[k].p[P_ITYPE] = (int16_t)(k == 0u ? 1 : k == 1u ? 5 : 7);   /* SOFT, CRUSH, FLANG */
            trk[k].p[P_IA] = (int16_t)(60 + 20 * k);
            trk[k].p[P_IB] = 70;
            trk[k].p[P_IC] = (int16_t)(k == 1u ? 90 : 112);
            trk[k].p[P_IMIX] = (int16_t)(80 + 15 * k);
            trk[k].p[P_LWAVE] = 4;                       /* S&H: a draw at every wrap */
            trk[k].p[P_LSYNC] = (int16_t)(6 + k);        /* 1/8, 8T, 1/16 of the tempo */
            trk[k].p[P_LTRIG] = (int16_t)(k & 1u);       /* NOTE / FREE */
            trk[k].p[P_LPOL] = (int16_t)(k == 2u);       /* BI / UNI */
            trk[k].p[P_LD_PIT] = 30;
            trk[k].p[P_LD_AMP] = 40;
            trk[k].p[P_ESYNC] = 1;
            trk[k].p[P_ATK] = (int16_t)(10 + 30 * k);
            trk[k].p[P_DEC] = (int16_t)(50 + 20 * k);
            trk[k].p[P_REL] = (int16_t)(40 + 10 * k);
            trk[k].p[P_REV] = 60;
            trk[k].p[P_DLY] = 50;
            chords(&trk[k], k, 4, 48 + 6 * k);
        }
        trk[0].p[P_FTYPE] = 1;                           /* BP */
        TDRUM->p[P_ITYPE] = 4;                           /* FUZZ on the drums */
        TDRUM->p[P_IA] = 50;
        TDRUM->p[P_IB] = 100;
        TDRUM->p[P_IC] = 112;
        TDRUM->p[P_IMIX] = 100;
        TDRUM->p[P_TFLT] = -45;
        song.g[G_CLOCK] = CLICK_ON;
        song.g[G_DUCK] = 60;
        song.g[G_TXSRC] = 1;                             /* VINYL */
        song.g[G_TXLVL] = 90;
        song.g[G_TXTONE] = -20;
        song.g[G_TXMOVE] = 80;
        song.g[G_TXRATE] = 0;
        song.g[G_TXWHEN] = 1;                            /* ALWAYS */
        song.g[G_TXDUCK] = 1;
        break;
    case 5:                                              /* (after 2.7.1) dpl */
        host_preset(&trk[0], 1, 0);                      /* DIGITAL: RHODES */
        host_preset(&trk[1], 2, 6);                      /* PHASE: CZ PAD */
        host_preset(&trk[2], 3, 4);                      /* LOFI: CHIP CHORD */
        long_chords(48, 2);
        trk[1].p[P_SLCR] = SL_GATE;                      /* the SLICER on a part */
        trk[1].p[P_SLPAT] = 5;
        trk[1].p[P_SLDEPTH] = 90;
        trk[0].p[P_TFLT] = -30;                          /* a part's FILTER: low-pass */
        trk[2].p[P_DIST] = 50;
        TDRUM->p[P_E0] = 4;                              /* DUST: the GM samples, filtered, fewer bits */
        break;
    case 6:                                              /* (after 2.7.1) stn */
        host_preset(&trk[0], 4, 0);                      /* SAMPLE: GRAND PNO (a factory set) */
        host_preset(&trk[1], 6, 5);                      /* TRIO: SAW PAD */
        host_preset(&trk[2], ENGI_NOISE, 2);             /* NOISE: NZ ARCADE (LFSR) */
        long_chords(50, 2);
        trk[2].p[P_SLCR] = SL_STUT;                      /* the SLICER on a part: the repeats */
        trk[2].p[P_SLPAT] = 9;
        trk[2].p[P_SLRATE] = 2;
        trk[2].p[P_SLDEPTH] = 100;
        trk[1].p[P_TFLT] = 30;                           /* a part's FILTER: high-pass */
        TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + 1u);       /* SYN2 */
        song.g[G_CLOCK] = CLICK_ON;
        break;
    case 7:                                              /* (after 2.7.1) formant1 */
        user_slots();
        host_preset(&trk[0], 0, 13);                     /* ANALOG: WARM PAD */
        host_preset(&trk[1], 4, 1);                      /* SAMPLE .. */
        trk[1].p[P_E0] = (int16_t)(SMP_NSETS + 1u);      /* .. on USR2 */
        host_preset(&trk[2], 5, 2);                      /* FORMANT: TALKBOX, the only one */
        long_chords(46, 2);
        TDRUM->p[P_E0] = (int16_t)DRUM_USR;              /* USR1 */
        TDRUM->p[P_TFLT] = -35;
        break;
    case 8:                                              /* (after 2.7.1) swap_a */
        host_preset(&trk[0], 0, 12);                     /* ANALOG: SUPERSAW */
        host_preset(&trk[1], ENGI_FM6, 0);               /* FM6: TINE EP */
        host_preset(&trk[2], ENGI_PHYS, 7);              /* PHYS: SITAR (SYMP) */
        long_chords(48, 1);
        TDRUM->p[P_E0] = (int16_t)DRUM_SAMPLED;          /* 808 */
        break;
    case 9:                                              /* (after 2.7.1) swap_b */
        user_slots();
        host_preset(&trk[0], 4, 0);                      /* SAMPLE .. */
        trk[0].p[P_E0] = (int16_t)(SMP_NSETS + 1u);      /* .. on USR2 */
        host_preset(&trk[1], ENGI_PHYS, 5);              /* PHYS: HAND DRUM (MEMB) */
        host_preset(&trk[2], 5, 0);                      /* FORMANT: CHOIR AAH */
        long_chords(52, 1);
        TDRUM->p[P_E0] = (int16_t)DRUM_SYN;              /* SYN1 */
        break;
    case 3:
        host_preset(&trk[0], 0, 4);                      /* ANALOG */
        host_preset(&trk[1], ENGI_GRAIN, 0);
        host_preset(&trk[2], 7, 0);                      /* DRAWBAR */
        trk[0].p[P_DIST] = 60;
        trk[1].p[P_REV] = 90;
        trk[2].p[P_CHOR] = 80;
        trk[2].p[P_DLY] = 70;
        for (k = 0; k < NPART; k++)
            chords(&trk[k], k, 4, 45 + 7 * k);
        song.g[G_CLOCK] = CLICK_ON;
        TDRUM->p[P_SLCR] = SL_GATE;
        TDRUM->p[P_SLPAT] = 3;
        TDRUM->p[P_SLDEPTH] = 100;
        TDRUM->p[P_TFLT] = -40;
        break;
    }
    transport_req = 1;
}

static void scene_block(uint32_t s, uint32_t b)          /* what the UI would do between blocks */
{
    uint32_t k;
    if (s == 2u && (b == 400u || b == 900u))
        for (k = 0; k < NPART; k++)                      /* MODEL under the sounding notes: phys_render resets, with a draw */
            trk[k].p[P_E0] = (int16_t)((trk[k].p[P_E0] + 1) % PM_COUNT);
    if (s == 3u) {
        if (b % 128u == 64u)
            trk[(b / 128u) % NPART].p[P_MUTE] ^= 1;      /* mute fades, each part in turn */
        if (b == 500u)
            song.solo = 2;
        if (b == 700u)
            song.solo = 0;
        if (b == 600u)
            TDRUM->p[P_SLCR] = SL_STUT;
        if (b == 1000u)
            TDRUM->p[P_TFLT] = 40;
    }
    if (s == 4u) {
        if (b == 300u)
            thr_req = 1;                                 /* REV THROW held .. */
        if (b == 420u)
            thr_req = 2;                                 /* .. then ECHO THROW */
        if (b == 560u)
            thr_req = 0;
        if (b == 500u) {                                 /* a preset on two parts under their notes (ui.c apply_preset_to):
                                                          * both fade out with their old sound in the same blocks */
            uint32_t k;
            for (k = 1; k < NPART; k++) {
                sound_change(&trk[k]);
                panic_req |= (uint8_t)(1u << k);
                host_preset_req(&trk[k], k == 1u ? ENGI_FM6 : 7, 4 + k);
            }
        }
        if (b == 650u)
            trk[0].p[P_FTYPE] = 2;                       /* HP */
        if (b == 700u)
            punch.req = PX_FREEZE;                       /* the reverb held, the dry mix fed into it */
        if (b == 900u)
            punch.req = -1;
        if (b == 800u)
            TDRUM->p[P_TFLT] = 0;                        /* the drums' INSERT without the FILTER */
        if (b == 1000u)
            drum_mute = 0x5;                             /* lanes 0 and 2 muted live (EDIT + a black key) */
        if (b == 1100u)
            trk[0].bend = 5000;
        if (b == 1200u)
            drum_mute = 0;
    }
    if (s == 5u) {                                       /* (after 2.7.1) dpl */
        if (b == 500u)
            TDRUM->p[P_E0] = 1;                          /* DEEP */
        if (b == 600u)
            trk[1].p[P_SLCR] = SL_STUT;
        if (b == 700u)
            trk[0].p[P_TFLT] = 35;                       /* high-pass */
        if (b == 1000u)
            TDRUM->p[P_E0] = 0;                          /* ACOUSTIC */
    }
    if (s == 6u) {                                       /* (after 2.7.1) stn */
        if (b == 500u)
            trk[2].p[P_E0] = 3;                          /* NOISE: META under the notes */
        if (b == 750u)
            TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + 3u);   /* SYN4 */
        if (b == 800u)
            trk[1].p[P_TFLT] = -40;                      /* low-pass */
        if (b == 1000u)
            trk[2].p[P_SLCR] = SL_GATE;
    }
    if (s == 7u && b == 750u)                            /* (after 2.7.1) formant1 */
        TDRUM->p[P_E0] = (int16_t)DRUM_PAIR;             /* USR3+4 */
    if (s == 8u) {                                       /* (after 2.7.1) swap_a */
        if (b == 292u)
            swap_to(0, ENGI_DRAWBAR, 1);                 /* ANALOG -> DRAWBAR: GOSPEL */
        if (b == 439u)
            swap_to(1, ENGI_PHYS, 2);                    /* FM6 -> PHYS: STR PLUCK .. */
        if (b == 441u)
            turn_e0(1, PM_MEMB);                         /* .. its MODEL turned while FM6 fades out */
        if (b == 587u)
            swap_to(2, ENGI_NOISE, 0);                   /* PHYS SYMP -> NOISE: WIND .. */
        if (b == 589u)
            turn_e0(2, 2);                               /* .. its MODE (LFSR) turned while PHYS fades out */
        if (b == 1030u)
            swap_to(1, ENGI_PHYS, 7);                    /* PHYS MEMB -> SITAR (SYMP), a preset .. */
        if (b == 1032u)
            turn_e0(1, PM_STRING);                       /* .. its MODEL turned in the middle of the fade */
        if (b == 1178u)
            swap_to(2, ENGI_FM6, 4);                     /* NOISE -> FM6: SOFT PAD (its patch after the fade) */
        if (b == 1473u)
            swap_to(0, ENGI_PHYS, 0);                    /* DRAWBAR -> PHYS: BELL TREE */
        if (b % 8u == 5u)
            fm6_poll();                                  /* (the main loop's, a few blocks late: the part waits, faded, on
                                                          * its old engine with no voice, then its patch, then FM6) */
    }
    if (s == 9u) {                                       /* (after 2.7.1) swap_b */
        if (b == 292u) {
            swap_to(0, ENGI_GRAIN, 0);                   /* SAMPLE -> GRAIN on USR2 */
            trk[0].p[P_E0] = (int16_t)(SMP_NSETS + 1u);
        }
        if (b == 439u)
            swap_to(1, ENGI_PHYS, 16);                   /* PHYS MEMB -> TANPURA (SYMP) .. */
        if (b == 441u)
            turn_e0(1, PM_STRING);                       /* .. the MODEL turned to STRNG in the middle of the fade */
        if (b == 587u)
            swap_to(2, ENGI_PHYS, 23);                   /* FORMANT -> PHYS: TABLA */
        if (b == 735u)
            swap_to(1, ENGI_PHYS, 0);                    /* PHYS STRNG -> BELL TREE (MODAL) .. */
        if (b == 737u)
            turn_e0(1, PM_SYMP);                         /* .. the MODEL turned to SYMP in the middle of the fade */
        if (b == 880u)
            turn_e0(2, PM_SYMP);                         /* TABLA's MODEL to SYMP under its three notes (the third keeps
                                                          * sounding with no slot: eng_phys.c phys_slot_of) .. */
        if (b == 882u)
            swap_to(0, 4, 2);                            /* GRAIN -> SAMPLE: VIBES */
        if (b == 883u)
            swap_to(2, ENGI_PHYS, 0);                    /* .. then BELL TREE (MODAL): the fade renders SYMP (the big
                                                          * layout) with three voices, t->p MODAL's (engines.c
                                                          * phys_render_draws' bound, 2.7.1) */
        if (b == 1178u)
            swap_to(2, 5, 4);                            /* PHYS -> FORMANT: HARMONICA */
        if (b == 1325u)
            swap_to(1, ENGI_GRAIN, 3);                   /* PHYS -> GRAIN: STR CLOUD (the arena handed over) */
    }
}

typedef struct {
    uint64_t hash;
    uint32_t jobs, timeouts;
    uint32_t c1[NTRK];                                   /* (after 2.7.1) blocks each unit (parts, drums) sounded on core 1 */
    uint32_t snd_e[NENGINES], c1_e[NENGINES];            /* blocks a part sounded on engine e (not fading); on core 1 */
    uint32_t snd_k[DRUM_KITS], c1_k[DRUM_KITS];          /* blocks the drums sounded on kit k, of them on core 1 */
    uint32_t fade, fade_busy, fade_c1;                   /* a part's fade blocks; of them, core 1 rendering other units; the
                                                          * fading part on core 1 */
    uint32_t queued;                                     /* fade blocks with notes waiting for the new sound (xp_n) */
    uint32_t c1rng;                                      /* (CORES=1) jobs in which core 1 drew from rng() */
    uint32_t midfade;
} res_t;

/* (after 2.7.1) what this block rendered where: u, the units handed to core 1 (0: none); xf0, each part's fade blocks
 * left before it (voice.c track_render counts them down while a fade renders; XF_BLOCKS + 1: none under way) */
static void tally(res_t *r, uint32_t u, const uint32_t *xf0)
{
    uint32_t k, i, snd, on1;
    for (k = 0; k < NPART; k++) {
        const track_t *t = &trk[k];
        on1 = (u >> k) & 1u;
        if (t->xf_on && t->xf < xf0[k]) {               /* a fade rendered: the old sound, the old engine */
            r->fade++;
            r->fade_busy += u != 0u;
            r->fade_c1 += on1;
            r->queued += t->xp_n != 0u;
            continue;
        }
        for (i = snd = 0; i < NVOICE; i++)
            snd |= t->v[i].active;
        if (snd) {
            r->snd_e[t->engine % NENGINES]++;
            r->c1_e[t->engine % NENGINES] += on1;
            r->c1[k] += on1;
        }
    }
    on1 = (u >> NPART) & 1u;
    for (i = snd = 0; i < NDRUM; i++)
        snd |= drums.v[i].active;
    if (snd) {
        r->snd_k[drum_kit()]++;
        r->c1_k[drum_kit()] += on1;
        r->c1[NPART] += on1;
    }
}

static res_t render(uint32_t s, int32_t mask)
{
    res_t r;
    uint32_t b, i, k;
    memset(&r, 0, sizeof r);
    r.hash = 0xcbf29ce484222325ull;
    cpu1_host_mode();                                    /* init: the env (CORES, CHAOS, CPU1_TIMEOUT_AT) */
    cpu1_units = mask;
    scene_setup(s);
    for (b = 0; b < BLOCKS; b++) {
        int32_t o[2 * CTL];
        uint32_t j0, xf0[NPART];
        scene_block(s, b);
        for (k = 0; k < NPART; k++)
            xf0[k] = trk[k].xf_on ? trk[k].xf : XF_BLOCKS + 1u;
        j0 = cpu1_host_jobs();
        mix_block(o, CTL);
        tally(&r, cpu1_host_jobs() != j0 ? c1h.arg & 0xFFu : 0u, xf0);   /* (the job's units: fx.c c1 | n << 8) */
        for (i = 0; i < 2u * CTL; i++) {
            uint32_t u = (uint32_t)o[i];
            for (k = 0; k < 4u; k++) {
                r.hash ^= (u >> (8u * k)) & 0xFFu;
                r.hash *= 0x100000001B3ull;
            }
        }
    }
    r.jobs = cpu1_host_jobs();
    r.timeouts = cpu1_host_timeouts();
    r.midfade = midfade;
    r.c1rng = cpu1_host_rng_jobs();
    return r;
}

/* (after 2.7.1) with every unit asked for and no job lost: what scene s is there for went (0), else why not */
static const char *went(uint32_t s, const res_t *r)
{
    uint32_t e, k;
    if (s < S_COVER)
        return 0;
    for (e = 0; e < NENGINES; e++)
        if (((WANT[s].eng >> e) & 1u) && (r->snd_e[e] < 32u || 2u * r->c1_e[e] <= r->snd_e[e]))
            return "an engine of the scene did not sound on core 1 in most of its blocks";
    for (k = 0; k < DRUM_KITS; k++)
        if (((WANT[s].kits >> k) & 1u) && (r->snd_k[k] < 32u || 2u * r->c1_k[k] <= r->snd_k[k]))
            return "a drum kit of the scene did not sound on core 1 in most of its blocks";
    if (r->fade < WANT[s].fades || 2u * r->fade_busy < r->fade)
        return "a switch did not fade (no note sounding), or core 1 was idle through the fades";
    return 0;
}

static void detail(uint32_t s, int32_t mask, const res_t *r)   /* (after 2.7.1) SPLIT_DETAIL=1 */
{
    uint32_t e, k;
    printf("        %-9s units %3d  core 1 / sounding:", SCENE_NAME[s], mask);
    for (e = 0; e < NENGINES; e++)
        if (r->snd_e[e])
            printf("  %s %u/%u", ENGINES[e]->name, r->c1_e[e], r->snd_e[e]);
    for (k = 0; k < DRUM_KITS; k++)
        if (r->snd_k[k])
            printf("  kit %s %u/%u", DRUM_KIT_NAMES[k], r->c1_k[k], r->snd_k[k]);
    printf("  fades %u (core 1 busy %u, the fading part there %u, notes waiting %u)  MODEL turns mid-fade %u\n", r->fade,
           r->fade_busy, r->fade_c1, r->queued, r->midfade);
}

static int child(uint32_t s, int32_t mask, res_t *out)  /* the render in a fresh process; 0 = it ran */
{
    int p[2], st = 0;
    pid_t pid;
    size_t got = 0;
    ssize_t n;
    if (pipe(p))
        return -1;
    fflush(stdout);
    if (!(pid = fork())) {
        res_t r = render(s, mask);
        close(p[0]);
        if (write(p[1], &r, sizeof r) != (ssize_t)sizeof r)
            _exit(1);
        _exit(0);
    }
    close(p[1]);
    while (got < sizeof *out && (n = read(p[0], (char *)out + got, sizeof *out - got)) > 0)
        got += (size_t)n;
    close(p[0]);
    waitpid(pid, &st, 0);
    return got == sizeof *out && WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    int quick = argc > 1 && !strcmp(argv[1], "QUICK"), fails = 0;
    const int32_t *masks = quick ? MASKS_QUICK : MASKS;
    uint32_t nm = quick ? sizeof MASKS_QUICK / sizeof MASKS_QUICK[0] : sizeof MASKS / sizeof MASKS[0], s, m;
    const char *e = getenv("CPU1_TIMEOUT_AT");
    uint32_t lost = e ? (uint32_t)atoi(e) : 0;
    int detail_on = getenv("SPLIT_DETAIL") && atoi(getenv("SPLIT_DETAIL"));
    if ((uintptr_t)host_slots < (uintptr_t)SMP_DATA || (uintptr_t)host_slots - (uintptr_t)SMP_DATA > 0xF0000000u) {
        printf("split: the user slots' image must lie above SMP_DATA within 4 GiB (32-bit offsets): FAIL\n");
        return 1;                                        /* (after 2.7.1: eng_sample.c smp_user_scan rebases on SMP_DATA) */
    }
    for (s = 0; s < NSCENE; s++) {
        res_t ref;
        if (child(s, 0, &ref)) {
            printf("split: %-9s no split: CRASH\n", SCENE_NAME[s]);
            fails++;
            continue;
        }
        for (m = 0; m < nm; m++) {
            res_t r;
            int32_t mask = masks[m];
            const char *why = 0;
            memset(&r, 0, sizeof r);                     /* (a crashed child leaves no numbers of its own) */
            if (child(s, mask, &r))
                why = "crashed";
            else if (r.hash != ref.hash)
                why = "the output differs from the unsplit render";
            else if (mask && !r.jobs && (s != 0u || mask < 0 || (mask & (1 << NPART))))
                why = "no job was handed to core 1";          /* (formant3: its parts never go; the drums do) */
            else if (!mask && r.jobs)
                why = "jobs handed to core 1 with no unit for it";
            else if (r.c1rng)
                why = "core 1 drew from rng() (the draws are core 0's: fx.c units_split)";
            else if (r.fade_c1)
                why = "a fading part rendered on core 1 (fx.c units_split keeps it on core 0)";
            else if (lost && mask && r.jobs > lost)
                why = "jobs handed over after the lost one (core 1 should be held)";
            else if (s >= S_COVER && r.midfade != WANT[s].midfade)
                why = "a MODEL turn missed the fade it was timed for";
            else if (mask == 15 && !lost && went(s, &r))
                why = went(s, &r);
            printf("split: %-9s units %3d  %016llx  jobs %4u  timeouts %u  on core 1 %4u %4u %4u %4u  %s\n", SCENE_NAME[s],
                   mask, (unsigned long long)r.hash, r.jobs, r.timeouts, r.c1[0], r.c1[1], r.c1[2], r.c1[NPART],
                   why ? why : "same");
            if (detail_on && (mask == 15 || mask < 0))
                detail(s, mask, &r);
            if (why)
                fails++;
        }
    }
    printf("render split: %u scenes x %u splits, CORES=%d CHAOS=%s CPU1_TIMEOUT_AT=%u: %s\n", NSCENE, nm, cpu1_host_mode(),
           getenv("CHAOS") ? getenv("CHAOS") : "0", lost, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
