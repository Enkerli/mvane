/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard 0.4. Everything played becomes MIDI-like events on one bus, which feeds the synth (synth.c), USB-MIDI out
 * (MPE, lower zone) and the always-on history (core/rec). REC captures from the history after the fact, PLAY
 * replays the take through the same bus, SAVE keeps it in the flash, and the companion services give it to a
 * computer as take.mvt (tools/mvt2mid.py makes a MIDI file of it).
 *
 *   keys       NOTES: notes, each on its own MPE channel with its pressure.
 *              CHORDS: white keys are the scale's degrees (up the keyboard, higher voicings); black keys pick the
 *              chord type and stay latched (9, 7, TRIAD, ADD9, SUS, 6/9, QUARTAL, 11, FIFTHS, V/); the last black
 *              key (F#5) is HOLD. A separate bass, smooth voice leading, common tones held, moving voices glide
 *   ALGO       NOTES / CHORDS    PRESET  the key (C .. B)    SELECT  the scale (pitch-class set)
 *   FX         voicing: CLOSE / OPEN / WIDE        LFO   upper voices: 3 .. 6      EDIT  bass on / off
 *   ENV        black keys: types from the scale (STACK 7 TRIAD ADD9 SUS 6/9 QUARTAL 11 FIFTHS V/) or fixed
 *              qualities, as Coral Flower's (M6 M7 M9 S2 S4 m6 m7 m9 M m)
 *   GLO        USB audio level: full / MASTER           SEL   POLY / MONO (legato) / POLY+BREATH / MONO+BREATH
 *   K1 tone    K2 release    K3 glide    K4 attack (CC74, CC72, CC5, CC73 on the master channel: recorded, replayed)
 *   OCT- OCT+  octave        REC  tap: capture the last phrase; hold: capture the whole history
 *   PLAY       play / stop the take                    SAVE  the settings and the take into its slot
 *   SEQ, HOME  the next, the previous of eight take slots (an unsaved take: a warning first)
 *
 * MIDI in (USB, TRS) plays the synth with MPE (per-note bend, slide, pressure) and goes into the history.
 * Part of the unity build (base/device/unity.c, base/host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "chords.c"
#include "rec.c"
#include "synth.h"

#define HIST_WORDS (1u << 15)            /* 128 KiB of history: 32,768 events, however long they took to play */
#define TAKE_WORDS 30000u                /* 117 KiB: a take, in RAM and in the flash */
#define PHRASE_GAP_MS 3000u              /* REC tap: what followed the last silence this long */
#define SLEEP_MS 4000u                   /* the history sleeps: a longer pause is kept as this long */
#define HOLD_KEY 25u                     /* F#5, the last black key */

/* ---------------------------------------------------------------- settings --- */
#define SET_MAGIC 0x4D564134u            /* "MVA4" */
typedef struct {
    uint32_t magic;
    uint8_t mode;                        /* 0 notes, 1 chords */
    uint8_t tonic;                       /* 0 C .. 11 B */
    uint8_t scale;                       /* CHORD_SCALES */
    int8_t octave;                       /* -2 .. +2 */
    uint8_t p[SP_N];                     /* tone, release, glide, attack: 0..127 */
    uint8_t type, style, bass, usb_full;
    uint8_t voices;                      /* upper voices, 3..6 */
    uint8_t bank;                        /* black keys: 0 types from the scale, 1 fixed qualities */
    uint8_t play;                        /* 0 POLY, 1 MONO, 2 POLY+BREATH, 3 MONO+BREATH (synth_mode) */
    uint8_t slot;                        /* the take's slot (multiple takes) */
} settings_t;
static settings_t set;
static uint8_t set_dirty;

static void settings_defaults(void)
{
    static const uint8_t P[SP_N] = {56, 60, 30, 24};
    uint32_t i;
    set.magic = SET_MAGIC;
    set.mode = 1;
    set.tonic = 0;
    set.scale = 0;
    set.octave = 0;
    for (i = 0; i < SP_N; i++)
        set.p[i] = P[i];
    set.type = 0;
    set.style = CHORD_OPEN;
    set.bass = 1;
    set.usb_full = 1;
    set.voices = 5;
    set.bank = 1;
    set.type = CHORD_BANK + 1u;          /* M7 */
}

static void settings_load(void)
{
    uint32_t i, bad = 0;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC)
        bad = 1;
    for (i = 0; i < SP_N; i++)
        bad |= set.p[i] > 127u;
    if (bad || set.mode > 1u || set.tonic > 11u || set.scale >= CHORD_NSCALES || set.octave < -2 || set.octave > 2
        || set.type >= CHORD_NTYPES || set.style >= CHORD_STYLES || set.bass > 1u || set.usb_full > 1u
        || set.voices < 3u || set.voices > CHORD_MAX || set.bank > 1u || set.play > 3u)
        settings_defaults();
}

/* ------------------------------------------------------------- the render --- */
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

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    while (sq_r != sq_w) {
        uint32_t w = sq[sq_r & 255u];
        synth_event(w >> 16, (w >> 8) & 0xFFu, w & 0xFFu);
        sq_r = sq_r + 1u;
    }
    synth_render(lr, n, master_q12);
}

