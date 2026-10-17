/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* HOME menu > SCREEN > KNOB LAYOUT: which of the SMK-37's eight knobs (K1..K8 as printed) does what. Included by panel.c
 * after its roles (EN_*); no hardware here: tests/smk_knobs_test.c runs it.
 *   FM-1 (0, the default): the FM-1's panel. K1 MASTER, K2 SELECT (tempo / pages), K3 ALGORITHM (track), K4 PRESETS
 *   (sound / kit); K5..K8 KNOB 1..4 (the screen's four dials).
 *   SMK (1): K1..K4 KNOB 1..4, K5 MASTER, K6 SELECT, K7 PRESETS, K8 ALGORITHM.
 * HARDWARE CALIBRATION stays per knob as printed: role r's slot of the panel table is the knob printed K(r + 2) (the
 * calibration screen names it so, E_NAME), K1 is the MASTER encoder (smk_input.h). Kept in the settings (project.c). */
#pragma once
static uint8_t knob_layout;
#define EN_MASTER NE                     /* (a role here: the MASTER volume) */
static const uint8_t KNOB_OF[2][NE + 1u] = {{1, 2, 3, 4, 5, 6, 7, 0}, {5, 7, 6, 0, 1, 2, 3, 4}};   /* role -> printed K - 1 */
static const char *const E_NAMES[2][NE] = {{"K2 SELECT", "K3 ALGORITHM", "K4 PRESETS", "K5", "K6", "K7", "K8"},
                                           {"K2", "K3", "K4", "K5 MASTER", "K6 SELECT", "K7 PRESETS", "K8 ALGORITHM"}};
#define E_NAME (E_NAMES[knob_layout & 1u])   /* the calibration's names: slot i is the knob printed K(i + 2) */
static uint32_t knob_of(uint32_t role) { return KNOB_OF[knob_layout & 1u][role <= NE ? role : NE]; }   /* 0 = K1 */
static char knob_digit(uint32_t role) { return (char)('1' + knob_of(role)); }   /* the printed number, for the screen */
