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
/* each type's upper voices in order of importance: the first n are used */
static const uint8_t TYPE_STEPS[][CHORD_MAX] = {
    {2, 4, 6, 8, 12, 10},  /* STACK: thirds of the scale, 3 5 7 9, then 13 and 11 */
    {2, 4, 6, R, 2, 4},    /* 7: to the seventh, the root on top, doubled */
    {2, 4, R, 2, 4, R},    /* TRIAD: third, fifth, root, doubled */
    {2, 4, 1, R, 4, 2},    /* ADD9 */
    {3, 4, 1, R, 3, 4},    /* SUS: fourth, fifth, second */
    {2, 4, 5, 1, R, 4},    /* 6/9 */
    {3, 6, 9, 12, 15, 18}, /* QUARTAL: stacked fourths of the scale */
    {4, 6, 8, 10, 12, R},  /* 11: fifth, seventh, ninth, eleventh, thirteenth, no third */
    {4, R, 8, 4, R, 8},    /* FIFTHS: open, with the ninth */
    {0, 0, 0, 0, 0, 0},    /* V/: the dominant of the degree, chromatic (3 b7 b9 13 5 #9) */
};
/* the second bank: fixed qualities on the degree's root, whatever the scale says (as Coral Flower's top row, in its
 * order), as semitones above the root, in order of importance */
static const uint8_t QUALITY[][CHORD_MAX] = {
    {4, 7, 9, 0, 4, 7},   /* M6 */
    {4, 7, 11, 0, 4, 7},  /* M7 */
    {4, 7, 11, 2, 0, 7},  /* M9 */
    {2, 7, 0, 2, 7, 0},   /* S2 */
    {5, 7, 0, 5, 7, 0},   /* S4 */
    {3, 7, 9, 0, 3, 7},   /* m6 */
    {3, 7, 10, 0, 3, 7},  /* m7 */
    {3, 7, 10, 2, 0, 7},  /* m9 */
    {4, 7, 0, 4, 7, 0},   /* M */
    {3, 7, 0, 3, 7, 0},   /* m */
};
#define NSCALE_TYPES (sizeof TYPE_STEPS / sizeof TYPE_STEPS[0])
const char *const CHORD_TYPE_NAMES[] = {"STACK", "7", "TRIAD", "ADD9", "SUS", "6/9", "QUARTAL", "11", "FIFTHS", "V/",
                                        "M6", "M7", "M9", "S2", "S4", "m6", "m7", "m9", "M", "m"};
const unsigned CHORD_NTYPES = NSCALE_TYPES + sizeof QUALITY / sizeof QUALITY[0];
const char *const CHORD_STYLE_NAMES[CHORD_STYLES] = {"CLOSE", "OPEN", "WIDE"};

unsigned chord_scale_size(uint16_t mask)
{
    unsigned n = 0, k;
    for (k = 0; k < 12u; k++)
        n += mask >> k & 1u;
    return n;
}

chord_pcs chord_build(uint16_t mask, uint8_t tonic, unsigned degree, unsigned type, unsigned n)
{
    uint8_t scale[12];
    unsigned ns = 0, k, j;
    chord_pcs c;
    mask |= 1u; /* the tonic is always in */
    for (k = 0; k < 12u; k++)
        if (mask >> k & 1u)
            scale[ns++] = (uint8_t)((tonic + k) % 12u);
    degree %= ns;
    type = type < CHORD_NTYPES ? type : 0;
    c.n = (uint8_t)(n < 3u ? 3u : n > CHORD_MAX ? CHORD_MAX : n);
    c.root = scale[degree];
    if (type >= NSCALE_TYPES) {
        for (j = 0; j < CHORD_MAX; j++)
            c.pc[j] = (uint8_t)((c.root + QUALITY[type - NSCALE_TYPES][j]) % 12u);
        return c;
    }
    if (type == CHROMATIC_DOM) {
        static const uint8_t I[CHORD_MAX] = {4, 10, 13, 21, 7, 15};
        c.root = (uint8_t)((c.root + 7u) % 12u);
        for (j = 0; j < CHORD_MAX; j++)
            c.pc[j] = (uint8_t)((c.root + I[j]) % 12u);
        return c;
    }
    for (j = 0; j < CHORD_MAX; j++) {
        uint8_t s = TYPE_STEPS[type][j];
        c.pc[j] = s == R ? c.root : scale[(degree + s) % ns];
    }
    return c;
}