void app_panic(void) { synth_panic(); }

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
static uint8_t knob[SP_N];               /* the knobs as the panel last sent them */

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
    for (c = 1; c < 16u; c++)
        if (ch_notes[c] < ch_notes[best] ||
            (ch_notes[c] == ch_notes[best] && now - ch_free_at[c] > now - ch_free_at[best]))
            best = c;
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

/* -------------------------------------------------------------- the takes --- */
/* Takes kept in the flash: TAKE_SLOTS slots (takes.c). A WebAssembly build that is not fm1-base's own emulator
 * (VIBE FM's converter: its platform has neither plat_file_* nor plat_data_*) keeps the take in RAM only.
 * MVY_TAKE_FLASH 0 or 1 overrides */
#ifndef MVY_TAKE_FLASH
#if defined(__wasm__) && !defined(PLAT_HOST)
#define MVY_TAKE_FLASH 0
#else
#define MVY_TAKE_FLASH 1
#endif
#endif
#if MVY_TAKE_FLASH
#include "takes.c"
/* 0.4's single take (plat_file_*, A/B at the data area's start): brought into RAM once, for the first SAVE */
static const plat_file_t OLD_TAKEF = {PLAT_DATA_LO, PLAT_DATA_LO + 30u * 4096u, 30u * 4096u};
#else
#define TAKE_SLOTS 8u
static int takes_init(void) { return 1; }
static int takes_size(uint32_t k) { (void)k; return -1; }
static const uint8_t *takes_data(uint32_t k) { (void)k; return 0; }
static uint32_t takes_free_sectors(void) { return 0; }
static int takes_begin(uint32_t k, uint32_t n) { (void)k, (void)n; return -1; }
static int takes_write(uint32_t o, const void *s, uint32_t n) { (void)o, (void)s, (void)n; return -1; }
static int takes_commit(uint32_t c) { (void)c; return -1; }
static int takes_delete(uint32_t k) { (void)k; return -1; }
#endif
static uint8_t old_take;                 /* 0.4's take is in RAM, not yet in a slot */

static void take_load(void)              /* the current slot's take into RAM (none: an empty take) */
{
    int n = takes_size(set.slot);
    const uint8_t *p = takes_data(set.slot);
    take.words = take_buf;
    take.count = take.duration_ms = 0;
    if (n > 0 && p && rec_load(&take, p, (uint32_t)n) != 0)
        take.count = 0;
    take_dirty = 0;
}

