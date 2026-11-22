/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's synthesizer (isod89/clipper-fm1, after SLOOP and Felucca: app/clipper/src, vendored) inside mvane yard:
 * its 23 engines, their factory sounds, its voices, modulation and effects, as one part, fed by mvane yard's bus.
 * Its own unit (fm1.toml o2): Clipper's names stay in it. Main loop: clp_init, clp_select; the render: clp_event,
 * clp_render. */
#pragma once
#include <stdint.h>

void clp_init(void);
void clp_cores(uint32_t n);                   /* 2: the second core runs (plat_cpu2_set): half the voices there */
uint32_t clp_sounds(void);                    /* the factory sounds, by kind (bass, keys, organ, pad, lead, ...) */
const char *clp_sound_name(uint32_t i);       /* its name, 0 past the last */
const char *clp_sound_engine(uint32_t i);     /* its engine's name */
const char *clp_sound_kind(uint32_t i);
int clp_sound_mono(uint32_t i);              /* 1: a mono sound (legato) */
uint32_t clp_sound_key(uint32_t i);           /* engine << 8 | preset (tools) */
uint32_t clp_sound_cost(uint32_t i);
/* the families (by how each sound behaves: tools/clp_bank.c; its load: tools/clp_cost.c): 0 CHORDS EZ, 1 LEADS EZ,
 * 2 HITS EZ, 3 CHORDS HRD, 4 LEADS HRD, 5 HITS HRD, 6 OTHER; the list runs through them in that order */
uint32_t clp_sound_family(uint32_t i);
const char *clp_family_name(uint32_t f);
uint32_t clp_sound_kind_id(uint32_t i);       /* Clipper's kind: 0 BASS 1 KEYS 2 ORGAN 3 PAD 4 LEAD 5 PLUCK 6 STAB 7 FX 8 none */
void clp_bank_on(int on);                     /* 1 (default): by family; 0: as made, by engine (tools) */
uint32_t clp_sound_old_key(uint32_t i);       /* engine << 8 | preset of index i in the order before 0.10 */
uint32_t clp_sound_of_key(uint32_t key);      /* the index of a sound by its key now (clp_sounds(): none) */          /* 0 LIGHT, 1 MID, 2 HEAVY: its measured CPU (tools/clp_cost.c) */       /* BASS KEYS ORGN PAD LEAD PLCK STAB FX */
void clp_select(uint32_t i);                  /* play sound i (main loop: the part switches at its next block) */
void clp_event(uint32_t st, uint32_t d1, uint32_t d2);   /* a MIDI message (render: before clp_render) */
void clp_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* n stereo frames, 24-bit, MASTER as the render has it */
void clp_panic(void);
uint32_t clp_glide(void);                   /* the sound's GLIDE now, 0..127 (host checks) */
void clp_expr_on(int on);
void clp_trim_on(int on);                    /* 1 (default): each sound's trim (gen/clp_trim.h); 0: as Clipper made it */                    /* 1 (default): the sounds deaf to pressure or slide get a matrix slot for it (tools/clp_expr.c) */
uint32_t clp_voices(void);                  /* voices with their gate open */
void clp_mode(uint32_t mono, uint32_t breath);
uint32_t clp_macro(uint32_t i);              /* BRITE TIMBR MOTN SPACE as CC 16..19 set them: 0..127 */
uint32_t clp_fx(uint32_t i);                 /* DRIVE (CC 12) CHORUS (93) DELAY (94) REVERB (91) sends: 0..127 */
const char *clp_reverb_step(int32_t d);      /* the reverb's model, d steps on (0: as it is) -> its name */   /* mvane yard's SEL: mono legato; breath (CC2) the level */
