/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the actual sequencer, scene loader and synth mixer together. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return trk_is_part(i) ? i % NENGINES : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"

static void capture(uint32_t slot) { proj_capture(&proj_slot[slot], &proj_slot_b[slot]); }

int main(int argc, char **argv)
{
    uint32_t i, k, frame, at_change = 0, at_stop = 0;
    uint32_t active_tracks = 0;
    uint32_t simultaneous = 0;
    int32_t out[CTL * 2];
    uint64_t energy = 0;
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0;
    const uint32_t frames = (FS * 9u / CTL) * CTL;
    host_tracks_init();
    host_preset(&trk[0], 0, 4);        /* bass */
    host_preset(&trk[1], 1, 5);        /* pad */
    host_preset(&trk[2], 3, 0);        /* melody */
    for (i = 0; i < NTRK; i++) {
        trk[i].p[P_SLEN] = 16;
        trk[i].p[P_SDIV] = 2;
        trk[i].p[P_AMODE] = 0;
        trk[i].p[P_LEVEL] = 90;
    }
    for (k = 0; k < 16; k++) {
        uint8_t bass[] = {36}, chord[] = {60, 63, 67}, lead[] = {(uint8_t)(72 + (k % 3u) * 3u)};
        uint8_t drum[] = {42, (uint8_t)(k % 8u == 4u ? 38 : 36)};
        if (!(k % 4u)) put_step(&trk[0], k, 1, bass, ST_NOTE, 0);
        if (!(k % 8u)) put_step(&trk[1], k, 3, chord, ST_NOTE, 0);
        if (!(k % 2u)) put_step(&trk[2], k, 1, lead, ST_NOTE, 0);
        put_step(TDRUM, k, k % 4u ? 1 : 2, drum, ST_NOTE, 0);
    }
    capture(0);
    for (i = 0; i < NP3; i++)
        for (k = 0; k < 16; k++) {
            uint32_t j;
            for (j = 0; j < trk[i].step[k].n; j++) trk[i].step[k].note[j] += 5;
        }
    host_preset_req(&trk[2], 0, 1);             /* change the lead's engine at the section boundary */
    song.g[G_DRLVL] = 80;
    TDRUM->p[P_E0] = 4;                       /* drum kit is saved with the section */
    capture(1);
    arr_defaults(&arrangement);
    arrangement.count = 2;
    arrangement.entry[0] = (arr_entry_t){0, 2};
    arrangement.entry[1] = (arr_entry_t){1, 2};
    arrangement_enabled = 1;
    transport_req = 1;
    if (f) wav_hdr(f, frames);
    for (frame = 0; frame < frames; frame += CTL) {
        mix_block(out, CTL);
        if (song.playing && arrangement_clock.index == 1u && !at_change) at_change = frame;
        if (!song.playing && frame && !at_stop) at_stop = frame;
        uint32_t sounding = 0;
        for (i = 0; i < NP3; i++)
            for (k = 0; k < NVOICE; k++)
                if (trk[i].v[k].active) sounding |= 1u << i;
        for (k = 0; k < NDRUM; k++) if (drums.v[k].active) sounding |= 8;
        active_tracks |= sounding;
        if (sounding == 15u) simultaneous++;
        for (k = 0; k < CTL; k++) {
            energy += (uint64_t)(out[k*2] < 0 ? -(int64_t)out[k*2] : out[k*2]);
            if (f) wav_put(f, out[k*2], out[k*2+1]);
        }
    }
    if (f) fclose(f);
    assert(active_tracks == 15u && simultaneous > 100u && energy > 1000000u);
    assert(at_change >= FS*4u && at_change < FS*4u + CTL);
    assert(at_stop >= FS*8u && at_stop < FS*8u + CTL);
    assert(trk[0].step[0].note[0] == 41);
    assert(trk[2].engine == 0 && song.g[G_DRLVL] == 80);
    assert(drum_kit() == 4);
    for (i = 0; i < NTRK; i++) {
        if (!trk_is_part(i)) continue;
        assert(trk[i].seq_n == 0);
        for (k = 0; k < NVOICE; k++) assert(!trk[i].v[k].gate);
    }
    /* A missing later scene must prevent even the first scene from starting. */
    arrangement.entry[1].scene = 3;
    transport_req = 1; events_block(CTL);
    assert(!song.playing && arrangement_clock.error);
    {   /* LIVE: A as the loop, SONG REC armed; B asked for in bar 2 starts on bar 3; stop in B's 3rd bar */
        uint32_t bar = 4u * 60u * FS / (uint32_t)song.g[G_BPM], t, jumped = 0;
        arrangement.entry[1].scene = 1;
        arrangement_enabled = 0;
        proj_apply(&proj_slot[0], &proj_slot_b[0], 1);
        live_sec = 0;
        srec = 1;
        transport_req = 1;
        for (t = 0; t < bar * 5u - bar / 2u; t += CTL) {
            if (t >= bar + bar / 2u && t < bar + bar / 2u + CTL) live_req = 1;
            mix_block(out, CTL);
            if (live_sec == 1 && !jumped) jumped = t;
        }
        transport_req = 2;
        mix_block(out, CTL);
        assert(jumped >= 2u * bar && jumped < 2u * bar + CTL);      /* exactly on the bar */
        assert(trk[0].step[0].note[0] == 41 && srec == 0 && srec_done == 2u);
        assert(arrangement.count == 2u && arrangement.entry[0].scene == 0 && arrangement.entry[0].bars == 2u &&
               arrangement.entry[1].scene == 1 && arrangement.entry[1].bars == 3u);
        printf("song live: B on the bar (%u), SONG REC -> A 2 bars, B 3 bars PASS\n", jumped);
    }
    printf("song audio: 3 synth parts + drums, section at %u, stop at %u, no held sequencer notes PASS\n",at_change,at_stop);
    return 0;
}
