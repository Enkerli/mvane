/* SPDX-License-Identifier: GPL-3.0-only */
/* LOUD (app/loud.h) measured on Clipper's sounds as the FM-1 plays them: the same ordinary phrase
 * (tools/loud_phrase.h) and a chord held 4 s, at OFF, +3 DB, +6 DB: the loudness, the held chord's level, the peak.
 * The average should come up by about the makeup, the peaks stay at the ceiling (about -3 dBFS), nothing clip.
 *   make loud-test */
#include <stdio.h>
#include <string.h>
#include "clp.c"
#include "loud.h"
#include "loud_phrase.h"
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a) { (void)f, (void)a; return 0; }
int plat_cpu2_wait(void) { return 0; }
static loud_t L;
static uint32_t step, clipped;
static void ev(uint32_t a, uint32_t b, uint32_t c) { clp_event(a, b & 127u, c & 127u); }
static void ren(int32_t *lr, uint32_t n)
{
    uint32_t i;
    clp_render(lr, n, 4096);
    loud_block(&L, lr, n, step, 44100.0f);
    for (i = 0; i < 2u * n; i++)
        clipped += lr[i] >= 8388607 || lr[i] <= -8388607;
}
static double held(unsigned s, double *pk)    /* a chord of five held 4 s: its last 2 s, rms dBFS */
{
    static const uint8_t CH[5] = {48, 55, 59, 64, 67};
    static int32_t lr[512];
    double sum = 0, p = 0;
    unsigned b, c, k, n = 0;
    for (c = 0; c < 5u; c++)
        ev(0x92u + c, CH[c], 100), ev(0xD2u + c, 64, 0);
    for (b = 0; b < 690; b++) {
        ren(lr, 256);
        if (b >= 345)
            for (k = 0; k < 512u; k++) {
                double v = lr[k] / 8388608.0;
                sum += v * v, n++, p = fabs(v) > p ? fabs(v) : p;
            }
    }
    for (c = 0; c < 5u; c++)
        ev(0x82u + c, CH[c], 0);
    for (b = 0; b < 200; b++)
        ren(lr, 256);
    *pk = 20 * log10(p + 1e-12);
    (void)s;
    return 10 * log10(sum / n + 1e-30);
}
int main(void)
{
    static const char *const S[] = {"AFRO KEYS", "SAMPLE CHOIR", "SOUL ORGAN", "SAW ENSEMBLE", "ARP PLUCK", "808 DIRTY", 0};
    static int32_t lr[512];
    unsigned i, s, n, b, bad = 0;
    clp_init();
    clp_trim_on(1);
    n = clp_sounds();
    printf("%-13s %-7s %9s %9s %9s %9s\n", "sound", "LOUD", "phrase", "held rms", "held pk", "pk all");
    for (i = 0; S[i]; i++) {
        double base = 0;
        for (s = 0; s < n && strcmp(clp_sound_name(s), S[i]); s++)
            ;
        for (step = 0; step < LOUD_STEPS; step++) {
            double pk, hp, l, h;
            clp_panic();
            clp_select(s);
            for (b = 0; b < 40; b++)
                clp_render(lr, 256, 4096);
            L.env = 0, L.g = 1, clipped = 0;
            l = lp_play(ev, ren, clp_sound_mono(s), 100, 64, 0, &pk);
            h = held(s, &hp);
            printf("%-13s %-7s %9.1f %9.1f %9.1f %9.1f%s\n", S[i], LOUD_NAMES[step], lp_mmax, h, hp, pk,
                   clipped ? "  CLIPPED" : "");
            if (!step)
                base = clp_sound_mono(s) ? l : h;
            bad += clipped != 0 || pk > -1.0 || (step && h > -60 && (clp_sound_mono(s) ? l : h) - base < 1.5 * step);   /* (a pluck: its peaks are what is compressed; only no clipping) */
        }
    }
    printf(bad ? "loud_test: FAILED\n" : "loud_test: the average up by about the makeup, nothing clipped\n");
    return bad != 0;
}
