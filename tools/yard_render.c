/* SPDX-License-Identifier: GPL-3.0-only */
/* YARD's sounds (app/synth.c SYNTH_SOUNDS) played on the desktop, to listen to and to time: a poly sound plays
 * Cmaj7 Am7 Dm7 G7 (1.5 s each, then a pause), a mono one a legato line with a slow bend and a pressure swell. Writes
 * one WAV of them all, a second of silence between, and prints each one's CPU on this computer (one core), against
 * SAW's. Usage: yard_render OUT.wav
 * yard_render race: every sound on one core, then with the upper half of the voices on a second thread (as on the
 * FM-1's second core), under ThreadSanitizer (make yard-race): no race, and the same output within 4 steps of 24 bits
 * (the voices' float sums in another order: about -126 dBFS) */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "../app/synth.c"

#define BLK 256u
static int32_t lr[2 * BLK];
static FILE *wav;
static uint32_t frames;
static int32_t *rec;                         /* (race: what was rendered, shared with the parent) */
static uint32_t nrec;
#define REC_MAX (44100u * 12u)
static pthread_t thr;
static void (*job)(uint32_t);
static int running;
static void *thr_main(void *a)
{
    (void)a;
    job(0);
    return 0;
}
static int run2(void (*f)(uint32_t), uint32_t a)
{
    (void)a;
    job = f, running = 1;
    return pthread_create(&thr, 0, thr_main, 0) == 0;
}
static int wait2(void)
{
    if (running)
        pthread_join(thr, 0), running = 0;
    return 0;
}

static void put(void)
{
    uint32_t i;
    for (i = 0; i < BLK; i++) {
        int32_t v = (lr[2 * i] + 128) >> 8;
        if (rec && nrec < REC_MAX)
            rec[nrec++] = lr[2 * i];
        int16_t x = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        if (wav)
            fwrite(&x, 2, 1, wav);
    }
    frames += BLK;
}
static void run(double sec)
{
    uint32_t b, nb = (uint32_t)(sec * 44100.0 / BLK);
    for (b = 0; b < nb; b++) {
        synth_render(lr, BLK, 4096u);
        put();
    }
}
static void ev(uint32_t a, uint32_t b, uint32_t c) { synth_event(a, b & 127u, c & 127u); }

static void phrase(unsigned s)
{
    static const uint8_t CH[4][5] = {{48, 64, 67, 71, 74}, {45, 64, 67, 69, 72}, {50, 62, 65, 69, 72}, {43, 62, 65, 67, 71}};
    static const uint8_t LINE[8] = {60, 62, 64, 67, 69, 67, 64, 62};
    unsigned k, c;
    synth_init(SYNTH_SOUNDS[s].p);
    synth_mode(SYNTH_SOUNDS[s].mono, 0);
    if (!SYNTH_SOUNDS[s].mono) {
        for (k = 0; k < 4u; k++) {
            for (c = 0; c < 5u; c++)
                ev(0x91u + c, CH[k][c], 96);
            run(1.4);
            for (c = 0; c < 5u; c++)
                ev(0x81u + c, CH[k][c], 0);
            run(0.1);
        }
    } else {
        unsigned off = SYNTH_SOUNDS[s].p[SP_SUB] > 60u ? 24u : 0u;   /* (a bass an octave or two down) */
        for (k = 0; k < 8u; k++) {
            ev(0x91, LINE[k] - off, 100);
            if (k)
                ev(0x81, LINE[k - 1u] - off, 0);
            if (k == 3u) {                       /* a held note: a slow bend up a semitone and back, pressure */
                uint32_t b;
                for (b = 0; b < 60u; b++) {
                    uint32_t bend = 8192u + (uint32_t)(171.0 * (b < 30u ? b : 60u - b) / 30.0);
                    ev(0xE1, bend & 127u, bend >> 7);
                    ev(0xD1, b < 30u ? b * 4u : (60u - b) * 4u, 0);
                    run(0.02);
                }
                ev(0xE1, 0, 64);
            } else
                run(0.35);
        }
        ev(0x81, LINE[7] - off, 0);
    }
    run(1.0);
}

int main(int argc, char **argv)
{
    unsigned s;
    double base = 0;
    if (argc < 2)
        return 2;
    if (!strcmp(argv[1], "race")) {      /* each render in a child of the same state (a fork): one core, then two */
        unsigned bad = 0, split = 0;
        int32_t *out = mmap(0, 2u * REC_MAX * sizeof *out, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        uint32_t *sh = mmap(0, 4 * sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        for (s = 0; s < SYNTH_NSOUNDS; s++) {
            int c, st;
            int32_t worst = 0;
            uint32_t i;
            for (c = 0; c < 2; c++) {
                pid_t p;
                fflush(stdout);
                p = fork();
                if (p == 0) {
                    synth_cores(c ? run2 : 0, c ? wait2 : 0);
                    rec = out + (uint32_t)c * REC_MAX, nrec = 0;
                    phrase(s);
                    sh[c] = nrec, sh[2 + c] = synth_split_blocks();
                    _exit(0);
                }
                waitpid(p, &st, 0);
                if (!WIFEXITED(st) || WEXITSTATUS(st))
                    bad++, printf("  RACE or crash: %s\n", SYNTH_SOUNDS[s].name);
            }
            split += sh[3];
            for (i = 0; i < sh[0] && i < sh[1]; i++) {
                int32_t d = out[i] - out[REC_MAX + i];
                d = d < 0 ? -d : d;
                worst = d > worst ? d : worst;
            }
            if (sh[0] != sh[1] || worst > 4)
                bad++, printf("  DIFFERS: %s (by %d)\n", SYNTH_SOUNDS[s].name, worst);
            else
                printf("  %-9s %u blocks on two cores, within %d of 24 bits\n", SYNTH_SOUNDS[s].name, sh[3], worst);
        }
        printf("yard_race: %u sounds on one core and on two threads (%u blocks split): %u differ or race\n",
               SYNTH_NSOUNDS, split, bad);
        return bad || !split;
    }
    wav = fopen(argv[1], "wb");
    if (!wav)
        return 1;
    fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x01\0\x44\xac\0\0\x88\x58\x01\0\x02\0\x10\0data\0\0\0\0", 44, 1, wav);
    for (s = 0; s < SYNTH_NSOUNDS; s++)
        phrase(s);
    {
        uint32_t data = frames * 2u, riff = data + 36u;
        fseek(wav, 4, SEEK_SET);
        fwrite(&riff, 4, 1, wav);
        fseek(wav, 40, SEEK_SET);
        fwrite(&data, 4, 1, wav);
        fclose(wav);
        wav = 0;
    }
    printf("%s: YARD's %u sounds, %.1f s\n", argv[1], SYNTH_NSOUNDS, frames / 44100.0);
    for (s = 0; s < SYNTH_NSOUNDS; s++) {
        clock_t t0 = clock();
        uint32_t f0 = frames, r;
        double ms;
        for (r = 0; r < 10u; r++)
            phrase(s);
        ms = (double)(clock() - t0) / CLOCKS_PER_SEC * 1000.0 / ((frames - f0) / 44100.0);
        if (!s)
            base = ms;
        printf("  %-9s %s %.2f ms of CPU a second of sound (%.2fx SAW)\n", SYNTH_SOUNDS[s].name,
               SYNTH_SOUNDS[s].mono ? "mono" : "poly", ms, ms / base);
    }
    return 0;
}
