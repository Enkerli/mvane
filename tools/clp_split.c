/* SPDX-License-Identifier: GPL-3.0-only */
/* How much of each Clipper sound the second core renders (voice.c's split, with a second core that runs each job at
 * once), as the factory made the sound and with mvane yard's expression slots (clp.c clp_expr_fill): the slots must
 * not take voices back to the first core (a slot on an EDIT value does: the FM-1's load went from about 50% to 78% on
 * HYBRID GLASS PAD). Prints the sounds that split, and fails when a sound splits less with the slots.
 *   make clp-split */
#include <stdio.h>
#include <string.h>
#include "clp.c"
static void (*job)(uint32_t);
static uint32_t job_arg;
int plat_cpu2_on(void) { return 1; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a) { job = f, job_arg = a; return 1; }
int plat_cpu2_wait(void)
{
    if (job)
        job(job_arg), job = 0;
    return 0;
}
static int32_t lr[512];

static uint32_t on_core1(unsigned s, int expr)
{
    unsigned b, c;
    clp_expr_on(expr);
    clp_panic();
    clp_select(s);
    for (b = 0; b < 30; b++)
        clp_render(lr, 256, 4096);
    vs_split = 0;
    for (c = 0; c < 4u; c++) {
        clp_event(0xD1u + c, 60, 0);         /* (some pressure and slide: the slots at work) */
        clp_event(0xB1u + c, 74, 90);
        clp_event(0x91u + c, 48u + c * 5u, 100);
    }
    for (b = 0; b < 40; b++)
        clp_render(lr, 256, 4096);
    for (c = 0; c < 4u; c++)
        clp_event(0x81u + c, 48u + c * 5u, 0);
    for (b = 0; b < 60; b++)
        clp_render(lr, 256, 4096);
    return vs_split;
}

int main(void)
{
    unsigned s, n, splits = 0, worse = 0;
    clp_init();
    clp_cores(2);
    n = clp_sounds();
    for (s = 0; s < n; s++) {
        uint32_t a = on_core1(s, 0), b = on_core1(s, 1);
        splits += a > 0;
        if (b < a) {
            worse++;
            printf("  %-14s %-8s the second core: %u voice-blocks as made, %u with the slots\n", clp_sound_name(s),
                   clp_sound_engine(s), a, b);
        }
    }
    printf("clp_split: %u of %u sounds split between the cores; %u split less with the expression slots\n", splits, n,
           worse);
    return worse ? 1 : 0;
}
