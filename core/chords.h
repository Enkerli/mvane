/* SPDX-License-Identifier: GPL-3.0-only */
/* Chords from any pitch-class set: a scale is a 12-bit mask (bit 0 the tonic), a chord is built on one of its
 * degrees by a type (scale steps above the root: stacked thirds, sus, quartal, ...), then voiced as a separate bass
 * plus four upper voices in a style (close, open, wide), led smoothly from the previous voicing. Portable C99. */
#ifndef MVANE_CHORDS_H
#define MVANE_CHORDS_H

#include <stdint.h>
#include "harmony.h" /* Voicing: bass + HARMONY_VOICES upper notes */

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
    uint8_t pc[HARMONY_VOICES];      /* the upper voices' pitch classes (repeats are octave doublings) */
} chord_pcs;

unsigned chord_scale_size(uint16_t mask);
/* the chord on degree (0 = tonic; past the scale's size wraps) of the scale on tonic, of type t */
chord_pcs chord_build(uint16_t mask, uint8_t tonic, unsigned degree, unsigned type);
/* voice it: style CHORD_*, lift semitones added to the register (an upper key plays higher), prev the voicing to
 * lead from (0: none). Returns 1 with out set */
int chord_voice(chord_pcs c, unsigned style, int lift, const Voicing *prev, Voicing *out);
/* a short name from the notes (Dm9, Gsus, C7, Bm7b5 ...): at most 11 characters */
void chord_name(chord_pcs c, char *out);

#endif
