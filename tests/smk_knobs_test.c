/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 HOME menu > SCREEN > KNOB LAYOUT (firmware/src/smk_knobs.h): each layout gives every printed knob K1..K8 one
 * role; FM-1 is the FM-1's panel (as before), SMK the dials on K1..K4, MASTER K5, SELECT K6, PRESETS K7, ALGORITHM K8;
 * the calibration's names stay the printed knobs; the screen's labels (menu rows, help) follow the layout. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };   /* (panel.c) */
#include "../firmware/src/smk_knobs.h"
static int fails;
static void check(int ok, const char *what) { printf("smk_knobs: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(void)
{
    uint32_t l, r, seen, ok = 1;
    static const char *const ROLE[NE + 1] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "MASTER"};
    for (l = 0; l < 2u; l++) {
        knob_layout = (uint8_t)l;
        for (seen = 0, r = 0; r <= NE; r++)
            seen |= 1u << knob_of(r);
        ok &= seen == 0xFFu;
    }
    check(ok, "both layouts: each printed knob K1..K8 has exactly one role");
    knob_layout = 0;
    check(knob_of(EN_MASTER) == 0 && knob_of(EN_SELECT) == 1 && knob_of(EN_ALGO) == 2 && knob_of(EN_PRESET) == 3 &&
          knob_of(EN_K1) == 4 && knob_of(EN_K4) == 7, "FM-1 (default): K1 MASTER, K2 SELECT, K3 ALGORITHM, K4 PRESETS, K5..K8 KNOB 1..4");
    knob_layout = 1;
    check(knob_of(EN_K1) == 0 && knob_of(EN_K2) == 1 && knob_of(EN_K3) == 2 && knob_of(EN_K4) == 3,
          "SMK: K1..K4 the four on-screen dials (KNOB 1..4)");
    check(knob_of(EN_MASTER) == 4 && knob_of(EN_SELECT) == 5 && knob_of(EN_PRESET) == 6 && knob_of(EN_ALGO) == 7,
          "SMK: K5 MASTER, K6 SELECT (tempo / pages), K7 PRESETS (sound / kit), K8 ALGORITHM (track)");
    for (ok = 1, l = 0; l < 2u; l++)
        for (r = 0; r < NE; r++)
            ok &= E_NAMES[l][r][0] == 'K' && E_NAMES[l][r][1] == (char)('2' + r);
    check(ok, "calibration: slot r is named for the knob printed K(r + 2) in both layouts (per knob)");
    for (ok = 1, l = 0; l < 2u; l++) {
        knob_layout = (uint8_t)l;
        for (r = 0; r < NE; r++) {
            uint32_t k = r + 1u;                 /* slot r = printed K(r + 2) = knob index r + 1 */
            uint32_t role;
            for (role = 0; role <= NE && knob_of(role) != k; role++)
                ;
            /* a name beyond "Kn" names that knob's role in this layout */
            if (strlen(E_NAME[r]) > 2u)
                ok &= strstr(E_NAME[r], ROLE[role]) != 0;
            else
                ok &= role >= EN_K1 && role <= EN_K4;   /* (the dials: the bare knob) */
        }
    }
    check(ok, "calibration: the names say what each knob does in this layout");
    knob_layout = 0;
    check(knob_digit(EN_K1) == '5' && knob_digit(EN_K4) == '8' && knob_digit(EN_SELECT) == '2', "FM-1: menu rows K5..K8, help K2 SECTION");
    knob_layout = 1;
    check(knob_digit(EN_K1) == '1' && knob_digit(EN_K4) == '4' && knob_digit(EN_SELECT) == '6', "SMK: menu rows K1..K4, help K6 SECTION");
    check(knob_of(NE + 7u) == knob_of(EN_MASTER), "a role out of range: MASTER's (no read past the table)");
    printf("smk_knobs: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
