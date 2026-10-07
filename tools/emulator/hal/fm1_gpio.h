/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_gpio.h: the port names, no registers */
#pragma once
#include <stdint.h>
enum { FM1_PA = 0, FM1_PB = 1, FM1_PC = 2, FM1_PH = 7 };
enum { FM1_OUT = 0x00, FM1_IN = 0x04, FM1_DIR = 0x08, FM1_DIE = 0x0C, FM1_PU = 0x10,
       FM1_PD = 0x14, FM1_HD0 = 0x18, FM1_HD = 0x1C };
