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
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
#include "vibe_dsp.h"
#include "vibe_gfx.h"
/* YARD as the FM-1 computes it: the SDK builds with -ffast-math, the FM-1 without it and without fused multiply-adds;
 * precise maths for YARD's code, so the two render the same samples (tools/vibe_cmp.c) */
#pragma clang fp reassociate(off) contract(off)
#include "../../app/synth.c"
#pragma clang fp reassociate(on) contract(fast)
#include "../../core/chords.c"
#include "../../core/scale_names.h"
#include "../../app/settings.h"
#include "../../core/rec.c"

VIBE_PARALLEL_AUDIO

/* ---- events to the audio: MIDI messages, one writer (the events), one reader (the audio) ---- */
#define QN 512u
#define Q_SOUND 0xF0u                    /* (not MIDI) d1: YARD's sound to load */
#define Q_MODE 0xF1u                     /* d1: mono, d2: breath (synth_mode) */
#define Q_REC 0xF2u                      /* (the recorder, step 7) d1: 0 REC tapped, 1 REC held */
#define Q_PLAY 0xF3u                     /* d1: 0 stop, 1 play / stop, 2 stop only */
#define Q_LOOP 0xF4u                     /* d1: LOOP off / on */
#define Q_SCALE 0xF5u                    /* d1, d2: the scale's number (7 + 5 bits) */
#define Q_TONIC 0xF6u                    /* d1: the key */
#define Q_TAKE 0xF7u                     /* d1: the file image to play from (0, 1), d2: 1 none (an empty slot) */
#define Q_EXPORT 0xF8u                   /* d1: the file image to copy the recording into (and play from) */
static uint8_t q_st[QN], q_d1[QN], q_d2[QN], q_rec[QN];
static uint8_t q_live = 1;               /* (send's: 0 while a sound loads, which the history does not keep) */
static volatile uint32_t qw, qr;
static void send(uint32_t st, uint32_t d1, uint32_t d2)   /* (control flow) full: dropped */
{
    uint32_t w = qw;
    if (w - qr >= QN)
        return;
    q_st[w % QN] = (uint8_t)st, q_d1[w % QN] = (uint8_t)(d1 & 0x7Fu), q_d2[w % QN] = (uint8_t)(d2 & 0x7Fu);
    q_rec[w % QN] = (uint8_t)(st < 0xF0u && q_live);   /* (what was played goes into the history) */
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
/* (the bank, below: yard.ysb) */
#define YB_N 32u
#define US_N 16u
#define YB_V1 1032u
#define YB_V2 3088u
#define YB_V3 3352u
#define YB_SOUND(i) (8u + 32u * (i))
#define YB_SCALE(i) (YB_V2 + 8u + 16u * (i))
static uint8_t bank[YB_V3];
static uint32_t us_pcs(uint32_t i);
static void bank_name(const uint8_t *b, char *out);
static const char *scale_name(void)      /* the list's name, else a known one, else 0 */
{
    uint32_t i;
    for (i = 0; i < CHORD_NSCALES; i++)
        if ((CHORD_SCALES[i].mask | 1u) == scale_mask())
            return CHORD_SCALES[i].name;
    for (i = 0; i < US_N; i++)           /* yours, by the name the page gave it */
        if ((us_pcs(i) & 0x8000u) && ((us_pcs(i) & 0xFFFu) | 1u) == scale_mask() && bank[YB_SCALE(i)]) {
            static char nm[13];
            bank_name(bank + YB_SCALE(i), nm);
            return nm;
        }
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

/* ---- your sounds and scales (step 6): the FM-1's bank, yard.ysb, version 3, byte for byte (app.c yard_bank_t,
 * tools/ysb.py): "MVYB", version, 32; 32 YARD sounds of 32 bytes (a name, flags: 1 used, 2 mono, the thirteen
 * values); "MVCB", 32; 32 Clipper sounds of 64 (kept as they are, for when Clipper comes); "MVSB", 16; 16 scales of
 * 16 (a name, 0x8000 | the number). A bank from 0.10 or 0.11 grows to version 3, as on the FM-1 ---- */
static void le32(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v, b[1] = (uint8_t)(v >> 8), b[2] = (uint8_t)(v >> 16), b[3] = (uint8_t)(v >> 24); }
static uint32_t rd32(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static void bank_load(void)
{
    int n = vibe_store_read("yard.ysb", bank, (int)YB_V3);
    uint32_t v = n >= (int)YB_V1 ? bank[4] : 0u;
    if (n < (int)YB_V1 || rd32(bank) != 0x4259564Du || v < 1u || v > 3u)
        memset(bank, 0, sizeof bank), v = 0;
    if (v < 2u || n < (int)YB_V2) {      /* (0.10's, or none: no Clipper part yet) */
        memset(bank + YB_V1, 0, YB_V3 - YB_V1);
        le32(bank + YB_V1, 0x4243564Du), bank[YB_V1 + 4u] = YB_N;
    }
    if (v < 3u || n < (int)YB_V3) {      /* (0.11's: no scales yet) */
        memset(bank + YB_V2, 0, YB_V3 - YB_V2);
        le32(bank + YB_V2, 0x4253564Du), bank[YB_V2 + 4u] = US_N;
    }
    le32(bank, 0x4259564Du), bank[4] = 3, bank[5] = YB_N;
}
static int bank_save(void) { return vibe_store_write("yard.ysb", bank, (int)YB_V3); }
static int user_used(uint32_t slot) { return slot < YB_N && (bank[YB_SOUND(slot) + 12u] & 1u); }
static void bank_name(const uint8_t *b, char *out)   /* 12 bytes, NUL-padded, ASCII (others as '?') */
{
    uint32_t k;
    for (k = 0; k < 12u && b[k]; k++)
        out[k] = b[k] >= ' ' && b[k] <= '~' ? (char)b[k] : '?';
    out[k] = 0;
}
static uint32_t us_pcs(uint32_t i) { const uint8_t *b = bank + YB_SCALE(i) + 12u; return (uint32_t)(b[0] | b[1] << 8); }

/* the sounds A steps through: YARD's thirteen (0..12), then yours (13 + the slot) */
#define SOUNDS_N (SYNTH_NSOUNDS + YB_N)
static int sound_exists(uint32_t n) { return n < SYNTH_NSOUNDS || user_used(n - SYNTH_NSOUNDS); }
static void sound_of(uint32_t n, uint8_t *p, uint8_t *mono, char *name)   /* its values, mono, name */
{
    uint32_t i;
    if (n < SYNTH_NSOUNDS) {
        for (i = 0; i < SP_N; i++)
            p[i] = SYNTH_SOUNDS[n].p[i];
        *mono = SYNTH_SOUNDS[n].mono;
        strcpy(name, SYNTH_SOUNDS[n].name);
    } else {
        const uint8_t *b = bank + YB_SOUND(n - SYNTH_NSOUNDS);
        for (i = 0; i < SP_N; i++)
            p[i] = b[13u + i] > 127u ? 127u : b[13u + i];
        *mono = (uint8_t)((b[12] >> 1) & 1u);
        bank_name(b, name);
        if (!name[0])
            strcpy(name, "USER");
    }
}
static const char *sound_name(void)
{
    static char nm[16];
    uint8_t p[SP_N], mono;
    sound_of(set.sound, p, &mono, nm);
    return nm;
}
static int sound_mono(void) { uint8_t p[SP_N], mono; char nm[16]; sound_of(set.sound, p, &mono, nm); return mono; }

/* the scales B steps through: the named list, bright to dark, then yours (in slot order) */
static uint32_t scale_list(uint32_t *nums)   /* the numbers, in order; how many */
{
    uint32_t n = 0, i;
    for (i = 0; i < CHORD_NSCALES; i++)
        nums[n++] = CHORD_SCALES[i].mask;
    for (i = 0; i < US_N; i++)
        if (us_pcs(i) & 0x8000u)
            nums[n++] = us_pcs(i) & 0xFFFu;
    return n;
}

/* a short message on top, for a moment (KEEP, a full list) */
static char msg[40];
static uint64_t msg_until;
static float srate = 48000.0f;
static void say(const char *a, const char *b)
{
    msg[0] = 0;
    cat(msg, a);
    if (b && b[0]) {
        cat(msg, ": ");
        cat(msg, b);
    }
    msg_until = vibe_frames() + (uint64_t)(srate * 1.6f);
}

static void param_set(uint32_t p, int32_t v)
{
    set.val[p] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    send(0xB0u, SP_CC[p], set.val[p]);
}
static void sound_pick(uint32_t n)       /* one of the list's, its values sent as YARD's controllers (as the FM-1) */
{
    uint32_t i, glide = set.val[SP_GLIDE];
    uint8_t p[SP_N], mono;
    char nm[16];
    set.sound = sound_exists(n) ? n : 0u;
    sound_of(set.sound, p, &mono, nm);
    q_live = 0;                          /* (a sound's values: not the playing, not kept in the history) */
    for (i = 0; i < SP_N; i++)
        param_set(i, p[i]);
    q_live = 1;
    if (set.started)                     /* the PLAY page's GLIDE is the player's: every sound gets it (as the FM-1) */
        param_set(SP_GLIDE, (int32_t)glide);
    set.started = 1;
    send(Q_MODE, (set.play & 1u) || mono, set.play >> 1);
}
static void sound_step(int d)            /* A: the next or previous that exists */
{
    uint32_t n = set.sound, k;
    for (k = 0; k < SOUNDS_N; k++) {
        n = (n + SOUNDS_N + (uint32_t)d) % SOUNDS_N;
        if (sound_exists(n))
            break;
    }
    sound_pick(n);
}
static void play_set(uint32_t p)
{
    all_off();
    set.play = (uint8_t)(p & 3u);
    send(Q_MODE, (set.play & 1u) || sound_mono(), set.play >> 1);
}
static void scale_step(int d)            /* B: the list's next or previous (the named, then yours) */
{
    uint32_t nums[CHORD_NSCALES + US_N], i, n = scale_list(nums);
    int32_t at = -1;
    for (i = 0; i < n; i++)
        if ((nums[i] | 1u) == scale_mask())
            at = (int32_t)i;
    if (at < 0)
        at = d > 0 ? (int32_t)set.scale - 1 : (int32_t)set.scale + 1;
    set.scale = (uint8_t)((((at + d) % (int32_t)n) + (int32_t)n) % (int32_t)n);
    set.pcs = (uint16_t)nums[set.scale];
    harmony_changed();
}
static void keep_sound(void)             /* F4 on SOUND: these values kept as yours (as the FM-1's FX + SAVE) */
{
    uint32_t slot = set.sound >= SYNTH_NSOUNDS ? set.sound - SYNTH_NSOUNDS : YB_N, i;
    char nm[16];
    for (i = 0; i < YB_N && slot == YB_N; i++)
        if (!user_used(i))
            slot = i;
    if (slot >= YB_N) {
        say("YARD", "32 SOUNDS: FULL");
        return;
    }
    if (!user_used(slot)) {              /* a new one: USER n */
        uint8_t *b = bank + YB_SOUND(slot);
        memset(b, 0, 32);
        nm[0] = 0;
        cat(nm, "USER ");
        vg_itoa((int)slot + 1, nm + 5);
        memcpy(b, nm, strlen(nm));
        b[12] = (uint8_t)(1u | (sound_mono() ? 2u : 0u));
    }
    for (i = 0; i < SP_N; i++)
        bank[YB_SOUND(slot) + 13u + i] = set.val[i];
    if (bank_save()) {
        say("YARD", "NOT KEPT");
        return;
    }
    set.sound = SYNTH_NSOUNDS + slot;
    bank_name(bank + YB_SOUND(slot), nm);
    say("KEPT", nm);
}
static void keep_scale(void)             /* F4 on SCALE: the number kept in your list, once (as SEL + SAVE) */
{
    uint32_t i, slot = US_N, num = set.pcs & 0xFFFu;
    char nb[8];
    for (i = 0; i < US_N; i++)
        if ((us_pcs(i) & 0x8000u) && ((us_pcs(i) & 0xFFFu) | 1u) == scale_mask()) {
            say("SCALE", "ALREADY KEPT");
            return;
        }
    for (i = 0; i < CHORD_NSCALES; i++)
        if ((CHORD_SCALES[i].mask | 1u) == scale_mask()) {
            say("SCALE", "IN THE LIST");
            return;
        }
    for (i = 0; i < US_N && slot == US_N; i++)
        if (!(us_pcs(i) & 0x8000u))
            slot = i;
    if (slot >= US_N) {
        say("SCALES", "16 KEPT: FULL");
        return;
    }
    memset(bank + YB_SCALE(slot), 0, 16);
    bank[YB_SCALE(slot) + 12u] = (uint8_t)num, bank[YB_SCALE(slot) + 13u] = (uint8_t)(0x80u | num >> 8);
    if (bank_save()) {
        say("SCALE", "NOT KEPT");
        return;
    }
    say("SCALE KEPT", vg_itoa((int)num, nb));
}

/* (the recorder's, below: the slot, LOOP, and what the SEQ page's controls do) */
static uint8_t slot, loop_on;
static void slot_step(int d);
static void slot_save(void);
static void send_loop(void);
static void recorder_init(void);

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
    } else if (page == PG_SEQ) {
        if (i == 0) {
            *label = "SLOT";
            vg_itoa(slot + 1, val);
        } else if (i == 1) {
            *label = "LOOP";
            strcpy(val, loop_on ? "ON" : "OFF");
        }
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
    } else if (page == PG_SEQ) {
        if (i == 0)
            slot_step(d);
        else if (i == 1) {
            loop_on = (uint8_t)(d > 0);
            send_loop();
        }
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
    static const char *const F[6] = {"MODE", "HOLD", "PLAY", "KEEP", "", "MORE"};
    int i;
    for (i = 0; i < PG_N; i++)
        vibe_label(VB_K1 + i, PG_NAME[i]);
    for (i = 0; i < 6; i++)
        vibe_label(VB_F1 + i, F[i]);
}
static volatile uint32_t pub_playing, pub_ov;   /* (the recorder's, below) */
static void lights(void)
{
    int i;
    vibe_led_button(VB_PLAY, 0x4FD88A, pub_playing ? 1.0f : 0.0f);
    vibe_led_button(VB_REC, 0xFF3030, pub_ov ? 1.0f : 0.0f);
    for (i = 0; i < PG_N; i++)
        vibe_led_button(VB_K1 + i, PG_COLOR[i], i == page ? 1.0f : 0.15f);
    vibe_led_button(VB_F1, 0xF2F2F2, set.mode ? 1.0f : 0.15f);
    vibe_led_button(VB_F2, 0xFF7A00, set.hold ? 1.0f : 0.0f);
}

/* ---- the DAW (step 5): the parameters it automates and a controller maps; their ids never change ----
 * Editor tab A: YARD (the sound, its values, the play mode); tab B: the harmony (NOTES / CHORDS, the key, QUANT, the
 * scale's number, the chord's voicing). The panel and the DAW stay in step: a change from the DAW comes as
 * VIBE_EV_PARAM and is applied as the panel would; after every event params_push tells the DAW what moved here. */
#define SOUNDS_MAX 45u                   /* (YARD's thirteen and the 32 slots: the arrays' size; SOUNDS_N in use) */
static char sound_names_buf[SOUNDS_MAX][16];
static const char *SOUND_NAMES[SOUNDS_MAX];  /* the DAW's choices: YARD's thirteen, then your 32 slots (as they were at
                                            * the start: "-" an empty one) */
static const char *const WAVE_C[3] = {"SAW", "PULSE", "TRI"}, *const FMODE_C[3] = {"LOW", "BAND", "HIGH"};
static const char *const MODE_C[2] = {"NOTES", "CHORDS"}, *const ONOFF_C[2] = {"OFF", "ON"};
static const char *const BANK_C[2] = {"SCALE", "QUALITY"}, *const VOICES_C[4] = {"3", "4", "5", "6"};
enum { PR_SOUND, PR_PLAY, PR_SP0, PR_MODE = PR_SP0 + SP_N, PR_KEY, PR_QUANT, PR_PCS, PR_STYLE, PR_VOICES, PR_BASS,
       PR_BANK, PR_HOLD, PR_N };
static vibe_param PRM[PR_N];
static const char *GRP[PR_N];
static void params_declare(void)
{
    static const char *const SP_ID[SP_N] = {"tone", "release", "glide", "attack", "decay", "res", "detune", "sub",
                                            "pluck", "wave", "pw", "noise", "filter"};
    static const char *const SP_NM[SP_N] = {"Tone", "Release", "Glide", "Attack", "Decay", "Resonance", "Detune", "Sub",
                                            "Pluck", "Wave", "Pulse width", "Noise", "Filter"};
    static const char *const SP_GR[SP_N] = {"A · TONE", "A · ENVELOPE", "A · PLAY", "A · ENVELOPE", "A · ENVELOPE",
                                            "A · TONE", "A · OSCILLATORS", "A · OSCILLATORS", "A · ENVELOPE",
                                            "A · OSCILLATORS", "A · OSCILLATORS", "A · OSCILLATORS", "A · TONE"};
    uint32_t i;
    for (i = 0; i < SOUNDS_N && i < SOUNDS_MAX; i++) {
        uint8_t pp[SP_N], mono;
        if (sound_exists(i))
            sound_of(i, pp, &mono, sound_names_buf[i]);
        else
            strcpy(sound_names_buf[i], "-");
        SOUND_NAMES[i] = sound_names_buf[i];
    }
    PRM[PR_SOUND] = (vibe_param){"sound", "Sound", 0, SOUNDS_N - 1, 0, "", SOUNDS_N, (const char *const *)SOUND_NAMES};
    GRP[PR_SOUND] = "A · PLAY";
    PRM[PR_PLAY] = (vibe_param){"play", "Play mode", 0, 3, 0, "", 4, PLAY_N}, GRP[PR_PLAY] = "A · PLAY";
    for (i = 0; i < SP_N; i++) {
        PRM[PR_SP0 + i] = (vibe_param){SP_ID[i], SP_NM[i], 0, 127, (float)SYNTH_SOUNDS[0].p[i], "", 128, 0};
        if (i == SP_WAVE)
            PRM[PR_SP0 + i] = (vibe_param){SP_ID[i], SP_NM[i], 0, 2, 0, "", 3, WAVE_C};
        if (i == SP_FMODE)
            PRM[PR_SP0 + i] = (vibe_param){SP_ID[i], SP_NM[i], 0, 2, 0, "", 3, FMODE_C};
        GRP[PR_SP0 + i] = SP_GR[i];
    }
    PRM[PR_MODE] = (vibe_param){"mode", "Mode", 0, 1, 1, "", 2, MODE_C}, GRP[PR_MODE] = "B · HARMONY";
    PRM[PR_KEY] = (vibe_param){"key", "Key", 0, 11, 0, "", 12, NOTE_N}, GRP[PR_KEY] = "B · HARMONY";
    PRM[PR_QUANT] = (vibe_param){"quant", "Quant", 0, 2, 1, "", 3, QUANT_N}, GRP[PR_QUANT] = "B · HARMONY";
    PRM[PR_PCS] = (vibe_param){"scale", "Scale (number)", 0, 4095, 2741, "", 4096, 0}, GRP[PR_PCS] = "B · HARMONY";
    PRM[PR_STYLE] = (vibe_param){"voicing", "Voicing", 0, CHORD_STYLES - 1, CHORD_OPEN, "", CHORD_STYLES, CHORD_STYLE_NAMES};
    PRM[PR_VOICES] = (vibe_param){"voices", "Voices", 3, 6, 5, "", 4, VOICES_C};
    PRM[PR_BASS] = (vibe_param){"bass", "Bass", 0, 1, 1, "", 2, ONOFF_C};
    PRM[PR_BANK] = (vibe_param){"bank", "Black keys", 0, 1, 1, "", 2, BANK_C};
    PRM[PR_HOLD] = (vibe_param){"hold", "Hold", 0, 1, 0, "", 2, ONOFF_C};
    for (i = PR_STYLE; i <= PR_HOLD; i++)
        GRP[i] = "B · CHORD";
    vibe_params(PRM, PR_N);
    vibe_param_groups(GRP);
}
static float param_now(uint32_t i)       /* a parameter's value as the panel has it */
{
    if (i >= PR_SP0 && i < PR_SP0 + SP_N) {
        uint32_t p = i - PR_SP0, v = set.val[p];
        return p == SP_WAVE || p == SP_FMODE ? (float)SYNTH_WAVE(v) : (float)v;
    }
    switch (i) {
    case PR_SOUND: return (float)set.sound;
    case PR_PLAY: return (float)set.play;
    case PR_MODE: return (float)set.mode;
    case PR_KEY: return (float)set.tonic;
    case PR_QUANT: return (float)set.quant;
    case PR_PCS: return (float)(set.pcs & 0xFFFu);
    case PR_STYLE: return (float)set.style;
    case PR_VOICES: return (float)set.voices;
    case PR_BASS: return (float)set.bass;
    case PR_BANK: return (float)set.bank;
    default: return (float)set.hold;
    }
}
static void send_scale(void);
static void params_push(void)            /* what moved on the panel, to the DAW (and the key and scale to the player) */
{
    uint32_t i;
    send_scale();
    for (i = 0; i < PR_N; i++)
        if (vibe_param_get((int)i) != param_now(i))
            vibe_param_set((int)i, param_now(i));
}
static void param_apply(uint32_t i, float value)   /* the DAW (or a mapped controller) moved one: as the panel would
                                                    * (the event carries the value the host stored) */
{
    int v = (int)(value + 0.5f);
    if (i >= PR_SP0 && i < PR_SP0 + SP_N) {
        uint32_t p = i - PR_SP0;
        param_set(p, p == SP_WAVE || p == SP_FMODE ? (v <= 0 ? 0 : v == 1 ? 64 : 127) : v);
        return;
    }
    switch (i) {
    case PR_SOUND: if ((uint32_t)v != set.sound) sound_pick((uint32_t)v); break;
    case PR_PLAY: if ((uint32_t)v != set.play) play_set((uint32_t)v); break;
    case PR_MODE: if ((uint32_t)v != set.mode) all_off(), set.mode = (uint8_t)(v != 0); break;
    case PR_KEY: set.tonic = (uint8_t)((uint32_t)v % 12u), harmony_changed(); break;
    case PR_QUANT: set.quant = (uint8_t)(v < 0 ? 0 : v > 2 ? 2 : v); break;
    case PR_PCS: set.pcs = (uint16_t)(v < 0 ? 0 : v > 4095 ? 4095 : v), harmony_changed(); break;
    case PR_STYLE: set.style = (uint8_t)((uint32_t)v % CHORD_STYLES), harmony_changed(); break;
    case PR_VOICES: set.voices = (uint8_t)(v < 3 ? 3 : v > 6 ? 6 : v), harmony_changed(); break;
    case PR_BASS: set.bass = (uint8_t)(v != 0), harmony_changed(); break;
    case PR_BANK:
        set.bank = (uint8_t)(v != 0);
        set.type = (uint8_t)(set.bank * CHORD_BANK + set.type % CHORD_BANK);
        harmony_changed();
        break;
    default:
        set.hold = (uint8_t)(v != 0);
        if (!set.hold && ch.key >= 0 && !key_role[ch.key & 127])
            chord_release();
    }
}

/* ---- the settings (step 6): the FM-1's record (app/settings.h), the file "settings": what the two share is read
 * and written (the mode, the key, the scale, the sound and its values, QUANT, the chord's voicing, the play mode, the
 * octave); the FM-1's own (PHONES MAX, the ARP's, MPE OUT ...) kept as they came. Saved a second after a change, and
 * at once when the host keeps the state (VIBE_EV_SAVE: a DAW's project, a preset) ---- */
static settings_t rec;
static uint8_t set_dirty;
static uint64_t set_dirty_at;
static void settings_save(void)
{
    int o = vibe_keys_octave_get() - 1;
    uint32_t i;
    rec.magic = SET_MAGIC;
    rec.mode = set.mode, rec.tonic = set.tonic, rec.octave = (int8_t)(o < -2 ? -2 : o > 2 ? 2 : o);
    rec.scale = set.scale, rec.pcs = (uint16_t)(0x8000u | (set.pcs & 0xFFFu));
    for (i = 0; i < SP_N; i++)
        rec.p[i] = set.val[i];
    rec.type = set.type, rec.style = set.style, rec.bass = set.bass, rec.voices = set.voices, rec.bank = set.bank;
    rec.play = set.play, rec.quant = set.quant;
    rec.sound = (uint16_t)(set.sound < SYNTH_NSOUNDS ? 0x4000u | set.sound : 0x4100u | (set.sound - SYNTH_NSOUNDS));
    rec.page = (uint8_t)(page == PG_SOUND);
    rec.slot = slot, rec.loop = loop_on;
    vibe_store_write("settings", &rec, (int)sizeof rec);
    set_dirty = 0;
}
static void settings_touch(void) { set_dirty = 1, set_dirty_at = vibe_frames(); }
static void settings_load(void)          /* (the defaults are set; a good record over them) */
{
    uint32_t i, bad = 0;
    int n = vibe_store_read("settings", &rec, (int)sizeof rec);
    if (n != (int)sizeof rec || rec.magic != SET_MAGIC) {
        memset(&rec, 0, sizeof rec);     /* (none, or an older FM-1's: the FM-1's first-start values for its own) */
        rec.usb_full = 1, rec.mpe_out = 1, rec.bend = 2, rec.phones = 20;
        rec.arp_rate = 3, rec.arp_gate = 50, rec.arp_oct = 1, rec.arp_bpm = 120;
        return;
    }
    for (i = 0; i < SP_N; i++)
        bad |= rec.p[i] > 127u;
    if (bad || rec.mode > 1u || rec.tonic > 11u || !(rec.pcs & 0x8000u) || rec.quant > 2u || rec.voices < 3u
        || rec.voices > CHORD_MAX || rec.style >= CHORD_STYLES || rec.bass > 1u || rec.bank > 1u
        || rec.type >= CHORD_NTYPES || rec.play > 3u)
        return;
    set.mode = rec.mode, set.tonic = rec.tonic, set.pcs = rec.pcs & 0xFFFu, set.scale = rec.scale;
    set.quant = rec.quant, set.voices = rec.voices, set.style = rec.style, set.bass = rec.bass, set.bank = rec.bank;
    set.type = rec.type, set.play = rec.play;
    if ((rec.sound & 0xFF00u) == 0x4000u && (rec.sound & 0xFFu) < SYNTH_NSOUNDS)
        set.sound = rec.sound & 0xFFu;
    else if ((rec.sound & 0xFF00u) == 0x4100u && user_used(rec.sound & 0xFFu))
        set.sound = SYNTH_NSOUNDS + (rec.sound & 0xFFu);
    else
        set.sound = 0;                   /* (a Clipper sound on the FM-1: YARD's SAW here, until Clipper comes) */
    sound_pick(set.sound);
    for (i = 0; i < SP_N; i++)           /* the values as they were left (a sound edited, not kept) */
        param_set(i, rec.p[i]);
    vibe_keys_octave(rec.octave + 1);
    page = rec.page ? PG_SOUND : PG_PLAY;
    slot = (uint8_t)(rec.slot & 7u), loop_on = (uint8_t)(rec.loop != 0);
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
    srate = sample_rate;
    page = PG_PLAY;
    bank_load();
    sound_pick(0);
    settings_load();
    recorder_init();
    labels();
    lights();
    params_declare();
    params_push();
    vf_clicks(1);                        /* (SDK 18: in the plugin and the browser, a click on a control too) */
}

static const char *const HELP[] = {
    "KEYS",          "NOTES: notes. CHORDS: white keys the degrees, black keys the qualities (latched; together: combined)",
    "K1 - K6",       "the pages: SOUND, PLAY, CHORD, ARP, SCALE, SEQ",
    "ENCODERS 1 - 4", "the page's four values",
    "A",             "the sound: YARD's thirteen, then yours (F4 on SOUND keeps one)",
    "F4",            "KEEP: on SOUND these values as yours; on SCALE the number in your list",
    "B",             "the scale, from bright to dark",
    "F1  F2  F3",    "NOTES / CHORDS, HOLD, POLY / MONO / +BREATH",
    "F6",            "MORE: the page's next four (SOUND)",
    "MIDI / DAW",    "notes on their channels (MPE); QUANT FULL: to the scale",
    "REC  PLAY  STOP", "REC: the last phrase (held: everything); while it loops: overdub (held: undo). SEQ: the slot, LOOP, SAVE (F4)",
    "DATA + ENTER",  "every control on the screen: DATA walks to it, ENTER presses it or edits it (DATA turns)",
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

#define FX_QUAL 100                      /* (FOCUS's buttons: the qualities, the scale's tones) */
#define FX_TONE 200
static void event(const vibe_event *e);
void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_SAVE) {       /* the host keeps the state now: everything, in this call */
        settings_save();
        bank_save();
        return;
    }
    if (e->type == VIBE_EV_PARAM) {      /* the DAW's: applied, then what it moved besides (a sound: its values) */
        param_apply(e->id, e->value);
        lights();
    } else
        event(e);
    params_push();
    if (e->type == VIBE_EV_PARAM || e->type == VIBE_EV_ENCODER || (e->type == VIBE_EV_BUTTON && e->down))
        settings_touch();
}
static void rec_press(int down);
static void event(const vibe_event *e)
{
    if (e->type == VIBE_EV_BUTTON && !e->down && e->id == VB_REC) {
        rec_press(0);
        return;
    }
    {                                    /* FOCUS first (SDK 10): DATA walks, ENTER presses */
        int id, r = vf_event(e, &id);
        if (r == VF_PRESS && id >= FX_QUAL && id < FX_QUAL + (int)CHORD_BANK) {
            qual_key((uint32_t)(id - FX_QUAL), 1);
            qual_key((uint32_t)(id - FX_QUAL), 0);
        } else if (r == VF_PRESS && id > FX_TONE && id < FX_TONE + 12) {
            set.pcs = (uint16_t)((set.pcs ^ (1u << (id - FX_TONE))) | 1u);
            harmony_changed();
        }
        if (r != VF_NO)
            return;
    }
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
            sound_step(d);
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
        else if (e->id == VB_F4 && page == PG_SOUND)
            keep_sound();
        else if (e->id == VB_F4 && page == PG_SCALE)
            keep_scale();
        else if (e->id == VB_F4 && page == PG_SEQ)
            slot_save();
        else if (e->id == VB_PLAY)
            send(Q_PLAY, 1, 0);
        else if (e->id == VB_STOP)
            send(Q_PLAY, 2, 0);
        else if (e->id == VB_REC)
            rec_press(1);
        else if (e->id == VB_F6)
            more = (uint8_t)((more + 1u) % (page == PG_SOUND ? 3u : 1u));
        lights();
    }
}

