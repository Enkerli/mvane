/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard on VIBE FM, native (the Hub's FROM-FM1.md, step 1: a skeleton that makes the sound). YARD (app/synth.c,
 * the FM-1's own file) plays the notes from the keys, MIDI and the DAW, each on its channel (MPE: its bend, pressure,
 * slide); encoders 1-4 are TONE ATTACK DECAY RELEASE, A the sound (YARD's thirteen). The audio has its own flow: the
 * events only queue MIDI messages, the audio alone touches the synth. Steps to follow: the controls (NOTES and CHORDS,
 * the chord builder), the pages, FOCUS, the DAW's parameters and transport, storage, the recorder, Clipper. */
#include "vibe.h"
#include "vibe_dsp.h"
#include "vibe_gfx.h"
/* YARD as the FM-1 computes it: the SDK builds with -ffast-math, the FM-1 without it and without fused multiply-adds;
 * precise maths for YARD's code, so the two render the same samples (tools/vibe_cmp.c) */
#pragma clang fp reassociate(off) contract(off)
#include "../../app/synth.c"
#pragma clang fp reassociate(on) contract(fast)

VIBE_PARALLEL_AUDIO

/* ---- events to the audio: MIDI messages, one writer (the events), one reader (the audio) ---- */
#define QN 256u
#define Q_SOUND 0xF0u                    /* (not MIDI) d1: YARD's sound to load */
static uint8_t q_st[QN], q_d1[QN], q_d2[QN];
static volatile uint32_t qw, qr;
static void send(uint32_t st, uint32_t d1, uint32_t d2)   /* (control flow) full: dropped */
{
    uint32_t w = qw;
    if (w - qr >= QN)
        return;
    q_st[w % QN] = (uint8_t)st, q_d1[w % QN] = (uint8_t)d1, q_d2[w % QN] = (uint8_t)d2;
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
    synth_mode(s->mono, 0);
    for (i = 0; i < SP_N; i++)
        synth_event(0xB0u, SP_CC[i], s->p[i]);
}

/* ---- what the player sees (control flow): the sound, the four values ---- */
static uint32_t sound;
static uint8_t val[SP_N];
static int help;
static const uint8_t KNOB_P[4] = {SP_TONE, SP_ATTACK, SP_DECAY, SP_REL};
static const char *const KNOB_N[4] = {"TONE", "ATTACK", "DECAY", "RELEASE"};
static void sound_pick(uint32_t n)
{
    uint32_t i;
    sound = n % SYNTH_NSOUNDS;
    for (i = 0; i < SP_N; i++)
        val[i] = SYNTH_SOUNDS[sound].p[i];
    send(Q_SOUND, sound, 0);
}

static const uint8_t START_P[SP_N] = {56, 60, 30, 24, 64, 0, 0, 0, 0, 0, 0, 0, 0};   /* (as the FM-1's first start) */
void vibe_on_init(float sample_rate)
{
    synth_rate(sample_rate);
    synth_init(START_P);
    vibe_notes(VIBE_NOTES);
    sound_pick(0);
}

static const char *const HELP[] = {"KEYS / MIDI / DAW", "play YARD (MPE: each note its channel)", "ENCODERS 1-4",
                                   "TONE, ATTACK, DECAY, RELEASE", "A", "the sound (YARD's thirteen)", "HOME",
                                   "this overview"};
void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_BUTTON && e->down && (e->id == VB_HOME || e->id == VB_RETURN)) {
        help = e->id == VB_HOME ? !help : 0;
        return;
    }
    if (e->type == VIBE_EV_NOTE) {
        int v = (int)(e->value * 127.0f + 0.5f);
        send((e->down ? 0x90u : 0x80u) | (e->midi[0] & 15u), e->id, e->down ? (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v) : 0u);
    } else if (e->type == VIBE_EV_MIDI) {  /* bend, pressure, CCs: as they come (a note's channel its own) */
        uint32_t st = e->midi[0];
        if (st >= 0xA0u && st < 0xF0u && (st & 0xF0u) != 0xC0u)
            send(st, e->midi[1], e->midi[2]);
    } else if (e->type == VIBE_EV_ENCODER) {
        int d = e->value > 0.0f ? 1 : e->value < 0.0f ? -1 : 0;
        if (e->id <= VE_4) {
            uint32_t p = KNOB_P[e->id];
            int nv = (int)val[p] + (int)(e->value * 2.0f + (e->value > 0.0f ? 0.5f : -0.5f));
            val[p] = (uint8_t)(nv < 0 ? 0 : nv > 127 ? 127 : nv);
            send(0xB0u, SP_CC[p], val[p]);
        } else if (e->id == VE_A && d)
            sound_pick(sound + SYNTH_NSOUNDS + (uint32_t)d);
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

void vibe_on_draw(uint16_t *fb)
{
    int i;
    (void)fb;
    if (help) {
        vg_controls("MVANE YARD", HELP, (int)(sizeof HELP / sizeof HELP[0] / 2u));
        return;
    }
    vg_clear(0x101114);
    vg_text("MVANE YARD", 16, 12, VG_BIG, 0xF2F2F2, 0);
    vg_text(SYNTH_SOUNDS[sound].name, 464, 12, VG_BIG, 0x2F8F78, 2);
    vg_text("YARD, ITS OWN LIGHT SYNTH   A: THE SOUND   HOME: THE CONTROLS", 16, 46, VG_SMALL, 0x8A8E98, 0);
    for (i = 0; i < 4; i++) {
        char t[8];
        int x = 16 + i * 116;
        vg_text(KNOB_N[i], x, 230, VG_MED, 0xB8BCC6, 0);
        vg_itoa(val[KNOB_P[i]], t);
        vg_text(t, x, 252, VG_BIG, 0xF2F2F2, 0);
        vg_rect(x, 284, 100, 6, 0x2A2D35);
        vg_rect(x, 284, val[KNOB_P[i]] * 100 / 127, 6, 0x2F8F78);
    }
}
