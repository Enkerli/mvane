/* SPDX-License-Identifier: GPL-3.0-only */
/* Retrospective performance recorder: an always-on history of MIDI-like events, takes captured from it after the
 * fact, and a player. Portable C99: no allocation, no platform calls. The firmware, the desktop tools and a future
 * plugin share it, so a take means the same everywhere.
 *
 * An event is one 32-bit word: dt (8 bits, ms since the previous word) | status | data1 | data2, most significant
 * byte first. A status of 0 is a time word, for pauses of 255 ms or more: a 24-bit gap in ms (up to about 4.6 hours
 * each), its top byte in the dt field and its low 16 bits in data1:data2. Everything else is an ordinary MIDI channel message (0x80..0xEF), so a take converts to a
 * standard MIDI file one word at a time. */
#ifndef MVANE_REC_H
#define MVANE_REC_H

#include <stdint.h>

#define REC_TIME 0x00u
#define REC_MAGIC 0x3154564Du /* "MVT1" little-endian: the take file's first four bytes */
#define REC_HEAD_WORDS 4u     /* magic, version, word count, duration in ms */
#define REC_MAX_HELD 32u      /* notes and controllers carried across a capture boundary */

typedef struct {
    uint32_t *buf;   /* capacity words, a power of two */
    uint32_t mask;   /* capacity - 1 */
    uint32_t head;   /* words ever written (the next one goes at head & mask) */
    uint32_t last_ms;
    uint8_t started;
} rec_ring;

typedef struct {
    uint32_t *words;
    uint32_t count, cap;
    uint32_t duration_ms;
} rec_take;

typedef void (*rec_emit_fn)(void *ctx, uint8_t status, uint8_t d1, uint8_t d2);

typedef struct {
    const rec_take *take;
    uint32_t pos, t_ms, start_ms, next_ms;
    uint8_t playing;
    uint8_t held[16][16]; /* per channel, a bitmap of the 128 notes this player started */
} rec_player;

void rec_init(rec_ring *r, uint32_t *buf, uint32_t capacity_pow2);
/* one message at time now_ms (any monotonic ms clock; wraps are fine) */
void rec_put(rec_ring *r, uint32_t now_ms, uint8_t status, uint8_t d1, uint8_t d2);
uint32_t rec_count(const rec_ring *r);            /* words held */
uint32_t rec_span_ms(const rec_ring *r, uint32_t now_ms);   /* how far back the history reaches */

/* Copy history into take. gap_ms 0 = everything held; otherwise only what follows the most recent silence of at
 * least gap_ms (the last phrase). Notes still sounding and the latest controller values at the capture's start
 * are written first, at time 0, from the history before it. Returns the words captured (0: nothing). */
uint32_t rec_capture(const rec_ring *r, uint32_t now_ms, uint32_t gap_ms, rec_take *take);

/* take <-> file bytes (little-endian words, REC_HEAD_WORDS of header). rec_load returns 0 on a good file */
uint32_t rec_file_size(const rec_take *take);
void rec_file_head(const rec_take *take, uint32_t head[REC_HEAD_WORDS]);
int rec_load(rec_take *take, const uint8_t *bytes, uint32_t n);
uint32_t rec_crc32(uint32_t crc, const uint8_t *p, uint32_t n);   /* zlib's crc32: chains */

void rec_play_start(rec_player *p, const rec_take *take, uint32_t now_ms);
/* emits everything due by now_ms; returns 1 while playing. At the end, every note it started is released */
int rec_play_poll(rec_player *p, uint32_t now_ms, rec_emit_fn emit, void *ctx);
void rec_play_stop(rec_player *p, rec_emit_fn emit, void *ctx);

#endif
