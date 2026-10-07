/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_adc.h: MASTER is the panel knob, the battery is full */
#pragma once
#include <stdint.h>
#include "emu_bridge.h"

enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static void fm1_adc_init(void) {}
static int32_t fm1_adc_read(uint32_t ch) { return emu_adc(ch); }