/* ---- the recorder (step 7): the FM-1's (core/rec.c), all in the audio flow, which alone writes the history: what
 * the events queue is played and kept there, on the audio's own clock (frames, as ms); the player plays a recording
 * there too, 32 frames at a time, in the key and scale now set (a note to the same degree, as on the FM-1); LOOP, and
 * overdub (REC while it loops: from the top of the next pass the recording's own playing goes into the history with
 * what is played over it, and each pass becomes the recording; REC held, the last layer undone). The events only send
 * commands; the audio publishes what the screen shows (single 32-bit values). The recordings are files take1..8.mvt,
 * as on the FM-1: the events read one into a file image and hand it over; to keep the one sounding, the audio copies
 * it into a file image (then plays from there, the history free again) and the events write that ---- */
#define HIST_WORDS (1u << 15)            /* 32,768 events, however long they took (the FM-1's) */
#define TAKE_MAX (HIST_WORDS + REC_PRE)
#define IMG_WORDS (REC_HEAD_WORDS + TAKE_MAX)
#define PHRASE_GAP_MS 3000u
#define SLEEP_MS 4000u
static uint32_t hist_buf[HIST_WORDS];
static uint32_t img[2][IMG_WORDS];       /* file images: a slot's file read in, or the recording copied out */
static rec_ring hist;
static rec_take take, ov_undo_take;
static rec_player player;
static uint64_t a_frames;                /* (audio) the frames done */
static uint32_t a_ms;
static uint8_t a_loop, a_tonic, a_take_img = 2;   /* (the file image the recording plays from: 2, none / the history) */
static uint16_t a_pcs = 2741u;
static struct { uint8_t armed, on, ending, has_undo, layers; uint32_t mark; } ov;
static uint16_t pl_mask;
static uint8_t pl_tonic, pl_map[16][128];
/* published by the audio, read by the events and the screen */
static volatile uint32_t pub_count, pub_dur, pub_pos, pub_layers, pub_hist_ms, pub_full, pub_dirty, pub_img = 2,
    pub_export;

