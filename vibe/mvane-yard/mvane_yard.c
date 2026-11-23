/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard on VIBE FM, native (the Hub's FROM-FM1.md). Step 1: YARD (app/synth.c, the FM-1's own file) on the notes.
 * Step 2: the controls, the VIBE's own (no imitated FM-1 keys):
 *   keys     NOTES: notes (QUANT on: to the scale). CHORDS (as on the FM-1, Coral Flower's): the white keys are the
 *            scale's degrees, higher up the keyboard; the ten black keys the qualities, latched as Coral Flower's
 *            Lock (pressed together: combined; one alone again: none, single notes)
 *   K1..K6   the pages: SOUND, PLAY (QUANT KEY SCALE GLIDE), CHORD (VOICING VOICES BASS BANK), ARP, SCALE (the set's
 *            number, a digit an encoder), SEQ; encoders 1-4 the page's four values
 *   F1..F6   NOTES / CHORDS, HOLD, POLY / MONO / +BREATH, -, -, MORE (the page's next values)
 *   A, B     the sound; the scale (the list from bright to dark)
 *   HOME     the controls of the page
 * MIDI and the DAW's notes play as notes, each on its channel (MPE); QUANT FULL snaps them to the scale. The audio has
 * its own flow: the events only queue MIDI messages, the audio alone touches YARD. To follow: the pages drawn in full,
 * FOCUS, the DAW's parameters and transport, storage, the recorder, the ARP, Clipper. */
#include "vibe.h"
#include <stddef.h>
size_t strlen(const char *s);   /* (the SDK's: vibe_sdk.c) */
char *strcpy(char *d, const char *s);
#include "vibe_dsp.h"
#include "vibe_gfx.h"
/* YARD as the FM-1 computes it: the SDK builds with -ffast-math, the FM-1 without it and without fused multiply-adds;
 * precise maths for YARD's code, so the two render the same samples (tools/vibe_cmp.c) */
#pragma clang fp reassociate(off) contract(off)
#include "../../app/synth.c"
#pragma clang fp reassociate(on) contract(fast)
#include "../../core/chords.c"
#include "../../core/scale_names.h"

VIBE_PARALLEL_AUDIO

/* ---- events to the audio: MIDI messages, one writer (the events), one reader (the audio) ---- */
#define QN 512u
#define Q_SOUND 0xF0u                    /* (not MIDI) d1: YARD's sound to load */
#define Q_MODE 0xF1u                     /* d1: mono, d2: breath (synth_mode) */
static uint8_t q_st[QN], q_d1[QN], q_d2[QN];
static volatile uint32_t qw, qr;
static void send(uint32_t st, uint32_t d1, uint32_t d2)   /* (control flow) full: dropped */
{
    uint32_t w = qw;
    if (w - qr >= QN)
        return;
    q_st[w % QN] = (uint8_t)st, q_d1[w % QN] = (uint8_t)(d1 & 0x7Fu), q_d2[w % QN] = (uint8_t)(d2 & 0x7Fu);
    VIBE_RELEASE();
    qw = w + 1u;
}

static const uint8_t SP_CC[SP_N] = {SYNTH_CC_TONE, SYNTH_CC_REL, SYNTH_CC_GLIDE, SYNTH_CC_ATTACK, SYNTH_CC_DECAY,
                                    SYNTH_CC_RES, SYNTH_CC_DETUNE, SYNTH_CC_SUB, SYNTH_CC_PLUCK, SYNTH_CC_WAVE,
                                    SYNTH_CC_PW, SYNTH_CC_NOISE, SYNTH_CC_FMODE};
static void sound_load(uint32_t n)       /* (audio flow) one of YARD's sounds, as the FM-1 loads it */
{
    const synth_sound_t *s = &SYNTH_SOUNDS[n % SYNTH_NSOUNDS];
    uint32_t i;
    for (i = 0; i < SP_N; i++)
        synth_event(0xB0u, SP_CC[i], s->p[i]);
}

/* ---- the settings (control flow), as the FM-1's ---- */
static struct {
    uint8_t mode;                        /* 0 NOTES, 1 CHORDS */
    uint8_t tonic, scale;                /* the key; the list's place last stepped to */
    uint16_t pcs;                        /* the scale: its pitch-class set's number (bit 0 the tonic, always in) */
    uint8_t quant;                       /* 0 OFF, 1 BASIC, 2 FULL */
    uint8_t voices, style, bass, bank, type;
    uint8_t play;                        /* 0 POLY, 1 MONO, 2 POLY+BR, 3 MONO+BR */
    uint8_t hold, started;
    uint32_t sound;
    uint8_t val[SP_N];                   /* YARD's values as last sent */
} set;
static const char *const QUANT_N[3] = {"OFF", "BASIC", "FULL"};
static const char *const PLAY_N[4] = {"POLY", "MONO", "POLY+BR", "MONO+BR"};
static const char *const NOTE_N[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static void cat(char *d, const char *src) { strcpy(d + strlen(d), src); }   /* (the SDK has no strcat) */
static uint16_t scale_mask(void) { return (uint16_t)((set.pcs & 0xFFFu) | 1u); }
static const char *scale_name(void)      /* the list's name, else a known one, else 0 */
{
    uint32_t i;
    for (i = 0; i < CHORD_NSCALES; i++)
        if ((CHORD_SCALES[i].mask | 1u) == scale_mask())
            return CHORD_SCALES[i].name;
    for (i = 0; i < SCALE_NNAMES; i++)
        if ((SCALE_NAMES[i].pcs | 1u) == scale_mask())
            return SCALE_NAMES[i].name;
    return 0;
}
/* a note to the nearest tone of the scale, down on a tie (Coral Flower's: C# to C in C major) */
static uint32_t scale_snap(uint32_t note)
{
    uint16_t mask = scale_mask();
    uint32_t d;
    for (d = 0; d < 12u; d++) {
        if (note >= d && mask >> ((note - d + 12u - set.tonic) % 12u) & 1u)
            return note - d;
        if (mask >> ((note + d + 12u - set.tonic) % 12u) & 1u)
            return note + d;
    }
    return note;
}

/* ---- MPE channels: a member channel (MIDI 2..16) a note, the one free longest ---- */
static uint8_t sounding[128];            /* (the screen's) notes on now, from the keys, the chords, MIDI and the DAW */
static void sound_count(uint32_t note, int on)
{
    note &= 127u;
    if (on)
        sounding[note]++;
    else if (sounding[note])
        sounding[note]--;
}
static uint8_t ch_notes[16];
static uint32_t ch_free_at[16], ch_clock;
static void note_out(uint32_t ch, uint32_t note, uint32_t vel)   /* a note on (vel > 0) or off, counted */
{
    if (vel)
        ch_notes[ch & 15u]++;
    else if (ch_notes[ch & 15u] && !--ch_notes[ch & 15u])
        ch_free_at[ch & 15u] = ++ch_clock;
    sound_count(note > 127u ? 127u : note, vel != 0u);
    send((vel ? 0x90u : 0x80u) | (ch & 15u), note > 127u ? 127u : note, vel);
}
static uint32_t mpe_channel(void)
{
    uint32_t c, best = 1;
    for (c = 2; c < 16u; c++)
        if (ch_notes[c] < ch_notes[best] || (ch_notes[c] == ch_notes[best] && ch_free_at[c] < ch_free_at[best]))
            best = c;
    return best;
}

/* ---- CHORDS: the FM-1's chord player on core/chords.c ---- */
static struct {
    int32_t key;                         /* the white key sounding (its note), -1 none */
    uint8_t degree, vel;
    int8_t lift;
    uint8_t n, note[CHORD_MAX + 1u], ch[CHORD_MAX + 1u];
    chord_voicing prev;
    uint8_t have_prev;
    uint16_t qlatch, qheld, qgest;       /* the black keys: Coral Flower's Lock */
    uint8_t qtoggle, qstamp[CHORD_BANK], qclock;
    char name[16];
} ch;
static uint8_t key_role[128];            /* each note the keys play: 0 none, 1 a note (NOTES), 2 a degree, 3 + q a quality */
static uint8_t key_out[128], key_ch[128];   /* NOTES: what it plays, on which channel */

static uint16_t qual_eff(void) { return ch.qheld ? ch.qgest : ch.qlatch; }
static uint32_t qual_order(uint16_t eff, uint8_t *q)   /* the latched, oldest pressed first */
{
    uint32_t n = 0, i, j;
    for (i = 0; i < CHORD_BANK; i++)
        if (eff >> i & 1u)
            q[n++] = (uint8_t)i;
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && ch.qstamp[q[j - 1u]] > ch.qstamp[q[j]]; j--) {
            uint8_t t = q[j];
            q[j] = q[j - 1u], q[j - 1u] = t;
        }
    return n;
}
static int32_t keys_lift(void) { return 12 * (vibe_keys_octave_get() - 1); }   /* (OCT: the chords' register too) */
static void chord_release(void)
{
    uint32_t i;
    for (i = 0; i < ch.n; i++)
        note_out(ch.ch[i], ch.note[i], 0);
    ch.n = 0;
    ch.key = -1;
}
static void chord_sound(void)
{
    uint32_t i, j, nn = 0;
    uint8_t notes[CHORD_MAX + 1u], chs[CHORD_MAX + 1u], q[CHORD_BANK];
    uint16_t mask = scale_mask(), eff = qual_eff();
    uint32_t nq = qual_order(eff, q);
    chord_pcs c = set.bank && nq ? chord_build_qualities(set.quant ? mask : 0xFFFu, set.tonic, ch.degree, q, nq, set.voices)
                                 : chord_build(set.quant ? mask : 0xFFFu, set.tonic, ch.degree, set.type, set.voices);
    chord_voicing v;
    if (set.quant == 2u)
        for (i = 0; i < c.n; i++)
            c.pc[i] = (uint8_t)scale_snap(c.pc[i]);
    if (!eff) {                          /* no quality latched: the degree's root alone (Coral Flower's single notes) */
        int32_t tn = 60 + (int32_t)set.tonic - (set.tonic > 6u ? 12 : 0);
        notes[nn++] = (uint8_t)(tn + (int32_t)((c.root + 12u - set.tonic) % 12u) + ch.lift + keys_lift());
        ch.name[0] = 0;
        strcpy(ch.name, NOTE_N[c.root % 12u]);
    } else {
        if (!chord_voice(c, set.style, ch.lift + keys_lift(), ch.have_prev ? &ch.prev : 0, &v))
            return;
        ch.prev = v, ch.have_prev = 1;
        if (set.bass)
            notes[nn++] = v.bass;
        for (i = 0; i < v.n; i++)
            notes[nn++] = v.upper[i];
        chord_name(c, ch.name);
    }
    for (i = 0; i < ch.n; i++) {         /* common tones hold; the others let go first */
        int keep = 0;
        for (j = 0; j < nn; j++)
            keep |= notes[j] == ch.note[i];
        if (!keep)
            note_out(ch.ch[i], ch.note[i], 0);
    }
    for (j = 0; j < nn; j++) {
        int held = -1;
        for (i = 0; i < ch.n; i++)
            if (ch.note[i] == notes[j])
                held = (int)i;
        chs[j] = held >= 0 ? ch.ch[held] : (uint8_t)mpe_channel();
        if (held < 0)
            note_out(chs[j], notes[j], ch.vel);
    }
    for (j = 0; j < nn; j++)
        ch.note[j] = notes[j], ch.ch[j] = chs[j];
    ch.n = (uint8_t)nn;
}
static void harmony_changed(void)        /* a change of key, scale or voicing: what sounds follows it */
{
    if (set.mode && ch.n)
        chord_sound();
}
static void qual_key(uint32_t i, int down)   /* a black key (quality i) down or up: Coral Flower's Lock */
{
    uint16_t bit = (uint16_t)(1u << i), was = qual_eff();
    if (down) {
        if (!ch.qheld) {                 /* a new press: it replaces the latch when let go */
            ch.qtoggle = (uint8_t)(ch.qlatch == bit);
            ch.qgest = bit;
        } else {
            ch.qgest |= bit;
            ch.qtoggle = 0;
        }
        ch.qheld |= bit;
        ch.qstamp[i] = ++ch.qclock;
        set.type = (uint8_t)(set.bank * CHORD_BANK + i);
    } else {
        ch.qheld &= (uint16_t)~bit;
        if (!ch.qheld) {
            ch.qlatch = ch.qtoggle ? 0 : ch.qgest;
            ch.qgest = 0;
            ch.qtoggle = 0;
        }
    }
    if ((qual_eff() != was || down) && ch.n)
        chord_sound();
}
/* a key's place: white keys 0..14 left to right, black keys 0..9 (C# D# F# G# A#, twice); the keyboard from its C */
static int key_white(uint32_t k) { static const int8_t W[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6}; return W[k % 12u] < 0 ? -1 : (int)(k / 12u) * 7 + W[k % 12u]; }
static int key_black(uint32_t k) { static const int8_t B[12] = {-1, 0, -1, 1, -1, -1, 2, -1, 3, -1, 4, -1}; return B[k % 12u] < 0 ? -1 : (int)(k / 12u) * 5 + B[k % 12u]; }
static void chord_key(uint32_t note, uint32_t vel, int down)
{
    int32_t base = 36 + 12 * vibe_keys_octave_get(), k = (int32_t)note - base;
    int w, b;
    if (!down) {                         /* (what the key was when it went down) */
        uint32_t r = key_role[note & 127u];
        key_role[note & 127u] = 0;
        if (r >= 3u)
            qual_key(r - 3u, 0);
        else if (r == 2u && ch.key == (int32_t)note && !set.hold)
            chord_release();
        return;
    }
    if (k < 0 || k > 24)
        return;
    b = key_black((uint32_t)k), w = key_white((uint32_t)k);
    if (b >= 0 && b < (int)CHORD_BANK) {
        key_role[note & 127u] = (uint8_t)(3 + b);
        qual_key((uint32_t)b, 1);
        return;
    }
    if (w < 0)
        return;
    key_role[note & 127u] = 2;
    if (!set.quant) {                    /* chromatic: the tonic on the fifth white key (Coral Flower's layout) */
        int32_t st = w - 4;
        ch.degree = (uint8_t)((st + 24) % 12);
        ch.lift = (int8_t)(st < 0 ? -12 : st >= 12 ? 12 : 0);
    } else {
        uint32_t n = chord_scale_size(scale_mask()), lift = (uint32_t)w / (n ? n : 7u);
        ch.degree = (uint8_t)((uint32_t)w % (n ? n : 7u));
        ch.lift = (int8_t)(12 * (lift > 2u ? 2u : lift));
    }
    ch.vel = (uint8_t)vel;
    ch.key = (int32_t)note;
    chord_sound();
}

static void all_off(void)                /* everything the keys and chords hold, let go */
{
    uint32_t n;
    chord_release();
    for (n = 0; n < 128u; n++) {
        if (key_role[n] == 1u)
            note_out(key_ch[n], key_out[n], 0);
        key_role[n] = 0;
    }
    ch.qheld = ch.qgest = 0;
}

/* ---- the pages ---- */
enum { PG_SOUND, PG_PLAY, PG_CHORD, PG_ARP, PG_SCALE, PG_SEQ, PG_N };
static const char *const PG_NAME[PG_N] = {"SOUND", "PLAY", "CHORD", "ARP", "SCALE", "SEQ"};
static const uint32_t PG_COLOR[PG_N] = {0x2F8F78, 0xE0A040, 0x8A7FD8, 0xD86A8A, 0x4FA8D8, 0x9AA040};
static uint8_t page, more, help;
static uint8_t in_map[16][128];          /* MIDI in, QUANT FULL: 1 + the note it became */
static const uint8_t SOUND_P[3][4] = {{SP_TONE, SP_ATTACK, SP_DECAY, SP_REL}, {SP_DETUNE, SP_SUB, SP_RES, SP_PLUCK},
                                      {SP_WAVE, SP_PW, SP_NOISE, SP_FMODE}};
static const char *const SOUND_N[3][4] = {{"TONE", "ATTACK", "DECAY", "RELEASE"}, {"DETUNE", "SUB", "RES", "PLUCK"},
                                          {"WAVE", "PW", "NOISE", "FILTER"}};

static void param_set(uint32_t p, int32_t v)
{
    set.val[p] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    send(0xB0u, SP_CC[p], set.val[p]);
}
static void sound_pick(uint32_t n)
{
    uint32_t i, glide = set.val[SP_GLIDE];
    set.sound = n % SYNTH_NSOUNDS;
    for (i = 0; i < SP_N; i++)
        set.val[i] = SYNTH_SOUNDS[set.sound].p[i];
    send(Q_SOUND, set.sound, 0);
    if (set.started)                     /* the PLAY page's GLIDE is the player's: every sound gets it (as the FM-1) */
        param_set(SP_GLIDE, (int32_t)glide);
    set.started = 1;
    send(Q_MODE, (set.play & 1u) || SYNTH_SOUNDS[set.sound].mono, set.play >> 1);
}
static void play_set(uint32_t p)
{
    all_off();
    set.play = (uint8_t)(p & 3u);
    send(Q_MODE, (set.play & 1u) || SYNTH_SOUNDS[set.sound].mono, set.play >> 1);
}
static void scale_step(int d)            /* B: the list's next or previous */
{
    uint32_t i, n = CHORD_NSCALES;
    int32_t at = -1;
    for (i = 0; i < n; i++)
        if ((CHORD_SCALES[i].mask | 1u) == scale_mask())
            at = (int32_t)i;
    if (at < 0)
        at = d > 0 ? (int32_t)set.scale - 1 : (int32_t)set.scale + 1;
    set.scale = (uint8_t)((((at + d) % (int32_t)n) + (int32_t)n) % (int32_t)n);
    set.pcs = CHORD_SCALES[set.scale].mask;
    harmony_changed();
}

static void page_value(uint32_t i, const char **label, char *val)   /* encoder i's on this page */
{
    val[0] = 0;
    *label = "";
    if (page == PG_SOUND) {
        uint32_t p = SOUND_P[more % 3u][i];
        *label = SOUND_N[more % 3u][i];
        if (p == SP_WAVE) {
            static const char *const W[3] = {"SAW", "PULSE", "TRI"};
            strcpy(val, W[SYNTH_WAVE(set.val[p])]);
        } else if (p == SP_FMODE) {
            static const char *const F[3] = {"LOW", "BAND", "HIGH"};
            strcpy(val, F[SYNTH_FMODE(set.val[p])]);
        } else
            vg_itoa(set.val[p], val);
    } else if (page == PG_PLAY) {
        static const char *const N[4] = {"QUANT", "KEY", "SCALE", "GLIDE"};
        const char *s;
        *label = N[i];
        if (i == 0)
            strcpy(val, QUANT_N[set.quant % 3u]);
        else if (i == 1)
            strcpy(val, NOTE_N[set.tonic % 12u]);
        else if (i == 2) {
            s = scale_name();
            if (s)
                strcpy(val, s);
            else
                vg_itoa(set.pcs & 0xFFFu, val);
        } else
            vg_itoa(set.val[SP_GLIDE], val);
    } else if (page == PG_CHORD) {
        static const char *const N[4] = {"VOICING", "VOICES", "BASS", "BANK"};
        *label = N[i];
        if (i == 0)
            strcpy(val, CHORD_STYLE_NAMES[set.style % CHORD_STYLES]);
        else if (i == 1)
            vg_itoa(set.voices, val);
        else if (i == 2)
            strcpy(val, set.bass ? "ON" : "OFF");
        else
            strcpy(val, set.bank ? "QUALITY" : "SCALE");
    } else if (page == PG_SCALE) {
        static const char *const N[4] = {"x1000", "x100", "x10", "x1"};
        static const uint16_t P[4] = {1000, 100, 10, 1};
        *label = N[i];
        vg_itoa((int)((set.pcs & 0xFFFu) / P[i] % 10u), val);
    }
}
static void page_turn(uint32_t i, float e)   /* encoder i turned e detents on this page */
{
    int d = e > 0.0f ? 1 : -1, de = (int)(e + (e > 0.0f ? 0.5f : -0.5f));
    if (de == 0)
        de = d;
    if (page == PG_SOUND) {
        uint32_t p = SOUND_P[more % 3u][i];
        if (p == SP_WAVE || p == SP_FMODE) {   /* a choice of three: a step a turn (0, 64, 127) */
            int z = (int)SYNTH_WAVE(set.val[p]) + d;
            z = z < 0 ? 0 : z > 2 ? 2 : z;
            param_set(p, z == 0 ? 0 : z == 1 ? 64 : 127);
        } else
            param_set(p, (int32_t)set.val[p] + 2 * de);
    } else if (page == PG_PLAY) {
        if (i == 0) {
            int q = (int)set.quant + d;
            set.quant = (uint8_t)(q < 0 ? 0 : q > 2 ? 2 : q);
        } else if (i == 1) {
            set.tonic = (uint8_t)((set.tonic + 12u + (uint32_t)d) % 12u);
            harmony_changed();
        } else if (i == 2)
            scale_step(d);
        else
            param_set(SP_GLIDE, (int32_t)set.val[SP_GLIDE] + 2 * de);
    } else if (page == PG_CHORD) {
        if (i == 0)
            set.style = (uint8_t)((set.style + CHORD_STYLES + (uint32_t)d) % CHORD_STYLES);
        else if (i == 1) {
            int v = (int)set.voices + d;
            set.voices = (uint8_t)(v < 3 ? 3 : v > (int)CHORD_MAX ? (int)CHORD_MAX : v);
        } else if (i == 2)
            set.bass = (uint8_t)(d > 0);
        else {
            set.bank = (uint8_t)(d > 0);
            set.type = (uint8_t)(set.bank * CHORD_BANK + set.type % CHORD_BANK);
        }
        harmony_changed();
    } else if (page == PG_SCALE) {
        static const uint16_t P[4] = {1000, 100, 10, 1};
        uint32_t num = set.pcs & 0xFFFu, dg = num / P[i] % 10u;
        int nd = (int)dg + de;
        nd = nd < 0 ? 0 : nd > (i == 0 ? 4 : 9) ? (i == 0 ? 4 : 9) : nd;
        num = num - dg * P[i] + (uint32_t)nd * P[i];
        set.pcs = (uint16_t)(num > 4095u ? 4095u : num);
        harmony_changed();
    }
}

static void labels(void)                 /* the captions under the keys (the keys carry no print) */
{
    static const char *const F[6] = {"MODE", "HOLD", "PLAY", "", "", "MORE"};
    int i;
    for (i = 0; i < PG_N; i++)
        vibe_label(VB_K1 + i, PG_NAME[i]);
    for (i = 0; i < 6; i++)
        vibe_label(VB_F1 + i, F[i]);
}
static void lights(void)
{
    int i;
    for (i = 0; i < PG_N; i++)
        vibe_led_button(VB_K1 + i, PG_COLOR[i], i == page ? 1.0f : 0.15f);
    vibe_led_button(VB_F1, 0xF2F2F2, set.mode ? 1.0f : 0.15f);
    vibe_led_button(VB_F2, 0xFF7A00, set.hold ? 1.0f : 0.0f);
}

void vibe_on_init(float sample_rate)
{
    static const uint8_t START_P[SP_N] = {56, 60, 30, 24, 64, 0, 0, 0, 0, 0, 0, 0, 0};   /* (as the FM-1's first start) */
    synth_rate(sample_rate);
    synth_init(START_P);
    vibe_notes(VIBE_NOTES);
    set.mode = 1, set.tonic = 0, set.scale = 1, set.pcs = 2741u, set.quant = 1;   /* (the FM-1's defaults) */
    set.voices = 5, set.style = CHORD_OPEN, set.bass = 1, set.bank = 1, set.type = CHORD_BANK + 1u;   /* M7 */
    ch.key = -1;
    ch.qlatch = (uint16_t)(1u << 1);     /* (M7 latched, as the FM-1 starts) */
    sound_pick(0);
    page = PG_PLAY;
    labels();
    lights();
}

static const char *const HELP[] = {
    "KEYS",          "NOTES: notes. CHORDS: white keys the degrees, black keys the qualities (latched; together: combined)",
    "K1 - K6",       "the pages: SOUND, PLAY, CHORD, ARP, SCALE, SEQ",
    "ENCODERS 1 - 4", "the page's four values",
    "A",             "the sound (YARD's thirteen)",
    "B",             "the scale, from bright to dark",
    "F1  F2  F3",    "NOTES / CHORDS, HOLD, POLY / MONO / +BREATH",
    "F6",            "MORE: the page's next four (SOUND)",
    "MIDI / DAW",    "notes on their channels (MPE); QUANT FULL: to the scale",
    "HOME",          "this overview"};

static void midi_note(const vibe_event *e)   /* a note from MIDI or the DAW: as it is (QUANT FULL: to the scale) */
{
    uint32_t c = e->midi[0] & 15u, n = e->id & 127u, m;
    if (e->down) {
        int v = (int)(e->value * 127.0f + 0.5f);
        m = set.quant == 2u ? scale_snap(n) : n;
        if (in_map[c][n])
            sound_count(in_map[c][n] - 1u, 0);
        in_map[c][n] = (uint8_t)(m + 1u);
        sound_count(m, 1);
        send(0x90u | c, m, (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v));
    } else {
        m = in_map[c][n] ? in_map[c][n] - 1u : n;
        if (in_map[c][n])
            sound_count(m, 0);
        in_map[c][n] = 0;
        send(0x80u | c, m, 0);
    }
}
static void key_note(const vibe_event *e)    /* a key, NOTES: a note on its own channel (QUANT on: to the scale) */
{
    uint32_t n = e->id & 127u;
    if (e->down) {
        int v = (int)(e->value * 127.0f + 0.5f);
        if (key_role[n] == 1u)
            note_out(key_ch[n], key_out[n], 0);
        key_role[n] = 1;
        key_out[n] = (uint8_t)(set.quant ? scale_snap(n) : n);
        key_ch[n] = (uint8_t)mpe_channel();
        note_out(key_ch[n], key_out[n], (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v));
    } else if (key_role[n] == 1u) {
        note_out(key_ch[n], key_out[n], 0);
        key_role[n] = 0;
    }
}

void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_BUTTON && e->down && (e->id == VB_HOME || e->id == VB_RETURN)) {
        help = e->id == VB_HOME ? !help : 0;
        return;
    }
    if (e->type == VIBE_EV_NOTE) {
        if (e->midi[1] == VIBE_NOTE_KEYS || e->midi[1] == VIBE_NOTE_PADS) {
            if (set.mode && (e->down || key_role[e->id & 127u] >= 2u))
                chord_key(e->id, e->value > 0.0f ? (uint32_t)(e->value * 126.0f + 1.5f) : 100u, e->down);
            else
                key_note(e);
        } else
            midi_note(e);
    } else if (e->type == VIBE_EV_MIDI) {    /* bend, pressure, CCs: as they come (a note's channel its own) */
        uint32_t st = e->midi[0];
        if (st >= 0xA0u && st < 0xF0u && (st & 0xF0u) != 0xC0u)
            send(st, e->midi[1], e->midi[2]);
    } else if (e->type == VIBE_EV_ENCODER) {
        int d = e->value > 0.0f ? 1 : e->value < 0.0f ? -1 : 0;
        if (e->id <= VE_4)
            page_turn(e->id, e->value);
        else if (e->id == VE_A && d)
            sound_pick(set.sound + SYNTH_NSOUNDS + (uint32_t)d);
        else if (e->id == VE_B && d)
            scale_step(d);
    } else if (e->type == VIBE_EV_BUTTON && e->down) {
        if (e->id >= VB_K1 && e->id < VB_K1 + PG_N) {
            page = (uint8_t)(e->id - VB_K1);
            more = 0;
        } else if (e->id == VB_F1) {
            all_off();
            set.mode ^= 1u;
        } else if (e->id == VB_F2) {
            set.hold ^= 1u;
            if (!set.hold && ch.key >= 0 && !key_role[ch.key & 127])
                chord_release();
        } else if (e->id == VB_F3)
            play_set(set.play + 1u);
        else if (e->id == VB_F6)
            more = (uint8_t)((more + 1u) % (page == PG_SOUND ? 3u : 1u));
        lights();
    }
}

