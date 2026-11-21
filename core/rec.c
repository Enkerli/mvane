/* SPDX-License-Identifier: GPL-3.0-only */
#include "rec.h"

#include "port.h"

#define W_DT(w) ((w) >> 24)
#define W_ST(w) (((w) >> 16) & 0xFFu)
#define W_D1(w) (((w) >> 8) & 0xFFu)
#define W_D2(w) ((w) & 0xFFu)
#define W_GAP(w) (((w) >> 24) << 16 | ((w) & 0xFFFFu))   /* a time word: the gap's top byte rides in dt */

static uint32_t rec__word_dt(uint32_t w) { return W_ST(w) == REC_TIME ? W_GAP(w) : W_DT(w); }
static uint32_t rec__make(uint32_t dt, uint32_t st, uint32_t d1, uint32_t d2)
{
    return dt << 24 | st << 16 | (d1 & 0x7Fu) << 8 | (d2 & 0x7Fu);
}

void rec_init(rec_ring *r, uint32_t *buf, uint32_t capacity_pow2)
{
    mvane_zero(r, sizeof *r);
    r->buf = buf;
    r->mask = capacity_pow2 - 1u;
}

static void rec__push(rec_ring *r, uint32_t w)
{
    r->buf[r->head & r->mask] = w;
    r->head++;
}

static uint32_t rec__room(const rec_ring *r)   /* words the ring can take before a locked capture */
{
    return r->locked ? r->mask + 1u - (r->head - r->lock) : 0xFFFFFFFFu;
}

void rec_put(rec_ring *r, uint32_t now_ms, uint8_t status, uint8_t d1, uint8_t d2)
{
    uint32_t gap = 0;
    if (status < 0x80u || status >= 0xF0u)
        return;
    if (rec__room(r) < 4u) {             /* (a time word or two and the event) */
        r->full = 1;
        return;
    }
    if (r->started)
        gap = now_ms - r->last_ms;
    if (r->sleep_ms && gap > r->sleep_ms)
        gap = r->sleep_ms;
    r->started = 1;
    r->last_ms = now_ms;
    while (gap >= 255u) {
        uint32_t g = gap > 0xFFFFFFu ? 0xFFFFFFu : gap;
        rec__push(r, (g >> 16) << 24 | (g & 0xFFFFu)); /* a time word: status 0 */
        gap -= g;
    }
    rec__push(r, rec__make(gap, status, d1, d2));
}

uint32_t rec_count(const rec_ring *r) { return r->head <= r->mask ? r->head : r->mask + 1u; }

uint32_t rec_span_ms(const rec_ring *r, uint32_t now_ms)
{
    uint32_t n = rec_count(r), i, t = 0;
    if (!n)
        return 0;
    for (i = r->head - n + 1u; i != r->head; i++) /* the oldest word's own dt reaches before the history */
        t += rec__word_dt(r->buf[i & r->mask]);
    t += now_ms - r->last_ms > r->sleep_ms && r->sleep_ms ? r->sleep_ms : now_ms - r->last_ms;
    return t;
}

int rec_sleeping(const rec_ring *r, uint32_t now_ms) { return r->sleep_ms && now_ms - r->last_ms > r->sleep_ms; }

/* ---------------------------------------------------------- capture --- */
typedef struct {
    uint8_t st[REC_MAX_HELD], d1[REC_MAX_HELD], d2[REC_MAX_HELD];
    uint32_t n;
} rec__held_set;

static void rec__held_note(rec__held_set *h, uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t ch = st & 0x0Fu, i;
    int on = (st & 0xF0u) == 0x90u && d2;
    for (i = 0; i < h->n; i++)
        if ((h->st[i] & 0x0Fu) == ch && h->d1[i] == d1)
            break;
    if (i < h->n && !on) { /* released: the last one takes its place */
        h->n--;
        h->st[i] = h->st[h->n], h->d1[i] = h->d1[h->n], h->d2[i] = h->d2[h->n];
    } else if (on && (i < h->n || h->n < REC_MAX_HELD)) {
        if (i == h->n)
            h->n++;
        h->st[i] = (uint8_t)(0x90u | ch), h->d1[i] = (uint8_t)d1, h->d2[i] = (uint8_t)d2;
    }
}