static uint32_t a_mask(void) { return (uint32_t)(a_pcs & 0xFFFu) | 1u; }
static uint32_t pcs_tone(uint32_t m, uint32_t j)   /* the j-th tone of a set, in semitones above its tonic */
{
    uint32_t k, n = 0;
    for (k = 0; k < 12u; k++)
        if (m >> k & 1u && n++ == j)
            return k;
    return 0;
}
static uint32_t take_note(uint32_t n)    /* (as app.c's) a recorded note in the key and scale now set */
{
    uint32_t mb = a_mask(), pc, k, i = 0, na = 0, nb = chord_scale_size((uint16_t)mb), ti = 0;
    int32_t shift = ((int32_t)a_tonic - (int32_t)pl_tonic + 12) % 12, out;
    if (mb == pl_mask && !shift)
        return n;
    if (shift > 6)
        shift -= 12;
    pc = (n + 12u - pl_tonic) % 12u;
    for (k = 0; k < 12u; k++)
        if (pl_mask >> k & 1u) {
            if (k <= pc)
                i = na, ti = k;
            na++;
        }
    out = (int32_t)n - (int32_t)ti + (int32_t)pcs_tone(mb, i * nb / na) + shift;
    return (uint32_t)(out < 0 ? 0 : out > 127 ? 127 : out);
}
static void play_emit(void *ctx, uint8_t st, uint8_t d1, uint8_t d2)   /* (audio) the recording's events */
{
    uint32_t ch = st & 15u, kind = st & 0xF0u, m = d1;
    (void)ctx;
    if (kind == 0x90u && d2)
        m = take_note(d1), pl_map[ch][d1] = (uint8_t)(m + 1u);
    else if (kind == 0x80u || kind == 0x90u || kind == 0xA0u) {
        if (pl_map[ch][d1])
            m = pl_map[ch][d1] - 1u;
        if (kind != 0xA0u)
            pl_map[ch][d1] = 0;
    }
    synth_event(st, m, d2);
    if (ov.on)                           /* overdub: the recording's own playing goes into the history too */
        rec_put(&hist, a_ms, st, (uint8_t)m, d2);
}
static void take_play(uint32_t at)
{
    pl_mask = (uint16_t)a_mask(), pl_tonic = a_tonic;
    memset(pl_map, 0, sizeof pl_map);
    rec_play_start(&player, &take, at);
}
static void ov_off(void)
{
    ov.armed = ov.on = ov.ending = 0;
    hist.sleep_ms = SLEEP_MS;
}
static void ov_lock(void)                /* the undo's words kept too, if in the history and older */
{
    if (ov.has_undo && ov_undo_take.ring && take.ring && ov_undo_take.start < take.start)
        hist.lock = ov_undo_take.start, hist.locked = 1;
}
static void a_capture(uint32_t gap)      /* REC: the last phrase (gap) or everything (0) */
{
    if (player.playing)
        rec_play_stop(&player, play_emit, 0);
    ov_off();
    ov.has_undo = 0, ov.layers = 0;
    if (rec_capture(&hist, a_ms, gap, &take)) {
        a_take_img = 2;
        pub_dirty = 1;
    }
}
static void a_undo(void)
{
    if (!ov.has_undo)
        return;
    rec_play_stop(&player, play_emit, 0);
    ov_off();
    take = ov_undo_take;
    ov.has_undo = 0;
    if (ov.layers)
        ov.layers--;
    if (take.ring)
        hist.lock = take.start, hist.locked = 1;
    pub_dirty = 1;
    take_play(a_ms);
}
static int ov_pass_end(uint32_t b)       /* a looping recording at its end: the pass becomes it if overdubbing */
{
    int got = 0;
    if (ov.on) {
        rec_take nt;
        if (hist.full || !rec_capture_from(&hist, ov.mark, take.duration_ms, &nt)) {
            ov_off();
            if (take.ring)
                hist.lock = take.start, hist.locked = 1;
        } else {
            ov_undo_take = take, ov.has_undo = 1;
            take = nt;
            a_take_img = 2;
            ov_lock();
            pub_dirty = 1, got = 1;
            if (ov.layers < 99u)
                ov.layers++;
        }
    }
    if (ov.ending)
        ov_off();
    if (ov.armed)
        ov.armed = 0, ov.on = 1, hist.sleep_ms = 0;
    if (ov.on)
        ov.mark = rec_mark(&hist, b);
    return got;
}
static void a_command(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st == Q_SOUND)
        sound_load(d1);
    else if (st == Q_MODE)
        synth_mode(d1, d2);
    else if (st == Q_SCALE)
        a_pcs = (uint16_t)(d1 | d2 << 7);
    else if (st == Q_TONIC)
        a_tonic = (uint8_t)(d1 % 12u);
    else if (st == Q_LOOP)
        a_loop = (uint8_t)(d1 != 0);
    else if (st == Q_REC) {
        if (player.playing && a_loop) {  /* while it loops: overdub (tapped), undo (held) */
            if (d1)
                a_undo();
            else if (ov.on)
                ov.ending ^= 1u;
            else
                ov.armed ^= 1u;
        } else
            a_capture(d1 ? 0u : PHRASE_GAP_MS);
    } else if (st == Q_PLAY) {
        if (player.playing || d1 == 2u) {
            if (player.playing)
                rec_play_stop(&player, play_emit, 0);
        } else if (d1 && take.count)
            take_play(a_ms);
    } else if (st == Q_TAKE || st == Q_EXPORT) {
        uint32_t b = d1 & 1u;
        if (player.playing)
            rec_play_stop(&player, play_emit, 0);
        ov_off();
        ov.has_undo = 0, ov.layers = 0;
        if (st == Q_EXPORT && take.count) {   /* the recording into file image b: then it plays from there */
            uint32_t i, head[REC_HEAD_WORDS];
            rec_file_head(&take, head);
            for (i = 0; i < REC_HEAD_WORDS; i++)
                img[b][i] = head[i];
            for (i = 0; i < take.count && i < TAKE_MAX; i++)
                img[b][REC_HEAD_WORDS + i] = rec_word(&take, i);
        }
        rec_unlock(&hist);
        memset(&take, 0, sizeof take);
        if ((st == Q_EXPORT || !d2) && rec_load(&take, (const uint8_t *)img[b], 4u * IMG_WORDS) != 0)
            take.count = 0;
        a_take_img = (uint8_t)(take.count ? b : 2u);
        pub_dirty = 0;
        if (st == Q_EXPORT) {
            VIBE_RELEASE();
            pub_export++;
        }
    }
}
static void a_publish(void)
{
    pub_count = take.count, pub_dur = take.duration_ms, pub_playing = player.playing;
    pub_pos = player.playing ? a_ms - player.start_ms : 0;
    pub_ov = ov.on ? 2u + ov.ending : ov.armed, pub_layers = ov.layers, pub_full = hist.full, pub_img = a_take_img;
    if ((a_frames & 0x3FFFu) < 256u)     /* (the history's reach: now and then) */
        pub_hist_ms = rec_span_ms(&hist, a_ms);
}
static void a_player(void)               /* the recording at a_ms: its events, and LOOP at its end */
{
    if (player.playing && !rec_play_poll(&player, a_ms, play_emit, 0)) {
        if (a_loop && take.duration_ms) {
            uint16_t m = pl_mask;
            uint8_t t = pl_tonic;
            uint32_t b = player.start_ms + take.duration_ms;
            int layer = ov_pass_end(b);
            take_play(b);
            if (!layer)
                pl_mask = m, pl_tonic = t;
            rec_play_poll(&player, a_ms, play_emit, 0);
        }
    }
    if (!player.playing && (ov.on || ov.armed))
        ov_off();
}