void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    static int32_t lr[2u * SYNTH_BLOCK];
    uint32_t w = qw, k;
    int i;
    (void)in;
    VIBE_ACQUIRE();
    for (k = qr; k != w; k++) {
        uint32_t st = q_st[k % QN];
        if (st == Q_SOUND)
            sound_load(q_d1[k % QN]);
        else if (st == Q_MODE)
            synth_mode(q_d1[k % QN], q_d2[k % QN]);
        else
            synth_event(st, q_d1[k % QN], q_d2[k % QN]);
    }
    VIBE_RELEASE();
    qr = w;
    synth_render(lr, (uint32_t)frames, 4096u);   /* (MASTER at full: the host has its own volume) */
    for (i = 0; i < frames; i++) {
        out_l[i] = (float)lr[2 * i] * (1.0f / 8388608.0f);
        out_r[i] = (float)lr[2 * i + 1] * (1.0f / 8388608.0f);
    }
}

/* ---- the screen (step 3): every page drawn whole each time, 480 x 320, the house layout ----
 *   0..40    the page in its colour, NOTES / CHORDS, the sound
 *   44..62   K1..K6: the six pages, the open one lit
 *   66..206  the page's own: what sounds, on a keyboard; the qualities; the sound's values; the scale's number
 *   210..226 the key, the scale, QUANT, the play mode
 *   230..284 encoders 1..4: the page's four values, a bar each
 *   290..318 F1..F6: their captions */
