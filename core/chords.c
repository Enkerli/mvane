/* SPDX-License-Identifier: GPL-3.0-only */
#include "chords.h"

#include "port.h"

const chord_scale CHORD_SCALES[] = {
    {"MAJOR", 0xAB5u},     /* 0 2 4 5 7 9 11 */
    {"MINOR", 0x5ADu},     /* 0 2 3 5 7 8 10 */
    {"DORIAN", 0x6ADu},    /* 0 2 3 5 7 9 10 */
    {"HARM MIN", 0x9ADu},  /* 0 2 3 5 7 8 11 */
    {"PENTA", 0x295u},     /* 0 2 4 7 9 */
    {"PCO", 0x6BBu},       /* Alex's pentatonic-compatible octatonic: 0 1 3 4 5 7 9 10 */
    {"NOODLING", 0x4F3u},  /* Alex's: 0 1 4 5 6 7 10 */
    {"OCTATONIC", 0xB6Du}, /* whole-half: 0 2 3 5 6 8 9 11 */
};
const unsigned CHORD_NSCALES = sizeof CHORD_SCALES / sizeof CHORD_SCALES[0];

#define R 0xFEu   /* the root, as an upper voice */
#define CHROMATIC_DOM 9u
static const uint8_t TYPE_STEPS[][HARMONY_VOICES] = {
    {2, 4, 6, 8},  /* 9: stacked thirds of the scale, to the ninth */
    {2, 4, 6, R},  /* 7: to the seventh, the root on top */
    {2, 4, R, 2},  /* triad: third, fifth, root, third */
    {2, 4, 1, R},  /* add9 */
    {3, 4, 1, R},  /* sus: fourth, fifth, second */
    {2, 4, 5, 1},  /* 6/9 */
    {3, 6, 9, 12}, /* quartal: stacked fourths of the scale */
    {4, 6, 8, 10}, /* 11: fifth, seventh, ninth, eleventh, no third */
    {4, R, 8, 4},  /* fifths: open, with the ninth */
    {0, 0, 0, 0},  /* V/: the dominant (7b9) of the degree, chromatic */
};
const char *const CHORD_TYPE_NAMES[] = {"9", "7", "TRIAD", "ADD9", "SUS", "6/9", "QUARTAL", "11", "FIFTHS", "V/"};
const unsigned CHORD_NTYPES = sizeof TYPE_STEPS / sizeof TYPE_STEPS[0];
const char *const CHORD_STYLE_NAMES[CHORD_STYLES] = {"CLOSE", "OPEN", "WIDE"};

unsigned chord_scale_size(uint16_t mask)
{
    unsigned n = 0, k;
    for (k = 0; k < 12u; k++)
        n += mask >> k & 1u;
    return n;
}

chord_pcs chord_build(uint16_t mask, uint8_t tonic, unsigned degree, unsigned type)
{
    uint8_t scale[12];
    unsigned n = 0, k, j;
    chord_pcs c;
    mask |= 1u; /* the tonic is always in */
    for (k = 0; k < 12u; k++)
        if (mask >> k & 1u)
            scale[n++] = (uint8_t)((tonic + k) % 12u);
    degree %= n;
    type = type < CHORD_NTYPES ? type : 0;
    c.root = scale[degree];
    if (type == CHROMATIC_DOM) {
        static const uint8_t I[HARMONY_VOICES] = {4, 7, 10, 13};
        c.root = (uint8_t)((c.root + 7u) % 12u);
        for (j = 0; j < HARMONY_VOICES; j++)
            c.pc[j] = (uint8_t)((c.root + I[j]) % 12u);
        return c;
    }
    for (j = 0; j < HARMONY_VOICES; j++) {
        uint8_t s = TYPE_STEPS[type][j];
        c.pc[j] = s == R ? c.root : scale[(degree + s) % n];
    }
    return c;
}

/* ------------------------------------------------------------ voicing --- */
typedef struct {
    int lo, hi, span_min, span_max, gap_min, center;
} chord_style_t;
static const chord_style_t STYLE[CHORD_STYLES] = {
    {52, 79, 0, 14, 1, 64},  /* close: within about an octave */
    {48, 84, 12, 26, 3, 65}, /* open: drop-2-like spreads */
    {45, 96, 20, 42, 5, 67}, /* wide: broad, every gap a fourth or more */
};

static int ch__motion(const Voicing *a, const Voicing *b)
{
    int cost = 0;
    unsigned i;
    for (i = 0; i < HARMONY_VOICES; i++) {
        int d = mvane_abs((int)a->upper[i] - (int)b->upper[i]);
        cost += 4 * d + (d > 4 ? 3 * (d - 4) : 0);
    }
    return cost;
}

