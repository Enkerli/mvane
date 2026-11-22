/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's synthesizer as one unit of mvane yard (clp.h). Clipper (isod89/clipper-fm1 7bc15a9, GPL-3.0-only, after
 * SLOOP and Felucca by Leo Kuroshita, Hügelton Instruments) is vendored in src/ as it is but for one mechanical change
 * (its section attributes as CLP_POOL / CLP_LOWRAM / CLP_NOINIT); its generated tables are in gen/. What Clipper's UI,
 * sequencer, USB and HAL gave the synth is here: the power-on, a sound loaded, MIDI in (seq.c's handling of notes,
 * bend, pressure, MPE and the controllers), the block render (audio.c), and stand-ins for the rest. */
#include <stdint.h>
#ifdef PLAT_HOST
#include <string.h>
#else
void *memset(void *d, int c, unsigned n);       /* felucca-bsp's chip/libc.c, in the platform's unit */
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);
#endif
#include "clp.h"

#define CLIPPER_SYNTH 1
#ifndef CLP_WAVE
#define CLP_WAVE 0                              /* WAVE: off, for RAM (src/eng_wave_off.c) */
#endif
#define CLP_DLY_LEN 16384u                      /* the delay line: 0.74 s at its half rate (Clipper: 1.49 s) */
#define CLP_FX_GATE 1                           /* the effect buses skipped once their sends are long silent (fx.c) */
#include <stdint.h>
#define CLP_FX_QUIET_BLOCKS (20u * 44100u / 32u)   /* 20 s of 32-sample blocks: past any reverb or delay tail */
static uint32_t clp_fx_quiet;
static int clp_fx_gate(const int32_t *c, const int32_t *d, const int32_t *r, uint32_t n)   /* 1: skip the buses */
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (c[i] | d[i] | r[i]) {
            clp_fx_quiet = 0;
            return 0;
        }
    if (clp_fx_quiet < CLP_FX_QUIET_BLOCKS) {
        clp_fx_quiet++;
        return 0;
    }
    return 1;
}
#ifdef PLAT_HOST
#define CLP_POOL
#define CLP_LOWRAM
#else
#define CLP_POOL __attribute__((section(".pool")))
#define CLP_LOWRAM __attribute__((section(".pool")))
#endif
#define CLP_NOINIT

/* ------------------------------------------------- Felucca's libc helpers --- */
static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static unsigned str_len(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1u < max)
        d[i] = s[i], i++;
    if (max)
        d[i] = 0;
}
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[16];
    int n = 0, k;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (v < 0)
        *b++ = '-';
    for (k = n - 1; k >= 0; k--)
        *b++ = t[k];
    *b = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }
