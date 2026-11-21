/* SPDX-License-Identifier: GPL-3.0-only */
/* Chords from any pitch-class set: a scale is a 12-bit mask (bit 0 the tonic), a chord is built on one of its
 * degrees by a type (scale steps above the root, in order of importance: stacked thirds, sus, quartal, ...), takes
 * its first 3 to 6 of them as upper voices, and is voiced over a separate bass in a style (close, open, wide), led
 * smoothly from the previous chord. Portable C99. */
#ifndef MVANE_CHORDS_H
#define MVANE_CHORDS_H

#include <stdint.h>

#define CHORD_MAX 6u                 /* upper voices at most */

typedef struct {
    const char *name;
    uint16_t mask; /* bit k: the tonic + k semitones is in the scale */
} chord_scale;

enum { CHORD_CLOSE, CHORD_OPEN, CHORD_WIDE, CHORD_STYLES };

extern const chord_scale CHORD_SCALES[];
extern const unsigned CHORD_NSCALES;
extern const char *const CHORD_TYPE_NAMES[];
extern const unsigned CHORD_NTYPES;
extern const char *const CHORD_STYLE_NAMES[CHORD_STYLES];

typedef struct {
    uint8_t root;                    /* pitch class 0..11 */
    uint8_t n;                       /* upper voices, 3..CHORD_MAX */
    uint8_t pc[CHORD_MAX];           /* their pitch classes (repeats are octave doublings) */
} chord_pcs;

typedef struct {
    uint8_t bass, n;
    uint8_t upper[CHORD_MAX];        /* lowest first */
} chord_voicing;

unsigned chord_scale_size(uint16_t mask);
/* the chord on degree (0 = tonic; past the scale's size wraps) of the scale on tonic, of type t, with n upper voices */
chord_pcs chord_build(uint16_t mask, uint8_t tonic, unsigned degree, unsigned type, unsigned n);
/* voice it: style CHORD_*, lift semitones added to the register (an upper key plays higher), prev the voicing to
 * lead from (0: none). Returns 1 with out set */
int chord_voice(chord_pcs c, unsigned style, int lift, const chord_voicing *prev, chord_voicing *out);
/* a short name from the notes (Dm9, Gsus, C13, Bm7b5 ...): at most 11 characters */
void chord_name(chord_pcs c, char *out);

#endif
