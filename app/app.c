/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane 0.1: the proof of concept. Everything played becomes MIDI-like events on one bus, which feeds the synth,
 * USB-MIDI out (MPE, lower zone) and the always-on history (core/rec). REC captures from the history after the
 * fact, PLAY replays the take through the same bus, SAVE keeps it in the flash, and the companion services give it
 * to a computer as take.mvt (tools/mvt2mid.py makes a MIDI file of it).
 *
 *   keys       NOTES: notes, each on its own MPE channel with its pressure; CHORDS: white keys are the scale's
 *              degrees, black keys the dominant of the degree to their left, voiced with smooth voice leading
 *   ALGO       NOTES / CHORDS          PRESET  the key (C .. B)         SELECT  major / minor
 *   K1 tone    K2 release    K3 glide (sent and recorded as CC74, CC72, CC5: replayed with the take)
 *   OCT- OCT+  octave        REC  tap: capture the last phrase; hold: capture the whole history
 *   PLAY       play / stop the take                    SAVE  the settings and the take, kept across power-off
 *
 * Part of the unity build (base/device/unity.c, base/host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "harmony.c"
#include "rec.c"

#define SR 44100.0f
#define NVOICE 8u
#define HIST_WORDS (1u << 15)            /* 128 KiB of history: tens of minutes of notes, minutes of dense pressure */
#define TAKE_WORDS (1u << 14)            /* 64 KiB: the take, in RAM and in the flash */
#define PHRASE_GAP_MS 3000u              /* REC tap: what followed the last silence this long */
#define CC_TONE 74u
#define CC_REL 72u
#define CC_GLIDE 5u

/* ---------------------------------------------------------------- settings --- */
#define SET_MAGIC 0x4D564131u            /* "MVA1" */
typedef struct {
    uint32_t magic;
    uint8_t mode;                        /* 0 notes, 1 chords */
    uint8_t tonic;                       /* 0 C .. 11 B */
    uint8_t minor;
    int8_t octave;                       /* -2 .. +2 */
    uint8_t tone, rel, glide;            /* 0..127, as their CCs */
    uint8_t rsv;
} settings_t;
static settings_t set;
static uint8_t set_dirty;

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.mode = 1;
    set.tonic = 0;
    set.minor = 0;
    set.octave = 0;
    set.tone = 70;
    set.rel = 50;
    set.glide = 30;
}

static void settings_load(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.mode > 1u || set.tonic > 11u || set.minor > 1u || set.octave < -2 || set.octave > 2
        || set.tone > 127u || set.rel > 127u || set.glide > 127u)
        settings_defaults();
}

/* ------------------------------------------------------------------- synth --- */
/* the main loop sends the render its events through a single-producer, single-consumer queue */
static volatile uint32_t sq[256];
static volatile uint32_t sq_w, sq_r;
static void synth_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t w = sq_w;
    if (w - sq_r >= 256u)
        return;                          /* full: dropped (the render is not running) */
    sq[w & 255u] = st << 16 | d1 << 8 | d2;
    sq_w = w + 1u;
}

typedef struct {
    uint8_t ch, note, gate, used;
    float vel, level, phase, inc, target, lp;
    uint32_t age;
} voice_t;
static voice_t vc[NVOICE];
static float press[16];                  /* channel pressure, 0..1 (channel 0: the MPE master, every voice) */
static volatile uint8_t p_tone = 70, p_rel = 50, p_glide = 30;   /* the sound's knobs, as the render has them */
static uint32_t vc_clock;
static float gain_now;

static float note_inc(uint32_t note) { return 440.0f * fm_exp2f(((float)note - 69.0f) / 12.0f) / SR; }

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
        score = v->gate ? 1000.0f + (float)(vc_clock - v->age) * -1e-3f
                        : (v->used ? fm_fabsf((float)v->note - (float)note) : 200.0f) + v->level * 50.0f;
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }
    v = &vc[best];
    v->target = note_inc(note);
    if (!v->used || !p_glide)
        v->inc = v->target;
    v->ch = (uint8_t)ch;
    v->note = (uint8_t)note;
    v->vel = 0.25f + 0.75f * (float)vel / 127.0f;
    v->gate = 1;
    v->used = 1;
    v->age = ++vc_clock;
}

