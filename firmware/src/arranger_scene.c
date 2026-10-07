/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared scene application for firmware and host audio integration tests. */
static uint32_t arrangement_ready(void)
{
    uint32_t i, ready = 0;
    for (i = 0; i < ARR_SCENES; i++)
        if (proj_ok(&proj_slot[i])) ready |= 1u << i;
    return ready;
}

/* Called only at an audio-block boundary, after validation of all slots at
 * start. The song keeps one tempo and global FX across every section (only the drum
 * level and reverb come from it). */
static void arrangement_apply(uint32_t scene)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        seq_release(t);
        trk_all_off(t);
        t->nheld = t->arp_phys = t->arp_note = t->rh_n = t->rskip_n = 0;
        t->rskip_lanes = 0;
    }
    if (!projb_ok(&proj_slot_b[scene], &proj_slot[scene]))   /* (made at boot or on store: not expected) */
        proj_b_default(&proj_slot_b[scene], &proj_slot[scene]);
    proj_apply(&proj_slot[scene], &proj_slot_b[scene], 0);
    sync_reload = 1;
    ui.force = 1;
}

/* ---- song mode keeps the loop you made: PLAY in song mode puts it aside (each section then plays
 * over the tracks), STOP (or the song's end) brings it back */
static project_t song_keep __attribute__((section(".pool")));
static project_b_t song_keep_b;                 /* (RAM: the pool keeps its 8 KiB spare) */
static uint8_t song_kept;
static void song_backup(void)                  /* (audio ISR: seq_start) */
{
    proj_capture(&song_keep, &song_keep_b);
    song_kept = 1;
}
static void song_restore(void)                 /* (audio ISR: seq_stop) */
{
    uint32_t i;
    if (!song_kept)
        return;
    song_kept = 0;
    for (i = 0; i < NTRK; i++) {
        trk_all_off(&trk[i]);
        trk[i].nheld = trk[i].arp_phys = trk[i].arp_note = 0;
    }
    proj_apply(&song_keep, &song_keep_b, 1);
    song.sel = (uint8_t)(song_keep.sel < NTRK ? song_keep.sel : 0u);
    sync_reload = 1;
    ui.force = 1;
}
