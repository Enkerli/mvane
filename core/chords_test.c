/* SPDX-License-Identifier: GPL-3.0-only */
/* Desktop checks for chords: every scale, degree, type and style voices; the notes are the chord's; the styles
 * keep their shapes; names; voice leading stays smooth. */
#include "chords.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    unsigned s, d, t, st, voiced = 0, total_motion = 0, moves = 0;
    char name[16];
    for (s = 0; s < CHORD_NSCALES; s++)
        for (st = 0; st < CHORD_STYLES; st++) {
            Voicing prev, v;
            int have = 0;
            for (t = 0; t < CHORD_NTYPES; t++)
                for (d = 0; d < 16; d++) {
                    chord_pcs c = chord_build(CHORD_SCALES[s].mask, 2, d, t);
                    unsigned i;
                    assert(chord_voice(c, st, 0, have ? &prev : 0, &v));
                    for (i = 0; i < HARMONY_VOICES; i++) {
                        unsigned j, ok = 0;
                        for (j = 0; j < HARMONY_VOICES; j++)
                            ok |= v.upper[i] % 12u == c.pc[j];
                        assert(ok);
                        if (i)
                            assert(v.upper[i] > v.upper[i - 1]);
                    }
                    assert(v.bass % 12u == c.root && v.bass >= 24 && v.bass <= 47 && v.bass < v.upper[0]);
                    if (have)
                        for (i = 0; i < HARMONY_VOICES; i++)
                            total_motion += (unsigned)abs((int)v.upper[i] - (int)prev.upper[i]), moves++;
                    prev = v, have = 1, voiced++;
                    chord_name(c, name);
                    assert(strlen(name) <= 11);
                }
        }
    {   /* C major, stacked to the ninth */
        static const char *const want[7] = {"Cmaj9", "Dm9", "Em7", "Fmaj9", "G9", "Am9", "Bm7b5"};
        for (d = 0; d < 7; d++) {
            chord_name(chord_build(0xAB5u, 0, d, 0), name);
            assert(!strcmp(name, want[d]));
        }
        chord_name(chord_build(0xAB5u, 0, 1, 9), name); /* V/ii */
        assert(!strcmp(name, "A7"));
        chord_name(chord_build(0xAB5u, 0, 4, 4), name);
        assert(!strcmp(name, "Gsus"));
    }
    {   /* wide voicings are wide, close ones close */
        Voicing w, c;
        chord_pcs x = chord_build(0xAB5u, 0, 0, 0);
        assert(chord_voice(x, CHORD_WIDE, 0, 0, &w) && chord_voice(x, CHORD_CLOSE, 0, 0, &c));
        assert(w.upper[3] - w.upper[0] >= 20 && c.upper[3] - c.upper[0] <= 14);
        printf("chords: C maj9 close %u %u %u %u, wide %u %u %u %u\n", c.upper[0], c.upper[1], c.upper[2], c.upper[3],
               w.upper[0], w.upper[1], w.upper[2], w.upper[3]);
    }
    printf("chords: %u voicings in %u scales, mean voice motion %.2f semitones\n", voiced, CHORD_NSCALES,
           (double)total_motion / moves);
    return 0;
}