void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    static int32_t lr[2u * SYNTH_BLOCK];
    uint32_t w = qw, k;
    int i, at;
    (void)in;
    if (!hist.buf) {                     /* (the first block: the history starts) */
        rec_init(&hist, hist_buf, HIST_WORDS);
        hist.sleep_ms = SLEEP_MS;
    }
    a_ms = (uint32_t)(a_frames * 1000u / (uint64_t)srate);
    VIBE_ACQUIRE();
    for (k = qr; k != w; k++) {
        uint32_t st = q_st[k % QN], d1 = q_d1[k % QN], d2 = q_d2[k % QN];
        if (st >= 0xF0u)
            a_command(st, d1, d2);
        else {
            synth_event(st, d1, d2);
            if (q_rec[k % QN])           /* what was played: into the history */
                rec_put(&hist, a_ms, (uint8_t)st, (uint8_t)d1, (uint8_t)d2);
        }
    }
    VIBE_RELEASE();
    qr = w;
    for (at = 0; at < frames; at += 32) {   /* 32 frames at a time: the recording's events on time */
        int n = frames - at < 32 ? frames - at : 32;
        a_ms = (uint32_t)((a_frames + (uint64_t)at) * 1000u / (uint64_t)srate);
        a_player();
        synth_render(lr + 2 * at, (uint32_t)n, 4096u);   /* (MASTER at full: the host has its own volume) */
    }
    a_frames += (uint64_t)frames;
    a_publish();
    for (i = 0; i < frames; i++) {
        out_l[i] = (float)lr[2 * i] * (1.0f / 8388608.0f);
        out_r[i] = (float)lr[2 * i + 1] * (1.0f / 8388608.0f);
    }
}

