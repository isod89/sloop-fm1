/* SPDX-License-Identifier: GPL-3.0-only */
/* Visible keyboard hints use complete scan slots, not longer waits inside the input ISR.
 * Spread the on-frames across an eight-frame cycle (~1.1 ms/frame on the FM-1).
 * LOW/MID/HIGH: scale 2/8, 3/8, 4/8; root 4/8, 5/8, 6/8. Played keys remain 8/8.
 * All instantaneous drive levels stay within the existing fully-lit LED setting. */
#pragma once
#include <stdint.h>

static inline uint32_t fm1_led_pwm_mask(uint32_t full, uint32_t scale, uint32_t roots,
                                     uint32_t phase, uint32_t level)
{
    if (level >= 1u && level <= 3u) {
        uint32_t slot = ((phase & 1u) << 2) | (phase & 2u) | ((phase & 4u) >> 2);
        if (slot < level + 1u)
            full |= scale;
        if (slot < level + 3u)
            full |= roots;
    }
    return full;
}
