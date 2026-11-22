/* SPDX-License-Identifier: GPL-3.0-only */
#include "synth.h"
#include "fastmath.h"

#define SR 44100.0f
#define NVOICE 12u

typedef struct {
    uint8_t ch, note, gate, used;
    float vel, level, phase, base, target, poly;
    float ic1, ic2, lp;                  /* the filter's state */
    float sp, ss, sb, sp1, ss1, sb1;     /* pressure, slide and bend, smoothed twice (no zipper) */
    float lb, lb_of, mul, mul_of;        /* caches: log2 of the pitch in Hz (for base lb_of), the bend's ratio (for sb) */
    uint32_t age;
} voice_t;

static voice_t vc[NVOICE];
static float bend[16], slide[16], press[16];   /* per channel: semitones, 0..1, 0..1 */
static uint8_t bend_range[16], rpn_msb[16], rpn_lsb[16];
static volatile uint8_t prm[SP_N];
static uint32_t clock_age;
static float gain_now;
static uint8_t mono, breath_on;          /* synth_mode */
static float breath_in, bsm1, bsm;       /* breath (CC2 / CC11), 0..1, and smoothed twice */
#define NSTACK 16u
static struct { uint8_t ch, note, vel; } stk[NSTACK];   /* mono: the notes held, the latest last */
static uint32_t nstk;

/* the filter's coefficient by the cutoff in octaves: g = tan(pi fc / SR) and the one-pole's c1 = min(1, 2 pi fc / SR),
 * 16 points an octave from 20 Hz to 16 kHz, interpolated (g within 0.03% below 4 kHz, 0.2% at 16 kHz: a few cents of
 * cutoff). -DSYNTH_EXACT computes them each time instead (for comparison: tools/synth_cmp.c) */
#define FT_LO 4.32192809f                /* log2(20) */
#define FT_HI 13.96578428f               /* log2(16000) */
#define FT_PER 16.0f
#define FT_N 157u                        /* (FT_HI - FT_LO) * FT_PER = 154.3 intervals, and guards */
static float ft_g[FT_N + 1u], ft_c1[FT_N + 1u];
static void ft_init(void)
{
    uint32_t i;
    for (i = 0; i <= FT_N; i++) {
        float fc = fm_exp2f(FT_LO + (float)i * (1.0f / FT_PER));
        ft_g[i] = fm_tanf(3.14159265f * fm_minf(fc, 16000.0f) * (1.0f / SR));
        ft_c1[i] = fm_minf(1.0f, 6.2831853f * fc * (1.0f / SR));
    }
}

static float note_inc(float note) { return 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f)) * (1.0f / SR); }

void synth_init(const uint8_t p[SP_N])
{
    uint32_t i;
    for (i = 0; i < SP_N; i++)
        prm[i] = p[i];
    ft_init();
    for (i = 0; i < NVOICE; i++)
        vc[i].lb_of = vc[i].mul_of = -1.0f;
    for (i = 0; i < 16u; i++) {
        bend_range[i] = i ? 48u : 2u;    /* MPE's defaults: members 48, the master 2 */
        slide[i] = 0.5f;                 /* CC74 at rest: 64 (bipolar controllers rest there) */
        rpn_msb[i] = rpn_lsb[i] = 127u;
    }
}

void synth_mode(uint32_t m, uint32_t b)
{
    uint32_t i;
    if (m != mono)                       /* a change of mode lets everything go */
        for (i = 0; i < NVOICE; i++)
            vc[i].gate = 0;
    mono = (uint8_t)(m != 0);
    breath_on = (uint8_t)(b != 0);
    nstk = 0;
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
    v->vel = 0.4f + 0.6f * (float)vel * (1.0f / 127.0f);   /* velocity: about 8 dB of level, and brightness */
    v->poly = 0.0f;
    v->sp = v->sp1 = fm_maxf(press[ch], press[0]);   /* the controls start where they are: no sweep from the last note */
    v->ss = v->ss1 = slide[ch];
    v->sb = v->sb1 = bend[ch] + (ch ? bend[0] : 0.0f);
    v->gate = 1;
    v->used = 1;
    v->age = ++clock_age;
}

/* mono: one voice; a note played while another is held slides to it (legato: the envelope goes on), its release
 * goes back to the note still held, the last release lets go */
static void mono_to(uint32_t ch, uint32_t note, uint32_t vel, int legato)
{
    voice_t *v = &vc[0];
    v->target = note_inc((float)note);
    if (!legato) {
        if (!v->used || !prm[SP_GLIDE])
            v->base = v->target;
        v->vel = 0.4f + 0.6f * (float)vel * (1.0f / 127.0f);
        v->gate = 1;
        v->age = ++clock_age;
    }
    v->ch = (uint8_t)ch;
    v->note = (uint8_t)note;
    v->used = 1;
    v->sb = v->sb1 = bend[ch] + (ch ? bend[0] : 0.0f);
}

