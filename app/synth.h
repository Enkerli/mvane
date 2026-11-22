/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard's own synth, YARD: 12 voices of a band-limited saw (a second, detuned; a sub square) through a key-tracked
 * lowpass (18 dB/oct, dark to bright, its resonance, PLUCK's contour), the voices' upper half on the second core, with
 * MPE: per-note pitch bend (bend range by RPN 0), slide (CC74 on a member channel), channel and poly pressure; mono
 * legato and breath (CC2 / CC11) for a wind controller.
 * Its own unit at -O2 (fm1.toml o2). The render calls synth_event for each message, then synth_render. */
#pragma once
#include <stdint.h>

/* the master channel's CC74, CC72, CC5, CC73; then (0.10) CC75, CC71, CC20, CC21, CC22: DECAY (PLUCK's fall), RES
 * (the filter's resonance), DETUNE (a second saw), SUB (a square an octave down), PLUCK (the level falls to a lower
 * sustain and the cutoff from five octaves up, at DECAY) */
enum { SP_TONE, SP_REL, SP_GLIDE, SP_ATTACK, SP_DECAY, SP_RES, SP_DETUNE, SP_SUB, SP_PLUCK, SP_N };
#define SP_N_09 4u                       /* (0.9's settings had the first four) */
#define SYNTH_CC_TONE 74u
#define SYNTH_CC_REL 72u
#define SYNTH_CC_GLIDE 5u
#define SYNTH_CC_ATTACK 73u
#define SYNTH_CC_DECAY 75u
#define SYNTH_CC_RES 71u
#define SYNTH_CC_DETUNE 20u
#define SYNTH_CC_SUB 21u
#define SYNTH_CC_PLUCK 22u

/* YARD's own sounds (app.c: first on PRESETS): their parameters; mono, one voice whatever PLAY says */
typedef struct {
    const char *name;
    uint8_t mono;
    uint8_t p[SP_N];
} synth_sound_t;
extern const synth_sound_t SYNTH_SOUNDS[];
extern const unsigned SYNTH_NSOUNDS;

void synth_init(const uint8_t p[SP_N]);
void synth_event(uint32_t st, uint32_t d1, uint32_t d2);
/* n stereo frames into lr (24-bit), at gain (MASTER as the render gets it, 0..4096) */
#define SYNTH_BLOCK 256u                 /* frames synth_render renders at most (app.c checks APP_BLOCK against it) */
void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void synth_panic(void);
/* mono 1: one voice, legato (a note over a held one slides, the envelope goes on); breath 1: CC2 or CC11 (any
 * channel) is the level and opens the filter, notes silent without it */
void synth_mode(uint32_t mono, uint32_t breath);
uint32_t synth_param(uint32_t which);   /* SP_*: the value the sound has now (a take may move it) */
void synth_set(uint32_t which, uint32_t v);
uint32_t synth_voices(void);
/* the second core: plat_cpu2_run / plat_cpu2_wait (the upper half of the voices there, while it sounds); 0: one core */
void synth_cores(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void));
uint32_t synth_split_blocks(void);     /* blocks the second core took part in (tests) */            /* voices with their gate open */
