/* SPDX-License-Identifier: GPL-3.0-only */
/* The flash parts the loader, the app and the USB rescue accept (firmware/hal/fm1_flash_id.h): the first FM-1s'
 * Puya P25Q80H and the later units' UCUN UC25HQ80 (SLOOP #228, #238), nothing else. And the three callers ask
 * fl_jedec_known(), none compares an id of its own. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/hal/fm1_flash_id.h"

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *s;
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    s = malloc((size_t)n + 1);
    if (!s || fread(s, 1, (size_t)n, f) != (size_t)n) { perror(path); exit(1); }
    s[n] = 0;
    fclose(f);
    return s;
}

int main(void)
{
    static const char *const callers[] = {"firmware/loader/loader.c", "firmware/src/project.c", "firmware/src/recovery.c"};
    unsigned i;
    assert(fl_jedec_known(0x856014u));          /* Puya P25Q80H */
    assert(fl_jedec_known(0xB36014u));          /* UCUN UC25HQ80 */
    assert(!fl_jedec_known(0));                 /* no answer (fl_enter failed) */
    assert(!fl_jedec_known(0xFFFFFFu));         /* a floating bus */
    assert(!fl_jedec_known(0xB36015u));         /* the same maker, 2 MiB: another layout */
    assert(!fl_jedec_known(0x856013u));         /* 512 KiB */
    assert(!fl_jedec_known(0xEF4014u));         /* an unknown maker */
    for (i = 0; i < sizeof callers / sizeof callers[0]; i++) {
        char *s = slurp(callers[i]);
        assert(strstr(s, "fl_jedec_known(") && "each caller asks fm1_flash_id.h");
        assert(!strstr(s, "0x856014") && !strstr(s, "0xB36014") && "no id compared on its own");
        free(s);
    }
    puts("flash id: P25Q80H and UC25HQ80 accepted, other ids refused, the loader, app and rescue use one list");
    return 0;
}
