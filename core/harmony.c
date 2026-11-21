/* SPDX-License-Identifier: GPL-3.0-only */
#include "harmony.h"

#include "port.h"

static const uint8_t hm__intervals[][HARMONY_VOICES] = {
    {3, 7, 10, 14}, {4, 10, 14, 21}, {4, 7, 11, 14},
    {3, 6, 10, 12}, {4, 7, 10, 13}, {3, 7, 9, 14}, {3, 7, 10, 12}
};

static unsigned hm__enumerate(Chord chord, Voicing *result)
{
    unsigned count = 0;
    if (chord.root > 11 || (unsigned) chord.quality >= QUALITY_COUNT) return 0;

    /* Enumerate inversions across the supported bottom-note range, with compact spacing. */
    for (int bottom = 48; bottom < 66; ++bottom) {
        Voicing candidate = { (uint8_t)(36 + chord.root), {0} };
        unsigned found = 0;
        for (int note = bottom; note <= 81 && found < HARMONY_VOICES; ++note) {
            unsigned pitch_class = (unsigned)(note - chord.root) % 12;
            for (unsigned i = 0; i < HARMONY_VOICES; ++i) {
                if (pitch_class == hm__intervals[chord.quality][i] % 12u)
                    candidate.upper[found++] = (uint8_t)note;
            }
            if (note == bottom && found == 0) break;
        }
        if (found == HARMONY_VOICES) result[count++] = candidate;
    }
    return count;
}

static int hm__placement(const Voicing *v)
{
    return mvane_abs((int)v->upper[0] + v->upper[3] - 127);
}

static int hm__motion(const Voicing *a, const Voicing *b)
{
    int cost = 0;
    for (unsigned i = 0; i < HARMONY_VOICES; ++i) {
        int distance = mvane_abs((int)a->upper[i] - b->upper[i]);
        cost += 4 * distance;
        if (distance > 4) cost += 3 * (distance - 4);
    }
    return cost;
}

int harmony_voice_live(Chord chord, const Voicing *previous, Voicing *result)
{
    Voicing candidates[HARMONY_CANDIDATES];
    if (!result) return 0;
    unsigned count = hm__enumerate(chord, candidates);
    int best = MVANE_INT_MAX;
    unsigned best_index = 0;
    for (unsigned i = 0; i < count; ++i) {
        int score = hm__placement(&candidates[i]);
        if (previous) score += hm__motion(previous, &candidates[i]);
        if (score < best) {
            best = score;
            best_index = i;
        }
    }
    if (count) *result = candidates[best_index];
    return count != 0;
}

int harmony_voice_loop(const Chord *chords, unsigned length,
                       HarmonyScratch *s, Voicing *result)
{
    if (!chords || !s || !result || !length || length > HARMONY_MAX_CHORDS) return 0;
    for (unsigned bar = 0; bar < length; ++bar) {
        s->count[bar] = (uint8_t)hm__enumerate(chords[bar], s->candidates[bar]);
        if (!s->count[bar]) return 0;
    }

    int best = MVANE_INT_MAX;
    for (unsigned start = 0; start < s->count[0]; ++start) {
        int previous[HARMONY_CANDIDATES];
        for (unsigned i = 0; i < s->count[0]; ++i)
            previous[i] = i == start ? hm__placement(&s->candidates[0][i]) : MVANE_INT_MAX;
        for (unsigned bar = 1; bar < length; ++bar) {
            int next[HARMONY_CANDIDATES];
            for (unsigned j = 0; j < s->count[bar]; ++j) {
                next[j] = MVANE_INT_MAX;
                for (unsigned i = 0; i < s->count[bar - 1]; ++i) {
                    if (previous[i] == MVANE_INT_MAX) continue;
                    int score = previous[i] + hm__placement(&s->candidates[bar][j])
                        + hm__motion(&s->candidates[bar - 1][i], &s->candidates[bar][j]);
                    if (score < next[j]) {
                        next[j] = score;
                        s->parent[bar][j] = (uint8_t)i;
                    }
                }
            }
            for (unsigned j = 0; j < s->count[bar]; ++j) previous[j] = next[j];
        }
        unsigned last = length - 1;
        for (unsigned end = 0; end < s->count[last]; ++end) {
            if (previous[end] == MVANE_INT_MAX) continue;
            int score = previous[end] + hm__motion(&s->candidates[last][end],
                                               &s->candidates[0][start]);
            if (score >= best) continue;
            best = score;
            unsigned choice = end;
            for (unsigned bar = length; bar-- > 0;) {
                result[bar] = s->candidates[bar][choice];
                if (bar) choice = s->parent[bar][choice];
            }
        }
    }
    return 1;
}

unsigned harmony_phrase(uint8_t tonic, unsigned variant, Chord *result)
{
    if (tonic > 11 || variant > 2 || !result) return 0;
    const uint8_t ii = (uint8_t)((tonic + 2) % 12);
    const uint8_t v = (uint8_t)((tonic + 7) % 12);
    if (variant == 2) {
        result[0] = (Chord){tonic, MAJ9};
        result[1] = (Chord){(uint8_t)((tonic + 9) % 12), DOM7_FLAT9};
        result[2] = (Chord){ii, MIN9};
        result[3] = (Chord){v, DOM13};
        return 4;
    }
    result[0] = (Chord){ii, variant ? HALF_DIM7 : MIN9};
    result[1] = (Chord){v, variant ? DOM7_FLAT9 : DOM13};
    result[2] = (Chord){tonic, variant ? MIN69 : MAJ9};
    return 3;
}

Chord harmony_degree(uint8_t tonic, unsigned degree, int minor, int secondary)
{
    static const uint8_t step[7] = {0, 2, 4, 5, 7, 9, 11};
    static const Quality major[7] = {MAJ9, MIN9, MIN7, MAJ9, DOM13, MIN9, HALF_DIM7};
    unsigned d = (degree + (minor ? 5u : 0u)) % 7u; /* the relative minor starts on the sixth */
    Chord c = {(uint8_t)((tonic % 12u + step[d] + (minor ? 3u : 0u)) % 12u), major[d]};
    if (secondary) {
        c.root = (uint8_t)((c.root + 7u) % 12u);
        c.quality = DOM7_FLAT9;
    }
    return c;
}

const char *harmony_quality_name(Quality q)
{
    static const char *const names[QUALITY_COUNT] = {"m9", "13", "maj9", "m7b5", "7b9", "m69", "m7"};
    return (unsigned)q < QUALITY_COUNT ? names[q] : "";
}
