/* SPDX-License-Identifier: GPL-3.0-only */
/* Each Clipper sound's loudness as the FM-1 plays it (trims on, MASTER full), for the same ordinary phrase
 * (tools/loud_phrase.h): velocity 100 and moderate pressure, then with no pressure. One line a sound:
 *   name engine mono trim_dB lufs lufs_nopressure peak momentary_max
 *   make loud */
#include <stdio.h>
#include <string.h>
#include "clp.c"
#include "loud_phrase.h"
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a) { (void)f, (void)a; return 0; }
int plat_cpu2_wait(void) { return 0; }

static void ev(uint32_t a, uint32_t b, uint32_t c) { clp_event(a, b & 127u, c & 127u); }
static void ren(int32_t *lr, uint32_t n) { clp_render(lr, n, 4096); }

int main(void)
{
    static int32_t lr[2 * LP_BLK];
    unsigned s, n, b;
    clp_init();
    clp_trim_on(1);
    n = clp_sounds();
    for (s = 0; s < n; s++) {
        unsigned k = clp_sound_key(s), q = (k >> 8) < NENGINES && (k & 0xFFu) < 40u ? CLP_TRIM_Q15[k >> 8][k & 0xFFu] : 0;
        int mono = clp_sound_mono(s);
        double pk, pk0, l, l0, mm;
        clp_panic();
        clp_select(s);
        for (b = 0; b < 40; b++)
            clp_render(lr, LP_BLK, 4096);
        l = lp_play(ev, ren, mono, 100, 64, 0, &pk);
        mm = lp_mmax;
        clp_panic();
        for (b = 0; b < 80; b++)
            clp_render(lr, LP_BLK, 4096);
        l0 = lp_play(ev, ren, mono, 100, 0, 0, &pk0);
        printf("%-16s %-8s %d %6.1f %6.1f %6.1f %6.1f %6.1f\n", clp_sound_name(s), clp_sound_engine(s), mono,
               q ? 20.0 * log10(q / 32768.0) : 0.0, l, l0, pk, mm);
    }
    return 0;
}
