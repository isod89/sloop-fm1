/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37: DAW mode in the web editor (editor.html DAW MAP; web/EDITOR_PROTOCOL.md). A device without it does not
 * answer these (the editor shows no DAW MAP then).
 *   80 DAW_GET [which]   -> which, 1 (this format), MODE, CHANNEL, LOCAL, CC MAP, KNOBS, N (71), N x (type, number,
 *                           channel): which 0 the DEFAULT map, 1 the USER map (no byte: 1)
 *   81 DAW_PUT MODE, CHANNEL, LOCAL, CC MAP, KNOBS [, N, N x (type, number, channel)] -> as DAW_GET 1, after it.
 *                           A setting out of range stays as it was; an entry out of range becomes its default; entries
 *                           past N stay. Kept in flash with the settings, once the device is quiet (no flash write here)
 *   82 STOCK_RELEASE        -> rc: 0 the stock firmware's marker erased (back on the M-Vave firmware it rebuilds its
 *                           factory key / knob / fader mapping), 1 there was none, 3 stop the song first (storage.c) */
enum { ED_DAW_GET = 80, ED_DAW_PUT, ED_STOCK_RELEASE };

static void ed_daw_reply(uint32_t which)
{
    uint32_t i;
    ed_b(which);
    ed_b(1);
    ed_b(daw_on);
    ed_b(daw_ch);
    ed_b(daw_local);
    ed_b(daw_user);
    ed_b(daw_rel);
    ed_b(DAW_N);
    for (i = 0; i < DAW_N; i++) {
        daw_ent_t e = which ? daw_umap[i] : daw_default(i);
        ed_b(e.type);
        ed_b(e.num);
        ed_b(e.ch);
    }
}

static int ed_daw_handle(uint32_t cmd, const uint8_t *a, uint32_t na)   /* 1: one of these (reply built) */
{
    uint32_t i, n;
    if (cmd == ED_STOCK_RELEASE) {
#if FELUCCA_FLASH
        ed_b(song.playing || transport_req ? 3u : !flash_ok ? 1u : st_stock_release() ? 0u : 1u);
#else
        ed_b(1);
#endif
        return 1;
    }
    if (cmd == ED_DAW_GET) {
        ed_daw_reply(na >= 1u ? a[0] != 0u : 1u);
        return 1;
    }
    if (cmd != ED_DAW_PUT)
        return 0;
    if (na >= 5u) {
        if (a[1] >= 1u && a[1] <= 16u)
            daw_ch = a[1];
        if (a[2] <= 1u)
            daw_local = a[2];
        if (a[3] <= 1u)
            daw_user = a[3];
        if (a[4] <= 1u)
            daw_rel = a[4];
        if (na >= 6u) {
            n = a[5] < DAW_N ? a[5] : DAW_N;
            for (i = 0; i < n && 6u + 3u * i + 2u < na; i++) {
                daw_ent_t e = {a[6u + 3u * i], a[7u + 3u * i], a[8u + 3u * i]};
                daw_umap[i] = daw_ent_ok(i, &e) ? e : daw_default(i);
            }
        }
        if (a[0] <= 1u && a[0] != daw_on)
            daw_set(a[0], 1);                     /* (the device shows the mode it is in now) */
        settings_later = 1;
        ui.force = 1;
    }
    ed_daw_reply(1);
    return 1;
}