#define C_BG 0x101114u
#define C_INK 0xF2F2F2u
#define C_DIM 0x8A8E98u
#define C_FAINT 0x3A3E48u
#define C_PANEL 0x1C1E24u
static void text_scale(char *t)          /* "C MAJOR", "D PCS 1234" */
{
    const char *s = scale_name();
    char nb[8];
    t[0] = 0;
    cat(t, NOTE_N[set.tonic % 12u]);
    cat(t, " ");
    if (s)
        cat(t, s);
    else {
        cat(t, "PCS ");
        cat(t, vg_itoa(set.pcs & 0xFFFu, nb));
    }
}
/* a keyboard from note lo, white keys w px wide: the scale's tones marked (the tonic in the page's colour), what
 * sounds lit in the page's colour, the lowest (the bass) in red */
static void keyboard(int x0, int y0, int w, int h, uint32_t lo, uint32_t whites, uint32_t accent)
{
    static const int8_t WI[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    uint16_t m = scale_mask();
    uint32_t n, k, lowest = 128u;
    int x;
    for (n = 0; n < 128u; n++)
        if (sounding[n]) {
            lowest = n;
            break;
        }
    for (k = 0; k < whites; k++) {       /* the white keys */
        uint32_t oct = k / 7u, wi = k % 7u, pc;
        static const uint8_t WPC[7] = {0, 2, 4, 5, 7, 9, 11};
        pc = WPC[wi];
        n = lo + oct * 12u + pc;
        x = x0 + (int)k * w;
        vg_rect(x, y0, w - 1, h, n < 128u && sounding[n] ? (n == lowest ? 0xE0484Cu : accent) : 0xA8ACB4);
        if (n < 128u && !sounding[n] && (m >> ((pc + 12u - set.tonic) % 12u) & 1u))
            vg_rect(x + 2, y0 + h - 8, w - 5, 5, (pc + 12u - set.tonic) % 12u ? 0x6A7080 : accent);
    }
    for (k = 0; k < whites; k++) {       /* the black keys above them */
        uint32_t oct = k / 7u, wi = k % 7u, pc;
        static const int8_t BPC[7] = {1, 3, -1, 6, 8, 10, -1};
        if (BPC[wi] < 0 || k + 1u >= whites)
            continue;
        pc = (uint32_t)BPC[wi];
        n = lo + oct * 12u + pc;
        x = x0 + (int)k * w + w - w / 3;
        vg_rect(x, y0, (w * 2) / 3, (h * 3) / 5, n < 128u && sounding[n] ? (n == lowest ? 0xE0484Cu : accent) : 0x24262C);
        if (n < 128u && !sounding[n] && (m >> ((pc + 12u - set.tonic) % 12u) & 1u))
            vg_rect(x + 2, y0 + (h * 3) / 5 - 6, (w * 2) / 3 - 4, 4, (pc + 12u - set.tonic) % 12u ? 0x6A7080 : accent);
    }
    (void)WI;
}
static void draw_frame(void)
{
    uint32_t i, c = PG_COLOR[page];
    vg_clear(C_BG);
    vg_rect(0, 0, 480, 4, c);
    vg_text(PG_NAME[page], 16, 10, VG_BIG, c, 0);
    vg_text(set.mode ? "CHORDS" : "NOTES", 150, 10, VG_BIG, C_INK, 0);
    vg_text(SYNTH_SOUNDS[set.sound].name, 464, 10, VG_BIG, PG_COLOR[PG_SOUND], 2);
    for (i = 0; i < PG_N; i++) {         /* K1..K6 */
        int x = 8 + (int)i * 78;
        if (i == page)
            vg_round(x, 44, 74, 18, 4, PG_COLOR[i]);
        else
            vg_frame(x, 44, 74, 18, 1, C_FAINT);
        vg_text(PG_NAME[i], x + 37, 46, VG_BOLD, i == page ? C_BG : C_DIM, 1);
    }
}
static void draw_values(void)
{
    uint32_t i;
    char t[64];
    text_scale(t);
    cat(t, "    QUANT ");
    cat(t, QUANT_N[set.quant % 3u]);
    cat(t, "    ");
    cat(t, PLAY_N[set.play & 3u]);
    if (set.hold)
        cat(t, "    HOLD");
    vg_text(t, 16, 210, VG_MED, C_DIM, 0);
    for (i = 0; i < 4u; i++) {           /* encoders 1..4, above F1..F6 */
        const char *lb;
        char v[24];
        int x = 8 + (int)i * 118, fill = -1;
        page_value(i, &lb, v);
        vg_round(x, 230, 112, 54, 6, C_PANEL);
        if (!lb[0])
            continue;
        vg_text(lb, x + 8, 234, VG_SMALL, C_DIM, 0);
        vg_text(v, x + 8, 250, VG_BIG, C_INK, 0);
        if (page == PG_SOUND)
            fill = set.val[SOUND_P[more % 3u][i]];
        else if (page == PG_PLAY && i == 3u)
            fill = set.val[SP_GLIDE];
        if (fill >= 0) {
            vg_rect(x + 8, 277, 96, 3, C_FAINT);
            vg_rect(x + 8, 277, fill * 96 / 127, 3, PG_COLOR[page]);
        }
    }
}
static void draw_fkeys(void)
{
    static const char *const F[6] = {"MODE", "HOLD", "PLAY", "", "", "MORE"};
    uint32_t i;
    for (i = 0; i < 6u; i++) {
        int x = 8 + (int)i * 78, lit = (i == 0u && set.mode) || (i == 1u && set.hold);
        const char *t = F[i];
        if (i == 5u && page != PG_SOUND)
            t = "";
        if (!t[0])
            continue;
        vg_frame(x, 292, 74, 24, 1, lit ? 0xFF7A00 : C_FAINT);
        vg_text(i == 0u ? (set.mode ? "CHORDS" : "NOTES") : i == 2u ? PLAY_N[set.play & 3u] : t, x + 37, 297, VG_BOLD,
                lit ? 0xFF7A00 : C_DIM, 1);
    }
}
static void draw_qualities(int y)        /* the black keys' qualities in this bank, two rows of five, latched lit */
{
    uint32_t i;
    uint16_t eff = qual_eff();
    for (i = 0; i < CHORD_BANK; i++) {
        int x = 16 + (int)(i % 5u) * 90, yy = y + (int)(i / 5u) * 26, on = eff >> i & 1u;
        if (on)
            vg_round(x, yy, 84, 22, 4, PG_COLOR[PG_CHORD]);
        else
            vg_frame(x, yy, 84, 22, 1, C_FAINT);
        vg_text(CHORD_TYPE_NAMES[set.bank * CHORD_BANK + i], x + 42, yy + 3, VG_MED, on ? C_BG : C_DIM, 1);
    }
}
static void draw_sounding(int y)         /* the chord's name, or the notes sounding */
{
    if (set.mode && ch.name[0])
        vg_text(ch.name, 240, y, VG_BIG | VG_X2, ch.n ? C_INK : 0x5A5E68, 1);   /* (VG_HUGE: capitals, digits only) */
    else {
        char t[64];
        uint32_t n, k = 0;
        t[0] = 0;
        for (n = 0; n < 128u && k < 8u; n++)
            if (sounding[n]) {
                char nb[4];
                if (k++)
                    cat(t, " ");
                cat(t, NOTE_N[n % 12u]);
                cat(t, vg_itoa((int)(n / 12u) - 1, nb));
            }
        vg_text(t[0] ? t : (set.mode ? "A WHITE KEY: A CHORD" : "PLAY"), 240, y + 12, VG_BIG, t[0] ? C_INK : C_FAINT, 1);
    }
}

void vibe_on_draw(uint16_t *fb)
{
    uint32_t i;
    char t[48];
    (void)fb;
    if (help) {
        vg_controls("MVANE YARD", HELP, (int)(sizeof HELP / sizeof HELP[0] / 2u));
        return;
    }
    draw_frame();
    if (page == PG_SOUND) {              /* YARD's sound: its name, its three pages of values (F6) */
        static const char *const SUB[3] = {"TONE ATTACK DECAY RELEASE", "DETUNE SUB RES PLUCK", "WAVE PW NOISE FILTER"};
        vg_text(SYNTH_SOUNDS[set.sound].name, 16, 72, VG_BIG | VG_X2, C_INK, 0);
        vg_text(SYNTH_SOUNDS[set.sound].mono ? "YARD, MONO" : "YARD", 16, 124, VG_MED, C_DIM, 0);
        for (i = 0; i < 3u; i++) {
            int y = 150 + (int)i * 18;
            vg_text(SUB[i], 16, y, VG_SMALL, i == more % 3u ? PG_COLOR[PG_SOUND] : C_FAINT, 0);
        }
        vg_text("A: THE SOUND   F6: THE NEXT FOUR", 464, 186, VG_SMALL, C_DIM, 2);
    } else if (page == PG_PLAY) {        /* what sounds, the scale on the keyboard */
        draw_sounding(66);
        keyboard(16, 128, 12, 70, 36u + 12u * (uint32_t)vibe_keys_octave_get() - 12u, 37u, PG_COLOR[PG_PLAY]);
    } else if (page == PG_CHORD) {       /* the chord, its voicing, the qualities */
        draw_sounding(66);
        keyboard(16, 118, 12, 34, 24u, 37u, PG_COLOR[PG_CHORD]);
        draw_qualities(158);
    } else if (page == PG_SCALE) {       /* the number, its name, its twelve tones from the key */
        uint16_t m = scale_mask();
        const char *nm = scale_name();
        vg_text(vg_itoa(set.pcs & 0xFFFu, t), 16, 66, VG_HUGE, C_INK, 0);
        vg_text(nm ? nm : "(NO NAME)", 464, 76, VG_BIG, nm ? PG_COLOR[PG_SCALE] : C_FAINT, 2);
        for (i = 0; i < 12u; i++) {
            int x = 16 + (int)i * 38, on = m >> i & 1u;
            if (on)
                vg_round(x, 140, 34, 34, 5, i ? C_INK : PG_COLOR[PG_SCALE]);
            else
                vg_frame(x, 140, 34, 34, 1, C_FAINT);
            vg_text(NOTE_N[(set.tonic + i) % 12u], x + 17, 148, VG_MED, on ? C_BG : C_DIM, 1);
        }
        vg_text("A BIT EACH TONE ABOVE THE KEY: C MAJOR IS 2741    B: THE LIST", 16, 182, VG_SMALL, C_DIM, 0);
    } else {                             /* ARP, SEQ: the steps to come */
        vg_text(page == PG_ARP ? "THE ARP" : "RECORDINGS", 16, 72, VG_BIG | VG_X2, C_FAINT, 0);
        vg_text(page == PG_ARP ? "A STEP TO COME: ON THE AUDIO'S CLOCK, THE DAW'S TEMPO" : "A STEP TO COME: THE RECORDER, LOOPS, OVERDUB",
                16, 130, VG_MED, C_DIM, 0);
        draw_sounding(150);
    }
    draw_values();
    draw_fkeys();
}