static uint32_t rng(void)
{
    static uint32_t s = 0x2545F491u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
#ifndef PLAT_HOST
/* the compiler's 64-bit unsigned divide, which the FM-1's toolchain does not link here: PHYS (phys_dsp.c) divides
 * once a note and block, never a sample. Shift and subtract */
uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    uint64_t q = 0, r = 0;
    int i;
    if (!d)
        return ~(uint64_t)0;
    if (!(n >> 32) && !(d >> 32))
        return (uint32_t)n / (uint32_t)d;
    for (i = 63; i >= 0; i--) {
        r = r << 1 | (n >> i & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    return q;
}
#endif
static void fm1_irq_on(void) {}
static void fm1_irq_off(void) {}

/* the SAMPLE engine's user slots (sent from Clipper's editor): none here, never a read of the flash */
static const uint8_t clp_no_slot[64] = {0xFF, 0xFF, 0xFF, 0xFF};
#define SMP_USER_XIP(k) ((void)(k), clp_no_slot)

/* ----------------------------------------------------------- the synth --- */
#include "felucca_tables.h"
#define bootguard clp_bootguard                 /* (core.h's global, unused by the synth: its own name, so a platform's
                                                 * bootguard never clashes: VIBE FM's converter) */
#include "core.h"
#include "cpu1.h"
/* the second core: fm1-base's (plat.h plat_cpu2_*, the same lineage as Clipper's cpu1.c: X0X, Melodee). voice.c hands it
 * half of a part's voices each block when cpu1_units and cpu1_on() say so (clp_cores) */
int plat_cpu2_on(void);
int plat_cpu2_run(void (*fn)(uint32_t), uint32_t arg);
int plat_cpu2_wait(void);
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { return plat_cpu2_run(fn, arg); }
static int cpu1_wait(void) { return plat_cpu2_wait(); }
static int cpu1_on(void) { return plat_cpu2_on(); }
static uint32_t cpu1_ticks(void) { return 0; }
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
/* (mvane yard) each sound's trim (Q15, 32768 none): Clipper's levels suit a few notes, mvane yard's chords are six
 * (a bass and five), and the master limiter worked 11 dB on the median chord sound, audible as grit (Alex,
 * 2026-11-22). tools/clp_trim.c measures each sound's chord (a mono sound's note) and writes gen/clp_trim.h: the gain
 * that keeps it under the limiter, never above 1. clp_load sets it */
static int32_t clp_trim = 32768;
#define CLP_TRIM clp_trim
#include "fx.c"
static volatile uint8_t panic_req;             /* (seq.c) bit per track: release every sounding note */
static uint8_t midi_local_off, midi_bank, midi_rpn[2] = {127, 127};
static int param_kept(uint32_t i)               /* (ui.c) what a sound loaded leaves as it is */
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_SLEN && i <= P_SGATE) ||
           (i >= P_ROOT && i <= P_QUANT) || i == P_CHORD || i == P_TFLT || i == P_STRUM || i == P_VLEAD;
}
#include "seq_ctl.c"                            /* (seq.c, verbatim) the controllers and the CC map */

/* ------------------------------------------------------------- the glue --- */
/* (seq.c events_block, what the synth needs of it) at each block's start: a panic asked for, the engine switch (its
 * fade, then the new engine, voice.c engine_block), the strummed notes due */
static void events_block(uint32_t n)
{
    uint32_t i, pr = panic_req;
    panic_req = 0;
    for (i = 0; i < NPART; i++) {
        if ((pr >> i) & 1u)
            trk_all_off(&trk[i]);
        engine_block(&trk[i]);
    }
    strum_block(n);
}

static uint8_t clp_mono, clp_breath;            /* clp_mode: mvane yard's SEL */
#define CLP_NSOUNDS 407u
static uint16_t clp_list[CLP_NSOUNDS];          /* engine << 8 | preset, the playable ones */
static uint32_t clp_n;
static volatile int32_t clp_req = -1;           /* the sound asked for (main loop), taken by the render */

/* (mvane yard) expression for every sound: the factory sounds that full pressure or slide leaves unchanged (measured:
 * tools/clp_expr.c, gen/clp_expr.h; most because their engine reads no cutoff, where Clipper sends both) get a matrix
 * slot when loaded, pressure or slide to the engine's tone (its CUT where it has one; DCW, TIMB, VOWL, TOP, MOD ...),
 * half its range, and pressure to the level too (up to +3.5 dB). All rest at 0, so the sound at rest is the factory's. clp_expr_on(0): none (to measure) */
#if __has_include("clp_expr.h")
#include "clp_expr.h"                           /* CLP_EXPR[engine][preset]: 1 pressure, 2 slide leaves it unchanged */
#else
static const uint8_t CLP_EXPR[NENGINES][40];
#endif
#define XE(k) ((uint8_t)(MT_E1 + (k) - 1u))     /* the engine's parameter k, 1..8 */
/* pressure's, slide's destination, by engine (N_ENGNAME). An engine whose voices split between the cores (voice.c
 * vs_ok) takes the matrix's CUT or SHAPE, which each voice carries: a slot on one of its EDIT values (E1..E8) would
 * write the part's values for each voice's call, which voice.c renders on this core after the other's (no split:
 * HYBRID GLASS PAD from about 50% to 78% on the FM-1, Alex 2026-11-22). The others take an EDIT value, free there */