static void synth_event(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu, i;
    if (kind == 0x90u && d2)
        voice_on(ch, d1, d2);
    else if (kind == 0x80u || kind == 0x90u) {
        for (i = 0; i < NVOICE; i++)
            if (vc[i].gate && vc[i].ch == ch && vc[i].note == d1)
                vc[i].gate = 0;
    } else if (kind == 0xD0u)
        press[ch] = (float)d1 / 127.0f;
    else if (kind == 0xB0u) {
        if (d1 == CC_TONE)
            p_tone = (uint8_t)d2;
        else if (d1 == CC_REL)
            p_rel = (uint8_t)d2;
        else if (d1 == CC_GLIDE)
            p_glide = (uint8_t)d2;
        else if (d1 == 123u)             /* all notes off */
            for (i = 0; i < NVOICE; i++)
                vc[i].gate = 0;
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, j;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f * 0.22f;
    float rel_s = 0.005f * fm_exp2f((float)p_rel / 127.0f * 8.6f);       /* 5 ms .. 2 s */
    float rel = 1.0f - fm_exp2f(-1.0f / (SR * rel_s * 0.25f));
    float glide = p_glide ? 1.0f / (SR * 0.005f * fm_exp2f((float)p_glide / 127.0f * 8.6f) * 0.25f) : 1.0f;
    float tone = (float)p_tone / 127.0f;
    while (sq_r != sq_w) {
        uint32_t w = sq[sq_r & 255u];
        synth_event(w >> 16, (w >> 8) & 0xFFu, w & 0xFFu);
        sq_r = sq_r + 1u;
    }
    for (i = 0; i < n; i++)
        lr[2 * i] = 0;
    for (j = 0; j < NVOICE; j++) {
        voice_t *v = &vc[j];
        float p, b, c, inv;
        if (!v->gate && v->level < 1e-4f)
            continue;
        p = press[v->ch] > press[0] ? press[v->ch] : press[0];
        b = tone * (0.35f + 0.65f * p);
        c = 0.01f + 0.85f * b * b;       /* one-pole lowpass: tone, opened by pressure */
        inv = 1.0f / (v->inc > 1e-6f ? v->inc : 1e-6f);
        for (i = 0; i < n; i++) {
            float s, t;
            v->inc += (v->target - v->inc) * (glide < 1.0f ? glide : 1.0f);
            v->phase += v->inc;
            if (v->phase >= 1.0f)
                v->phase -= 1.0f;
            t = v->phase;
            s = 2.0f * t - 1.0f;         /* a saw, its steps smoothed (polyBLEP) */
            if (t < v->inc) {
                float x = t * inv;
                s -= x + x - x * x - 1.0f;
            } else if (t > 1.0f - v->inc) {
                float x = (t - 1.0f) * inv;
                s -= x * x + x + x + 1.0f;
            }
            v->lp += (s - v->lp) * c;
            v->level += v->gate ? (v->vel - v->level) * 0.004f : -v->level * rel;
            lr[2 * i] += (int32_t)(v->lp * v->level * 4096.0f);
        }
    }
    for (i = 0; i < n; i++) {
        float s;
        gain_now += (gain - gain_now) * 0.004f;
        s = (float)lr[2 * i] * (1.0f / 4096.0f) * gain_now;
        if (s > 8300000.0f)
            s = 8300000.0f;
        else if (s < -8300000.0f)
            s = -8300000.0f;
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}

void app_panic(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        vc[i].gate = 0, vc[i].level = 0.0f;
}

/* --------------------------------------------------------------------- bus --- */
#define TO_SYNTH 1u
#define TO_OUT 2u
#define TO_HIST 4u
static uint32_t hist_buf[HIST_WORDS] PLAT_POOL, take_buf[TAKE_WORDS] PLAT_POOL;
static rec_ring hist;
static rec_take take = {0, 0, TAKE_WORDS, 0};
static rec_player player;
static uint8_t take_dirty;
static uint8_t ch_notes[16];             /* notes sounding per channel (out): the MPE allocator's view */
static uint32_t ch_free_at[16];

static void bus(uint32_t st, uint32_t d1, uint32_t d2, uint32_t to)
{
    uint32_t kind = st & 0xF0u, ch = st & 0x0Fu;
    d1 &= 0x7Fu, d2 &= 0x7Fu;
    if (kind == 0x90u && d2)
        ch_notes[ch]++;
    else if ((kind == 0x80u || kind == 0x90u) && ch_notes[ch]) {
        ch_notes[ch]--;
        ch_free_at[ch] = plat_ms();
    }
    if (to & TO_SYNTH)
        synth_send(st, d1, d2);
    if (to & TO_OUT)
        plat_midi_send(kind >> 4 | st << 8 | d1 << 16 | d2 << 24, TO_USB);
    if (to & TO_HIST)
        rec_put(&hist, plat_ms(), (uint8_t)st, (uint8_t)d1, (uint8_t)d2);
}
#define LIVE (TO_SYNTH | TO_OUT | TO_HIST)

static void play_emit(void *ctx, uint8_t st, uint8_t d1, uint8_t d2)
{
    (void)ctx;
    bus(st, d1, d2, TO_SYNTH | TO_OUT);
}

static uint32_t mpe_channel(void)        /* a member channel (MIDI 2..16): free longest, else the one with fewest notes */
{
    uint32_t c, best = 1, now = plat_ms();
    for (c = 1; c < 16u; c++) {
        if (ch_notes[c] < ch_notes[best] ||
            (ch_notes[c] == ch_notes[best] && now - ch_free_at[c] > now - ch_free_at[best]))
            best = c;
    }
    return best;
}

static void mpe_config(void)             /* MPE configuration message: lower zone, 15 member channels; bend range 48 */
{
    uint32_t c;
    bus(0xB0u, 101u, 0u, TO_OUT);
    bus(0xB0u, 100u, 6u, TO_OUT);
    bus(0xB0u, 6u, 15u, TO_OUT);
    for (c = 1; c < 16u; c++) {
        bus(0xB0u | c, 101u, 0u, TO_OUT);
        bus(0xB0u | c, 100u, 0u, TO_OUT);
        bus(0xB0u | c, 6u, 48u, TO_OUT);
    }
}

/* -------------------------------------------------------------- the take --- */
static const plat_file_t TAKEF = {0x97000u, 0xA8000u, 0x11000u};   /* A/B, 68 KiB each, in the data area */

static void take_load(void)
{
    int n = plat_file_size(&TAKEF);
    const uint8_t *p = plat_file_data(&TAKEF);
    take.words = take_buf;
    take.count = 0;
    if (n > 0 && p && rec_load(&take, p, (uint32_t)n) != 0)
        take.count = 0;
}

static int take_save(void)
{
    uint32_t head[REC_HEAD_WORDS], size = rec_file_size(&take), crc;
    rec_file_head(&take, head);
    crc = rec_crc32(0, (const uint8_t *)head, sizeof head);   /* the FM-1 and the host are little-endian */
    crc = rec_crc32(crc, (const uint8_t *)take.words, 4u * take.count);
    if (plat_file_begin(&TAKEF, size) || plat_file_write(&TAKEF, 0, head, sizeof head)
        || (take.count && plat_file_write(&TAKEF, sizeof head, take.words, 4u * take.count))
        || plat_file_commit(&TAKEF, size, crc))
        return -1;
    take_dirty = 0;
    return 0;
}

void app_init(void)
{
    settings_load();
    p_tone = set.tone, p_rel = set.rel, p_glide = set.glide;
    rec_init(&hist, hist_buf, HIST_WORDS);
    take_load();
}

/* ---------------------------------------------------------------- helpers --- */
static const char *const NOTE_N[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void time_text(uint32_t ms, char *b)   /* m:ss */
{
    uint32_t s = ms / 1000u;
    char t[12];
    int_text((int32_t)(s / 60u), b);
    text_cat(b, ":");
    t[0] = (char)('0' + s % 60u / 10u), t[1] = (char)('0' + s % 10u), t[2] = 0;
    text_cat(b, t);
}

/* ------------------------------------------------------------------- play --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint8_t vel[NKEYS], prs[NKEYS], prs_sent[NKEYS];
    uint32_t prs_at[NKEYS];
    uint8_t key_ch[NKEYS], key_note[NKEYS];
    int32_t chord_key;                   /* the key whose chord sounds, -1 none */
    uint8_t chord_n, chord_note[5], chord_ch[5];
    Voicing prev;
    uint8_t have_prev;
    char chord_name[12];
    uint32_t rec_t;
    uint32_t link;
    uint32_t hist_ms;                    /* the history's reach, refreshed now and then */
    uint32_t hist_at;
    uint8_t redraw;
} ui;

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static int32_t out_note(int32_t n) { return clamp(n + 12 * set.octave, 0, 127); }

static void chord_release(void)
{
    uint32_t i;
    for (i = 0; i < ui.chord_n; i++)
        bus(0x80u | ui.chord_ch[i], ui.chord_note[i], 0, LIVE);
    ui.chord_n = 0;
    ui.chord_key = -1;
    if (press[0] > 0.0f)
        bus(0xD0u, 0, 0, LIVE);
}

static void chord_play(uint32_t k, uint32_t vel)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each black key follows */
    uint32_t i, j, degree = 0, secondary = 0, nn = 0;
    uint8_t notes[5], chs[5];
    Chord c;
    Voicing v;
    for (i = 0; i < 16u; i++)
        if (WK[i] == k)
            degree = i;
    for (i = 0; i < 11u; i++)
        if (BK[i] == k)
            degree = AT[i], secondary = 1;
    c = harmony_degree(set.tonic, degree % 7u, set.minor, (int)secondary);
    if (!harmony_voice_live(c, ui.have_prev ? &ui.prev : NULL, &v))
        return;
    ui.prev = v;
    ui.have_prev = 1;
    notes[nn++] = (uint8_t)out_note(v.bass);
    for (i = 0; i < HARMONY_VOICES; i++)
        notes[nn++] = (uint8_t)out_note(v.upper[i]);
    /* common tones hold; the voices that leave are released first, so the new ones glide from them */
    for (i = 0; i < ui.chord_n; i++) {
        int keep = 0;
        for (j = 0; j < nn; j++)
            keep |= notes[j] == ui.chord_note[i];
        if (!keep)
            bus(0x80u | ui.chord_ch[i], ui.chord_note[i], 0, LIVE);
    }
    for (j = 0; j < nn; j++) {
        int held = -1;
        for (i = 0; i < ui.chord_n; i++)
            if (ui.chord_note[i] == notes[j])
                held = (int)i;
        if (held >= 0)
            chs[j] = ui.chord_ch[held];
        else {
            chs[j] = (uint8_t)mpe_channel();
            bus(0x90u | chs[j], notes[j], vel, LIVE);
        }
    }
    for (j = 0; j < nn; j++)
        ui.chord_note[j] = notes[j], ui.chord_ch[j] = chs[j];
    ui.chord_n = (uint8_t)nn;
    ui.chord_key = (int32_t)k;
    ui.chord_name[0] = 0;
    text_cat(ui.chord_name, NOTE_N[c.root]);
    text_cat(ui.chord_name, harmony_quality_name(c.quality));
    ui.redraw = 1;
}

static void all_off(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if (ui.keys >> k & 1u && !set.mode)
            bus(0x80u | ui.key_ch[k], ui.key_note[k], 0, LIVE);
    chord_release();
}

static void knob(uint8_t *p, uint32_t cc, int32_t e, const char *name)
{
    char t[8];
    int32_t v = clamp((int32_t)*p + e * 2, 0, 127);
    if (v == (int32_t)*p)
        return;
    *p = (uint8_t)v;
    bus(0xB0u, cc, (uint32_t)v, LIVE);   /* the master channel: the whole zone; recorded, so a take replays it */
    set_dirty = 1;
    int_text(v, t);
    app_say(name, t);
}

static void capture(uint32_t gap)
{
    char t[16];
    if (player.playing)
        rec_play_stop(&player, play_emit, 0);
    take.words = take_buf;
    if (!rec_capture(&hist, plat_ms(), gap, &take)) {
        app_say("HISTORY", "EMPTY");
        return;
    }
    take_dirty = 1;
    time_text(take.duration_ms, t);
    app_say(gap ? "PHRASE" : "ALL", t);
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, pass, now = plat_ms(), pkt;
    int port;
    int32_t e;
    if (plat_midi_out_link() != ui.link) {   /* a new USB connection: tell it this is MPE */
        ui.link = plat_midi_out_link();
        mpe_config();
    }
    while (plat_midi_in(&pkt, &port)) {  /* MIDI in plays the synth and goes into the history (not echoed) */
        uint32_t st = (pkt >> 8) & 0xFFu;
        if (st >= 0x80u && st < 0xF0u)
            bus(st, (pkt >> 16) & 0x7Fu, pkt >> 24, TO_SYNTH | TO_HIST);
    }
    touch_poll();
    keys = touch_keys();
    for (pass = 0; pass < 2u * NKEYS; pass++) {   /* presses first, then releases: a chord change in one scan
                                                   * holds its common tones */
        uint32_t bit, v, releasing;
        k = pass % NKEYS;
        bit = 1u << k;
        releasing = !(keys & bit) && (ui.keys & bit);
        if (releasing != (pass >= NKEYS))
            continue;
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)(v ? v : 100u);
            ui.prs[k] = ui.prs_sent[k] = 0;
            if (set.mode)
                chord_play(k, ui.vel[k]);
            else {
                ui.key_ch[k] = (uint8_t)mpe_channel();
                ui.key_note[k] = (uint8_t)out_note(53 + (int32_t)k);
                bus(0x90u | ui.key_ch[k], ui.key_note[k], ui.vel[k], LIVE);
            }
        } else if (!(keys & bit) && (ui.keys & bit)) {
            if (set.mode) {
                if (ui.chord_key == (int32_t)k)
                    chord_release();
            } else
                bus(0x80u | ui.key_ch[k], ui.key_note[k], 0, LIVE);
        } else if (keys & bit) {         /* pressure: at most 50 a second a key, and only real changes */
            uint32_t p = touch_pressure(k), d = p > ui.prs_sent[k] ? p - ui.prs_sent[k] : ui.prs_sent[k] - p;
            ui.prs[k] = (uint8_t)p;
            if (d >= 2u && now - ui.prs_at[k] >= 20u) {
                ui.prs_sent[k] = (uint8_t)p;
                ui.prs_at[k] = now;
                if (!set.mode)
                    bus(0xD0u | ui.key_ch[k], p, 0, LIVE);
                else if (ui.chord_key == (int32_t)k)
                    bus(0xD0u, p, 0, LIVE);   /* a chord's pressure on the master channel: every voice */
            }
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_OCTDN || k == B_OCTUP) {
            int32_t o = clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[4];
            if (o != set.octave) {
                all_off();
                set.octave = (int8_t)o;
                set_dirty = 1;
                t[0] = o < 0 ? '-' : '+', t[1] = (char)('0' + (o < 0 ? -o : o)), t[2] = 0;
                app_say("OCTAVE", t);
            }
        } else if (k == B_REC)
            ui.rec_t = now ? now : 1u;
        else if (k == B_PLAY) {
            if (player.playing) {
                rec_play_stop(&player, play_emit, 0);
                app_say("TAKE", "STOPPED");
            } else if (take.count) {
                rec_play_start(&player, &take, now);
                app_say("TAKE", "PLAYING");
            } else
                app_say("TAKE", "EMPTY");
        } else if (k == B_SAVE) {
            int bad;
            set.tone = p_tone, set.rel = p_rel, set.glide = p_glide;
            bad = plat_store_save(OBJ_SETTINGS, &set, sizeof set) != 0;
            if (!bad)
                set_dirty = 0;
            if (take_dirty && !player.playing)
                bad |= take_save() != 0;
            app_say(bad ? "SAVE" : "SAVED", bad ? "FAILED" : "");
        }
    }
    if (ui.rec_t) {                      /* REC: held, the whole history; a tap, the last phrase */
        if (btn >> B_REC & 1u) {
            if (now - ui.rec_t >= 700u) {
                ui.rec_t = 0;
                capture(0);
            }
        } else {
            ui.rec_t = 0;
            capture(PHRASE_GAP_MS);
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (k == EN_ALGO) {
            all_off();
            set.mode = (uint8_t)(e > 0);
            app_say("MODE", set.mode ? "CHORDS" : "NOTES");
        } else if (k == EN_PRESET) {
            all_off();
            set.tonic = (uint8_t)((set.tonic + 12 + (e > 0 ? 1 : 11)) % 12u);
            ui.have_prev = 0;
            app_say("KEY", NOTE_N[set.tonic]);
        } else if (k == EN_SELECT) {
            all_off();
            set.minor = (uint8_t)(e > 0);
            app_say("SCALE", set.minor ? "MIN" : "MAJ");
        } else if (k == EN_K1) {
            knob((uint8_t *)&p_tone, CC_TONE, e, "TONE");
        } else if (k == EN_K2) {
            knob((uint8_t *)&p_rel, CC_REL, e, "REL");
        } else if (k == EN_K3) {
            knob((uint8_t *)&p_glide, CC_GLIDE, e, "GLIDE");
        }
        set_dirty = 1;
        ui.redraw = 1;
    }
    if (player.playing && !rec_play_poll(&player, now, play_emit, 0))
        ui.redraw = 1;
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn | (set_dirty || take_dirty ? 1u << B_SAVE : 0u) | (player.playing ? 1u << B_PLAY : 0u), keys);
    plat_play_red(0);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen --- */
