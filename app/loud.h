/* SPDX-License-Identifier: GPL-3.0-only */
/* LOUD (0.16, Alex: headphones straight from the FM-1 too quiet, its analog stage clean and flat into a line input,
 * weaker into headphones): an optional gentle compressor on the output, after both engines, so the average comes up
 * and the peaks stay where they were. Stereo-linked; a peak follower (1 ms attack, about 400 ms release) sets the
 * gain once a block, glided across it: OFF; +3 DB; +6 DB (below). At the ceiling the output stays at or under it. A last soft knee keeps any overshoot from clipping. It works on the render, so the USB recording
 * has it too (a setting, OFF by default). Shared by the FM-1 (app.c) and its tests (tools/loud_test.c). */
#ifndef MVANE_LOUD_H
#define MVANE_LOUD_H
#include <stdint.h>
#include "fastmath.h"                    /* (fm_powf, fm_expf, fm_tanhf: the FM-1 has no libm) */

#define LOUD_STEPS 3u                    /* OFF, +3 DB, +6 DB */
#define LOUD_CEIL (8388607.0f * 0.708f)  /* the loudest peak as the engines give it (about -3 dBFS) */
static const char *const LOUD_NAMES[LOUD_STEPS] = {"OFF", "+3 DB", "+6 DB"};

typedef struct { float env, g; } loud_t;

/* Each step: its threshold and ratio, its makeup the most that keeps a peak at the ceiling at the ceiling,
 * (ceiling - threshold) x (1 - 1 / ratio). With 11 dB or so between a held chord's peaks and its average (Alex's
 * recordings, 2026-11-24), the average comes up only by what the peaks go down: +3 DB, 2:1 from 12 dB under the
 * ceiling (makeup 4.5 dB: a chord about +3); +6 DB, 4:1 from 15 dB under (makeup 9 dB: about +6), quiet tails
 * and reverbs brought up most */
static const float LOUD_T[LOUD_STEPS] = {0.0f, LOUD_CEIL * 0.2512f, LOUD_CEIL * 0.1778f};   /* -12, -15 dB */
static const float LOUD_INV[LOUD_STEPS] = {1.0f, 0.5f, 0.25f};                             /* 1 / ratio */
static const float LOUD_M[LOUD_STEPS] = {1.0f, 1.6788f, 2.8184f};   /* +4.5, +9 dB (the most minus about 2 dB: a block's glide lets a new peak through) */

static float loud_gain(float env, uint32_t step)
{
    if (env <= LOUD_T[step])
        return LOUD_M[step];
    /* above the threshold: out = T * (env / T)^(1/ratio) * makeup, so the gain is that over env */
    return LOUD_M[step] * LOUD_T[step] * fm_powf(env / LOUD_T[step], LOUD_INV[step]) / env;
}

/* lr: n stereo frames, 24-bit in int32; step 0 leaves them as they are (and resets the follower) */
static void loud_block(loud_t *s, int32_t *lr, uint32_t n, uint32_t step, float sr)
{
    uint32_t i;
    float pk = 0.0f, g0, g1, a, r;
    if (!step || step >= LOUD_STEPS) {
        s->env = 0.0f, s->g = 1.0f;
        return;
    }
    for (i = 0; i < 2u * n; i++) {
        float v = (float)(lr[i] < 0 ? -lr[i] : lr[i]);
        pk = v > pk ? v : pk;
    }
    a = 1.0f - fm_expf(-(float)n / (0.001f * sr));          /* attack ~1 ms (a block at most) */
    r = 1.0f - fm_expf(-(float)n / (0.4f * sr));            /* release ~400 ms (no pumping on an ARP's gaps) */
    s->env += (pk - s->env) * (pk > s->env ? a : r);
    if (s->env < pk && pk > LOUD_T[step])                      /* (a sudden peak: its gain at once, no overshoot) */
        s->env = pk;
    g0 = s->g > 0.0f ? s->g : 1.0f;
    g1 = loud_gain(s->env, step);
    for (i = 0; i < n; i++) {
        float g = g0 + (g1 - g0) * (float)(i + 1u) / (float)n, c;
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            float x = (float)lr[2u * i + k] * g;
            c = x < 0 ? -x : x;
            if (c > LOUD_CEIL)                           /* (a soft knee from the ceiling to full scale) */
                x = (x < 0 ? -1.0f : 1.0f) * (LOUD_CEIL + (8388607.0f - LOUD_CEIL) * fm_tanhf((c - LOUD_CEIL) / (8388607.0f - LOUD_CEIL)));
            lr[2u * i + k] = (int32_t)x;
        }
    }
    s->g = g1;
}
#endif