static const uint8_t CLP_EXPR_DST[NENGINES][2] = {
    {MT_CUT, MT_CUT},       /* ANALOG   (splits) */
    {MT_CUT, MT_SHAPE},     /* DIGITAL  (splits) */
    {MT_CUT, MT_SHAPE},     /* PHASE    (splits) */
    {MT_OFF, MT_OFF},       /* LOFI     (not offered) */
    {MT_CUT, MT_CUT},       /* SAMPLE   (splits, since mvane yard's voice.c vs_ok) */
    {XE(5), XE(1)},         /* VOICE    BUZZ, VOWL */
    {MT_CUT, MT_SHAPE},     /* TRIO     (splits) */
    {MT_OFF, MT_OFF},       /* WHEEL    (splits; its bars and drive are the part's, set once a block: no voice's
                             * matrix reaches them; pressure to the level only) */
    {XE(8), XE(2)},         /* GRAIN    TONE, POS */
    {MT_CUT, MT_SHAPE},     /* FM6      (splits) */
    {XE(3), XE(5)},         /* PHYS     BRIT, POS */
    {MT_OFF, MT_OFF},       /* NOISE    (not offered) */
    {MT_CUT, MT_CUT},       /* HYBRID   (splits) */
    {MT_OFF, MT_OFF},       /* WAVE     (not offered) */
    {MT_OFF, MT_OFF},       /* ACID     (not offered) */
    {MT_CUT, MT_CUT},       /* STRINGS  (splits) */
    {MT_CUT, MT_CUT},       /* MACRO    (splits) */
    {MT_CUT, MT_SHAPE},     /* VPS      (splits) */
    {MT_CUT, MT_SHAPE},     /* TERRAIN  (splits) */
    {MT_SHAPE, MT_SHAPE},   /* FMX      (splits; reads no cutoff) */
    {MT_CUT, MT_CUT},       /* WSEQ     (splits) */
    {MT_CUT, MT_SHAPE},     /* JUNO     (splits) */
    {MT_CUT, MT_SHAPE},     /* VECTOR   (splits) */
};
#define CLP_EXPR_AMT 32
static uint8_t clp_expr_enabled = 1;
void clp_expr_on(int on) { clp_expr_enabled = (uint8_t)(on != 0); }
static void clp_expr_fill(track_t *t, uint32_t e, uint32_t pi)
{
    uint32_t k, w, deaf = pi < 40u ? CLP_EXPR[e][pi] : 0u, used = 0;
    if (!clp_expr_enabled || !deaf)
        return;
    for (k = 0; k < MX_SLOTS; k++) {             /* what the sound's own matrix already takes */
        int32_t sr = t->p[mx_pid(k)];
        if (sr == MS_AT && t->p[mx_pid(k) + 2u])
            deaf &= ~1u;
        if (sr == MS_SLIDE && t->p[mx_pid(k) + 2u])
            deaf &= ~2u;
    }
    for (w = 0; w < 3u; w++) {           /* pressure to the tone, slide to the tone, pressure to the level too (a swell
                                          * every engine answers, the organ's drawbars and saturated tones included) */
        uint32_t dst = w == 2u ? MT_AMP : CLP_EXPR_DST[e][w];
        if (!(deaf >> (w & 1u) & 1u) || dst == MT_OFF)
            continue;
        for (k = MX_SLOTS; k-- > 0;) {           /* the last free slot */
            uint32_t id = mx_pid(k);
            if (used >> k & 1u || (t->p[id] > MS_OFF && t->p[id + 2u]))
                continue;
            t->p[id] = (int16_t)(w == 1u ? MS_SLIDE : MS_AT);
            t->p[id + 1u] = (int16_t)dst;
            t->p[id + 2u] = dst >= MT_E1 ? CLP_EXPR_AMT : 56;   /* (CUT and SHAPE: half of x first, voice.c) */
            used |= 1u << k;
            break;
        }
    }
}