static void label_value(int32_t x, int32_t y, const char *label, const char *value)
{
    cv_text(x, y, &FONT_XS, label, C_GRAY);
    cv_text(x, y + 14, &FONT_S, value, C_HI);
}

static void draw(void)
{
    char t[24];
    uint32_t now = plat_ms(), i;
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(10, 6, &FONT_B, APP_NAME, C_WHITE);
        x = cv_text(x + 10, 6, &FONT_B, set.mode ? "CHORDS" : "NOTES", C_HI);
        t[0] = 0;
        text_cat(t, NOTE_N[set.tonic]);
        text_cat(t, set.minor ? " MIN" : " MAJ");
        cv_text(230 - text_w(&FONT_B, t), 6, &FONT_B, t, C_GRAY);
    }
    cv_blit(0, 0);

    cv_begin(240, 60, C_BLACK);          /* what sounds: the chord's name, or the notes held */
    if (set.mode && ui.chord_key >= 0)
        cv_text((240 - text_w(&FONT_L, ui.chord_name)) / 2, 12, &FONT_L, ui.chord_name, C_WHITE);
    else if (set.mode && ui.chord_name[0])
        cv_text((240 - text_w(&FONT_L, ui.chord_name)) / 2, 12, &FONT_L, ui.chord_name, C_DIM);
    else {
        int32_t x = 10;
        for (i = 0; i < NKEYS && x < 220; i++)
            if (ui.keys >> i & 1u) {
                uint32_t n = ui.key_note[i];
                t[0] = 0;
                text_cat(t, NOTE_N[n % 12u]);
                x = cv_text(x, 18, &FONT_M, t, C_WHITE) + 8;
            }
    }
    cv_blit(0, 36);

    cv_begin(240, 40, C_BLACK);          /* the knobs */
    int_text(p_tone, t);
    label_value(10, 0, "K1 TONE", t);
    int_text(p_rel, t);
    label_value(90, 0, "K2 REL", t);
    int_text(p_glide, t);
    label_value(170, 0, "K3 GLIDE", t);
    cv_blit(0, 100);

    cv_begin(240, 96, C_BLACK);          /* the history and the take */
    cv_rect(10, 0, 220, 1, C_LINE);
    time_text(ui.hist_ms, t);
    label_value(10, 8, "HISTORY", t);
    int_text((int32_t)(rec_count(&hist) * 100u / HIST_WORDS), t);
    text_cat(t, "% FULL");
    cv_text(90, 22, &FONT_XS, t, C_DIM);
    t[0] = 0;
    if (take.count)
        time_text(take.duration_ms, t);
    else
        text_cat(t, "EMPTY");
    label_value(10, 46, "TAKE", t);
    if (player.playing) {
        time_text(now - player.start_ms, t);
        cv_text(90, 60, &FONT_S, t, C_WHITE);
        cv_text(140, 60, &FONT_XS, "PLAYING", C_HI);
    } else if (take_dirty)
        cv_text(90, 62, &FONT_XS, "NOT SAVED", C_DIM);
    cv_text(10, 82, &FONT_XS, "REC: TAP PHRASE, HOLD ALL", C_DIM);
    cv_blit(0, 144);
}

