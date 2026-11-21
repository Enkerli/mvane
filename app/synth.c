/* SPDX-License-Identifier: GPL-3.0-only */
#include "synth.h"
#include "fastmath.h"

#define SR 44100.0f
#define NVOICE 8u

typedef struct {
    uint8_t ch, note, gate, used;
    float vel, level, phase, base, target, poly;
    float ic1, ic2, lp;                  /* the filter's state */
    uint32_t age;
} voice_t;

static voice_t vc[NVOICE];
static float bend[16], slide[16], press[16];   /* per channel: semitones, 0..1, 0..1 */
static uint8_t bend_range[16], rpn_msb[16], rpn_lsb[16];
static volatile uint8_t prm[SP_N];
static uint32_t clock_age;
static float gain_now;

static float note_inc(float note) { return 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f)) * (1.0f / SR); }

void synth_init(const uint8_t p[SP_N])
{
    uint32_t i;
    for (i = 0; i < SP_N; i++)
        prm[i] = p[i];
    for (i = 0; i < 16u; i++) {
        bend_range[i] = i ? 48u : 2u;    /* MPE's defaults: members 48, the master 2 */
        rpn_msb[i] = rpn_lsb[i] = 127u;
    }
}

uint32_t synth_param(uint32_t which) { return which < SP_N ? prm[which] : 0u; }
void synth_set(uint32_t which, uint32_t v)
{
    if (which < SP_N)
        prm[which] = (uint8_t)(v > 127u ? 127u : v);
}

uint32_t synth_voices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += vc[i].gate;
    return n;
}

void synth_panic(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        vc[i].gate = 0, vc[i].level = 0.0f;
}

static void voice_on(uint32_t ch, uint32_t note, uint32_t vel)
{
    uint32_t i, best = 0;
    float best_score = 1e9f;
    voice_t *v;
    for (i = 0; i < NVOICE; i++) {       /* the same note again, else a free voice (nearest pitch: it glides), else the oldest */
        float score;
        v = &vc[i];
        if (v->gate && v->ch == ch && v->note == note) {
            best = i;
            break;
        }
        score = v->gate ? 1000.0f - (float)(clock_age - v->age) * 1e-3f
                        : (v->used ? fm_fabsf((float)v->note - (float)note) : 200.0f) + v->level * 50.0f;
        if (score < best_score)
            best_score = score, best = i;
    }
    v = &vc[best];
    v->target = note_inc((float)note);
    if (!v->used || !prm[SP_GLIDE])
        v->base = v->target;
    v->ch = (uint8_t)ch;
    v->note = (uint8_t)note;
    v->vel = 0.25f + 0.75f * (float)vel * (1.0f / 127.0f);
    v->poly = 0.0f;
    v->gate = 1;
    v->used = 1;
    v->age = ++clock_age;
}

void synth_event(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu, i;
    if (kind == 0x90u && d2)
        voice_on(ch, d1, d2);
    else if (kind == 0x80u || kind == 0x90u) {
        for (i = 0; i < NVOICE; i++)
            if (vc[i].gate && vc[i].ch == ch && vc[i].note == d1)
                vc[i].gate = 0;
    } else if (kind == 0xA0u) {          /* poly pressure */
        for (i = 0; i < NVOICE; i++)
            if (vc[i].gate && vc[i].ch == ch && vc[i].note == d1)
                vc[i].poly = (float)d2 * (1.0f / 127.0f);
    } else if (kind == 0xD0u)
        press[ch] = (float)d1 * (1.0f / 127.0f);
    else if (kind == 0xE0u)
        bend[ch] = ((float)(d2 << 7 | d1) - 8192.0f) * (1.0f / 8192.0f) * (float)bend_range[ch];
    else if (kind == 0xB0u) {
        if (d1 == SYNTH_CC_TONE) {
            if (ch)
                slide[ch] = (float)d2 * (1.0f / 127.0f);   /* MPE slide: this note's brightness */
            else
                prm[SP_TONE] = (uint8_t)d2;
        } else if (d1 == SYNTH_CC_REL && !ch)
            prm[SP_REL] = (uint8_t)d2;
        else if (d1 == SYNTH_CC_GLIDE && !ch)
            prm[SP_GLIDE] = (uint8_t)d2;
        else if (d1 == SYNTH_CC_ATTACK && !ch)
            prm[SP_ATTACK] = (uint8_t)d2;
        else if (d1 == 101u)
            rpn_msb[ch] = (uint8_t)d2;
        else if (d1 == 100u)
            rpn_lsb[ch] = (uint8_t)d2;
        else if (d1 == 6u && rpn_msb[ch] == 0u && rpn_lsb[ch] == 0u)
            bend_range[ch] = (uint8_t)(d2 > 96u ? 96u : d2);   /* RPN 0: this channel's bend range */
        else if (d1 == 121u)             /* reset controllers */
            bend[ch] = slide[ch] = press[ch] = 0.0f;
        else if (d1 == 123u)
            for (i = 0; i < NVOICE; i++)
                vc[i].gate = 0;
    }
}

