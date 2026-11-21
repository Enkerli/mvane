/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FM1_HARMONY_H
#define FM1_HARMONY_H

#include <stdint.h>

#define HARMONY_MAX_CHORDS 8
#define HARMONY_CANDIDATES 18
#define HARMONY_VOICES 4

typedef enum { MIN9, DOM13, MAJ9, HALF_DIM7, DOM7_FLAT9, MIN69, MIN7, QUALITY_COUNT } Quality;
typedef struct { uint8_t root; Quality quality; } Chord;
typedef struct { uint8_t bass, upper[HARMONY_VOICES]; } Voicing;
typedef struct {
    Voicing candidates[HARMONY_MAX_CHORDS][HARMONY_CANDIDATES];
    uint8_t count[HARMONY_MAX_CHORDS];
    uint8_t parent[HARMONY_MAX_CHORDS][HARMONY_CANDIDATES];
} HarmonyScratch;

/* Caller owns scratch. Run on the control side, outside audio rendering. */
int harmony_voice_loop(const Chord *chords, unsigned length,
                       HarmonyScratch *scratch, Voicing *result);
int harmony_voice_live(Chord chord, const Voicing *previous, Voicing *result);
/* Variant 0 is major ii–V–I; 1 is minor; 2 is a major turnaround. */
unsigned harmony_phrase(uint8_t tonic, unsigned variant, Chord *result);
/* The diatonic chord on scale degree 0..6 of a major key (minor 0: I maj9, ii m9, iii m7, IV maj9, V 13, vi m9,
 * vii m7b5) or of its relative natural minor (minor 1: i m9, ii m7b5, III maj9, iv m9, v m7, VI maj9, VII 13).
 * secondary 1: the dominant of that chord instead (V7b9 of it). */
Chord harmony_degree(uint8_t tonic, unsigned degree, int minor, int secondary);
const char *harmony_quality_name(Quality q);

#endif