int chord_voice(chord_pcs c, unsigned style, int lift, const Voicing *prev, Voicing *out)
{
    const chord_style_t *s = &STYLE[style < CHORD_STYLES ? style : 0];
    int lo = s->lo + lift, hi = s->hi + lift, centre = s->center + lift, pass, best = MVANE_INT_MAX;
    int cand[HARMONY_VOICES][8], nc[HARMONY_VOICES], idx[HARMONY_VOICES];
    unsigned j;
    Voicing v, found;
    if (lo < 24)
        lo = 24, hi = lo + (s->hi - s->lo);
    if (hi > 108)
        hi = 108;
    for (j = 0; j < HARMONY_VOICES; j++) { /* each voice's choices: its pitch class in every octave of the range */
        int n;
        nc[j] = 0;
        for (n = lo; n <= hi && nc[j] < 8; n++)
            if (n % 12 == (int)(c.pc[j] % 12u))
                cand[j][nc[j]++] = n;
        if (!nc[j])
            return 0;
    }
    for (pass = 0; pass < 2 && best == MVANE_INT_MAX; pass++) { /* the style's limits, then none if nothing fits */
        for (idx[0] = 0; idx[0] < nc[0]; idx[0]++)
            for (idx[1] = 0; idx[1] < nc[1]; idx[1]++)
                for (idx[2] = 0; idx[2] < nc[2]; idx[2]++)
                    for (idx[3] = 0; idx[3] < nc[3]; idx[3]++) {
                        int note[HARMONY_VOICES], a, b, ok = 1, span, cost;
                        for (a = 0; a < (int)HARMONY_VOICES; a++)
                            note[a] = cand[a][idx[a]];
                        for (a = 1; a < (int)HARMONY_VOICES; a++) /* in order, lowest first */
                            for (b = a; b > 0 && note[b] < note[b - 1]; b--) {
                                int t = note[b];
                                note[b] = note[b - 1], note[b - 1] = t;
                            }
                        for (a = 1; a < (int)HARMONY_VOICES; a++) {
                            int gap = note[a] - note[a - 1];
                            if (gap == 0 || (!pass && gap < s->gap_min))
                                ok = 0;
                        }
                        span = note[HARMONY_VOICES - 1] - note[0];
                        if (!ok || (!pass && (span < s->span_min || span > s->span_max)))
                            continue;
                        for (a = 0; a < (int)HARMONY_VOICES; a++)
                            v.upper[a] = (uint8_t)note[a];
                        cost = 2 * mvane_abs((note[0] + note[HARMONY_VOICES - 1]) / 2 - centre);
                        if (prev)
                            cost += ch__motion(prev, &v);
                        if (cost < best)
                            best = cost, found = v;
                    }
    }
    if (best == MVANE_INT_MAX)
        return 0;
    { /* the bass: the root between A1 and G#2, the octave nearer the previous bass */
        int b = 33 + (int)((c.root + 12u - 9u) % 12u);
        if (prev && mvane_abs(b + 12 - (int)prev->bass) < mvane_abs(b - (int)prev->bass) && b + 12 <= 47
            && b + 12 < (int)found.upper[0])
            b += 12;
        while (b >= (int)found.upper[0] && b >= 36) /* always under the chord */
            b -= 12;
        found.bass = (uint8_t)b;
    }
    *out = found;
    return 1;
}

/* --------------------------------------------------------------- names --- */
static char *ch__cat(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}

void chord_name(chord_pcs c, char *out)
{
    static const char *const N[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    unsigned m = 0, j;
    int min3, maj3, b5, p5, sixth, b7, maj7, ninth, fourth;
    char *p = out;
    for (j = 0; j < HARMONY_VOICES; j++)
        m |= 1u << ((c.pc[j] + 12u - c.root) % 12u);
    min3 = m >> 3 & 1u, maj3 = m >> 4 & 1u, b5 = m >> 6 & 1u, p5 = m >> 7 & 1u, sixth = m >> 9 & 1u;
    b7 = m >> 10 & 1u, maj7 = m >> 11 & 1u, ninth = m >> 2 & 1u, fourth = m >> 5 & 1u;
    p = ch__cat(p, N[c.root % 12u]);
    if (min3 && !maj3 && b5 && !p5)
        p = ch__cat(p, b7 ? "m7b5" : sixth ? "dim7" : "dim");
    else {
        int sus = !min3 && !maj3;
        if (min3 && !maj3)
            p = ch__cat(p, "m");
        if (maj7)
            p = ch__cat(p, ninth ? "maj9" : "maj7");
        else if (b7)
            p = ch__cat(p, ninth ? "9" : "7");
        else if (sixth && ninth && !sus)
            p = ch__cat(p, "6/9");
        else if (sixth && !sus)
            p = ch__cat(p, "6");
        else if (ninth && !sus)
            p = ch__cat(p, "add9");
        if (sus)
            p = ch__cat(p, fourth ? "sus" : ninth ? "sus2" : p5 ? "5" : "");
        if (maj3 && min3)
            p = ch__cat(p, "#9");
    }
    (void)p;
}
