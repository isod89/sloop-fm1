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
 *              sound at once, one on each core), TEXTURE (VINYL, ducked), REV THROW then ECHO THROW, the punch-in
 *              FREEZE, a pitch bend, live drum mutes, the metronome
 * Runs under every mode of the host double of core 1 (tests/cpu1_host.c: CORES, CHAOS, CPU1_TIMEOUT_AT) and
 * under ThreadSanitizer (tests/cpu1_modes.sh). Prints the jobs handed to core 1, to show the split happened.
 *   build/host/mix_split_test [QUICK]       QUICK: the splits 0, 15 and -1 only (the TSan run) */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define BLOCKS 1500u                                     /* ~2.2 s at 44.1 kHz */
static const int32_t MASKS[] = {0, -1, 1, 2, 4, 8, 3, 5, 12, 7, 15};
static const int32_t MASKS_QUICK[] = {0, 15, -1};
static const char *const SCENE_NAME[] = {"formant3", "fm6x3", "phys_lfo", "mix", "fx26"};
#define NSCENE 5u

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
    default:
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
}

typedef struct { uint64_t hash; uint32_t jobs, timeouts; } res_t;

static res_t render(uint32_t s, int32_t mask)
{
    res_t r = {0xcbf29ce484222325ull, 0, 0};
    uint32_t b, i, k;
    cpu1_host_mode();                                    /* init: the env (CORES, CHAOS, CPU1_TIMEOUT_AT) */
    cpu1_units = mask;
    scene_setup(s);
    for (b = 0; b < BLOCKS; b++) {
        int32_t o[2 * CTL];
        scene_block(s, b);
        mix_block(o, CTL);
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
    return r;
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
            if (child(s, mask, &r))
                why = "crashed";
            else if (r.hash != ref.hash)
                why = "the output differs from the unsplit render";
            else if (mask && !r.jobs && (s != 0u || mask < 0 || (mask & (1 << NPART))))
                why = "no job was handed to core 1";          /* (formant3: its parts never go; the drums do) */
            else if (!mask && r.jobs)
                why = "jobs handed to core 1 with no unit for it";
            else if (lost && mask && r.jobs > lost)
                why = "jobs handed over after the lost one (core 1 should be held)";
            printf("split: %-9s units %3d  %016llx  jobs %4u  timeouts %u  %s\n", SCENE_NAME[s], mask,
                   (unsigned long long)r.hash, r.jobs, r.timeouts, why ? why : "same");
            if (why)
                fails++;
        }
    }
    printf("render split: %u scenes x %u splits, CORES=%d CHAOS=%s CPU1_TIMEOUT_AT=%u: %s\n", NSCENE, nm, cpu1_host_mode(),
           getenv("CHAOS") ? getenv("CHAOS") : "0", lost, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
