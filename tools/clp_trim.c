/* SPDX-License-Identifier: GPL-3.0-only */
/* Each Clipper sound's trim (clp.c, fx.c CLP_TRIM), by loudness (0.16; Alex: "patches vary widely in overall loudness,
 * some almost inaudible, with the same MIDI"): before, the trim only kept the hardest playing under the limiter, so a
 * pluck's peaks set its level and ordinary playing came out up to 17 dB under the median sound.
 * Now: the ordinary phrase (tools/loud_phrase.h: four-note chords at velocity 100, moderate pressure; a mono sound, a
 * line) is measured at its loudest moment (400 ms, K-weighted), and the trim brings it to TARGET_DB (-26), up to +12 dB,
 * as far as the hardest playing (reduction(): velocity 127, pressure 100, chords of six) leaves the master limiter
 * working at most LIM_MAX_DB (6), the grit Alex heard at 11 dB (2026-11-22) far off. Writes app/clipper/gen/clp_trim.h
 * (Q15, above 32768 a boost), then plays every sound again with its trim: the spread and the limiting left.
 *   make clp-trim */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clp.c"
#include "loud_phrase.h"
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a) { (void)f, (void)a; return 0; }
int plat_cpu2_wait(void) { return 0; }
static int32_t lr[512];
static double TARGET_DB = -26.0;   /* (TARGET=, LIM= in the environment: to try others) */
static double LIM_MAX_DB = 6.0;
#define BOOST_MAX_DB 12.0
#define HARD_PRS 100u        /* (Alex's recordings to 2026-11-22: pressure's median 31, its highest 112) */

static void settle(unsigned blocks)
{
    unsigned b;
    for (b = 0; b < blocks; b++)
        clp_render(lr, 256, 4096);
}
static void ev(uint32_t a, uint32_t b, uint32_t c) { clp_event(a, b & 127u, c & 127u); }
static int32_t g_ord = 32768;                /* the limiter's lowest gain in the ordinary phrase */
static void ren(int32_t *o, uint32_t n)
{
    clp_render(o, n, 4096);
    g_ord = lim_dac.g < g_ord ? lim_dac.g : g_ord;
}
static void load(unsigned s, int32_t q)        /* the sound, at this trim (q < 0: its table's) */
{
    clp_panic();
    clp_select(s);
    settle(40);
    if (q >= 0)
        clp_trim = q;
}
static double loud(unsigned s, int32_t q)      /* the ordinary phrase's loudest moment, dB */
{
    double pk;
    load(s, q);
    g_ord = 32768;
    lp_play(ev, ren, clp_sound_mono(s), 100, 64, 0, &pk);
    settle(80);
    return lp_mmax;
}

/* dB the limiter took off, at its deepest, played hard: velocity 127 and pressure on every note, chords of six
 * struck every quarter second over a held bass, eight of them; a mono sound, eight notes. Pressure HARD_PRS, not 127:
 * Alex's playing never reaches it, and 127 made some sounds 25 dB louder than ordinary playing */
static double reduction(unsigned s, int32_t q)
{
    static const uint8_t CH[4][5] = {{52, 55, 59, 64, 67}, {57, 60, 64, 67, 72}, {53, 57, 60, 62, 65}, {55, 59, 62, 65, 67}};
    unsigned b, c, k, r, mono = (unsigned)clp_sound_mono(s);
    int32_t gmin = 32768;
    load(s, q);
    lim_dac.g = 32768, lim_dac.env = LIM_T, lim_dac.hold = 0;
    for (c = 0; c < 6u; c++)
        clp_event(0xD1u + c, HARD_PRS, 0);
    if (mono)
        for (r = 0; r < 8u; r++) {
            clp_event(0x91, 60u + (r % 4u) * 3u, 127);
            for (b = 0; b < 40; b++) {
                clp_render(lr, 256, 4096);
                gmin = lim_dac.g < gmin ? lim_dac.g : gmin;
            }
            clp_event(0x81, 60u + (r % 4u) * 3u, 0);
        }
    else {
        clp_event(0x91, 36, 127);
        for (r = 0; r < 8u; r++) {
            k = r % 4u;
            for (c = 0; c < 5u; c++)
                clp_event(0x92u + c, CH[k][c], 127);
            for (b = 0; b < 43; b++) {
                clp_render(lr, 256, 4096);
                gmin = lim_dac.g < gmin ? lim_dac.g : gmin;
            }
            for (c = 0; c < 5u; c++)
                clp_event(0x82u + c, CH[k][c], 0);
        }
        clp_event(0x81, 36, 0);
    }
    for (c = 0; c < 6u; c++)
        clp_event(0xD1u + c, 0, 0);
    settle(150);
    return -20.0 * log10((double)gmin / 32768.0);
}
static int32_t q_of(double db) { return (int32_t)floor(32768.0 * pow(10.0, db / 20.0) + 0.5); }