static void rec__held_ctl(rec__held_set *h, uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t i, kind = st & 0xF0u;
    for (i = 0; i < h->n; i++)
        if (h->st[i] == st && (kind != 0xB0u || h->d1[i] == d1))
            break;
    if (i == h->n) {
        if (h->n == REC_MAX_HELD)
            return;
        h->n++;
    }
    h->st[i] = (uint8_t)st, h->d1[i] = (uint8_t)d1, h->d2[i] = (uint8_t)d2;
}

static void rec__track(rec__held_set *notes, rec__held_set *ctls, uint32_t w)
{
    uint32_t st = W_ST(w), kind = st & 0xF0u;
    if (st == REC_TIME)
        return;
    if (kind == 0x80u || kind == 0x90u)
        rec__held_note(notes, st, W_D1(w), W_D2(w));
    else if (kind == 0xB0u || kind == 0xD0u || kind == 0xE0u)
        rec__held_ctl(ctls, st, W_D1(w), W_D2(w));
}

void rec_unlock(rec_ring *r) { r->locked = r->full = 0; }

uint32_t rec_word(const rec_take *k, uint32_t i)
{
    uint32_t w;
    if (i < k->npre)
        return k->pre[i];
    i -= k->npre;
    if (!k->ring)
        return k->flat[i];
    w = k->ring->buf[(k->start + i) & k->ring->mask];
    return i ? w : (W_ST(w) == REC_TIME ? 0u : w & 0x00FFFFFFu);   /* the capture starts on its first event */
}

uint32_t rec_capture(rec_ring *r, uint32_t now_ms, uint32_t gap_ms, rec_take *take)
{
    uint32_t n = rec_count(r), oldest = r->head - n, start = oldest, i, out, t, last_t;
    rec__held_set pre_n, pre_c, end_n;
    rec_unlock(r);
    if (!n)
        return 0;
    if (gap_ms) { /* the most recent silence of gap_ms or more, before the last event */
        uint32_t run = 0, ev = r->head; /* ev: the event the time words before it lead to */
        for (i = r->head; i-- != oldest + 1u;) {
            uint32_t w = r->buf[i & r->mask];
            if (W_ST(w) == REC_TIME)
                run += W_GAP(w);
            else
                ev = i, run = W_DT(w);
            if (run >= gap_ms && ev != r->head) {
                start = ev;
                break;
            }
        }
    }
    mvane_zero(&pre_n, sizeof pre_n);    /* what sounds at the start */
    mvane_zero(&pre_c, sizeof pre_c);
    for (i = oldest; i != start; i++)
        rec__track(&pre_n, &pre_c, r->buf[i & r->mask]);
    out = 0;
    for (i = 0; i < pre_c.n; i++)
        take->pre[out++] = rec__make(0, pre_c.st[i], pre_c.d1[i], pre_c.d2[i]);
    for (i = 0; i < pre_n.n; i++)
        take->pre[out++] = rec__make(0, pre_n.st[i], pre_n.d1[i], pre_n.d2[i]);
    take->npre = out;
    take->ring = r;
    take->flat = 0;
    take->start = start;
    take->nring = r->head - start;
    take->count = take->npre + take->nring;
    mvane_zero(&end_n, sizeof end_n);
    for (i = 0; i < pre_n.n; i++)
        rec__held_note(&end_n, pre_n.st[i], pre_n.d1[i], pre_n.d2[i]);
    t = last_t = 0;
    for (i = 0; i < take->nring; i++) {
        uint32_t w = rec_word(take, take->npre + i);
        t += rec__word_dt(w);
        if (W_ST(w) != REC_TIME) {
            last_t = t;
            rec__track(&end_n, &pre_c, w);
        }
    }
    /* notes still held at the press last until the press; otherwise the take ends on its last event */
    {
        uint32_t tail = now_ms - r->last_ms;
        if (r->sleep_ms && tail > r->sleep_ms)
            tail = r->sleep_ms;
        take->duration_ms = end_n.n ? t + tail : last_t;
    }
    r->lock = start;
    r->locked = 1;
    return take->count;
}