static int take_save(void)               /* the RAM take into the current slot */
{
    uint32_t head[REC_HEAD_WORDS], size = rec_file_size(&take), crc;
    if (!MVY_TAKE_FLASH)
        return take_dirty = 0, 0;        /* kept in RAM: nowhere to save it */
    rec_file_head(&take, head);
    crc = rec_crc32(0, (const uint8_t *)head, sizeof head);   /* the FM-1 and the host are little-endian */
    crc = rec_crc32(crc, (const uint8_t *)take.words, 4u * take.count);
    if (takes_begin(set.slot, size) || takes_write(0, head, sizeof head)
        || (take.count && takes_write(sizeof head, take.words, 4u * take.count)) || takes_commit(crc))
        return -1;
    take_dirty = old_take = 0;
    return 0;
}

void app_init(void)
{
    uint32_t i;
    settings_load();
    synth_init(set.p);
    for (i = 0; i < SP_N; i++)
        knob[i] = set.p[i];
    rec_init(&hist, hist_buf, HIST_WORDS);
    hist.sleep_ms = SLEEP_MS;
    if (set.slot >= TAKE_SLOTS)
        set.slot = 0;
    take.words = take_buf;
#if MVY_TAKE_FLASH
    if (!takes_init()) {                 /* no slots yet: 0.4's take, if there is one, waits in RAM for SAVE */
        int n = plat_file_size(&OLD_TAKEF);
        const uint8_t *p = plat_file_data(&OLD_TAKEF);
        if (n > 0 && p && rec_load(&take, p, (uint32_t)n) == 0 && take.count)
            take_dirty = old_take = 1;
    } else
#endif
        take_load();
    plat_usb_audio_full(set.usb_full);
    synth_mode(set.play & 1u, set.play >> 1);
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

static void time_text(uint32_t ms, char *b)   /* m:ss, or seconds and tenths under a minute ("8.4 S") */
{
    uint32_t s = ms / 1000u;
    char t[12];
    b[0] = 0;
    if (ms < 60000u) {
        int_text((int32_t)s, b);
        t[0] = '.', t[1] = (char)('0' + ms % 1000u / 100u), t[2] = 0;
        text_cat(b, t);
        text_cat(b, " S");
        return;
    }
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
    int32_t chord_key;                   /* the key whose chord sounds (-1 none; held by HOLD: still set) */
    uint8_t chord_degree, chord_vel, hold;
    int8_t chord_lift;
    uint8_t chord_n, chord_note[CHORD_MAX + 1u], chord_ch[CHORD_MAX + 1u];
    chord_voicing prev;
    uint8_t have_prev;
    char chord_name[16];
    uint32_t rec_t;
    uint32_t link;
    uint32_t hist_ms, hist_at;
    uint32_t slot_warn;                  /* when a slot change last warned of an unsaved take */
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
    bus(0xD0u, 0, 0, LIVE);
    ui.redraw = 1;
}

/* the chord on ui.chord_degree with the current type, scale and voicing, from what sounds now */
static void chord_sound(void)
{
    uint32_t i, j, nn = 0;
    uint8_t notes[CHORD_MAX + 1u], chs[CHORD_MAX + 1u];
    chord_pcs c = chord_build(CHORD_SCALES[set.scale].mask, set.tonic, ui.chord_degree, set.type, set.voices);
    chord_voicing v;
    if (!chord_voice(c, set.style, ui.chord_lift, ui.have_prev ? &ui.prev : 0, &v))
        return;
    ui.prev = v;
    ui.have_prev = 1;
    if (set.bass)
        notes[nn++] = (uint8_t)out_note(v.bass);
    for (i = 0; i < v.n; i++)
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
            bus(0x90u | chs[j], notes[j], ui.chord_vel, LIVE);
        }
    }
    for (j = 0; j < nn; j++)
        ui.chord_note[j] = notes[j], ui.chord_ch[j] = chs[j];
    ui.chord_n = (uint8_t)nn;
    chord_name(c, ui.chord_name);
    ui.redraw = 1;
}