/* ---- the recorder, the events' side: commands, the slots' files ---- */
static uint32_t slot_size[8];            /* each slot's file (bytes; under 16: empty) */
static uint8_t sent_tonic = 255, export_wait, export_buf;
static uint16_t sent_pcs = 0xFFFFu;
static uint32_t export_seen, slot_warn_at;
static uint64_t rec_down_at;
static uint8_t rec_down, rec_fired;
static void slot_name(uint32_t k, char *nm) { strcpy(nm, "take"); vg_itoa((int)k + 1, nm + 4); cat(nm, ".mvt"); }
static void send_loop(void) { send(Q_LOOP, loop_on, 0); }
static void send_scale(void)             /* the key and scale, to the player (a recording plays in them) */
{
    if ((set.pcs & 0xFFFu) != sent_pcs)
        sent_pcs = (uint16_t)(set.pcs & 0xFFFu), send(Q_SCALE, sent_pcs & 0x7Fu, sent_pcs >> 7);
    if (set.tonic != sent_tonic)
        sent_tonic = set.tonic, send(Q_TONIC, sent_tonic, 0);
}
static uint32_t free_img(void) { return pub_img == 0u ? 1u : 0u; }   /* (the file image the audio does not play from) */
static void slot_load(void)              /* the slot's file into a file image, handed to the audio */
{
    char nm[16];
    uint32_t b = free_img();
    int n;
    slot_name(slot, nm);
    n = vibe_store_read(nm, img[b], (int)(4u * IMG_WORDS));
    slot_size[slot] = n > 0 ? (uint32_t)n : 0u;
    VIBE_RELEASE();
    send(Q_TAKE, b, n < 16 ? 1u : 0u);
}
static void slot_step(int d)             /* SLOT: the next or previous (a recording not saved: warned first) */
{
    uint32_t now = (uint32_t)(vibe_frames() * 1000u / (uint64_t)srate);
    if (pub_dirty && now - slot_warn_at > 3000u) {
        slot_warn_at = now;
        say("NOT SAVED", "AGAIN: LOSE IT");
        return;
    }
    slot_warn_at = 0;
    slot = (uint8_t)((slot + 8u + (uint32_t)d) % 8u);
    slot_load();
}
static void slot_save(void)              /* SAVE (F4 on SEQ): the recording into its slot's file */
{
    if (!pub_count) {
        say("RECORDING", "EMPTY");
        return;
    }
    export_buf = (uint8_t)free_img();
    export_seen = pub_export;
    export_wait = 1;
    send(Q_EXPORT, export_buf, 0);
}
static void export_check(void)           /* (the events, the screen) the audio has copied it: written to its file */
{
    char nm[16];
    uint32_t count, bytes;
    if (!export_wait || pub_export == export_seen)
        return;
    VIBE_ACQUIRE();
    export_wait = 0;
    count = img[export_buf][2];
    bytes = 4u * (REC_HEAD_WORDS + (count < TAKE_MAX ? count : TAKE_MAX));
    slot_name(slot, nm);
    if (vibe_store_write(nm, img[export_buf], (int)bytes))
        say("SAVE", "FAILED");
    else {
        slot_size[slot] = bytes;
        say("SAVED", nm);
    }
}
static void rec_press(int down)          /* REC: tapped, the last phrase (while it loops: overdub); held, all (undo) */
{
    uint64_t now = vibe_frames();
    if (down) {
        rec_down = 1, rec_fired = 0, rec_down_at = now;
        return;
    }
    if (rec_down && !rec_fired)
        send(Q_REC, now - rec_down_at > (uint64_t)(srate * 0.7f), 0);
    rec_down = 0;
}
static void rec_hold_check(void)         /* (the screen) REC held 0.7 s: at once, as on the FM-1 */
{
    if (rec_down && !rec_fired && vibe_frames() - rec_down_at > (uint64_t)(srate * 0.7f)) {
        rec_fired = 1;
        send(Q_REC, 1, 0);
    }
}
static void recorder_init(void)
{
    uint32_t k;
    char nm[16];
    for (k = 0; k < 8u; k++) {
        int n;
        slot_name(k, nm);
        n = vibe_store_size(nm);
        slot_size[k] = n > 0 ? (uint32_t)n : 0u;
    }
    send_scale();
    send_loop();
    slot_load();
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
static void transport_text(char *t)     /* the DAW's (or a MIDI clock's) tempo and place: "120 BPM  > 5.2", or "" */
{
    const vibe_transport_info *tp = vibe_transport();
    char nb[12];
    t[0] = 0;
    if (!tp->has || tp->bpm <= 0.0f)
        return;
    cat(t, vg_itoa((int)(tp->bpm + 0.5f), nb));
    cat(t, " BPM  ");
    cat(t, tp->playing ? "> " : "|| ");
    {
        double bar_len = tp->sig_den ? tp->sig_num * 4.0 / tp->sig_den : 4.0;
        int bar = (int)(tp->bar_start / bar_len + 0.5) + 1, beat = (int)((tp->beat - tp->bar_start) * tp->sig_den / 4.0) + 1;
        cat(t, vg_itoa(bar, nb));
        cat(t, ".");
        cat(t, vg_itoa(beat, nb));
    }
}
static void draw_frame(void)
{
    uint32_t i, c = PG_COLOR[page];
    vg_clear(C_BG);
    vg_rect(0, 0, 480, 4, c);
    vg_text(PG_NAME[page], 16, 10, VG_BIG, c, 0);
    vg_text(set.mode ? "CHORDS" : "NOTES", 150, 10, VG_BIG, C_INK, 0);
    vg_text(sound_name(), 464, 10, VG_BIG, PG_COLOR[PG_SOUND], 2);
    if (vibe_transport()->has && vibe_transport()->playing)   /* the DAW plays */
        vg_text(">", 276, 12, VG_BIG, 0x4FD88A, 0);
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
    static const char *const F[6] = {"MODE", "HOLD", "PLAY", "KEEP", "", "MORE"};
    uint32_t i;
    for (i = 0; i < 6u; i++) {
        int x = 8 + (int)i * 78, lit = (i == 0u && set.mode) || (i == 1u && set.hold);
        const char *t = F[i];
        if ((i == 5u && page != PG_SOUND) || (i == 3u && page != PG_SOUND && page != PG_SCALE && page != PG_SEQ))
            t = "";
        if (i == 3u && page == PG_SEQ)
            t = "SAVE";
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
    if (set_dirty && vibe_frames() - set_dirty_at > (uint64_t)srate)   /* (a second after the last change) */
        settings_save();
    rec_hold_check();
    export_check();
    {                                    /* the PLAY and REC lights follow the recorder */
        static uint32_t lit = 0xFFFFFFFFu;
        uint32_t now = pub_playing | pub_ov << 1;
        if (now != lit)
            lit = now, lights();
    }
    if (help) {
        vg_controls("MVANE YARD", HELP, (int)(sizeof HELP / sizeof HELP[0] / 2u));
        return;
    }
    draw_frame();
    if (msg[0] && vibe_frames() < msg_until) {   /* KEEP and its kind: a moment, over the header */
        vg_rect(0, 4, 480, 36, C_BG);
        vg_text(msg, 240, 10, VG_BIG, 0xFF8A1F, 1);
    }
    if (page == PG_SOUND) {              /* YARD's sound: its name, its three pages of values (F6) */
        static const char *const SUB[3] = {"TONE ATTACK DECAY RELEASE", "DETUNE SUB RES PLUCK", "WAVE PW NOISE FILTER"};
        vg_text(sound_name(), 16, 72, VG_BIG | VG_X2, C_INK, 0);
        vg_text(set.sound >= SYNTH_NSOUNDS ? (sound_mono() ? "YOURS, YARD, MONO" : "YOURS, YARD") : sound_mono() ? "YARD, MONO" : "YARD",
                16, 124, VG_MED, C_DIM, 0);
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
    } else if (page == PG_SEQ) {         /* the recordings: the eight slots, this one, the history */
        char nb[12];
        for (i = 0; i < 8u; i++) {
            int x = 16 + (int)i * 56, full = i == slot ? pub_count != 0u : slot_size[i] > 16u;
            if (full)
                vg_round(x, 70, 48, 40, 5, i == slot ? PG_COLOR[PG_SEQ] : 0x4A4E58);
            else
                vg_frame(x, 70, 48, 40, i == slot ? 2 : 1, i == slot ? PG_COLOR[PG_SEQ] : C_FAINT);
            vg_text(vg_itoa((int)i + 1, nb), x + 24, 78, VG_BIG, full ? C_BG : i == slot ? PG_COLOR[PG_SEQ] : C_DIM, 1);
        }
        t[0] = 0;
        if (pub_count) {
            cat(t, "RECORDING ");
            cat(t, vg_itoa(slot + 1, nb));
            cat(t, ": ");
            cat(t, vg_ftoa((float)pub_dur / 1000.0f, 1, nb));
            cat(t, " S");
        } else
            cat(t, "EMPTY: PLAY, THEN REC (HELD: ALL)");
        vg_text(t, 16, 120, VG_BIG, pub_count ? C_INK : C_FAINT, 0);
        if (pub_playing) {
            t[0] = 0;
            if (pub_ov) {
                cat(t, pub_ov == 1u ? "OVERDUB AT THE TOP" : "OVERDUB ");
                if (pub_ov >= 2u)
                    cat(t, vg_itoa((int)pub_layers + 1, nb));
                if (pub_ov == 3u)
                    cat(t, " (ENDS)");
            } else
                cat(t, loop_on ? "LOOPING" : "PLAYING");
            vg_text(t, 16, 150, VG_BIG, 0xFF8A1F, 0);
            vg_text(vg_ftoa((float)pub_pos / 1000.0f, 1, nb), 464, 120, VG_BIG, C_INK, 2);
        } else if (pub_dirty)
            vg_text("NOT SAVED: F4", 16, 150, VG_MED, 0xFF8A1F, 0);
        t[0] = 0;
        cat(t, "HISTORY ");
        cat(t, vg_ftoa((float)pub_hist_ms / 1000.0f, 1, nb));
        cat(t, " S");
        if (pub_full)
            cat(t, "   FULL: SAVE");
        vg_text(t, 16, 180, VG_SMALL, pub_full ? 0xFF8A1F : C_DIM, 0);
        vg_text("REC: LAST PHRASE (HELD: ALL)   LOOPING: OVERDUB (HELD: UNDO)", 16, 194, VG_SMALL, C_DIM, 0);
    } else {                             /* ARP: the step to come */
        vg_text("THE ARP", 16, 72, VG_BIG | VG_X2, C_FAINT, 0);
        vg_text("A STEP TO COME: ON THE AUDIO'S CLOCK, THE DAW'S TEMPO", 16, 130, VG_MED, C_DIM, 0);
        transport_text(t);
        vg_text(t[0] ? t : "NO DAW CLOCK: ITS OWN TEMPO", 464, 80, VG_BIG, t[0] ? 0x4FD88A : C_FAINT, 2);
        draw_sounding(150);
    }
    draw_values();
    draw_fkeys();
    /* FOCUS (SDK 10): every control drawn, reachable with DATA (walk, then turn) and ENTER (press), in reading order */
    vf_begin();
    vf_encoder(VE_A, 330, 6, 142, 30);   /* the sound */
    for (i = 0; i < PG_N; i++)
        vf_key(VB_K1 + (int)i, 8 + (int)i * 78, 44, 74, 18);
    if (page == PG_CHORD)                /* the qualities: ENTER latches one, as a black key tapped */
        for (i = 0; i < CHORD_BANK; i++)
            vf_button(FX_QUAL + (int)i, 16 + (int)(i % 5u) * 90, 158 + (int)(i / 5u) * 26, 84, 22);
    if (page == PG_SCALE)                /* the twelve tones: ENTER adds or takes one (the key's stays) */
        for (i = 1; i < 12u; i++)
            vf_button(FX_TONE + (int)i, 16 + (int)i * 38, 140, 34, 34);
    vf_encoder(VE_B, 8, 206, 260, 22);   /* the scale */
    for (i = 0; i < 4u; i++) {
        const char *lb;
        char v[24];
        page_value(i, &lb, v);
        if (lb[0])
            vf_encoder(VE_1 + (int)i, 8 + (int)i * 118, 230, 112, 54);
    }
    vf_key(VB_F1, 8, 292, 74, 24);
    vf_key(VB_F2, 86, 292, 74, 24);
    vf_key(VB_F3, 164, 292, 74, 24);
    if (page == PG_SOUND)
        vf_key(VB_F6, 398, 292, 74, 24);
    if (page == PG_SOUND || page == PG_SCALE || page == PG_SEQ)
        vf_key(VB_F4, 242, 292, 74, 24);
    vf_end(0xFF8A1F);
}