static void speak(void)
{
    char t[64];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, APP_NAME ". ");
        text_cat(t, set.mode ? "CHORDS. " : "NOTES. ");
        text_cat(t, NOTE_N[set.tonic]);
        text_cat(t, set.minor ? " MIN" : " MAJ");
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.chord_key = -1;
    ui.link = ~plat_midi_out_link();     /* the first frame sends the MPE configuration */
    ui.redraw = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    speak();
    if (now - ui.hist_at >= 500u) {      /* the history's reach on screen, twice a second */
        ui.hist_at = now;
        ui.hist_ms = rec_span_ms(&hist, now);
        ui.redraw = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

/* --------------------------------------------------------------- companion --- */
/* file 0 (after the settings object): the take, take.mvt (tools/mvt2mid.py) */
int app_file(uint32_t i, plat_file_info_t *f)
{
    int n;
    if (i)
        return -1;
    n = plat_file_size(&TAKEF);
    f->name = "take.mvt";
    f->kind = PLAT_FK_PATTERN;
    f->flags = PLAT_FF_READ | PLAT_FF_WRITE | PLAT_FF_DELETE;
    f->size = n > 0 ? (uint32_t)n : 0u;
    f->room = TAKEF.n - PLAT_FILE_HEAD;
    return 0;
}
int app_file_read(uint32_t i, uint32_t off, uint8_t *dst, uint32_t n) { return i ? -1 : plat_file_read(&TAKEF, off, dst, n); }
int app_file_begin(uint32_t i, uint32_t size) { return i ? -1 : plat_file_begin(&TAKEF, size); }
int app_file_write(uint32_t i, uint32_t off, const uint8_t *src, uint32_t n)
{
    return i ? -1 : plat_file_write(&TAKEF, off, src, n);
}
int app_file_commit(uint32_t i, uint32_t size, uint32_t crc)
{
    int rc = i ? -1 : plat_file_commit(&TAKEF, size, crc);
    if (!rc) {                           /* a take from the computer: the one PLAY plays */
        take_load();
        take_dirty = 0;
        ui.redraw = 1;
    }
    return rc;
}
int app_file_delete(uint32_t i)
{
    int rc = i ? -1 : plat_file_delete(&TAKEF);
    if (!rc)
        take.count = 0, ui.redraw = 1;
    return rc;
}
int app_file_busy(void) { return player.playing; }
void app_file_changed(uint32_t obj)
{
    if (obj != OBJ_SETTINGS)
        return;
    settings_load();
    p_tone = set.tone, p_rel = set.rel, p_glide = set.glide;
    set_dirty = 0;
    ui.redraw = 1;
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "mode"))
        *got = set.mode;
    else if (!strcmp(what, "tonic"))
        *got = set.tonic;
    else if (!strcmp(what, "chord_n"))
        *got = ui.chord_n;
    else if (!strcmp(what, "voices"))   /* voices with their gate open */
    {
        uint32_t i;
        *got = 0;
        for (i = 0; i < NVOICE; i++)
            *got += vc[i].gate;
    } else if (!strcmp(what, "hist"))
        *got = (int)rec_count(&hist);
    else if (!strcmp(what, "take"))
        *got = (int)take.count;
    else if (!strcmp(what, "take_ms"))
        *got = (int)take.duration_ms;
    else if (!strcmp(what, "playing"))
        *got = player.playing;
    else if (!strcmp(what, "glide"))
        *got = p_glide;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else
        return 0;
    return 1;
}
#endif
