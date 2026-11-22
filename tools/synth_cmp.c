/* SPDX-License-Identifier: GPL-3.0-only */
/* The saw (app/synth.c) through a fixed expressive scenario, on the desktop: its output to a file and the time it
 * took. Built twice by `make synth-cmp`, with the filter's coefficient tables and with -DSYNTH_EXACT, and compared
 * by tools/synth_cmp.py: the tables must not change what is heard.
 *   0-3 s  a six-note chord (C2..B4, MPE channels 2-7), TONE swept 0 to 127
 *   3-6 s  TONE 64: each note bent slowly (+-2 semitones, out of phase), slide and pressure moving
 *   6-8 s  high notes (C7, E7, G7) bright and pressed: the cutoff at its top
 * Usage: synth_cmp OUT.raw [REPEATS] (int32 mono at 44.1 kHz; REPEATS renders for the timing) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../app/synth.c"

#define BLK 256u
static int32_t lr[2 * BLK];

static void ev(uint32_t st, uint32_t d1, uint32_t d2) { synth_event(st, d1 & 127u, d2 & 127u); }

static void scenario(FILE *out)
{
    static const uint8_t CHORD[6] = {36, 52, 55, 59, 64, 71}, HIGH[3] = {96, 100, 103};
    const uint8_t p[SP_N] = {0, 40, 0, 0};
    uint32_t b, i, c, nb = (uint32_t)(8.0 * 44100.0 / BLK);
    synth_init(p);
    synth_mode(0, 0);
    for (b = 0; b < nb; b++) {
        double t = (double)b * BLK / 44100.0;
        if (b == 0)
            for (c = 0; c < 6u; c++)
                ev(0x91u + c, CHORD[c], 100);
        if (t < 3.0)
            synth_set(SP_TONE, (uint32_t)(t / 3.0 * 127.0));
        else if (t < 6.0) {
            synth_set(SP_TONE, 64);
            for (c = 0; c < 6u; c++) {
                double ph = 2.0 * 3.14159265358979 * (0.5 * (t - 3.0) + c / 6.0);
                uint32_t bend = 8192u + (uint32_t)(int32_t)(341.0 * sin(ph));    /* +-2 semitones at 48 */
                ev(0xE1u + c, bend & 127u, bend >> 7);
                ev(0xB1u + c, 74, (uint32_t)(64.0 + 50.0 * sin(ph * 0.7)));
                ev(0xD1u + c, (uint32_t)(64.0 + 60.0 * cos(ph * 1.3)), 0);
            }
        } else {
            if (fabs(t - 6.0) < BLK / 44100.0) {
                for (c = 0; c < 6u; c++)
                    ev(0x81u + c, CHORD[c], 0);
                for (c = 0; c < 3u; c++) {
                    ev(0xE1u + c, 0, 64);
                    ev(0x91u + c, HIGH[c], 120);
                    ev(0xD1u + c, 120, 0);
                }
            }
            synth_set(SP_TONE, 127);
        }
        synth_render(lr, BLK, 4096u);
        if (out)
            for (i = 0; i < BLK; i++)
                fwrite(&lr[2u * i], 4, 1, out);
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    int r, reps = argc > 2 ? atoi(argv[2]) : 20;
    clock_t t0;
    if (argc < 2) {
        fprintf(stderr, "usage: synth_cmp OUT.raw [REPEATS]\n");
        return 2;
    }
    f = fopen(argv[1], "wb");
    if (!f)
        return 1;
    scenario(f);
    fclose(f);
    t0 = clock();
    for (r = 0; r < reps; r++)
        scenario(0);
    printf("%s: %.3f ms of CPU per second of sound\n", argv[1],
           (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0 / (reps * 8.0));
    return 0;
}
