/* SPDX-License-Identifier: GPL-3.0-only */
/* YARD's factory sounds' loudness, for the same ordinary phrase as Clipper's (tools/loud_phrase.h, tools/clp_loud.c):
 *   name engine mono trim_dB lufs lufs_nopressure peak momentary_max   (the same columns; engine YARD, trim 0)
 *   make loud */
#include <stdio.h>
#include "../app/synth.c"
#include "loud_phrase.h"
static void ev(uint32_t a, uint32_t b, uint32_t c) { synth_event(a, b & 127u, c & 127u); }
static void ren(int32_t *lr, uint32_t n) { synth_render(lr, n, 4096u); }
int main(void)
{
    unsigned s;
    for (s = 0; s < SYNTH_NSOUNDS; s++) {
        double pk, pk0, l, l0, mm;
        unsigned off = SYNTH_SOUNDS[s].p[SP_SUB] > 60u ? 24u : 0u;
        synth_init(SYNTH_SOUNDS[s].p);
        synth_mode(SYNTH_SOUNDS[s].mono, 0);
        l = lp_play(ev, ren, SYNTH_SOUNDS[s].mono, 100, 64, off, &pk);
        mm = lp_mmax;
        synth_init(SYNTH_SOUNDS[s].p);
        synth_mode(SYNTH_SOUNDS[s].mono, 0);
        l0 = lp_play(ev, ren, SYNTH_SOUNDS[s].mono, 100, 0, off, &pk0);
        printf("%-16s %-8s %d %6.1f %6.1f %6.1f %6.1f %6.1f\n", SYNTH_SOUNDS[s].name, "YARD", SYNTH_SOUNDS[s].mono, 0.0, l, l0, pk, mm);
    }
    return 0;
}
