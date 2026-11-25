/* SPDX-License-Identifier: GPL-3.0-only */
/* Desktop checks for the recorder: history, last-phrase and full captures, held notes across the boundary,
 * long pauses, the file round trip and the player. */
#include "rec.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ring_buf[1u << 12];
static uint32_t file_w[REC_HEAD_WORDS + (1u << 12)];   /* (4-aligned: a take reads its file in place) */
static uint8_t *const file = (uint8_t *)file_w;
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
    static rec_take take, back;
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
            uint32_t w = i < REC_HEAD_WORDS ? head[i] : rec_word(&take, i - REC_HEAD_WORDS);
            file[4 * i] = (uint8_t)w, file[4 * i + 1] = (uint8_t)(w >> 8);
            file[4 * i + 2] = (uint8_t)(w >> 16), file[4 * i + 3] = (uint8_t)(w >> 24);
        }
        assert(rec_load(&back, file, n) == 0);
        assert(back.count == take.count && back.duration_ms == take.duration_ms && !back.ring);
        for (i = 0; i < take.count; i++)
            assert(rec_word(&back, i) == rec_word(&take, i));
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
        for (i = 0; i < take.count; i++) {
            uint32_t w = rec_word(&take, i);
            if (((w >> 16) & 0xFF) == REC_TIME)
                gap += (w >> 24) << 16 | (w & 0xFFFF), t++;
        }
        assert(gap >= 120000u);
    }

    /* asleep: a pause past sleep_ms is kept at sleep_ms, and the span stops growing */
    r.sleep_ms = 4000;
    {
        uint32_t span = rec_span_ms(&r, now);
        assert(!rec_sleeping(&r, now) || span > 0);
        rec_put(&r, now += 600000u, 0x96, 64, 90); /* ten minutes later */
        assert(rec_span_ms(&r, now) <= span + 4000u);
        assert(rec_span_ms(&r, now + 999999u) <= span + 8000u && rec_sleeping(&r, now + 5000u));
        rec_put(&r, now += 100, 0x86, 64, 0);
    }
    r.sleep_ms = 0;

    /* a capture is locked in the history: playing on fills the rest, then the history is full and the take intact */
    {
        uint32_t first, i;
        rec_put(&r, now += 300, 0x97, 70, 100);
        rec_put(&r, now += 300, 0x87, 70, 0);
        assert(rec_capture(&r, now, 200, &take));
        first = rec_word(&take, take.npre);
        for (i = 0; i < 10000; i++)
            rec_put(&r, now += 5, 0xD1, (uint8_t)(i & 127), 0);
        assert(r.full && rec_word(&take, take.npre) == first);
        play(&take);
        {
            unsigned j, seen = 0;
            for (j = 0; j < ngot; j++)
                seen |= got[j].st == 0x87 && got[j].d1 == 70;
            assert(seen);
        }
        rec_unlock(&r);
        rec_put(&r, now += 5, 0xD1, 1, 0);
        assert(!r.full);
    }
    /* overdub from mid-pass (rec_mark_splice): the pass's first part copied after the mark, so the capture at the
     * pass's end is the whole pass, its first notes at their own times, the new note at its own */
    {
        static rec_take loop, layer;
        rec_player p;
        uint32_t i, t, seen = 0;
        rec_unlock(&r);
        rec_put(&r, now += 1000, 0x90, 60, 100);       /* the loop: C at 0, off at 300, E at 500, off at 700; 1000 ms */
        rec_put(&r, now += 300, 0x80, 60, 0);
        rec_put(&r, now += 200, 0x90, 64, 100);
        rec_put(&r, now += 200, 0x80, 64, 0);
        assert(rec_capture(&r, now, 900, &loop));
        loop.duration_ms = 1000u;
        rec_play_start(&p, &loop, now += 1000);         /* a pass: polled to 400 ms, then the tap */
        t = now;
        ngot = 0;
        for (clock_ms = t; clock_ms <= t + 400u; clock_ms++)
            rec_play_poll(&p, clock_ms, emit, 0);
        assert(ngot >= 2 && got[ngot - 1].st == 0x80 && got[ngot - 1].d1 == 60);   /* (a prefix first: the latest pressure) */
        {
            uint32_t m = rec_mark_splice(&r, t + 400u, &loop, p.pos, 400u);
            assert(m != r.head);
            for (clock_ms = t + 401u; clock_ms <= t + 1000u; clock_ms++) {   /* the rest of the pass into the history */
                unsigned before = ngot;
                rec_play_poll(&p, clock_ms, emit, 0);
                for (i = before; i < ngot; i++)
                    rec_put(&r, clock_ms, got[i].st, got[i].d1, got[i].d2);
                if (clock_ms == t + 800u)
                    rec_put(&r, clock_ms, 0x90, 67, 90), rec_put(&r, clock_ms + 50u, 0x80, 67, 0);   /* the overdub */
            }
            assert(rec_capture_from(&r, m, 1000u, &layer));
        }
        play(&layer);
        for (i = 0; i < ngot; i++) {
            uint32_t at = got[i].t - 1000000u;
            if (got[i].st == 0x90 && got[i].d1 == 60) seen |= 1, assert(at == 0);
            if (got[i].st == 0x80 && got[i].d1 == 60) seen |= 2, assert(at == 300);
            if (got[i].st == 0x90 && got[i].d1 == 64) seen |= 4, assert(at >= 500 && at <= 502);
            if (got[i].st == 0x90 && got[i].d1 == 67) seen |= 8, assert(at >= 800 && at <= 802);
        }
        assert(seen == 15);
        rec_unlock(&r);
    }
    /* unlocked, the ring keeps the newest */
    for (unsigned i = 0; i < 10000; i++)
        rec_put(&r, now += 5, 0xD1, (uint8_t)(i & 127), 0);
    assert(rec_count(&r) == 1u << 12);
    assert(rec_capture(&r, now, 0, &take) >= 1u << 12);
    printf("rec: all checks passed (history %u words, the last capture %u words, none copied)\n", (unsigned)rec_count(&r),
           (unsigned)take.count);
    return 0;
}
