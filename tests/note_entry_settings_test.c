/* SPDX-License-Identifier: GPL-3.0-only */
/* Existing 40-byte MIDI part and real flash storage, unchanged project/settings layouts. */
#include "editor_host.c"

int main(void)
{
    uint8_t baseline[MIDI_PART], raw[MIDI_PART];
    uint32_t mode, res, map, clk;
    persist_t p;
    harness_init();
    assert(MIDI_PART == 40 && sizeof(persist_t) == 88 && sizeof(project_t) == 3816);
    memset(ml_tab, 0, sizeof ml_tab); fxfade = 0; flt_reso = 0;
    drum_preview = step_adv_off = 0; step_entry_len = ENTRY_STEP;
    { const uint8_t def[10] = MIDI_SET_DEF; memcpy(midi_set, def, sizeof def); }
    midi_part_fill(raw); assert(midi_part_default(raw));
    step_entry_len = ENTRY_EIGHTH; midi_part_fill(raw); assert(!midi_part_default(raw));
    for (res = 0; res <= 10; res++) for (map = 0; map <= 1; map++) for (clk = 0; clk <= 1; clk++) {
        step_entry_len = 0; flt_reso = (uint8_t)res; midi_set[7] = (uint8_t)map; midi_set[9] = (uint8_t)clk;
        midi_set[6] = 15; midi_set[8] = 16; fxfade = 11; drum_preview = step_adv_off = 1;
        midi_part_fill(baseline);
        for (mode = 0; mode < ENTRY_LEN_N; mode++) {
            step_entry_len = (uint8_t)mode; midi_part_fill(raw);
            assert(!memcmp(raw, baseline, 38) && raw[39] == baseline[39]);
            assert((raw[38] & 63u) == baseline[38] && raw[38] >> 6 == mode);
            /* These are exactly the old firmware's decoders for byte 38. */
            assert((raw[38] & 1u) == map && ((raw[38] >> 1) & 1u) == clk && ((raw[38] >> 2) & 15u) == res);
            step_entry_len = 0; midi_part_read(raw, MIDI_PART);
            assert(step_entry_len == mode && flt_reso == res && midi_set[7] == map && midi_set[9] == clk);
            assert(midi_set[6] == 15 && midi_set[8] == 16 && fxfade == 11 && drum_preview && step_adv_off);
        }
    }
    step_entry_len = ENTRY_HALF; midi_part_read(raw, 32); assert(step_entry_len == ENTRY_STEP);
    step_entry_len = ENTRY_HALF; midi_part_read(raw, 36); assert(step_entry_len == ENTRY_STEP);
    step_entry_len = ENTRY_EIGHTH;
    persist_fill(&p); assert(settings_write(&p) == 0);
    step_entry_len = ENTRY_STEP;
    persist_boot(); assert(step_entry_len == ENTRY_EIGHTH);
    puts("NOTE_ENTRY_SETTINGS_OK: 176 packed-mode/filter/MIDI combinations, legacy sizes, defaults and flash boot");
    return 0;
}
