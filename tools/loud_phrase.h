/* SPDX-License-Identifier: GPL-3.0-only */
/* The same ordinary phrase for every sound (tools/clp_loud.c, tools/yard_loud.c), and its loudness: four-note chords
 * (velocity VEL, channel pressure PRS on each note's channel, MPE style: a member channel per note) over a bass, four of
 * them, 1.5 s each; a mono sound, a line of eight notes, 0.4 s each, on one channel. Loudness: K-weighted (BS.1770's
 * two filters, at 44.1 kHz), the mean square of 400 ms blocks over 75% overlap, gated at -70 and then 10 dB under
 * that mean (as LUFS is), in dB of full scale (24 bits). */
#include <math.h>
#include <stdint.h>

#define LP_SR 44100.0
#define LP_BLK 256u
typedef void (*lp_ev_f)(uint32_t st, uint32_t d1, uint32_t d2);
typedef void (*lp_render_f)(int32_t *lr, uint32_t n);

typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } lp_bq;
static double lp_bq_run(lp_bq *f, double x)
{
    double y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1, f->x1 = x, f->y2 = f->y1, f->y1 = y;
    return y;
}
static void lp_kweight(lp_bq f[2])          /* (pyloudnorm's derivation of BS.1770's filters for any rate) */
{
    double G = 3.999843853973347, fc = 1681.974450955533, Q = 0.7071752369554196, K = tan(M_PI * fc / LP_SR);
    double Vh = pow(10.0, G / 20.0), Vb = pow(Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
    f[0] = (lp_bq){(Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                   2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0, 0, 0, 0, 0};
    fc = 38.13547087602444, Q = 0.5003270373238773, K = tan(M_PI * fc / LP_SR);
    a0 = 1.0 + K / Q + K * K;
    f[1] = (lp_bq){1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0, 0, 0, 0, 0};
}

#define LP_MAXF (44100u * 9u)
static float lp_ms[LP_MAXF];                /* each frame's weighted power, both channels summed */
static uint32_t lp_n;
static double lp_peak;
static lp_bq lp_f[2][2];

static void lp_take(lp_render_f render, uint32_t blocks)
{
    static int32_t lr[2 * LP_BLK];
    uint32_t b, i, c;
    for (b = 0; b < blocks; b++) {
        render(lr, LP_BLK);
        for (i = 0; i < LP_BLK && lp_n < LP_MAXF; i++) {
            double p = 0;
            for (c = 0; c < 2u; c++) {
                double x = lr[2u * i + c] / 8388608.0, y = lp_bq_run(&lp_f[c][1], lp_bq_run(&lp_f[c][0], x));
                p += y * y;
                if (fabs(x) > lp_peak)
                    lp_peak = fabs(x);
            }
            lp_ms[lp_n++] = (float)p;
        }
    }
}
static double lp_mmax;                      /* the loudest 400 ms block's loudness (momentary, as LUFS's M), dB */
static double lp_lufs(void)
{
    uint32_t w = (uint32_t)(0.4 * LP_SR), h = w / 4u, s, i, k, nb = 0;
    static double bl[256];
    double sum, gate, m;
    for (s = 0; s + w <= lp_n && nb < 256u; s += h) {
        for (sum = 0, i = 0; i < w; i++)
            sum += lp_ms[s + i];
        bl[nb++] = sum / w;
    }
    for (lp_mmax = -99.0, i = 0; i < nb; i++)
        if (bl[i] > 1e-10 && -0.691 + 10.0 * log10(bl[i]) > lp_mmax)
            lp_mmax = -0.691 + 10.0 * log10(bl[i]);
    for (gate = 1e-7, k = 0; k < 2u; k++) {     /* -70 absolute, then the relative gate */
        for (sum = 0, m = 0, i = 0; i < nb; i++)
            if (bl[i] > gate)
                sum += bl[i], m++;
        if (m == 0)
            return -99.0;
        gate = sum / m * 0.1;
        if (k == 1)
            return -0.691 + 10.0 * log10(sum / m);
    }
    return -99.0;
}

/* the phrase, then its loudness (and the peak, dBFS, in *peak) */
static double lp_play(lp_ev_f ev, lp_render_f render, int mono, unsigned vel, unsigned prs, unsigned off, double *peak)
{
    static const uint8_t CH[4][4] = {{55, 59, 64, 67}, {57, 60, 64, 67}, {57, 60, 62, 65}, {55, 59, 62, 65}};
    static const uint8_t BASS[4] = {48, 45, 50, 43}, LINE[8] = {60, 62, 64, 67, 69, 67, 64, 62};
    unsigned r, c;
    lp_n = 0, lp_peak = 0;
    lp_kweight(lp_f[0]), lp_kweight(lp_f[1]);
    if (mono)
        for (r = 0; r < 8u; r++) {
            ev(0x91, LINE[r] - off, vel);
            ev(0xD1, prs, 0);
            lp_take(render, 69);                /* 0.4 s */
            ev(0x81, LINE[r] - off, 0);
        }
    else
        for (r = 0; r < 4u; r++) {
            ev(0x91, BASS[r] - 12u, vel);
            ev(0xD1, prs, 0);
            for (c = 0; c < 4u; c++)
                ev(0x92u + c, CH[r][c], vel), ev(0xD2u + c, prs, 0);
            lp_take(render, 258);               /* 1.5 s */
            ev(0x81, BASS[r] - 12u, 0);
            for (c = 0; c < 4u; c++)
                ev(0x82u + c, CH[r][c], 0), ev(0xD2u + c, 0, 0);
            ev(0xD1, 0, 0);
        }
    lp_take(render, 172);                       /* the release, 1 s */
    *peak = 20.0 * log10(lp_peak + 1e-12);
    return lp_lufs();
}
