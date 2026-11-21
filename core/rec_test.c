/* SPDX-License-Identifier: GPL-3.0-only */
/* Desktop checks for the recorder: history, last-phrase and full captures, held notes across the boundary,
 * long pauses, the file round trip and the player. */
#include "rec.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ring_buf[1u << 12], take_buf[1u << 11], load_buf[1u << 11];
static uint8_t file[4u * (REC_HEAD_WORDS + (1u << 11))];
static struct { uint8_t st, d1, d2; uint32_t t; } got[4096];
static unsigned ngot;
static uint32_t clock_ms;

static void emit(void *ctx, uint8_t st, uint8_t d1, uint8_t d2)
{
    (void)ctx;
    got[ngot].st = st, got[ngot].d1 = d1, got[ngot].d2 = d2, got[ngot].t = clock_ms;
    ngot++;
}

static void play(const rec_take *t)
{
    rec_player p;
    ngot = 0;
    clock_ms = 1000000u;
    rec_play_start(&p, t, clock_ms);
    while (rec_play_poll(&p, clock_ms, emit, 0))
        clock_ms++;
}

int main(void)
{
    rec_ring r;
    rec_take take = {take_buf, 0, 1u << 11, 0}, back = {load_buf, 0, 1u << 11, 0};
    uint32_t now = 0xFFFFF000u; /* the clock wraps during the test */
    rec_init(&r, ring_buf, 1u << 12);

    /* phrase 1: a held pad (ch 2) under a CC sweep and three notes */
    rec_put(&r, now, 0xB0, 74, 20);
    rec_put(&r, now, 0x91, 48, 90);                  /* held across the capture boundary */
    for (unsigned i = 0; i < 3; i++) {
        rec_put(&r, now += 400, 0x92, (uint8_t)(60 + i), 100);
        rec_put(&r, now += 200, 0x82, (uint8_t)(60 + i), 0);
        rec_put(&r, now, 0xB0, 74, (uint8_t)(40 + 10 * i));
    }
    now += 6000;                                       /* a silence: the next phrase starts after it */
    rec_put(&r, now, 0x93, 67, 80);
    rec_put(&r, now += 1000, 0x83, 67, 0);
    rec_put(&r, now += 500, 0x81, 48, 0);              /* the pad's release, in phrase 2 */

    assert(rec_span_ms(&r, now) == 400u * 3 + 200u * 3 + 6000u + 1500u);

    /* last phrase: the CC and the pad come first, at time 0, then the phrase */
    assert(rec_capture(&r, now, 4000, &take));
    assert(take.duration_ms == 1500u);
    play(&take);
    assert(ngot == 5);
    assert(got[0].st == 0xB0 && got[0].d1 == 74 && got[0].d2 == 60 && got[0].t == 1000000u);
    assert(got[1].st == 0x91 && got[1].d1 == 48);
    assert(got[2].st == 0x93 && got[2].t == 1000000u);
    assert(got[3].st == 0x83 && got[3].t == 1001000u);
    assert(got[4].st == 0x81 && got[4].t == 1001500u);

    /* everything: the long pause survives as time words */
    assert(rec_capture(&r, now, 0, &take));
    assert(take.duration_ms == 400u * 3 + 200u * 3 + 6000u + 1500u);
    play(&take);
    assert(ngot == 14 && got[13].st == 0x81 && got[13].t == 1000000u + take.duration_ms);

    /* the file round trip */
    {
        uint32_t head[REC_HEAD_WORDS], i, n = rec_file_size(&take);
        rec_file_head(&take, head);
        for (i = 0; i < REC_HEAD_WORDS + take.count; i++) {
            uint32_t w = i < REC_HEAD_WORDS ? head[i] : take.words[i - REC_HEAD_WORDS];
            file[4 * i] = (uint8_t)w, file[4 * i + 1] = (uint8_t)(w >> 8);
            file[4 * i + 2] = (uint8_t)(w >> 16), file[4 * i + 3] = (uint8_t)(w >> 24);
        }
        assert(rec_load(&back, file, n) == 0);
        assert(back.count == take.count && back.duration_ms == take.duration_ms);
        assert(!memcmp(back.words, take.words, 4 * take.count));
        file[0] ^= 1;
        assert(rec_load(&back, file, n) != 0);
    }

    /* a note still held at the press: the take lasts until the press, and the player releases it */
    rec_put(&r, now += 300, 0x94, 72, 100);
    assert(rec_capture(&r, now + 2000, 4000, &take));
    play(&take);
    assert(take.duration_ms == 1000u + 500u + 300u + 2000u);
    assert(got[ngot - 1].st == 0x84 && got[ngot - 1].d1 == 72);

    /* a two-minute pause survives (wider than the data bytes) */
    rec_put(&r, now += 120000u, 0x95, 60, 90);
    rec_put(&r, now += 100, 0x85, 60, 0);
    assert(rec_capture(&r, now, 0, &take));
    assert(rec_span_ms(&r, now) >= 120100u);
    {
        uint32_t i, t = 0, gap = 0;
        for (i = 0; i < take.count; i++)
            if (((take.words[i] >> 16) & 0xFF) == REC_TIME)
                gap += (take.words[i] >> 24) << 16 | (take.words[i] & 0xFFFF), t++;
        assert(gap >= 120000u);
    }

    /* overflow: the ring keeps the newest, and a capture still starts with the right state */
    for (unsigned i = 0; i < 10000; i++)
        rec_put(&r, now += 5, 0xD1, (uint8_t)(i & 127), 0);
    assert(rec_count(&r) == 1u << 12);
    assert(rec_capture(&r, now, 0, &take) == 1u << 11);  /* the take's capacity bounds it */
    assert(rec_crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926u);
    printf("rec: all checks passed (history %u words, take %u words)\n", (unsigned)rec_count(&r), (unsigned)take.count);
    return 0;
}