#if __has_include("clp_trim.h")
#include "clp_trim.h"                           /* CLP_TRIM_Q15[engine][preset] (0: none measured, 32768) */
#else
static const uint16_t CLP_TRIM_Q15[NENGINES][40];
#endif
static uint8_t clp_trim_enabled = 1;
void clp_trim_on(int on) { clp_trim_enabled = (uint8_t)(on != 0); }
int32_t clp_trim_now(void) { return clp_trim; }
static void clp_load(uint32_t e, uint32_t pi)   /* as ui.c set_engine_of + apply_preset_to: the sound, its sends */
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    track_t *t = &trk[0];
    const engine_t *eng = ENGINES[e % NENGINES];
    const preset_t *pr;
    uint32_t i;
    if (!eng->npresets)
        return;
    pi %= eng->npresets;
    pr = &eng->presets[pi];
    sound_fade_begin(t);
    t->eng_req = (uint8_t)(e % NENGINES);
    t->preset = (uint8_t)pi;
    t->user = 0;
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_ED_FX] = preset_trim(e % NENGINES, pi);
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
        if (P_DIST + i == P_DLY || P_DIST + i == P_REV)
            t->p[P_DIST + i] = 0;                /* (Clipper 1.0: factory sounds dry) */
        t->p[P_AMODE + i] = (int16_t)TP[P_AMODE + i].def;   /* (no arpeggiator: mvane yard plays the notes) */
    }
    preset_extras(t->p, pr);
    if (clp_mono)                                /* mvane yard's MONO: legato, whatever the sound says */
        t->p[P_VOICE] = V_LEGATO;
    mseq_init(t);
    fm6_track_loaded(t);
    clp_expr_fill(t, e % NENGINES, pi);
    {
        uint32_t q = pi < 40u ? CLP_TRIM_Q15[e % NENGINES][pi] : 0u;
        clp_trim = clp_trim_enabled && q ? (int32_t)q : 32768;
    }
}

void clp_mode(uint32_t mono, uint32_t breath)   /* SEL: MONO (legato), BREATH (CC2 the level, as Clipper's EWI) */
{
    clp_mono = (uint8_t)(mono != 0);
    clp_breath = (uint8_t)(breath != 0);
    midi_breath = clp_breath;                    /* (voice.c: 1 VOL, the breath the level; and the part legato) */
    if (clp_mono)
        trk[0].p[P_VOICE] = V_LEGATO;
}

void clp_cores(uint32_t n) { cpu1_units = n > 1u ? 1 : 0; }   /* the app started the second core (app_init) */

static void clp_order(void);
void clp_init(void)
{
    uint32_t i, e, p;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = 120;
    fm6_init();
    for (i = 0; i < NTRK; i++)
        for (p = 0; p < P_E0; p++)
            trk[i].p[p] = TP[p].def;
    song.master_q12 = 4096;
    mpe_mode = 1;                                /* MPE: the member channels' own bend (48), pressure, slide */
    clp_order();
    trk[0].engine = 0;
    clp_load(clp_list[0] >> 8, clp_list[0] & 0xFFu);
    trk[0].engine = trk[0].eng_req;
}

/* the sounds' order. As made: the engines in an order for chords (Clipper's sounds within each), also the order a
 * settings record from before 0.10 counts by (clp_sound_old_key). Banked (the default, Alex 2026-11-22): by family,
 * as each sound behaves (tools/clp_bank.c, gen/clp_bank.h): CHORDS LIGHT, CHORDS (poly, sustained), HITS LIGHT, HITS
 * (poly, short), LEADS (mono, sustained: a wind controller's), OTHER (mono and short, Clipper's FX); within one, by
 * Clipper's kind (its ui.c BANK[], gen/clp_kind.h: pads, organs, keys, stabs, plucks, leads, basses), then as made */
