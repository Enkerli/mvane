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
static void fm1_irq_on(void) {}
static void fm1_irq_off(void) {}

/* the SAMPLE engine's user slots (sent from Clipper's editor): none here, never a read of the flash */
static const uint8_t clp_no_slot[64] = {0xFF, 0xFF, 0xFF, 0xFF};
#define SMP_USER_XIP(k) ((void)(k), clp_no_slot)

/* ----------------------------------------------------------- the synth --- */
#include "felucca_tables.h"
#include "core.h"
#include "cpu1.h"
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn, (void)arg; return 0; }   /* one core: nothing handed */
static int cpu1_wait(void) { return 0; }
static int cpu1_on(void) { return 0; }
static uint32_t cpu1_ticks(void) { return 0; }
#include "engines.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
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
}

void clp_mode(uint32_t mono, uint32_t breath)   /* SEL: MONO (legato), BREATH (CC2 the level, as Clipper's EWI) */
{
    clp_mono = (uint8_t)(mono != 0);
    clp_breath = (uint8_t)(breath != 0);
    midi_breath = clp_breath;                    /* (voice.c: 1 VOL, the breath the level; and the part legato) */
    if (clp_mono)
        trk[0].p[P_VOICE] = V_LEGATO;
}

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
    clp_n = 0;
    {   /* every factory sound, the engines in an order for chords: pads and keys first, basses and noises last */
        static const uint8_t ORDER[] = {ENGI_HYBRID, ENGI_JUNO, ENGI_STRINGS, 0 /* ANALOG */, ENGI_FM6, ENGI_FMX,
                                        1 /* DIGITAL */, 7 /* WHEEL */, 2 /* PHASE */, 5 /* VOICE */, ENGI_MACRO,
                                        ENGI_PHYS, 6 /* TRIO */, ENGI_VPS, ENGI_TERRAIN, ENGI_WAVE, ENGI_WSEQ,
                                        ENGI_VECTOR, ENGI_GRAIN, 4 /* SAMPLE */, 3 /* LOFI */, ENGI_ACID, ENGI_NOISE};
        uint32_t k;
        for (k = 0; k < sizeof ORDER; k++) {
            e = ORDER[k];
            for (p = 0; e < NENGINES && p < ENGINES[e]->npresets && clp_n < CLP_NSOUNDS; p++)
                clp_list[clp_n++] = (uint16_t)(e << 8 | p);
        }
    }
    trk[0].engine = 0;
    clp_load(clp_list[0] >> 8, clp_list[0] & 0xFFu);
    trk[0].engine = trk[0].eng_req;
}

uint32_t clp_sounds(void) { return clp_n; }
const char *clp_sound_name(uint32_t i)
{
    return i < clp_n ? ENGINES[clp_list[i] >> 8]->presets[clp_list[i] & 0xFFu].name : 0;
}
const char *clp_sound_engine(uint32_t i) { return i < clp_n ? ENGINES[clp_list[i] >> 8]->name : 0; }
const char *clp_sound_kind(uint32_t i) { (void)i; return ""; }
void clp_select(uint32_t i)
{
    if (i < clp_n)
        clp_req = (int32_t)i;
}

void clp_event(uint32_t st, uint32_t d1, uint32_t d2)   /* (seq.c's events_block: MIDI in, for part 1) */
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu;
    track_t *t = &trk[0];
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
        midi_cc(t, d1, d2);                      /* mod wheel, expression, breath, sustain, the macros (16..19),
                                                  * the standard CC map (74 cutoff, 71 resonance, 73 attack, ...) */
    }
}

void clp_panic(void) { trk_all_off(&trk[0]); }

void clp_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t b;
    int32_t r = clp_req;
    if (r >= 0) {                                /* a sound asked for: loaded here, between two blocks */
        clp_req = -1;
        clp_load(clp_list[r] >> 8, clp_list[r] & 0xFFu);
    }
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