static void mono_event(uint32_t ch, uint32_t note, uint32_t vel)
{
    uint32_t i, j;
    for (i = j = 0; i < nstk; i++)       /* the note, out of the stack wherever it is */
        if (!(stk[i].ch == ch && stk[i].note == note))
            stk[j++] = stk[i];
    nstk = j;
    if (vel) {
        int legato = vc[0].gate && nstk > 0;
        if (nstk == NSTACK) {
            for (i = 1; i < NSTACK; i++)
                stk[i - 1] = stk[i];
            nstk--;
        }
        stk[nstk].ch = (uint8_t)ch, stk[nstk].note = (uint8_t)note, stk[nstk].vel = (uint8_t)vel;
        nstk++;
        mono_to(ch, note, vel, legato);
    } else if (vc[0].gate && vc[0].ch == ch && vc[0].note == note) {
        if (nstk)
            mono_to(stk[nstk - 1].ch, stk[nstk - 1].note, stk[nstk - 1].vel, 1);
        else
            vc[0].gate = 0;
    }
}

void synth_event(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu, i;
    if (mono && (kind == 0x90u || kind == 0x80u))
        mono_event(ch, d1, kind == 0x90u ? d2 : 0u);
    else if (kind == 0x90u && d2)
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
                slide[ch] = (float)d2 * (1.0f / 127.0f);   /* MPE slide: this note's brightness, 64 neutral */
            else
                prm[SP_TONE] = (uint8_t)d2;
        } else if (d1 == 2u || d1 == 11u)   /* breath (CC2), or expression (CC11) from a wind controller */
            breath_in = (float)d2 * (1.0f / 127.0f);
        else if (d1 == SYNTH_CC_REL && !ch)
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
        else if (d1 == 121u) {           /* reset controllers */
            bend[ch] = press[ch] = 0.0f;
            slide[ch] = 0.5f;
        }
        else if (d1 == 123u) {
            for (i = 0; i < NVOICE; i++)
                vc[i].gate = 0;
            nstk = 0;
        }
    }
}