#if __has_include("clp_kind.h")
#include "clp_kind.h"
#else
enum { CK_BASS, CK_KEYS, CK_ORGAN, CK_PAD, CK_LEAD, CK_PLUCK, CK_STAB, CK_FX, CK_NONE };
static const struct { uint8_t kind, e; const char *name; } CLP_KIND[] = {{CK_NONE, 0, ""}};
#endif
#if __has_include("clp_bank.h")
#include "clp_bank.h"
#else
static const uint8_t CLP_FAM[NENGINES][40];
#endif
#if __has_include("clp_cost.h")
#include "clp_cost.h"                           /* tools/clp_cost.c: each sound's measured cost, 0 light .. 2 heavy */
#else
static const uint8_t CLP_COST[NENGINES][40];
#endif
static const char *const CLP_KIND_N[CK_NONE + 1] = {"BASS", "KEYS", "ORGAN", "PAD", "LEAD", "PLUCK", "STAB", "FX", ""};
/* the families PRESETS runs through (Alex, 2026-11-22): the light ones first, by use, then the same for the heavy;
 * EZ and HRD say the load, not the sound's size */
static const char *const CLP_FAM_N[7] = {"CHORDS EZ", "LEADS EZ", "HITS EZ", "CHORDS HRD", "LEADS HRD", "HITS HRD",
                                         "OTHER"};
static uint16_t clp_list0[CLP_NSOUNDS];         /* as made */
static uint8_t clp_bank_wanted = 1;
static uint32_t clp_kind_of(uint32_t key)
{
    uint32_t j, e = key >> 8;
    const char *nm = ENGINES[e % NENGINES]->presets[key & 0xFFu].name;
    for (j = 0; j < sizeof CLP_KIND / sizeof CLP_KIND[0]; j++)
        if (CLP_KIND[j].e == e && str_eq(CLP_KIND[j].name, nm))
            return CLP_KIND[j].kind;
    return CK_NONE;
}
static uint32_t clp_cost_of(uint32_t key)
{
    uint32_t e = key >> 8, p = key & 0xFFu;
    return e < NENGINES && p < 40u ? CLP_COST[e][p] : 0u;
}
static uint32_t clp_fam_of(uint32_t key)        /* 0 CHORDS EZ .. 6 OTHER (CLP_FAM_N) */
{
    static const uint8_t AT[3] = {0, 2, 1};      /* clp_bank.h's CHORDS, HITS, LEADS: their places among the EZ */
    uint32_t e = key >> 8, p = key & 0xFFu, f = e < NENGINES && p < 40u ? CLP_FAM[e][p] : 3u;
    return f >= 3u ? 6u : AT[f] + (clp_cost_of(key) ? 3u : 0u);
}
static uint32_t clp_rank(uint32_t key, uint32_t at)   /* the banked order's sort key */
{
    static const uint8_t KR[CK_NONE + 1] = {6, 2, 1, 0, 5, 4, 3, 7, 8};   /* PAD ORGAN KEYS STAB PLUCK LEAD BASS FX */
    return clp_fam_of(key) << 16 | (uint32_t)KR[clp_kind_of(key)] << 12 | at;
}
static void clp_order(void)
{
    static const uint8_t ORDER[] = {ENGI_HYBRID, ENGI_JUNO, ENGI_STRINGS, 0 /* ANALOG */, ENGI_FM6, ENGI_FMX,
                                    1 /* DIGITAL */, 7 /* WHEEL */, 2 /* PHASE */, 5 /* VOICE */, ENGI_MACRO,
                                    ENGI_PHYS, 6 /* TRIO */, ENGI_VPS, ENGI_TERRAIN, ENGI_WAVE, ENGI_WSEQ,
                                    ENGI_VECTOR, ENGI_GRAIN, 4 /* SAMPLE */};
    /* (left out, Alex 2026-11-21: ACID, NOISE, LOFI; their code stays, out of the list) */
    static uint32_t rk[CLP_NSOUNDS];
    uint32_t k, e, p, i, j;
    clp_n = 0;
    for (k = 0; k < sizeof ORDER; k++) {
        e = ORDER[k];
        for (p = 0; e < NENGINES && p < ENGINES[e]->npresets && clp_n < CLP_NSOUNDS; p++)
            clp_list0[clp_n] = clp_list[clp_n] = (uint16_t)(e << 8 | p), clp_n++;
    }
    if (!clp_bank_wanted)
        return;
    for (i = 0; i < clp_n; i++)
        rk[i] = clp_rank(clp_list[i], i);
    for (i = 1; i < clp_n; i++)                  /* (insertion: once, at the start) */
        for (j = i; j > 0 && rk[j - 1u] > rk[j]; j--) {
            uint32_t r = rk[j];
            uint16_t l = clp_list[j];
            rk[j] = rk[j - 1u], clp_list[j] = clp_list[j - 1u];
            rk[j - 1u] = r, clp_list[j - 1u] = l;
        }
}
void clp_bank_on(int on)                        /* 1 (default): by family; 0: as made (tools). Call before clp_init */
{
    clp_bank_wanted = (uint8_t)(on != 0);
    if (clp_n)
        clp_order();
}
uint32_t clp_sound_family(uint32_t i) { return i < clp_n ? (clp_bank_wanted ? clp_fam_of(clp_list[i]) : 0u) : 0u; }
const char *clp_family_name(uint32_t f) { return f < 7u ? CLP_FAM_N[f] : ""; }
uint32_t clp_sound_kind_id(uint32_t i) { return i < clp_n ? clp_kind_of(clp_list[i]) : CK_NONE; }
uint32_t clp_sound_old_key(uint32_t i) { return i < clp_n ? clp_list0[i] : 0xFFFFu; }   /* (before 0.10: as made) */
uint32_t clp_sound_of_key(uint32_t key)         /* its place in the list now, or clp_sounds() */
{
    uint32_t i;
    for (i = 0; i < clp_n && clp_list[i] != key; i++)
        ;
    return i;
}

