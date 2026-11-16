/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Patrick F. Kellogg
 *
 * The FM-1 Big Screen mirror in SLOOP: while an app asks (F0 7D 'F' 'S' 00/01 F7, about once a second), every
 * rectangle sent to the LCD (lcd.c lcd_blit / lcd_blit_stride / lcd_fill) also goes out over USB MIDI SysEx, so
 * FM-1 Mirror (github.com/pfkellogg/fm1-mirror) and FM-1 Mirror + Spyglass show the screen big. screen_mirror.c
 * makes the messages; this is the glue: the hello from usb.c sysex_byte, sending through usb.c's SysEx ring without
 * ever waiting on it, and the main loop's poll; and the sound playing ("T1 FM6 ICE BELL") as its own message, also to
 * apps that ask for only that (F0 7D 'F' 'S' 02 F7). With nobody asking it does nothing. USB only (no Bluetooth MIDI in SLOOP).
 */
#define SM_BUFFER 6144u                          /* 6 KB of queued messages (Big Screen: 16 KB) */
#include "screen_mirror.c"

static uint16_t mirror_row[240];                 /* one row, back in RGB565 order (the LCD's buffers are byte-swapped) */
static uint8_t mirror_lost;                      /* rows dropped: the whole screen again when there's room */
static uint32_t mirror_full_at;                  /* the last whole-screen redraw asked for (at most twice a second) */

/* One SysEx message into usb.c's ring, or -2 (busy) when it has no room for all of it: never waits */
static int mirror_send(const uint8_t *m, unsigned n)
{
    if (!usb.config)
        return -1;
    if (SXQ - (so_w - so_r) < (n + 2u) / 3u + 1u)
        return -2;
    return ota_wire_send(m, n) ? -1 : 0;
}

/* lcd.c: a rectangle about to go to the LCD; px (byte-swapped, stride pixels a row) or, for a fill, NULL and the
 * colour c */
static void mirror_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint16_t c)
{
    uint32_t r, i, start = fm1_ms;
    if (!sm_on() || mirror_lost)
        return;
    for (r = 0; r < h; r++) {
        uint32_t t0 = fm1_ms;
        if (fm1_ms - start > 400u) {                   /* the host isn't keeping up: never hold the screen longer */
            mirror_lost = 1;
            return;
        }
        for (i = 0; i < w; i++)
            mirror_row[i] = px ? (uint16_t)((px[r * stride + i] >> 8) | (px[r * stride + i] << 8)) : c;
        while (!sm_rows(mirror_row, x, y + r, w, 1)) {   /* full: send some, briefly; else the whole screen later */
            sm_poll(fm1_ms, mirror_send);
            if (fm1_ms - t0 > 60u) {                     /* (a row is at most ~10 messages: a few ms at USB speed) */
                mirror_lost = 1;
                return;
            }
        }
    }
}

/* the sound playing now, as the header's middle shows it: "T1 FM6 ICE BELL" (the selected track, its engine, its
 * sound: a user patch's name or the engine's preset) */
static void mirror_sound(char *b)
{
    const track_t *t = &trk[song.sel % NTRK];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    unsigned n;
    b[0] = 'T';
    b[1] = (char)('1' + song.sel % NTRK);
    b[2] = ' ';
    b[3] = 0;
    str_cpy(b + 3, is_drum(t) ? "DRUM" : e->name, 12);   /* as the footer (ui_draw.c draw_foot): engine, then sound */
    n = str_len(b);
    b[n] = ' ';
    b[n + 1u] = 0;
    if (is_drum(t))
        str_cpy(b + n + 1u, DRUM_KIT_NAMES[drum_kit()], 16);
    else if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b + n + 1u);
    else if (e->npresets)
        str_cpy(b + n + 1u, e->presets[t->preset % e->npresets].name, 16);
    if (!b[n + 1u])
        b[n] = 0;
}

/* SLOOP's tempo for an app that plays along (Scrub-a-dub on YouTube: it sends the MIDI clock at this BPM while its video
 * plays): F0 7D 'F' 'S' 03 bpm_lo bpm_hi F7, while anyone asks (screen_mirror.c's name listeners: any hello in 3 s),
 * when it changes and once a second. Stopped, the BPM is the BPM knob's; following a clock, the clock's. */
static void mirror_bpm(void)
{
    static uint32_t sent_at, last;
    uint32_t bpm = (uint32_t)song.g[G_BPM];
    uint8_t m[8];
    if (!sm_name_live || (bpm == last && fm1_ms - sent_at < 1000u))
        return;
    m[0] = 0xf0; m[1] = 0x7d; m[2] = 'F'; m[3] = 'S'; m[4] = 3;
    m[5] = (uint8_t)(bpm & 127u); m[6] = (uint8_t)(bpm >> 7 & 127u); m[7] = 0xf7;
    if (mirror_send(m, 8) == 0) {
        last = bpm;
        sent_at = fm1_ms;
    }
}

/* main loop: send what's queued; a new viewer (or rows dropped) gets the whole screen redrawn; the sound's name to
 * anyone asking (every 100 ms it's looked at: sm_name sends it only after a hello or a change) */
static void mirror_service(void)
{
    static uint32_t named_at;
    sm_poll(fm1_ms, mirror_send);
    if (fm1_ms - named_at >= 100u) {
        char b[48];
        named_at = fm1_ms;
        mirror_sound(b);
        sm_name(b, fm1_ms, mirror_send);
        mirror_bpm();
    }
    if (sm_take_full_request() || (mirror_lost && sm_on() && fm1_ms - mirror_full_at >= 500u)) {
        mirror_lost = 0;
        mirror_full_at = fm1_ms;
        ui.force = 1;
    }
}
