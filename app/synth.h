/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard's own synth, YARD: 12 voices of a band-limited saw (a second, detuned; a sub square) through a key-tracked
 * lowpass (18 dB/oct, dark to bright, its resonance, PLUCK's contour), the voices' upper half on the second core, with
 * MPE: per-note pitch bend (bend range by RPN 0), slide (CC74 on a member channel), channel and poly pressure; mono
 * legato and breath (CC2 / CC11) for a wind controller.
 * Its own unit at -O2 (fm1.toml o2). The render calls synth_event for each message, then synth_render. */
#pragma once
#include <stdint.h>

/* the master channel's CC74, CC72, CC5, CC73; then (0.10) CC75, CC71, CC20, CC21, CC22: DECAY (PLUCK's fall), RES
 * (the filter's resonance), DETUNE (a second saw), SUB (a square two octaves down), PLUCK (the level falls to a lower
 * sustain and the cutoff from five octaves up, at DECAY) */
/* (0.11) then CC23..26: WAVE (0..42 SAW, 43..84 PULSE, 85..127 TRIANGLE), PW (the pulse's width, 50% down to 5%;
 * pressure narrows it too), NOISE (white noise into the filter; with breath, the breath's), FILTER (0..42 LOWPASS,
 * 43..84 BANDPASS, 85..127 HIGHPASS). Each at 0 leaves a sound as 0.10 made it */
enum { SP_TONE, SP_REL, SP_GLIDE, SP_ATTACK, SP_DECAY, SP_RES, SP_DETUNE, SP_SUB, SP_PLUCK,
       SP_WAVE, SP_PW, SP_NOISE, SP_FMODE, SP_ONSET, SP_ONSET_LVL, SP_N };
#define SP_N_SET 13u                     /* (the settings record keeps the first thirteen in p[]; the rest: their own fields) */
#define SP_N_09 4u                       /* (0.9's settings had the first four) */
#define SP_N_10 9u                       /* (0.10's, the first nine) */
#define SYNTH_WAVE(v) ((v) < 43u ? 0u : (v) < 85u ? 1u : 2u)   /* 0 saw, 1 pulse, 2 triangle */
#define SYNTH_FMODE(v) ((v) < 43u ? 0u : (v) < 85u ? 1u : 2u)  /* 0 lowpass, 1 bandpass, 2 highpass */
/* ONSET (0.15, Alex: Vane's attack transients): a noise burst at each note's start, Vane's ten synthesised ones
 * (Assets/transients/gen_inharmonic.py, its recipes), over the note whatever its envelope; ONSET LVL its level */
#define SYNTH_ONSETS 11u                 /* OFF, then the ten */
#define SYNTH_ONSET(v) ((v) * SYNTH_ONSETS / 128u)
extern const char *const SYNTH_ONSET_NAMES[SYNTH_ONSETS];
#define SYNTH_CC_TONE 74u
#define SYNTH_CC_REL 72u
#define SYNTH_CC_GLIDE 5u
#define SYNTH_CC_ATTACK 73u
#define SYNTH_CC_DECAY 75u
#define SYNTH_CC_RES 71u
#define SYNTH_CC_DETUNE 20u
#define SYNTH_CC_SUB 21u
#define SYNTH_CC_PLUCK 22u
#define SYNTH_CC_WAVE 23u
#define SYNTH_CC_PW 24u
#define SYNTH_CC_NOISE 25u
#define SYNTH_CC_FMODE 26u
#define SYNTH_CC_ONSET 27u
#define SYNTH_CC_ONSET_LVL 28u

/* YARD's own sounds (app.c: first on PRESETS): their parameters; mono, one voice whatever PLAY says */
typedef struct {
    const char *name;
    uint8_t mono;
    uint8_t p[SP_N];
} synth_sound_t;
extern const synth_sound_t SYNTH_SOUNDS[];
extern const unsigned SYNTH_NSOUNDS;

void synth_rate(float sr);              /* another sample rate than the FM-1's 44.1 kHz (VIBE FM), before synth_init */
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