uint32_t clp_sounds(void) { return clp_n; }
const char *clp_sound_name(uint32_t i)
{
    return i < clp_n ? ENGINES[clp_list[i] >> 8]->presets[clp_list[i] & 0xFFu].name : 0;
}
const char *clp_sound_engine(uint32_t i) { return i < clp_n ? ENGINES[clp_list[i] >> 8]->name : 0; }
const char *clp_sound_kind(uint32_t i) { return i < clp_n ? CLP_KIND_N[clp_kind_of(clp_list[i])] : ""; }
uint32_t clp_sound_key(uint32_t i) { return i < clp_n ? clp_list[i] : 0xFFFFu; }   /* engine << 8 | preset */
uint32_t clp_sound_cost(uint32_t i)             /* 0 EZ, 1 HRD (measured: tools/clp_cost.c) */
{
    uint32_t e, p;
    if (i >= clp_n)
        return 0;
    e = clp_list[i] >> 8, p = clp_list[i] & 0xFFu;
    return e < NENGINES && p < 40u ? CLP_COST[e][p] : 0u;
}
int clp_sound_mono(uint32_t i)                  /* 1: a mono sound (legato), as Clipper's preset says */
{
    return i < clp_n && ENGINES[clp_list[i] >> 8]->presets[clp_list[i] & 0xFFu].mono;
}
void clp_select(uint32_t i)
{
    if (i < clp_n)
        clp_req = (int32_t)i;
}

static void clp_pending(void)                   /* a sound asked for: loaded before what follows it */
{
    int32_t r = clp_req;
    if (r >= 0) {
        clp_req = -1;
        clp_load(clp_list[r] >> 8, clp_list[r] & 0xFFu);
    }
}