/* ------------------------------------------------------------- files --- */
uint32_t rec_file_size(const rec_take *take) { return 4u * (REC_HEAD_WORDS + take->count); }

void rec_file_head(const rec_take *take, uint32_t head[REC_HEAD_WORDS])
{
    head[0] = REC_MAGIC;
    head[1] = 1u;
    head[2] = take->count;
    head[3] = take->duration_ms;
}

static uint32_t rec__le32(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

int rec_load(rec_take *take, const uint8_t *bytes, uint32_t n)
{
    uint32_t count;
    if (n < 4u * REC_HEAD_WORDS || rec__le32(bytes) != REC_MAGIC || rec__le32(bytes + 4) != 1u)
        return -1;
    count = rec__le32(bytes + 8);
    if (n < 4u * (REC_HEAD_WORDS + count) || ((uintptr_t)bytes & 3u))
        return -1;
    take->npre = take->nring = take->start = 0;
    take->ring = 0;
    take->flat = (const uint32_t *)(const void *)(bytes + 4u * REC_HEAD_WORDS);
    take->count = count;
    take->duration_ms = rec__le32(bytes + 12);
    return 0;
}

uint32_t rec_crc32(uint32_t c, const uint8_t *p, uint32_t n)
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    c = ~c;
    while (n--) {
        c ^= *p++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return ~c;
}

/* ------------------------------------------------------------ player --- */
void rec_play_start(rec_player *p, const rec_take *take, uint32_t now_ms)
{
    mvane_zero(p, sizeof *p);
    p->take = take;
    p->start_ms = now_ms;
    p->playing = take && take->count;
}

static void rec__release_all(rec_player *p, rec_emit_fn emit, void *ctx)
{
    uint32_t ch, note;
    for (ch = 0; ch < 16u; ch++)
        for (note = 0; note < 128u; note++)
            if (p->held[ch][note >> 3] >> (note & 7u) & 1u)
                emit(ctx, (uint8_t)(0x80u | ch), (uint8_t)note, 0);
    mvane_zero(p->held, sizeof p->held);
}

int rec_play_poll(rec_player *p, uint32_t now_ms, rec_emit_fn emit, void *ctx)
{
    uint32_t elapsed = now_ms - p->start_ms;
    const rec_take *k = p->take;
    if (!p->playing)
        return 0;
    while (p->pos < k->count) {
        uint32_t w = rec_word(k, p->pos), st = W_ST(w), due = p->t_ms + rec__word_dt(w);
        if (due > elapsed)
            break;
        p->t_ms = due;
        p->pos++;
        if (st == REC_TIME)
            continue;
        if ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u) {
            uint32_t ch = st & 0x0Fu, note = W_D1(w);
            if ((st & 0xF0u) == 0x90u && W_D2(w))
                p->held[ch][note >> 3] |= (uint8_t)(1u << (note & 7u));
            else
                p->held[ch][note >> 3] &= (uint8_t)~(1u << (note & 7u));
        }
        emit(ctx, (uint8_t)st, (uint8_t)W_D1(w), (uint8_t)W_D2(w));
    }
    if (p->pos >= k->count && elapsed >= k->duration_ms) {
        rec__release_all(p, emit, ctx);
        p->playing = 0;
    }
    return p->playing;
}

void rec_play_stop(rec_player *p, rec_emit_fn emit, void *ctx)
{
    if (p->playing)
        rec__release_all(p, emit, ctx);
    p->playing = 0;
}
