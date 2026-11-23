/* SPDX-License-Identifier: GPL-3.0-only */
/* VIBE FM port, step 1's check (the Hub's FROM-FM1.md): YARD rendered on the desktop, as the FM-1 renders it, with
 * the events of vibe/cmp.json at the same 128-frame blocks wasmtest gives the .wasm (44.1 kHz), written as raw int32
 * stereo for tools/vibe_cmp.py to compare with the .wasm's audio.wav. The events: one per line on stdin,
 * "BLOCK STATUS D1 D2" (decimal). make vibe-cmp */
#include <stdio.h>
#include <stdlib.h>
#include "../app/synth.c"

int main(int argc, char **argv)
{
    static const uint8_t START_P[SP_N] = {56, 60, 30, 24, 64, 0, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t SP_CC[SP_N] = {SYNTH_CC_TONE, SYNTH_CC_REL, SYNTH_CC_GLIDE, SYNTH_CC_ATTACK, SYNTH_CC_DECAY,
                                        SYNTH_CC_RES, SYNTH_CC_DETUNE, SYNTH_CC_SUB, SYNTH_CC_PLUCK, SYNTH_CC_WAVE,
                                        SYNTH_CC_PW, SYNTH_CC_NOISE, SYNTH_CC_FMODE, SYNTH_CC_ONSET,
                                        SYNTH_CC_ONSET_LVL, SYNTH_CC_LEVEL};
    static int32_t lr[2 * 128];
    unsigned blocks = argc > 2 ? (unsigned)atoi(argv[2]) : 0, b, i, eb, st, d1, d2;
    int have;
    FILE *out = fopen(argv[1], "wb");
    if (!out || !blocks)
        return fprintf(stderr, "usage: vibe_cmp OUT.raw BLOCKS < events\n"), 2;
    synth_rate(44100.0f);
    synth_init(START_P);
    synth_mode(SYNTH_SOUNDS[0].mono, 0);   /* (the .wasm's first sound, loaded as it loads it) */
    for (i = 0; i < SP_N; i++)
        synth_event(0xB0u, SP_CC[i], SYNTH_SOUNDS[0].p[i]);
    have = scanf("%u %u %u %u", &eb, &st, &d1, &d2) == 4;
    for (b = 0; b < blocks; b++) {
        while (have && eb <= b) {
            synth_event(st, d1, d2);
            have = scanf("%u %u %u %u", &eb, &st, &d1, &d2) == 4;
        }
        for (i = 0; i < 128u; i += 32u)  /* (the port renders 32 frames at a time, its recorder's step: YARD's output
                                          * depends a little on the step, -90 dB at most, the FM-1's being 256) */
            synth_render(lr + 2u * i, 32, 4096u);
        fwrite(lr, sizeof lr, 1, out);
    }
    fclose(out);
    return 0;
}