void clp_event(uint32_t st, uint32_t d1, uint32_t d2)   /* (seq.c's events_block: MIDI in, for part 1) */
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu;
    track_t *t = &trk[0];
    clp_pending();                               /* (the notes after a change are the new sound's) */
    d1 &= 0x7Fu, d2 &= 0x7Fu;
    if (mpe_mode && ch && (kind == 0xE0u || kind == 0xD0u || (kind == 0xB0u && d1 == 74u))) {
        if (kind == 0xE0u)                       /* an MPE member channel's own controls */
            mpe_bend[ch] = (int16_t)((((int32_t)d2 << 7 | (int32_t)d1) - 8192) * 4);
        else if (kind == 0xD0u)
            mpe_press[ch] = (uint8_t)d1;
        else
            mpe_slide[ch] = (int8_t)((int32_t)d2 - 64);
        return;
    }
    if (kind == 0x90u && d2) {
        mpe_cur_ch = (uint8_t)(mpe_mode ? ch : 0u);
        trk_note_on(t, d1, d2);
        mpe_cur_ch = 0;
    } else if (kind == 0x80u || kind == 0x90u)
        trk_note_off(t, d1);
    else if (kind == 0xE0u || kind == 0xD0u || kind == 0xA0u)
        part_ctl_event(t, kind, d1, d2);
    else if (kind == 0xB0u) {
        if (d1 == 12u) {                         /* (mvane yard) CC 12: the DRIVE send, which Clipper's map has none for */
            t->p[P_DIST] = (int16_t)(TP[P_DIST].min + ((int32_t)d2 * (TP[P_DIST].max - TP[P_DIST].min) + 63) / 127);
            return;
        }
        midi_cc(t, d1, d2);                      /* mod wheel, expression, breath, sustain, the macros (16..19),
                                                  * the standard CC map (74 cutoff, 71 resonance, 73 attack, ...) */
    }
}

void clp_panic(void) { trk_all_off(&trk[0]); }
uint32_t clp_glide(void) { return (uint32_t)trk[0].p[P_GLIDE]; }   /* the sound's GLIDE now (0..127) */
uint32_t clp_voices(void)                       /* voices with their gate open */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += trk[0].v[i].active && trk[0].v[i].gate;
    return n;
}

/* a parameter as 0..127 over its range (as a CC sets it, seq.c midi_cc), and back */
static uint32_t clp_q(uint32_t pi, int32_t v)
{
    int32_t lo = TP[pi].min, hi = TP[pi].max;
    return hi > lo ? (uint32_t)clamp((v - lo) * 127 / (hi - lo), 0, 127) : 0u;
}
uint32_t clp_macro(uint32_t i)                  /* BRITE TIMBR MOTN SPACE, 0..127 (as CC 16..19 set them) */
{
    const track_t *t = &trk[0];
    if (i > 3u)
        return 0;
    if (t->eng_req == ENGI_HYBRID)
        return (uint32_t)clamp(t->p[P_E4 + i], 0, 127);
    return (uint32_t)clamp(t->p[P_MAC1 + i] + (i < 2u ? 64 : 0), 0, 127);
}
uint32_t clp_fx(uint32_t i)                     /* DRIVE CHORUS DELAY REVERB sends, 0..127 */
{
    static const uint8_t P[4] = {P_DIST, P_CHOR, P_DLY, P_REV};
    return i < 4u ? clp_q(P[i], trk[0].p[P[i]]) : 0u;
}
const char *clp_reverb_step(int32_t d)          /* the reverb's model (main loop): -> its name */
{
    const param_desc_t *g = &GP[G_RTYPE];
    int32_t n = g->max - g->min + 1, v = song.g[G_RTYPE] - g->min;
    if (d)
        song.g[G_RTYPE] = (int16_t)(g->min + ((v + (d > 0 ? 1 : n - 1)) % n));
    return g->names ? g->names[song.g[G_RTYPE] - g->min] : "";
}

void clp_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t b;
    clp_pending();
    if (fm6_pend)                                /* (eng_fm6.c, main loop in Clipper) FM6: the new sound's patch */
        fm6_poll();
    song.master_q12 = (uint16_t)(master_q12 > 4096u ? 4096u : master_q12);
    for (b = 0; b + CTL <= n; b += CTL) {        /* Clipper's blocks (audio.c audio_block): Q15 -> 24-bit */
        uint32_t i;
        mix_block(lr + 2u * b, CTL);
        for (i = 0; i < 2u * CTL; i++)
            lr[2u * b + i] <<= 7;
    }
}
