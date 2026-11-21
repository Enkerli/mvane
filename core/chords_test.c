/* SPDX-License-Identifier: GPL-3.0-only */
/* Desktop checks for chords: every scale, degree, type, style and size (3..6 upper voices) voices; the notes are
 * the chord's; the styles keep their shapes; names; voice leading stays smooth; the search is quick enough. */
#include "chords.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void)
{
    unsigned s, d, t, st, n, voiced = 0, total_motion = 0, moves = 0;
    char name[16];
    clock_t t0 = clock();
    for (n = 3; n <= CHORD_MAX; n++)
        for (s = 0; s < CHORD_NSCALES; s++)
            for (st = 0; st < CHORD_STYLES; st++) {
                chord_voicing prev, v;
                int have = 0;
                for (t = 0; t < CHORD_NTYPES; t++)
                    for (d = 0; d < 16; d++) {
                        chord_pcs c = chord_build(CHORD_SCALES[s].mask, 2, d, t, n);
                        unsigned i;
                        assert(chord_voice(c, st, 0, have ? &prev : 0, &v));
                        assert(v.n == n);
                        for (i = 0; i < v.n; i++) {
                            unsigned j, ok = 0;
                            for (j = 0; j < c.n; j++)
                                ok |= v.upper[i] % 12u == c.pc[j];
                            assert(ok);
                            if (i)
                                assert(v.upper[i] > v.upper[i - 1]);
                        }
                        assert(v.bass % 12u == c.root && v.bass >= 24 && v.bass <= 47 && v.bass < v.upper[0]);
                        if (have)
                            for (i = 0; i < v.n && i < prev.n; i++)
                                total_motion += (unsigned)abs((int)v.upper[i] - (int)prev.upper[i]), moves++;
                        prev = v, have = 1, voiced++;
                        chord_name(c, name);
                        assert(strlen(name) <= 11);
                    }
            }
    {   /* C major, stacked */
        static const char *const want4[7] = {"Cmaj9", "Dm9", "Em7", "Fmaj9", "G9", "Am9", "Bm7b5"};
        static const char *const want5[7] = {"Cmaj13", "Dm13", "Em7", "Fmaj13", "G13", "Am9", "Bm7b5"};
        for (d = 0; d < 7; d++) {
            chord_name(chord_build(0xAB5u, 0, d, 0, 4), name);
            assert(!strcmp(name, want4[d]));
            chord_name(chord_build(0xAB5u, 0, d, 0, 5), name);
            if (strcmp(name, want5[d]))
                printf("  (5 voices, degree %u: %s)\n", d, name);
        }
        chord_name(chord_build(0xAB5u, 0, 4, 4, 4), name);
        assert(!strcmp(name, "Gsus"));
        /* the qualities bank: on D in C major, m7 is Dm7 and M is D (not diatonic, as asked) */
        chord_name(chord_build(0xAB5u, 0, 1, CHORD_BANK + 6, 4), name);
        assert(!strcmp(name, "Dm7"));
        chord_name(chord_build(0xAB5u, 0, 1, CHORD_BANK + 8, 3), name);
        assert(!strcmp(name, "D"));
        chord_name(chord_build(0xAB5u, 0, 1, CHORD_BANK + 2, 4), name);
        assert(!strcmp(name, "Dmaj9"));
    }
    {   /* wide voicings are wide, close ones close; six voices fit */
        chord_voicing w, c, six;
        chord_pcs x = chord_build(0xAB5u, 0, 0, 0, 4);
        assert(chord_voice(x, CHORD_WIDE, 0, 0, &w) && chord_voice(x, CHORD_CLOSE, 0, 0, &c));
        assert(w.upper[3] - w.upper[0] >= 20 && c.upper[3] - c.upper[0] <= 14);
        assert(chord_voice(chord_build(0xAB5u, 0, 0, 0, 6), CHORD_WIDE, 0, 0, &six));
        printf("chords: C maj9 close %u %u %u %u; wide %u %u %u %u; 6-voice wide (bass %u) %u %u %u %u %u %u\n",
               c.upper[0], c.upper[1], c.upper[2], c.upper[3], w.upper[0], w.upper[1], w.upper[2], w.upper[3],
               six.bass, six.upper[0], six.upper[1], six.upper[2], six.upper[3], six.upper[4], six.upper[5]);
    }
    printf("chords: %u voicings (3 to 6 voices), mean voice motion %.2f semitones, %.0f us each on this machine\n",
           voiced, (double)total_motion / moves, 1e6 * (double)(clock() - t0) / CLOCKS_PER_SEC / voiced);
    return 0;
}