static void chord_key_down(uint32_t k, uint32_t vel)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    uint32_t i, n = chord_scale_size(CHORD_SCALES[set.scale].mask);
    if (k == HOLD_KEY) {                 /* HOLD: chords keep sounding after their key; again, they stop */
        ui.hold ^= 1u;
        if (!ui.hold && ui.chord_key >= 0 && !(ui.keys >> ui.chord_key & 1u))
            chord_release();
        app_say("HOLD", ui.hold ? "ON" : "OFF");
        return;
    }
    for (i = 0; i < 11u; i++)            /* a black key: the chord type, latched; what sounds takes it at once */
        if (BK[i] == k) {
            set.type = (uint8_t)(set.bank * CHORD_BANK + (i < CHORD_BANK ? i : 0u));
            set_dirty = 1;
            if (ui.chord_n)
                chord_sound();
            app_say("TYPE", CHORD_TYPE_NAMES[set.type]);
            return;
        }
    for (i = 0; i < 16u; i++)            /* a white key: a degree, higher up the keyboard */
        if (WK[i] == k) {
            uint32_t lift = i / (n ? n : 7u);
            ui.chord_degree = (uint8_t)(i % (n ? n : 7u));
            ui.chord_lift = (int8_t)(12 * (lift > 2u ? 2u : lift));
            ui.chord_vel = (uint8_t)vel;
            ui.chord_key = (int32_t)k;
            chord_sound();
        }
}

static void all_off(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if (ui.keys >> k & 1u && !set.mode)
            bus(0x80u | ui.key_ch[k], ui.key_note[k], 0, LIVE);
    chord_release();
}

/* a change of key, scale or voicing: a sounding chord follows it; otherwise nothing sounds across it */
static void harmony_changed(void)
{
    if (set.mode && ui.chord_n)
        chord_sound();
    else
        all_off();
    set_dirty = 1;
}