/* ------------------------------------------------------------ voicing --- */
typedef struct {
    int lo, hi, span_min, span_max, gap_min, center;
} chord_style_t;
/* spans for four voices; more voices widen them (a sixth voice needs room) */
static const chord_style_t STYLE[CHORD_STYLES] = {
    {52, 81, 0, 14, 1, 64},  /* close: within about an octave */
    {48, 86, 12, 26, 3, 65}, /* open: drop-2-like spreads */
    {45, 98, 20, 42, 5, 67}, /* wide: broad, every gap a fourth or more */
};

typedef struct {
    const chord_style_t *s;
    int lo, hi, centre, span_extra, pass;
    int cand[CHORD_MAX][8], nc[CHORD_MAX], pick[CHORD_MAX], pcs[CHORD_MAX];
    unsigned n;
    const chord_voicing *prev;
    int best;
    chord_voicing found;
} ch__search;

static int ch__motion(const chord_voicing *prev, const int *note, unsigned n)
{   /* each new note's distance to the nearest note of the previous chord */
    int cost = 0;
    unsigned i, j;
    for (i = 0; i < n; i++) {
        int d = 1000;
        for (j = 0; j < prev->n; j++) {
            int e = mvane_abs(note[i] - (int)prev->upper[j]);
            if (e < d)
                d = e;
        }
        cost += 4 * d + (d > 4 ? 3 * (d - 4) : 0);
    }
    return cost;
}

static void ch__try(ch__search *q)
{
    int note[CHORD_MAX], a, b, span, cost;
    unsigned n = q->n;
    for (a = 0; a < (int)n; a++)
        note[a] = q->pick[a];
    for (a = 1; a < (int)n; a++) /* in order, lowest first */
        for (b = a; b > 0 && note[b] < note[b - 1]; b--) {
            int t = note[b];
            note[b] = note[b - 1], note[b - 1] = t;
        }
    for (a = 1; a < (int)n; a++) {
        int gap = note[a] - note[a - 1];
        if (gap == 0 || (!q->pass && gap < q->s->gap_min))
            return;
    }
    span = note[n - 1] - note[0];
    if (!q->pass && (span < q->s->span_min || span > q->s->span_max + q->span_extra || note[0] < q->lo
                     || note[n - 1] > q->hi))
        return;
    cost = 2 * mvane_abs((note[0] + note[n - 1]) / 2 - q->centre);
    if (q->prev)
        cost += ch__motion(q->prev, note, n);
    if (cost < q->best) {
        q->best = cost;
        q->found.n = (uint8_t)n;
        for (a = 0; a < (int)n; a++)
            q->found.upper[a] = (uint8_t)note[a];
    }
}

static int ch__near(const chord_voicing *prev, int note)
{   /* one note's cost: its distance to the nearest note of the previous chord */
    int d = 1000;
    unsigned j;
    for (j = 0; j < prev->n; j++) {
        int e = mvane_abs(note - (int)prev->upper[j]);
        if (e < d)
            d = e;
    }
    return 4 * d + (d > 4 ? 3 * (d - 4) : 0);
}

/* voice j's note, given the others: the search prunes a branch that already breaks the style's span (first pass),
 * repeats an ordering of the same pitch class, or costs more than the best so far */
