/* SPDX-License-Identifier: GPL-3.0-only */
/* ONSET's levels (app/synth.c, its table's gain): each of the ten bursts, alone, 64 times at full level and velocity
 * 127, its mean peak; the gain that brings it to 0.85, as Vane's gen_inharmonic.py normalises its samples. Prints the
 * gains to paste in. make onset-cal */
#include <stdio.h>
#include "../app/synth.c"

int main(void)
{
    uint32_t t, r, i;
    static const uint8_t P[SP_N] = {0};
    synth_init(P);
    prm[SP_ONSET_LVL] = 127;
    for (t = 1; t < SYNTH_ONSETS; t++) {
        double sum = 0;
        prm[SP_ONSET] = (uint8_t)((t * 128u + SYNTH_ONSETS - 1u) / SYNTH_ONSETS);
        for (r = 0; r < 64u; r++) {
            voice_t v = {0};
            float pk = 0;
            v.on_nz = 0x1234567u + r * 7919u;
            onset_start(&v, 127);
            v.on_amp /= ONSET[t].gain;   /* (as if its gain were 1) */
            for (i = 0; v.on_n && i < 48000u; i++) {
                float x = onset_tick(&v);
                pk = fm_maxf(pk, fm_fabsf(x));
            }
            sum += pk;
        }
        printf("%-8s mean peak %.3f  gain %.2f\n", SYNTH_ONSET_NAMES[t], sum / 64.0, 0.85 / (sum / 64.0));
    }
    return 0;
}
