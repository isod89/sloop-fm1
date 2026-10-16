/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 wheels (firmware/src/smk_wheel.h): HOME menu > MIDI > BEND UP / BEND DOWN (the range math each way, the
 * steps, the settings bytes, 2.6.4's BEND RANGE byte), the wheels taught by HARDWARE CALIBRATION (the full travel gives the full range, the re-centre at power-on,
 * the mod wheel, older data), then the drift test (+-7 semitones, not taught): the bend is back to 0 once the
 * wheel rests again, however it was moved. v0.1 moved the centre by one count every 200 calls; the main loop calls it some 10 000 to
 * 50 000 times a second, so a slow touch dragged the centre along, the spring brought the wheel back outside
 * the dragged dead band and a bend of several semitones stayed on, on whichever synth track was selected:
 * switching tracks took it off one track and put it on the next ("track 1 transposes when I change track"). */
#include <stdio.h>
#include <string.h>
#include "../firmware/src/smk_wheel.h"
#define SMK_PW_ST 7                /* the drift test: the old fixed range */
static int fails;
static void check(int ok, const char *what) { printf("smk_wheel: %-66s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

/* v0.1 (main.c smk_wheels before the fix): the centre's drift counted in calls */
typedef struct { int32_t pc, plo, phi, rest; } old_t;
static int32_t old_bend(old_t *w, int32_t p)
{
    int32_t d, b;
    if (!w->pc) {
        w->pc = p;
        w->plo = p - SMK_PW_SPAN0;
        w->phi = p + SMK_PW_SPAN0;
    }
    if (p < w->plo) w->plo = p;
    if (p > w->phi) w->phi = p;
    d = p - w->pc;
    if (d > -SMK_PW_DEAD && d < SMK_PW_DEAD) {
        b = 0;
        if (++w->rest > 200) {
            w->pc += d > 0 ? 1 : d < 0 ? -1 : 0;
            w->rest = 0;
        }
    } else {
        w->rest = 0;
        b = d > 0 ? (d - SMK_PW_DEAD) * (SMK_PW_ST * 4096) / (w->phi - w->pc - SMK_PW_DEAD > 1 ? w->phi - w->pc - SMK_PW_DEAD : 1)
                  : (d + SMK_PW_DEAD) * (SMK_PW_ST * 4096) / (w->pc - w->plo - SMK_PW_DEAD > 1 ? w->pc - w->plo - SMK_PW_DEAD : 1);
        b = b < -SMK_PW_ST * 4096 ? -SMK_PW_ST * 4096 : b > SMK_PW_ST * 4096 ? SMK_PW_ST * 4096 : b;
    }
    return b;
}

#define CALLS_PER_MS 20           /* main loop: ui_input() + smk_wheels() some 20 000 times a second */
#define REST 540
static uint32_t now;
static smk_pw_t nw;
static const smk_wcal_t nocal;   /* not taught (older data): the self-calibration */
static old_t ow;
static int32_t nb, ob;
/* move the wheel linearly from a to b over ms milliseconds (the ADC's /4 IIR: a reading every 2.4 ms) */
static void move(int32_t a, int32_t b, uint32_t ms)
{
    uint32_t t, c;
    int32_t f = a;
    for (t = 0; t < ms; t++, now++) {
        int32_t raw = ms ? a + (b - a) * (int32_t)t / (int32_t)ms : b;
        if (t % 2u == 0u)
            f += (raw - f) / 4;
        for (c = 0; c < CALLS_PER_MS; c++) {
            nb = smk_pw_bend(&nw, &nocal, f, now, SMK_PW_ST, SMK_PW_ST);
            ob = old_bend(&ow, f);
        }
    }
}
static void reset(void)
{
    smk_pw_t z = {0};
    old_t zo = {0};
    nw = z;
    ow = zo;
    now = 1000;
    move(REST, REST, 500);                      /* calibrated at rest */
}

/* ---- BEND UP / DOWN, the calibration ---- */
static int32_t pb(smk_pw_t *w, const smk_wcal_t *c, int32_t p, uint32_t st) { return smk_pw_bend(w, c, p, 1000, st, st); }
static int32_t pb2(smk_pw_t *w, const smk_wcal_t *c, int32_t p, uint32_t up, uint32_t dn) { return smk_pw_bend(w, c, p, 1000, up, dn); }
static void new_features(void)
{
    smk_wcal_t c = {0}, keep;
    smk_pw_t w;
    smk_mw_t m;
    uint32_t i, ok;
    char b[96];

    /* BEND UP / DOWN: the steps, the settings bytes */
    check(BEND_ST[0] == 0 && BEND_ST[1] == 1 && BEND_ST[BEND_N - 1u] == 12 && bend_up == 7u && bend_dn == 7u && BEND_DEF == 7u,
          "BEND UP / DOWN: 0 1 2 3 4 5 7 12, default 7 / 7 (as before)");
    check(bend_step(2, 1) == 3 && bend_step(5, 1) == 7 && bend_step(7, 1) == 12 && bend_step(12, 1) == 12 &&
          bend_step(1, -1) == 0 && bend_step(0, -1) == 0 && bend_step(7, -1) == 5, "BEND UP / DOWN: a knob turn steps, stops at the ends");
    check(bend_step(12, 0) == 0 && bend_step(0, 0) == 1 && bend_step(9, 1) == BEND_DEF, "BEND UP / DOWN: OCT+ steps round; a bad value: 7");
    for (ok = 1, i = 0; i < BEND_N; i++) {
        uint32_t j;
        ok &= bend_up_of_byte(bend_up_byte(BEND_ST[i])) == BEND_ST[i];
        for (j = 0; j < BEND_N; j++)
            ok &= bend_dn_of_byte(bend_dn_byte(BEND_ST[j], BEND_ST[i]), bend_up_of_byte(bend_up_byte(BEND_ST[i]))) == BEND_ST[j];
    }
    check(ok, "BEND UP / DOWN: every pair round trips in its two settings bytes");
    check(bend_up_byte(7) == 0u && bend_dn_byte(7, 7) == 0u && bend_dn_byte(12, 2) == 12u && bend_dn_byte(2, 2) == 0u &&
          bend_up_byte(0) == BEND_NONE && bend_dn_byte(0, 7) == BEND_NONE, "BEND UP / DOWN: 7 / 7 (the default): two 0 bytes; OFF its own");
    check(bend_up_of_byte(0) == 7u && bend_dn_of_byte(0, 7) == 7u && bend_up_of_byte(6) == 7u && bend_up_of_byte(255) == 7u,
          "BEND UP / DOWN: older data (0, 0) or garbage: 7 / 7");
    for (ok = 1, i = 1; i < BEND_N; i++) {                   /* 2.6.4 wrote BEND RANGE in UP's byte, 0 in DOWN's */
        uint32_t b264 = BEND_ST[i] == 7u ? 0u : BEND_ST[i], up = bend_up_of_byte(b264);
        ok &= up == BEND_ST[i] && bend_dn_of_byte(0, up) == BEND_ST[i];
    }
    check(ok, "BEND UP / DOWN: 2.6.4's BEND RANGE (1..12) comes back as both");
    memset(&c, 0, sizeof c);
    smk_wcal_pitch(&c, 533, 537, 448, 636);
    memset(&w, 0, sizeof w);
    check(pb2(&w, &c, 633, 2, 12) == 2 * 4096 && pb2(&w, &c, 451, 2, 12) == -12 * 4096,
          "BEND UP 2 / DOWN 12: full up +2, full down -12 semitones");
    memset(&w, 0, sizeof w);
    check(pb2(&w, &c, 633, 0, 7) == 0 && pb2(&w, &c, 451, 0, 7) == -7 * 4096 && pb2(&w, &c, 451, 7, 0) == 0,
          "BEND UP / DOWN OFF: that way does not bend, the other does");
    memset(&c, 0, sizeof c);

    /* the calibration: rest 533..537, full down 448, full up 636 */
    check(smk_wcal_pitch(&c, 533, 537, 448, 636) && c.pc == 535 && c.plo == 451 && c.phi == 633 && c.pdead == 14,
          "calibration: pitch rest 535, ends 3 counts in, dead band from the noise (14)");
    check(smk_wcal_mod(&c, 627, 629, 476) && c.mrest == 628 && c.mtop == 479 && c.mdead == 10,
          "calibration: mod bottom 628, top 3 counts in, dead band 10");
    keep = c;
    check(!smk_wcal_pitch(&c, 533, 537, 520, 550) && !memcmp(&c, &keep, sizeof c),
          "calibration: too little travel is refused, the old one kept");
    check(!smk_wcal_mod(&c, 600, 602, 590) && !memcmp(&c, &keep, sizeof c), "calibration: mod, too little travel refused");

    /* the range math with the calibration: the full travel = the full range, each BEND RANGE */
    for (ok = 1, i = 1; i < BEND_N; i++) {              /* (0: OFF, below) */
        uint32_t st = BEND_ST[i];
        int32_t full = (int32_t)st * 4096, half;
        memset(&w, 0, sizeof w);
        ok &= pb(&w, &c, 535, st) == 0;
        ok &= pb(&w, &c, 535 + 13, st) == 0 && pb(&w, &c, 535 + 15, st) > 0;   /* the dead band */
        half = pb(&w, &c, 535 + 14 + (633 - 535 - 14) / 2, st);              /* half the travel past the dead band */
        ok &= half > full / 2 - 64 && half < full / 2 + 64;
        ok &= pb(&w, &c, 451, st) == -full && pb(&w, &c, 448, st) == -full;   /* full down (the taught end, the stop) */
        ok &= pb(&w, &c, 633, st) == full && pb(&w, &c, 636, st) == full;
    }
    check(ok, "taught: full down / up = -/+ BEND (0..12), half way = half, the dead band");
    memset(&w, 0, sizeof w);
    snprintf(b, sizeof b, "BEND UP 2: +2 semitones = %d/4096 at full up", (int)pb(&w, &c, 640, 2));
    check(pb(&w, &c, 640, 2) == 2 * 4096, b);

    /* power-on: a wheel resting near the taught rest is the centre; one held is not */
    memset(&w, 0, sizeof w);
    pb(&w, &c, 545, 2);
    check(w.pc == 545 && pb(&w, &c, 545, 2) == 0, "power-on: resting within the dead band (545): re-centred there");
    memset(&w, 0, sizeof w);
    check(pb(&w, &c, 600, 2) > 0 && w.pc == 535, "power-on: a wheel held up (600): the taught rest, it bends");
    check(pb(&w, &c, 535, 2) == 0, "... and let go: no bend");

    /* not taught (older data): as before, the first reading the centre, the ends 80 away, dead band 12 */
    memset(&w, 0, sizeof w);
    check(pb(&w, &nocal, 540, 7) == 0 && w.pc == 540 && w.plo == 460 && w.phi == 620 && w.dead == 12,
          "not taught (older data): the first reading the centre, as before");
    check(pb(&w, &nocal, 620, 7) == 7 * 4096, "not taught: +80 = the full range");
    c.magic = 0;
    memset(&w, 0, sizeof w);
    check(!smk_wcal_pitch_ok(&c) && !smk_wcal_mod_ok(&c) && pb(&w, &c, 540, 2) == 0 && w.dead == 12,
          "a calibration without its mark is not used");
    c.magic = SMK_WCAL_MAGIC;

    /* the mod wheel */
    memset(&m, 0, sizeof m);
    check(smk_mw_val(&m, &c, 628) == 0 && smk_mw_val(&m, &c, 620) == 0, "mod taught: at the bottom (and its noise): 0");
    check(smk_mw_val(&m, &c, 479) == 127 && smk_mw_val(&m, &c, 476) == 127, "mod taught: full up: 127");
    memset(&m, 0, sizeof m);
    i = (uint32_t)smk_mw_val(&m, &c, 628 - 10 - (628 - 479 - 10) / 2);
    check(i >= 62 && i <= 65, "mod taught: half way: 64");
    memset(&m, 0, sizeof m);
    check(smk_mw_val(&m, &nocal, 615) == 0 && smk_mw_val(&m, &nocal, 485) == 127 && m.dead == 12,
          "mod not taught: 485..615 as before");
}

int main(void)
{
    char b[96];
    new_features();
    reset();
    check(nb == 0 && ob == 0, "at rest: no bend");
    move(REST, REST + 100, 60);                 /* a quick bend to the top */
    move(REST + 100, REST + 100, 200);
    check(nb == SMK_PW_ST * 4096, "a quick bend to the top: +7 semitones");
    move(REST + 100, REST, 40);                 /* the spring */
    move(REST, REST, 1000);
    check(nb == 0, "let go: back to 0");

    reset();                                    /* the report: a slow touch of the wheel, then the spring */
    move(REST, REST + 60, 1500);
    move(REST + 60, REST + 60, 300);
    move(REST + 60, REST, 40);
    move(REST, REST, 3000);
    snprintf(b, sizeof b, "v0.1: a slow touch leaves a bend at rest (%d/4096 st)", (int)ob);
    check(ob != 0, b);                          /* (shows the fault: the old algorithm sticks) */
    check(nb == 0, "fixed: a slow touch, then let go: back to 0");

    reset();                                    /* very slow, in both directions */
    move(REST, REST - 70, 4000);
    move(REST - 70, REST, 30);
    move(REST, REST, 2000);
    check(nb == 0, "fixed: a very slow bend down, then let go: back to 0");

    reset();                                    /* a held bend stays */
    move(REST, REST + 50, 100);
    move(REST + 50, REST + 50, 5000);
    check(nb > 0, "fixed: a bend held for 5 s stays");
    move(REST + 50, REST, 40);
    move(REST, REST, 1000);
    check(nb == 0, "fixed: ... and goes when let go");

    reset();                                    /* the rest drifts (temperature, supply): followed */
    move(REST, REST + 8, 20000);
    move(REST + 8, REST + 8, 5000);
    move(REST + 8, REST + 20, 60000);
    move(REST + 20, REST + 20, 5000);
    check(nb == 0 && nw.pc >= REST + 15, "fixed: a slow drift of the rest is followed (no bend)");

    printf("smk_wheel: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