static void ch__walk(ch__search *q, unsigned j, int lo, int hi, int cost)
{
    int i;
    if (j == q->n) {
        ch__try(q);
        return;
    }
    for (i = 0; i < q->nc[j]; i++) {
        unsigned k;
        int note = q->cand[j][i], used = 0, l = note < lo ? note : lo, h = note > hi ? note : hi, c = cost;
        for (k = 0; k < j; k++)
            used |= q->pick[k] == note || (q->pcs[k] == q->pcs[j] && q->pick[k] > note);
        if (used)
            continue;
        if (!q->pass && (note < q->lo || note > q->hi || h - l > q->s->span_max + q->span_extra))
            continue;
        if (q->prev)
            c += ch__near(q->prev, note);
        if (c >= q->best)
            continue;
        q->pick[j] = note;
        ch__walk(q, j + 1, l, h, c);
    }
}

int chord_voice(chord_pcs c, unsigned style, int lift, const chord_voicing *prev, chord_voicing *out)
{
    ch__search q;
    unsigned j;
    q.s = &STYLE[style < CHORD_STYLES ? style : 0];
    q.n = c.n < 3u ? 3u : c.n > CHORD_MAX ? CHORD_MAX : c.n;
    q.span_extra = ((int)q.n - 4) * (style == CHORD_WIDE ? 8 : style == CHORD_OPEN ? 6 : 4);
    q.lo = q.s->lo + lift, q.hi = q.s->hi + lift + (q.span_extra > 0 ? q.span_extra / 2 : 0);
    q.centre = q.s->center + lift;
    q.prev = prev && prev->n ? prev : 0;
    q.best = MVANE_INT_MAX;
    if (q.lo < 24)
        q.lo = 24;
    if (q.hi > 108)
        q.hi = 108;
    for (j = 0; j < q.n; j++) { /* each voice's choices: its pitch class in every octave of the range, and one
                                 * octave beyond each side for the second pass (a pitch class asked for thrice) */
        int n;
        q.nc[j] = 0;
        q.pcs[j] = c.pc[j] % 12u;
        for (n = q.lo - 12 < 24 ? 24 : q.lo - 12; n <= (q.hi + 12 > 108 ? 108 : q.hi + 12) && q.nc[j] < 8; n++)
            if (n % 12 == (int)(c.pc[j] % 12u))
                q.cand[j][q.nc[j]++] = n;
        if (!q.nc[j])
            return 0;
    }
    for (q.pass = 0; q.pass < 2 && q.best == MVANE_INT_MAX; q.pass++) /* the style's limits, then none */
        ch__walk(&q, 0, 1000, -1000, 0);
    if (q.best == MVANE_INT_MAX)
        return 0;
    { /* the bass: the root between A1 and G#2, the octave nearer the previous bass, always under the chord */
        int b = 33 + (int)((c.root + 12u - 9u) % 12u);
        if (q.prev && mvane_abs(b + 12 - (int)q.prev->bass) < mvane_abs(b - (int)q.prev->bass) && b + 12 <= 47
            && b + 12 < (int)q.found.upper[0])
            b += 12;
        while (b >= (int)q.found.upper[0] && b >= 36)
            b -= 12;
        q.found.bass = (uint8_t)b;
    }
    *out = q.found;
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
    for (j = 0; j < c.n && j < CHORD_MAX; j++)
        m |= 1u << ((c.pc[j] + 12u - c.root) % 12u);
    min3 = m >> 3 & 1u, maj3 = m >> 4 & 1u, b5 = m >> 6 & 1u, p5 = m >> 7 & 1u, sixth = m >> 9 & 1u;
    b7 = m >> 10 & 1u, maj7 = m >> 11 & 1u, ninth = m >> 2 & 1u, fourth = m >> 5 & 1u;
    p = ch__cat(p, N[c.root % 12u]);
    if (min3 && !maj3 && b5 && !p5)
        p = ch__cat(p, b7 ? "m7b5" : sixth ? "dim7" : "dim");
    else {
        int sus = !min3 && !maj3;
        const char *ext = sixth ? "13" : fourth && !sus ? "11" : ninth ? "9" : "7";
        if (min3 && !maj3)
            p = ch__cat(p, "m");
        if (maj7) {
            p = ch__cat(p, "maj");
            p = ch__cat(p, ext);
        } else if (b7)
            p = ch__cat(p, ext);
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