int main(int argc, char **argv)
{
    static uint32_t q[64][40];
    static double before[512], lim0[512];
    unsigned s, n, e, p, it, boosted = 0, cut = 0, capped = 0, left3 = 0;
    double worst = 0;
    FILE *f;
    static double got[512], ord[512];
    if (getenv("TARGET"))
        TARGET_DB = atof(getenv("TARGET"));
    if (getenv("LIM"))
        LIM_MAX_DB = atof(getenv("LIM"));
    clp_init();
    clp_trim_on(0);
    n = clp_sounds();
    if (argc > 1 && !strcmp(argv[1], "survey")) {   /* each sound at no trim: its loudness, the limiting played hard
                                                     * at no trim and at +12 dB (to choose TARGET_DB and LIM_MAX_DB) */
        for (s = 0; s < n; s++)
            printf("%-16s %-8s %d %6.1f %5.1f %5.1f\n", clp_sound_name(s), clp_sound_engine(s), clp_sound_mono(s),
                   loud(s, 32768), reduction(s, 32768), reduction(s, q_of(12.0)));
        return 0;
    }
    for (s = 0; s < n && s < 512u; s++) {
        unsigned k = clp_sound_key(s);
        double l, db, r, up;
        before[s] = loud(s, q_of(-30.0)) + 30.0;    /* (a reading under the limiter: as made, it would be) */
        db = TARGET_DB - before[s];
        for (it = 0; it < 8u; it++) {            /* to the target, as far as the limiter lets it */
            db = db > BOOST_MAX_DB ? BOOST_MAX_DB : db < -40.0 ? -40.0 : db;
            r = reduction(s, q_of(db));
            if (r > LIM_MAX_DB) {
                db -= r - LIM_MAX_DB + 0.2;
                capped += it == 0u;
                continue;
            }
            l = loud(s, q_of(db));
            up = TARGET_DB - l;
            if (fabs(up) < 0.3 || (up > 0 && (db >= BOOST_MAX_DB || r > LIM_MAX_DB - 0.3)))
                break;
            db += up;
        }
        lim0[s] = r;
        got[s] = loud(s, q_of(db));
        ord[s] = -20.0 * log10(g_ord / 32768.0);
        if ((k >> 8) < 64u && (k & 0xFFu) < 40u)
            q[k >> 8][k & 0xFFu] = (uint32_t)q_of(db);
        boosted += db > 0.5, cut += db < -0.5;
    }
    f = fopen(argc > 1 ? argv[1] : "app/clipper/gen/clp_trim.h", "w");
    if (!f)
        return 1;
    fprintf(f, "/* generated by tools/clp_trim.c (make clp-trim): each factory sound's trim, Q15 (above 32768 a boost), "
               "[engine][preset]:\n * its ordinary phrase's loudest moment to %.0f dB, the hardest playing limited %.0f dB "
               "at most; 0 none */\nstatic const uint32_t CLP_TRIM_Q15[NENGINES][40] = {\n", TARGET_DB, LIM_MAX_DB);
    for (e = 0; e < 23u; e++) {
        fprintf(f, "    {");
        for (p = 0; p < 40u; p++)
            fprintf(f, "%u%s", q[e][p], p < 39u ? "," : "");
        fprintf(f, "},\n");
    }
    fprintf(f, "};\n");
    fclose(f);
    printf("clp_trim: %u sounds, %u turned up, %u down, %u held back by the limiter\n", n, boosted, cut, capped);
    {
        double lo = 99, hi = -99;
        unsigned u3 = 0, u6 = 0;
        for (s = 0; s < n && s < 512u; s++) {
            lo = got[s] < lo ? got[s] : lo, hi = got[s] > hi ? got[s] : hi;
            u3 += got[s] < TARGET_DB - 3.0, u6 += got[s] < TARGET_DB - 6.0;
        }
        {
            double o[512], t;
            unsigned i, j;
            for (i = 0; i < n && i < 512u; i++)
                o[i] = ord[i];
            for (i = 0; i < n && i < 512u; i++)
                for (j = i + 1; j < n && j < 512u; j++)
                    if (o[j] < o[i])
                        t = o[i], o[i] = o[j], o[j] = t;
            printf("the limiter in the ordinary phrase: median %.1f dB, 90%% under %.1f, the deepest %.1f\n", o[n / 2],
                   o[n * 9 / 10], o[n - 1]);
        }
        printf("target %.0f dB, limiting at most %.0f dB played hard: %.1f to %.1f dB; %u over 3 dB under, %u over 6\n",
               TARGET_DB, LIM_MAX_DB, lo, hi, u3, u6);
        for (s = 0; s < n && s < 512u; s++)
            if (got[s] < TARGET_DB - 6.0)
                printf("  %-16s %-8s %.1f dB, limited %.1f played hard\n", clp_sound_name(s), clp_sound_engine(s), got[s], lim0[s]);
    }
    if (argc > 2) {                              /* (built again with the table: what is left) */
        static double after[512];
        double lo = 99, hi = -99;
        unsigned under6 = 0;
        clp_trim_on(1);
        for (s = 0; s < n && s < 512u; s++) {
            double r = reduction(s, -1), l = after[s] = loud(s, -1);
            left3 += r > LIM_MAX_DB + 0.5;
            if (r > worst)
                worst = r;
            lo = l < lo ? l : lo, hi = l > hi ? l : hi;
            under6 += l < TARGET_DB - 6.0;
            if (l < TARGET_DB - 6.0 || r > LIM_MAX_DB + 0.5)
                printf("  %-16s %-8s %.1f dB (%.1f as made; limited %.1f dB played hard)\n", clp_sound_name(s),
                       clp_sound_engine(s), l, before[s], r);
        }
        printf("with the trims: %.1f to %.1f dB; %u sounds over 6 dB under %.0f; %u limited over %.1f dB, the deepest "
               "%.1f\n", lo, hi, under6, TARGET_DB, left3, LIM_MAX_DB + 0.5, worst);
    }
    return 0;
}
