/* SPDX-License-Identifier: GPL-3.0-only */
#include "harmony.h"

#include <assert.h>
#include <stdio.h>

static void check(Chord chord, const Voicing *v)
{
    static const unsigned masks[] = {0x48c, 0x614, 0x894, 0x449, 0x492, 0x28c, 0x489};
    unsigned pitch_classes = 0;
    assert(v->bass == 36 + chord.root);
    for (unsigned i = 0; i < HARMONY_VOICES; ++i) {
        if (i) assert(v->upper[i] > v->upper[i - 1]);
        pitch_classes |= 1u << ((v->upper[i] - chord.root) % 12);
    }
    assert(pitch_classes == masks[chord.quality]);
}

int main(void)
{
    static HarmonyScratch scratch;
    Chord chords[HARMONY_MAX_CHORDS];
    Voicing result[HARMONY_MAX_CHORDS];
    for (unsigned tonic = 0; tonic < 12; ++tonic) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            unsigned length = harmony_phrase((uint8_t)tonic, variant, chords);
            assert(harmony_voice_loop(chords, length, &scratch, result));
            Voicing live;
            for (unsigned i = 0; i < length; ++i) {
                check(chords[i], &result[i]);
                assert(harmony_voice_live(chords[i], i ? &live : NULL, &live));
                check(chords[i], &live);
            }
        }
    }
    assert(!harmony_voice_loop(chords, 0, &scratch, result));
    assert(!harmony_voice_loop(chords, HARMONY_MAX_CHORDS + 1, &scratch, result));
    assert(!harmony_voice_live((Chord){12, MIN9}, NULL, result));
    assert(!harmony_voice_live((Chord){0, (Quality)99}, NULL, result));
    unsigned length = harmony_phrase(0, 0, chords);
    assert(harmony_voice_loop(chords, length, &scratch, result));
    for (unsigned i = 0; i < length; ++i) {
        printf("Bass %u | upper", (unsigned)result[i].bass);
        for (unsigned j = 0; j < HARMONY_VOICES; ++j)
            printf(" %u", (unsigned)result[i].upper[j]);
        puts("");
    }
    /* the diatonic degrees: C major and A minor share their chords */
    for (unsigned d = 0; d < 7; ++d) {
        Chord a = harmony_degree(0, d, 0, 0), b = harmony_degree(9, (d + 2) % 7, 1, 0);
        Voicing v;
        assert(a.root == b.root && a.quality == b.quality);
        assert(harmony_voice_live(a, NULL, &v));
        check(a, &v);
    }
    assert(harmony_degree(0, 1, 0, 1).root == 9 && harmony_degree(0, 1, 0, 1).quality == DOM7_FLAT9); /* V/ii = A7b9 */
    printf("Checked 36 phrases in 12 keys. Voicer scratch: %zu bytes.\n", sizeof scratch);
    return 0;
}
