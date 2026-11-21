/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane's synth: 8 voices of a band-limited saw through a key-tracked lowpass (18 dB/oct, dark to bright), with
 * MPE: per-note pitch bend (bend range by RPN 0), slide (CC74 on a member channel), channel and poly pressure.
 * Its own unit at -O2 (fm1.toml o2). The render calls synth_event for each message, then synth_render. */
#pragma once
#include <stdint.h>

enum { SP_TONE, SP_REL, SP_GLIDE, SP_ATTACK, SP_N };   /* the master channel's CC74, CC72, CC5, CC73 */
#define SYNTH_CC_TONE 74u
#define SYNTH_CC_REL 72u
#define SYNTH_CC_GLIDE 5u
#define SYNTH_CC_ATTACK 73u

void synth_init(const uint8_t p[SP_N]);
void synth_event(uint32_t st, uint32_t d1, uint32_t d2);
/* n stereo frames into lr (24-bit), at gain (MASTER as the render gets it, 0..4096) */
void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void synth_panic(void);
uint32_t synth_param(uint32_t which);   /* SP_*: the value the sound has now (a take may move it) */
void synth_set(uint32_t which, uint32_t v);
uint32_t synth_voices(void);            /* voices with their gate open */