static void turn_knob(uint32_t which, uint32_t cc, int32_t e, const char *name)
{
    char t[8];
    int32_t v = clamp((int32_t)synth_param(which) + e * 2, 0, 127);
    if (v == (int32_t)synth_param(which))
        return;
    synth_set(which, (uint32_t)v);       /* at once, for the screen; the event follows on the bus */
    knob[which] = (uint8_t)v;
    bus(0xB0u, cc, (uint32_t)v, LIVE);   /* the master channel: recorded, so a take replays it */
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

static void slot_step(int32_t d)          /* SEQ / HOME: the next or previous slot's take */
{
    char t[16] = "TAKE ", n[12];
    uint32_t now = plat_ms();
    if (take_dirty && now - ui.slot_warn > 3000u) {
        ui.slot_warn = now;
        app_say("NOT SAVED", "AGAIN: LOSE IT");
        return;
    }
    ui.slot_warn = 0;
    if (player.playing)
        rec_play_stop(&player, play_emit, 0);
    set.slot = (uint8_t)((set.slot + TAKE_SLOTS + (uint32_t)d) % TAKE_SLOTS);
    set_dirty = 1;
    old_take = 0;
    take_load();
    int_text((int32_t)set.slot + 1, n);
    text_cat(t, n);
    if (take.count)
        time_text(take.duration_ms, n);
    else
        n[0] = 0, text_cat(n, "EMPTY");
    app_say(t, n);
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
                chord_key_down(k, ui.vel[k]);
            else {
                ui.key_ch[k] = (uint8_t)mpe_channel();
                ui.key_note[k] = (uint8_t)out_note(53 + (int32_t)k);
                bus(0x90u | ui.key_ch[k], ui.key_note[k], ui.vel[k], LIVE);
            }
        } else if (releasing) {
            if (set.mode) {
                if (ui.chord_key == (int32_t)k && !ui.hold)
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
        } else if (k == B_FX) {
            set.style = (uint8_t)((set.style + 1u) % CHORD_STYLES);
            harmony_changed();
            app_say("VOICING", CHORD_STYLE_NAMES[set.style]);
        } else if (k == B_LFO) {
            set.voices = (uint8_t)(set.voices >= CHORD_MAX ? 3u : set.voices + 1u);
            harmony_changed();
            {
                char t[4] = {(char)('0' + set.voices), 0};
                app_say("VOICES", t);
            }
        } else if (k == B_SEQ || k == B_HOME) {
            slot_step(k == B_SEQ ? 1 : -1);
        } else if (k == B_SEL) {         /* how the synth plays: poly or mono legato, with or without breath */
            static const char *const PLAY_N[4] = {"POLY", "MONO", "POLY+BREATH", "MONO+BREATH"};
            set.play = (uint8_t)((set.play + 1u) & 3u);
            set_dirty = 1;
            all_off();
            synth_mode(set.play & 1u, set.play >> 1);
            app_say("PLAY", PLAY_N[set.play]);
        } else if (k == B_ENV) {         /* the black keys' bank: types from the scale, or fixed qualities */
            set.bank ^= 1u;
            set.type = (uint8_t)(set.bank * CHORD_BANK + set.type % CHORD_BANK);
            harmony_changed();
            app_say("CHORDS", set.bank ? "QUALITIES" : "FROM SCALE");
        } else if (k == B_EDIT) {
            set.bass ^= 1u;
            harmony_changed();
            app_say("BASS", set.bass ? "ON" : "OFF");
        } else if (k == B_GLO) {
            set.usb_full ^= 1u;
            set_dirty = 1;
            plat_usb_audio_full(set.usb_full);
            app_say("USB AUDIO", set.usb_full ? "FULL" : "MASTER");
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
            uint32_t i;
            for (i = 0; i < SP_N; i++)
                set.p[i] = knob[i];      /* the knobs as set, not as a take left them */
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
            set_dirty = 1;
            app_say("MODE", set.mode ? "CHORDS" : "NOTES");
        } else if (k == EN_PRESET) {
            set.tonic = (uint8_t)((set.tonic + (e > 0 ? 1u : 11u)) % 12u);
            harmony_changed();
            app_say("KEY", NOTE_N[set.tonic]);
        } else if (k == EN_SELECT) {
            set.scale = (uint8_t)((set.scale + (e > 0 ? 1u : CHORD_NSCALES - 1u)) % CHORD_NSCALES);
            harmony_changed();
            app_say("SCALE", CHORD_SCALES[set.scale].name);
        } else if (k == EN_K1)
            turn_knob(SP_TONE, SYNTH_CC_TONE, e, "TONE");
        else if (k == EN_K2)
            turn_knob(SP_REL, SYNTH_CC_REL, e, "RELEASE");
        else if (k == EN_K3)
            turn_knob(SP_GLIDE, SYNTH_CC_GLIDE, e, "GLIDE");
        else if (k == EN_K4)
            turn_knob(SP_ATTACK, SYNTH_CC_ATTACK, e, "ATTACK");
        ui.redraw = 1;
    }
    if (player.playing && !rec_play_poll(&player, now, play_emit, 0))
        ui.redraw = 1;
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn | (set_dirty || take_dirty ? 1u << B_SAVE : 0u) | (player.playing ? 1u << B_PLAY : 0u)
                  | (set.usb_full ? 1u << B_GLO : 0u) | (set.bass && set.mode ? 1u << B_EDIT : 0u),
              keys | (ui.hold && set.mode ? 1u << HOLD_KEY : 0u));
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
    static const char *const KN[SP_N] = {"TONE", "REL", "GLIDE", "ATTACK"};
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(10, 6, &FONT_B, set.mode ? "CHORDS" : "NOTES", C_HI);
        (void)x;
        t[0] = 0;
        text_cat(t, NOTE_N[set.tonic]);
        text_cat(t, " ");
        text_cat(t, CHORD_SCALES[set.scale].name);
        cv_text(230 - text_w(&FONT_B, t), 6, &FONT_B, t, C_GRAY);
    }
    cv_blit(0, 0);

    cv_begin(240, 64, C_BLACK);          /* what sounds: the chord's name, or the notes held */
    if (set.mode) {
        if (ui.chord_name[0])
            cv_text((240 - text_w(&FONT_L, ui.chord_name)) / 2, 2, &FONT_L, ui.chord_name,
                    ui.chord_n ? C_WHITE : C_DIM);
        t[0] = 0;
        text_cat(t, CHORD_TYPE_NAMES[set.type]);
        text_cat(t, "  ");
        text_cat(t, CHORD_STYLE_NAMES[set.style]);
        {
            char nv[4] = {' ', ' ', (char)('0' + set.voices), 0};
            text_cat(t, nv);
        }
        text_cat(t, set.bass ? "+BASS" : "");
        {
            char pcs[8] = "  ";
            int_text((int32_t)CHORD_SCALES[set.scale].mask, pcs + 2);   /* the set's number (music-suite: C major 2741) */
            text_cat(t, pcs);
        }
        cv_text(10, 44, &FONT_XS, t, C_GRAY);
        if (ui.hold)
            cv_text(200, 44, &FONT_XS, "HOLD", C_HI);
    } else {
        int32_t x = 10;
        for (i = 0; i < NKEYS && x < 220; i++)
            if (ui.keys >> i & 1u)
                x = cv_text(x, 14, &FONT_M, NOTE_N[ui.key_note[i] % 12u], C_WHITE) + 8;
    }
    cv_blit(0, 34);

    cv_begin(240, 36, C_BLACK);          /* the knobs */
    for (i = 0; i < SP_N; i++) {
        int_text((int32_t)synth_param(i), t);
        label_value(8 + (int32_t)i * 58, 0, KN[i], t);
    }
    cv_blit(0, 100);

    cv_begin(240, 100, C_BLACK);         /* the history and the take */
    cv_rect(10, 0, 220, 1, C_LINE);
    time_text(ui.hist_ms, t);
    label_value(10, 6, "HISTORY", t);
    int_text((int32_t)(rec_count(&hist) * 100u / HIST_WORDS), t);
    text_cat(t, "% FULL");
    cv_text(80, 20, &FONT_XS, t, C_DIM);
    if (rec_sleeping(&hist, now) && rec_count(&hist))
        cv_text(150, 20, &FONT_XS, "ASLEEP", C_DIM);
    t[0] = 0;
    if (take.count)
        time_text(take.duration_ms, t);
    else
        text_cat(t, "EMPTY");
    {
        char l[12] = "TAKE ";
        l[5] = (char)('1' + set.slot), l[6] = 0;
        text_cat(l, "/8");
        label_value(10, 42, l, t);
    }
    if (player.playing) {
        time_text(now - player.start_ms, t);
        cv_text(80, 56, &FONT_S, t, C_WHITE);
        cv_text(130, 58, &FONT_XS, "PLAYING", C_HI);
    } else if (old_take)
        cv_text(80, 58, &FONT_XS, "0.4'S TAKE: SAVE KEEPS IT", C_HI);
    else if (take_dirty)
        cv_text(80, 58, &FONT_XS, "NOT SAVED", C_DIM);
    t[0] = 0;                            /* the render's load and its missed deadlines */
    text_cat(t, "CPU ");
    int_text((int32_t)plat_cpu_pct(), t + 4);
    text_cat(t, "%  XRUNS ");
    {
        char x[12];
        int_text((int32_t)plat_xruns(), x);
        text_cat(t, x);
    }
    cv_text(10, 82, &FONT_XS, t, plat_xruns() ? C_HI : C_DIM);
    {
        static const char *const PLAY_S[4] = {"POLY", "MONO", "POLY+BR", "MONO+BR"};
        cv_text(132, 82, &FONT_XS, PLAY_S[set.play], set.play ? C_HI : C_DIM);
    }
    cv_text(188, 82, &FONT_XS, set.usb_full ? "USB FULL" : "USB MAS", C_DIM);
    cv_blit(0, 140);
}