void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, j;
    float gain = (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f * 0.2f;
    float rel_s = 0.005f * fm_exp2f((float)prm[SP_REL] * (8.6f / 127.0f));      /* 5 ms .. 2 s */
    float att_s = 0.002f * fm_exp2f((float)prm[SP_ATTACK] * (10.0f / 127.0f));  /* 2 ms .. 2 s */
    float rel = 1.0f - fm_exp2f(-4.0f / (SR * rel_s));
    float att = 1.0f - fm_exp2f(-4.0f / (SR * att_s));
    float glide = prm[SP_GLIDE] ? 4.0f / (SR * 0.005f * fm_exp2f((float)prm[SP_GLIDE] * (8.6f / 127.0f))) : 1.0f;
    float tone = (float)prm[SP_TONE] * (1.0f / 127.0f);
    float acc[256];
    if (glide > 1.0f)
        glide = 1.0f;
    for (i = 0; i < n && i < 256u; i++)
        acc[i] = 0.0f;
    for (j = 0; j < NVOICE; j++) {
        voice_t *v = &vc[j];
        float p, b, mul, fc, g, k = 1.1f, a1, a2, a3, c1, oct;
        if (!v->gate && v->level < 1e-4f)
            continue;
        p = fm_maxf(fm_maxf(press[v->ch], press[0]), v->poly);
        b = bend[v->ch] + (v->ch ? bend[0] : 0.0f);
        mul = fm_exp2f(b * (1.0f / 12.0f));
        /* the cutoff follows the note: tone 0 under the fundamental (nearly a sine), 127 seven octaves above */
        oct = -0.7f + 7.0f * tone + 2.5f * p + 2.5f * slide[v->ch] + 0.6f * (v->vel - 0.25f);
        fc = fm_clampf(v->base * mul * SR * fm_exp2f(oct), 30.0f, 15000.0f);
        g = fm_tanf(3.14159265f * fc * (1.0f / SR));
        a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        c1 = fm_minf(1.0f, 6.2831853f * fc * (1.0f / SR));
        for (i = 0; i < n && i < 256u; i++) {
            float s, t, inc, v1, v2, v3;
            v->base += (v->target - v->base) * glide;
            inc = v->base * mul;
            v->phase += inc;
            if (v->phase >= 1.0f)
                v->phase -= 1.0f;
            t = v->phase;
            s = 2.0f * t - 1.0f;         /* a saw, its steps smoothed (polyBLEP, the division only at a step) */
            if (t < inc) {
                float x = t / inc;
                s -= x + x - x * x - 1.0f;
            } else if (t > 1.0f - inc) {
                float x = (t - 1.0f) / inc;
                s -= x * x + x + x + 1.0f;
            }
            v3 = s - v->ic2;             /* state-variable lowpass (12 dB/oct), then a one-pole: 18 dB/oct */
            v1 = a1 * v->ic1 + a2 * v3;
            v2 = v->ic2 + a2 * v->ic1 + a3 * v3;
            v->ic1 = 2.0f * v1 - v->ic1;
            v->ic2 = 2.0f * v2 - v->ic2;
            v->lp += (v2 - v->lp) * c1;
            v->level += v->gate ? (v->vel - v->level) * att : -v->level * rel;
            acc[i] += v->lp * v->level;
        }
        v->ic1 = fm_flush(v->ic1), v->ic2 = fm_flush(v->ic2), v->lp = fm_flush(v->lp);
    }
    for (i = 0; i < n && i < 256u; i++) {
        float s;
        gain_now += (gain - gain_now) * 0.004f;
        s = fm_clampf(acc[i] * gain_now, -8300000.0f, 8300000.0f);
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}
