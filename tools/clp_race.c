/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's voices on two cores, for real: the second core's job runs on a second thread while the first renders its
 * half (as on the FM-1), built with ThreadSanitizer, which reports any memory both touch unsafely. Each sound of the
 * engines named (all: every sound) plays a chord with pressure and slide, rendered once on one core and once on two:
 * the output must be the same, bit for bit (integer sums, whoever renders what).
 *   make clp-race                 (SAMPLE and WHEEL, the engines mvane yard added to voice.c vs_ok)
 *   make clp-race ENGINES=all */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "clp.c"

static pthread_t thr;
static void (*job)(uint32_t);
static uint32_t job_arg;
static int running, two = 1;
static void *thr_main(void *a)
{
    (void)a;
    job(job_arg);
    return 0;
}
int plat_cpu2_on(void) { return two; }
int plat_cpu2_run(void (*f)(uint32_t), uint32_t a)
{
    if (!two)
        return 0;
    job = f, job_arg = a, running = 1;
    return pthread_create(&thr, 0, thr_main, 0) == 0;
}
int plat_cpu2_wait(void)
{
    if (running)
        pthread_join(thr, 0), running = 0;
    return 0;
}

#define NB 120
static int32_t (*out)[NB * 512];        /* shared with the children: each render from the same state (a fork) */
static uint32_t *split_n;

static void play(unsigned s, int cores, int32_t *dst)
{
    unsigned b, c;
    two = cores == 2;
    clp_cores((uint32_t)cores);
    clp_panic();
    clp_select(s);
    for (b = 0; b < 30; b++)
        clp_render(dst, 256, 4096);
    for (c = 0; c < 6u; c++) {
        clp_event(0xD1u + c, 40u + c * 10u, 0);
        clp_event(0xB1u + c, 74, 50u + c * 8u);
        clp_event(0x91u + c, 43u + c * 4u, 100);
    }
    for (b = 0; b < NB; b++) {
        if (b == 60)
            for (c = 0; c < 6u; c++)
                clp_event(0x81u + c, 43u + c * 4u, 0);
        clp_render(dst + b * 512u, 256, 4096);
    }
}

static int play_apart(unsigned s, int cores, int32_t *dst)   /* 0: rendered, no race reported */
{
    int st = 0;
    pid_t p = fork();
    if (p == 0) {
        vs_split = 0;
        play(s, cores, dst);
        split_n[cores - 1] = vs_split;
        _exit(0);
    }
    waitpid(p, &st, 0);
    return !(WIFEXITED(st) && WEXITSTATUS(st) == 0);   /* (ThreadSanitizer: its report, then a non-zero exit) */
}

int main(int argc, char **argv)
{
    unsigned s, n, tested = 0, differ = 0;
    const char *want = argc > 1 ? argv[1] : "SAMPLE,WHEEL";
    out = mmap(0, 2 * sizeof *out, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    split_n = mmap(0, 8, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    clp_init();
    n = clp_sounds();
    for (s = 0; s < n; s++) {
        char key[16];
        const char *e = clp_sound_engine(s);
        snprintf(key, sizeof key, "%s", e);
        if (strcmp(want, "all") && !strstr(want, key))
            continue;
        tested++;
        if (play_apart(s, 1, out[0]) | play_apart(s, 2, out[1])) {
            differ++;
            printf("  RACE or crash: %s (%s)\n", clp_sound_name(s), e);
        } else if (memcmp(out[0], out[1], sizeof out[0])) {
            differ++;
            printf("  DIFFERS: %s (%s)\n", clp_sound_name(s), e);
        } else if (!split_n[1] && !clp_sound_mono(s))
            printf("  (not split: %s, %s)\n", clp_sound_name(s), e);
    }
    printf("clp_race: %u sounds (%s) on one core and on two threads: %u differ or race\n", tested, want, differ);
    return differ ? 1 : 0;
}
