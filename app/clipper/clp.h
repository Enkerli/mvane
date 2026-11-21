/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's synthesizer (isod89/clipper-fm1, after SLOOP and Felucca: app/clipper/src, vendored) inside mvane yard:
 * its 23 engines, their factory sounds, its voices, modulation and effects, as one part, fed by mvane yard's bus.
 * Its own unit (fm1.toml o2): Clipper's names stay in it. Main loop: clp_init, clp_select; the render: clp_event,
 * clp_render. */
#pragma once
#include <stdint.h>

void clp_init(void);
uint32_t clp_sounds(void);                    /* the factory sounds, by kind (bass, keys, organ, pad, lead, ...) */
const char *clp_sound_name(uint32_t i);       /* its name, 0 past the last */
const char *clp_sound_engine(uint32_t i);     /* its engine's name */
const char *clp_sound_kind(uint32_t i);       /* BASS KEYS ORGN PAD LEAD PLCK STAB FX */
void clp_select(uint32_t i);                  /* play sound i (main loop: the part switches at its next block) */
void clp_event(uint32_t st, uint32_t d1, uint32_t d2);   /* a MIDI message (render: before clp_render) */
void clp_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* n stereo frames, 24-bit, MASTER as the render has it */
void clp_panic(void);
void clp_mode(uint32_t mono, uint32_t breath);
uint32_t clp_macro(uint32_t i);              /* BRITE TIMBR MOTN SPACE as CC 16..19 set them: 0..127 */
uint32_t clp_fx(uint32_t i);                 /* DRIVE (CC 12) CHORUS (93) DELAY (94) REVERB (91) sends: 0..127 */
const char *clp_reverb_step(int32_t d);      /* the reverb's model, d steps on (0: as it is) -> its name */   /* mvane yard's SEL: mono legato; breath (CC2) the level */