void synth_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, j;
    /* each voice: a seven-note chord at full velocity peaks near -10 dBFS (the FM-1's output stage distorts well
     * before digital full scale: Alex, 2026-11-21) */
    float gain = (float)master_q12 * (1.0f / 4096.0f) * 0.06f;
    float rel_s = 0.005f * fm_exp2f((float)prm[SP_REL] * (8.6f / 127.0f));      /* 5 ms .. 2 s */
    float att_s = 0.002f * fm_exp2f((float)prm[SP_ATTACK] * (10.0f / 127.0f));  /* 2 ms .. 2 s */
    float rel = 1.0f - fm_exp2f(-4.0f / (SR * rel_s));
    float att = 1.0f - fm_exp2f(-4.0f / (SR * att_s));
    float glide = prm[SP_GLIDE] ? 4.0f / (SR * 0.005f * fm_exp2f((float)prm[SP_GLIDE] * (8.6f / 127.0f))) : 1.0f;
    float tone = (float)prm[SP_TONE] * (1.0f / 127.0f);
    float acc[SYNTH_BLOCK], bsub[16], batt = 1.0f - fm_exp2f(-4.0f / (SR * 0.004f));   /* breath: the level within 4 ms */
    uint32_t k;
    if (glide > 1.0f)
        glide = 1.0f;
    if (mono && glide >= 1.0f)           /* mono: legato always slides, 20 ms when GLIDE is 0 */
        glide = 4.0f / (SR * 0.02f);
    for (k = 0; k < 16u; k++) {          /* breath, smoothed twice, once every 16 samples; a gentle curve */
        bsm1 += (breath_in - bsm1) * 0.15f;
        bsm += (bsm1 - bsm) * 0.15f;
        bsub[k] = bsm * (0.6f + 0.4f * bsm);
    }
    for (i = 0; i < n && i < SYNTH_BLOCK; i++)
        acc[i] = 0.0f;
    for (j = 0; j < NVOICE; j++) {
        voice_t *v = &vc[j];
        float p, b, k = 1.1f, shape = -1.5f + 9.0f * tone * tone * (3.0f - 2.0f * tone) + 0.8f * (v->vel - 0.4f);
        uint32_t i0;
        if (!v->gate && v->level < 1e-4f)
            continue;
        p = fm_maxf(fm_maxf(press[v->ch], press[0]), v->poly);
        b = bend[v->ch] + (v->ch ? bend[0] : 0.0f);
        /* every 16 samples: the controls glide toward where MIDI left them (two stages of about 9 ms), and the filter
         * follows: a control arriving 50 times a second in steps moves the sound smoothly */
        for (i0 = 0; i0 < n && i0 < SYNTH_BLOCK; i0 += 16u) {
            float mul, fc, g, a1, a2, a3, c1, oct;
            uint32_t i, end = i0 + 16u < n ? i0 + 16u : n;
            float br = bsub[(i0 >> 4) & 15u], tv = breath_on ? v->vel * br : v->vel, a = breath_on ? batt : att;
            v->sp1 += (p - v->sp1) * 0.04f, v->sp += (v->sp1 - v->sp) * 0.04f;   /* two stages, ~9 ms each */
            v->ss1 += (slide[v->ch] - v->ss1) * 0.04f, v->ss += (v->ss1 - v->ss) * 0.04f;
            v->sb1 += (b - v->sb1) * 0.04f, v->sb += (v->sb1 - v->sb) * 0.04f;
            if (v->sb != v->mul_of)      /* (a bend at rest costs nothing) */
                v->mul = fm_exp2f(v->sb * (1.0f / 12.0f)), v->mul_of = v->sb;
            mul = v->mul;
            /* the cutoff follows the note: tone 0 under the fundamental (a sine), 127 eight octaves above; pressure
             * up to three more; slide four octaves either way of 64 */
            oct = shape + 3.0f * v->sp + 8.0f * (v->ss - 0.5f) + (breath_on ? 4.0f * br - 1.5f : 0.0f);
#ifdef SYNTH_EXACT
            fc = fm_clampf(v->base * mul * SR * fm_exp2f(oct), 20.0f, 16000.0f);
            g = fm_tanf(3.14159265f * fc * (1.0f / SR));
            c1 = fm_minf(1.0f, 6.2831853f * fc * (1.0f / SR));
#else
            if (v->base != v->lb_of)     /* (the pitch's log again only while it glides) */
                v->lb = fm_log2f(v->base * SR), v->lb_of = v->base;
            fc = (fm_clampf(v->lb + v->sb * (1.0f / 12.0f) + oct, FT_LO, FT_HI) - FT_LO) * FT_PER;
            {
                uint32_t ix = (uint32_t)fc;
                float fr = fc - (float)ix;
                g = ft_g[ix] + (ft_g[ix + 1u] - ft_g[ix]) * fr;
                c1 = ft_c1[ix] + (ft_c1[ix + 1u] - ft_c1[ix]) * fr;
            }
#endif
            a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (i = i0; i < end; i++) {
                float s, t, inc, v1, v2, v3;
                v->base += (v->target - v->base) * glide;
                inc = v->base * mul;
                v->phase += inc;
                if (v->phase >= 1.0f)
                    v->phase -= 1.0f;
                t = v->phase;
                s = 2.0f * t - 1.0f;     /* a saw, its steps smoothed (polyBLEP, the division only at a step) */
                if (t < inc) {
                    float x = t / inc;
                    s -= x + x - x * x - 1.0f;
                } else if (t > 1.0f - inc) {
                    float x = (t - 1.0f) / inc;
                    s -= x * x + x + x + 1.0f;
                }
                v3 = s - v->ic2;         /* state-variable lowpass (12 dB/oct), then a one-pole: 18 dB/oct */
                v1 = a1 * v->ic1 + a2 * v3;
                v2 = v->ic2 + a2 * v->ic1 + a3 * v3;
                v->ic1 = 2.0f * v1 - v->ic1;
                v->ic2 = 2.0f * v2 - v->ic2;
                v->lp += (v2 - v->lp) * c1;
                v->level += v->gate ? (tv - v->level) * a : -v->level * rel;
                acc[i] += v->lp * v->level;
            }
        }
        v->ic1 = fm_flush(v->ic1), v->ic2 = fm_flush(v->ic2), v->lp = fm_flush(v->lp);
    }
    for (i = 0; i < n && i < SYNTH_BLOCK; i++) {   /* MASTER, then a soft limiter: no hard clip at any volume */
        float s;
        gain_now += (gain - gain_now) * 0.004f;
        s = acc[i] * gain_now;
        if (s > 0.5f || s < -0.5f)          /* clean to half scale, then bends smoothly toward 0.95 */
            s = (s > 0.0f ? 1.0f : -1.0f) * (0.5f + 0.45f * fm_tanhf((fm_fabsf(s) - 0.5f) * (1.0f / 0.45f)));
        s *= 8300000.0f;
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}