static void speak(void)
{
    char t[64];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, APP_NAME ". ");
        text_cat(t, set.mode ? "CHORDS. " : "NOTES. ");
        text_cat(t, NOTE_N[set.tonic]);
        text_cat(t, " ");
        text_cat(t, CHORD_SCALES[set.scale].name);
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
    if (now - ui.hist_at >= 500u) {      /* the history's reach and the load on screen, twice a second */
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
/* files after the settings object: the slots' takes, take1.mvt .. take8.mvt (tools/yard.py, web/) */
static char take_names[TAKE_SLOTS][12];
static int write_slot = -1;
int app_file(uint32_t i, plat_file_info_t *f)
{
    int n;
    if (i >= TAKE_SLOTS)
        return -1;
    n = takes_size(i);
    if (!take_names[i][0]) {
        text_cat(take_names[i], "take");
        take_names[i][4] = (char)('1' + i), take_names[i][5] = 0;
        text_cat(take_names[i], ".mvt");
    }
    f->name = take_names[i];
    f->kind = PLAT_FK_PATTERN;
    f->flags = PLAT_FF_READ | PLAT_FF_WRITE | PLAT_FF_DELETE;
    f->size = n > 0 ? (uint32_t)n : 0u;
    f->room = 4u * (REC_HEAD_WORDS + TAKE_WORDS);
    return 0;
}
int app_file_read(uint32_t i, uint32_t off, uint8_t *dst, uint32_t n)
{
    int size = takes_size(i);
    const uint8_t *p = takes_data(i);
    uint32_t k;
    if (size < 0 || !p || off > (uint32_t)size || n > (uint32_t)size - off)
        return -1;
    for (k = 0; k < n; k++)
        dst[k] = p[off + k];
    return 0;
}
int app_file_begin(uint32_t i, uint32_t size)
{
    if (size > 4u * (REC_HEAD_WORDS + TAKE_WORDS))
        return -1;
    write_slot = (int)i;
    return takes_begin(i, size);
}
int app_file_write(uint32_t i, uint32_t off, const uint8_t *src, uint32_t n)
{
    return (int)i == write_slot ? takes_write(off, src, n) : -1;
}
int app_file_commit(uint32_t i, uint32_t size, uint32_t crc)
{
    int rc = (int)i == write_slot ? takes_commit(crc) : -1;
    (void)size;
    write_slot = -1;
    if (!rc && i == set.slot) {          /* the current slot from the computer: the take PLAY plays */
        take_load();
        ui.redraw = 1;
    }
    return rc;
}
int app_file_delete(uint32_t i)
{
    int rc = takes_delete(i);
    if (!rc && i == set.slot)
        take.count = 0, take_dirty = 0, ui.redraw = 1;
    return rc;
}
int app_file_busy(void) { return player.playing; }
void app_file_changed(uint32_t obj)
{
    uint32_t i;
    if (obj != OBJ_SETTINGS)
        return;
    settings_load();
    for (i = 0; i < SP_N; i++)
        synth_set(i, set.p[i]), knob[i] = set.p[i];
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
    else if (!strcmp(what, "scale"))
        *got = set.scale;
    else if (!strcmp(what, "type"))
        *got = set.type;
    else if (!strcmp(what, "style"))
        *got = set.style;
    else if (!strcmp(what, "old_take"))
        *got = old_take;
    else if (!strcmp(what, "slot"))
        *got = set.slot;
    else if (!strcmp(what, "play"))
        *got = set.play;
    else if (!strcmp(what, "nvoices"))
        *got = set.voices;
    else if (!strcmp(what, "hold"))
        *got = ui.hold;
    else if (!strcmp(what, "chord_n"))
        *got = ui.chord_n;
    else if (!strcmp(what, "voices"))
        *got = (int)synth_voices();
    else if (!strcmp(what, "hist"))
        *got = (int)rec_count(&hist);
    else if (!strcmp(what, "hist_ms"))
        *got = (int)rec_span_ms(&hist, plat_ms());
    else if (!strcmp(what, "take"))
        *got = (int)take.count;
    else if (!strcmp(what, "take_ms"))
        *got = (int)take.duration_ms;
    else if (!strcmp(what, "playing"))
        *got = player.playing;
    else if (!strcmp(what, "glide"))
        *got = (int)synth_param(SP_GLIDE);
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else
        return 0;
    return 1;
}
#endif
