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
 *   ALGO       NOTES / CHORDS    PRESETS the sound (mvane yard's saw, then Clipper's 407)
 *   SELECT     the key (C .. B); SEL held + SELECT: the scale (pitch-class set); SEL tapped: POLY / MONO / +BREATH
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
#include "clp.h"

#define HIST_WORDS (1u << 15)            /* 128 KiB of history: 32,768 events, however long they took to play */
#define TAKE_MAX (HIST_WORDS + REC_PRE)  /* the longest take: the whole history (a saved one: in the flash, read in place) */
#define PHRASE_GAP_MS 3000u              /* REC tap: what followed the last silence this long */
#define SLEEP_MS 4000u                   /* the history sleeps: a longer pause is kept as this long */
#define HOLD_KEY 25u                     /* F#5, the last black key */

/* ---------------------------------------------------------------- settings --- */
#define SET_MAGIC 0x4D564136u            /* "MVA6" */
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
    uint16_t sound;                      /* 0 mvane yard's saw; 1.. Clipper's factory sounds (clp.h) */
    uint8_t quant;                       /* QUANT: 0 off, 1 basic, 2 full (the PLAY page; not acting yet) */
    uint8_t mpe_out;                     /* 1: a channel a note (MPE); 0: everything on channel 1 */
    uint8_t bend;                        /* the MPE members' bend range sent: BEND_RANGES[bend] */
    uint8_t page;                        /* the knobs' page: 0 PLAY, 1 SOUND */
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
    set.sound = 0;
    set.quant = 1;                       /* BASIC: the roots the scale's degrees */
    set.mpe_out = 1;
    set.bend = 2;                        /* 48 */
    set.page = 0;
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
        || set.voices < 3u || set.voices > CHORD_MAX || set.bank > 1u || set.play > 3u || set.quant > 2u
        || set.mpe_out > 1u || set.bend > 3u || set.page > 1u)
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

/* the engine the render plays: 0 mvane yard's synth (synth.c), 1 Clipper's (clp.c); a change silences the other first */
static volatile uint8_t eng_want, eng_now;
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    if (eng_want != eng_now) {
        synth_panic();
        clp_panic();
        eng_now = eng_want;
    }
    while (sq_r != sq_w) {
        uint32_t w = sq[sq_r & 255u];
        if (eng_now)
            clp_event(w >> 16, (w >> 8) & 0xFFu, w & 0xFFu);
        else
            synth_event(w >> 16, (w >> 8) & 0xFFu, w & 0xFFu);
        sq_r = sq_r + 1u;
    }
    if (eng_now)
        clp_render(lr, n, master_q12);
    else
        synth_render(lr, n, master_q12);
}

void app_panic(void) { synth_panic(); clp_panic(); }

static void sound_apply(void)            /* the sound setting to the engines (main loop) */
{
    if (set.sound > clp_sounds())
        set.sound = 0;
    if (set.sound)
        clp_select(set.sound - 1u);
    eng_want = set.sound != 0;
}
static int sound_mono(void) { return set.sound && clp_sound_mono(set.sound - 1u); }
static void sound_change_hold(void);     /* (below: what sounds lets go, a held chord comes back on the new sound) */

/* --------------------------------------------------------------------- bus --- */
#define TO_SYNTH 1u
#define TO_OUT 2u
#define TO_HIST 4u
static uint32_t hist_buf[HIST_WORDS] PLAT_POOL;
static rec_ring hist;
static rec_take take;                   /* a capture (in the history, locked) or a saved take (in the flash) */
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

static uint32_t mpe_channel(void)        /* a member channel (MIDI 2..16): free longest, else the one with fewest notes;
                                          * MPE OUT off: channel 1 */
{
    uint32_t c, best = 1, now = plat_ms();
    if (!set.mpe_out)
        return 0;
    for (c = 1; c < 16u; c++)
        if (ch_notes[c] < ch_notes[best] ||
            (ch_notes[c] == ch_notes[best] && now - ch_free_at[c] > now - ch_free_at[best]))
            best = c;
    return best;
}

static const uint8_t BEND_RANGES[4] = {12, 24, 48, 96};
static void mpe_config(void)             /* MPE configuration message: lower zone, 15 member channels (MPE OUT off: none);
                                          * the members' bend range */
{
    uint32_t c;
    bus(0xB0u, 101u, 0u, TO_OUT);
    bus(0xB0u, 100u, 6u, TO_OUT);
    bus(0xB0u, 6u, set.mpe_out ? 15u : 0u, TO_OUT);
    if (!set.mpe_out)
        return;
    for (c = 1; c < 16u; c++) {
        bus(0xB0u | c, 101u, 0u, TO_OUT);
        bus(0xB0u | c, 100u, 0u, TO_OUT);
        bus(0xB0u | c, 6u, BEND_RANGES[set.bend & 3u], TO_OUT);
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
    rec_unlock(&hist);                   /* a capture not saved is let go */
    mvane_zero(&take, sizeof take);
    if (n > 0 && p && rec_load(&take, p, (uint32_t)n) != 0)
        take.count = 0;
    take_dirty = 0;
}

static int take_save(void)               /* the RAM take into the current slot */
{
    uint32_t head[REC_HEAD_WORDS], chunk[128], size = rec_file_size(&take), crc, i, k;
    if (!MVY_TAKE_FLASH)
        return take_dirty = 0, 0;        /* kept in RAM (in the history): nowhere to save it */
    rec_file_head(&take, head);
    crc = rec_crc32(0, (const uint8_t *)head, sizeof head);   /* the FM-1 and the host are little-endian */
    for (i = 0; i < take.count; i += k) {   /* the words through a small buffer: the take is not one array */
        for (k = 0; k < 128u && i + k < take.count; k++)
            chunk[k] = rec_word(&take, i + k);
        crc = rec_crc32(crc, (const uint8_t *)chunk, 4u * k);
    }
    if (takes_begin(set.slot, size) || takes_write(0, head, sizeof head))
        return -1;
    for (i = 0; i < take.count; i += k) {
        for (k = 0; k < 128u && i + k < take.count; k++)
            chunk[k] = rec_word(&take, i + k);
        if (takes_write(4u * (REC_HEAD_WORDS + i), chunk, 4u * k))
            return -1;
    }
    if (takes_commit(crc))
        return -1;
    old_take = 0;
    take_load();                         /* from the flash now: the history is free again */
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
#if MVY_TAKE_FLASH
    if (!takes_init()) {                 /* no slots yet: 0.4's take, if there is one, waits in the history for SAVE
                                          * (copied: its flash is where the first save writes) */
        int n = plat_file_size(&OLD_TAKEF);
        const uint8_t *p = plat_file_data(&OLD_TAKEF);
        rec_take old;
        if (n > 0 && p && rec_load(&old, p, (uint32_t)n) == 0 && old.count && old.count <= HIST_WORDS) {
            uint32_t i;
            for (i = 0; i < old.count; i++)
                hist_buf[i] = rec_word(&old, i);
            hist.head = old.count;
            mvane_zero(&take, sizeof take);
            take.ring = &hist, take.start = 0, take.nring = take.count = old.count;
            take.duration_ms = old.duration_ms;
            hist.lock = 0, hist.locked = 1;
            take_dirty = old_take = 1;
        }
    } else
#endif
        take_load();
    plat_usb_audio_full(set.usb_full);
    synth_mode(set.play & 1u, set.play >> 1);
    clp_init();
    clp_cores(plat_cpu2_set(1) == 0 ? 2u : 1u);   /* the second core renders half of Clipper's voices */
    clp_mode(set.play & 1u, set.play >> 1);
    sound_apply();
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
    int8_t held, latched;                /* the layer button held, the layer a tap left open (-1: none) */
    uint32_t resound_at;                 /* a held chord sounds again on the new sound then (0: none) */
    uint8_t held_used;                   /* a knob turned while it was held: no tap */
    uint32_t knob_at[NE];                /* when each encoder last turned (its label lit) */
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

/* a pitch class (or a note) to the nearest tone of the scale, down on a tie (Coral Flower's: C# to C in C major) */
static uint32_t scale_snap(uint32_t note)
{
    uint16_t mask = CHORD_SCALES[set.scale].mask | 1u;
    uint32_t d;
    for (d = 0; d < 12u; d++) {
        if (note >= d && mask >> ((note - d + 12u - set.tonic) % 12u) & 1u)
            return note - d;
        if (mask >> ((note + d + 12u - set.tonic) % 12u) & 1u)
            return note + d;
    }
    return note;
}

/* the chord on ui.chord_degree with the current type, scale and voicing, from what sounds now */
static void chord_sound(void)
{
    uint32_t i, j, nn = 0;
    uint8_t notes[CHORD_MAX + 1u], chs[CHORD_MAX + 1u];
    /* QUANT (as Coral Flower's): OFF, the roots chromatic from the tonic (a semitone a key); BASIC, the roots the
     * scale's degrees; FULL, the chord's tones too, each to the nearest tone of the scale (the qualities diatonic) */
    uint16_t mask = CHORD_SCALES[set.scale].mask;
    chord_pcs c = chord_build(set.quant ? mask : 0xFFFu, set.tonic, ui.chord_degree, set.type, set.voices);
    chord_voicing v;
    if (set.quant == 2u)
        for (i = 0; i < c.n; i++)
            c.pc[i] = (uint8_t)scale_snap(c.pc[i]);
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
    for (i = 0; i < 16u; i++)            /* a white key: a degree (QUANT OFF: a semitone), higher up the keyboard */
        if (WK[i] == k) {
            uint32_t lift;
            if (!set.quant) {            /* chromatic: the tonic on the fifth key (Coral Flower's layout) */
                int32_t st = (int32_t)i - 4;
                ui.chord_degree = (uint8_t)((st + 24) % 12);
                ui.chord_lift = (int8_t)(st < 0 ? -12 : st >= 12 ? 12 : 0);
            } else {
                lift = i / (n ? n : 7u);
                ui.chord_degree = (uint8_t)(i % (n ? n : 7u));
                ui.chord_lift = (int8_t)(12 * (lift > 2u ? 2u : lift));
            }
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
    if (!rec_capture(&hist, plat_ms(), gap, &take)) {
        app_say("HISTORY", "EMPTY");
        return;
    }
    take_dirty = 1;
    time_text(take.duration_ms, t);
    app_say(gap ? "PHRASE" : "ALL", t);
}

static const char *sound_name(void) { return set.sound ? clp_sound_name(set.sound - 1u) : "SAW"; }

static const char *sound_label(void)    /* the sound's name, then MONO for a mono sound and HEAVY for a costly one */
{
    static char t[32];
    t[0] = 0;
    text_cat(t, sound_name());
    if (sound_mono())
        text_cat(t, " MONO");
    if (set.sound && clp_sound_cost(set.sound - 1u) == 2u)
        text_cat(t, " HEAVY");
    return t;
}

/* a new sound: what sounds lets go first (the engine fades it out and the notes would be lost to it); a chord kept
 * by HOLD, or held under a finger, sounds again on the new sound once its switch is done (Clipper fades for 6 ms, and
 * only four notes wait meanwhile) */
static void sound_change_hold(void)
{
    int keep = set.mode && ui.chord_n && ui.chord_key >= 0 && (ui.hold || (ui.keys >> ui.chord_key & 1u));
    int32_t ck = ui.chord_key;
    all_off();
    sound_apply();
    if (keep) {
        ui.chord_key = ck;
        ui.resound_at = plat_ms() + 40u;
        if (!ui.resound_at)
            ui.resound_at = 1;
    }
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

/* ------------------------------------------------------------- the panel --- */
/* The knobs' meaning comes from the context: a layer held (EDIT, FX, GLO, SEQ), or one a tap left open, else the
 * page (HOME: PLAY, SOUND). knob_info names them for the screen; knob_turn and select_turn act. docs/PANEL.md */
static const char *const QUANT_N[3] = {"OFF", "BASIC", "FULL"};
static const char *const PLAY_N[4] = {"POLY", "MONO", "POLY+BR", "MONO+BR"};
static int32_t layer_now(void) { return ui.held >= 0 ? ui.held : ui.latched; }
static const char *layer_name(int32_t l)
{
    return l == B_EDIT ? "CHORD" : l == B_FX ? "FX" : l == B_GLO ? "SETTINGS" : l == B_SEQ ? "TAKES" : 0;
}

static void knob_info(uint32_t i, const char **label, char *val)
{
    static const char *const SAW_N[4] = {"TONE", "ATTACK", "RELEASE", "GLIDE"};
    static const uint8_t SAW_P[4] = {SP_TONE, SP_ATTACK, SP_REL, SP_GLIDE};
    static const char *const MAC_N[4] = {"BRITE", "TIMBR", "MOTN", "SPACE"};
    static const char *const FX_N[4] = {"DRIVE", "CHORUS", "DELAY", "REVERB"};
    int32_t l = layer_now();
    val[0] = 0;
    *label = "";
    if (l == B_EDIT) {
        static const char *const N[4] = {"VOICING", "VOICES", "BASS", "BANK"};
        *label = N[i];
        if (i == 0)
            text_cat(val, CHORD_STYLE_NAMES[set.style]);
        else if (i == 1)
            int_text(set.voices, val);
        else if (i == 2)
            text_cat(val, set.bass ? "ON" : "OFF");
        else
            text_cat(val, set.bank ? "QUALITY" : "SCALE");
    } else if (l == B_FX) {
        *label = FX_N[i];
        if (set.sound)
            int_text((int32_t)clp_fx(i), val);
        else
            text_cat(val, "-");
    } else if (l == B_GLO) {
        static const char *const N[4] = {"PLAY", "USB AUDIO", "MPE OUT", "BEND"};
        *label = N[i];
        if (i == 0)
            text_cat(val, PLAY_N[set.play & 3u]);
        else if (i == 1)
            text_cat(val, set.usb_full ? "FULL" : "MASTER");
        else if (i == 2)
            text_cat(val, set.mpe_out ? "ON" : "OFF");
        else
            int_text(BEND_RANGES[set.bend & 3u], val);
    } else if (l == B_SEQ) {
        if (i == 0) {
            *label = "SLOT";
            int_text((int32_t)set.slot + 1, val);
        }
    } else if (!set.page) {              /* PLAY: as Coral Flower's */
        static const char *const N[4] = {"QUANT", "KEY", "SCALE", "GLIDE"};
        *label = N[i];
        if (i == 0)
            text_cat(val, QUANT_N[set.quant % 3u]);
        else if (i == 1)
            text_cat(val, NOTE_N[set.tonic]);
        else if (i == 2)
            text_cat(val, CHORD_SCALES[set.scale].name);
        else
            int_text(knob[SP_GLIDE], val);
    } else if (set.sound) {              /* SOUND: Clipper's four macros */
        *label = MAC_N[i];
        int_text((int32_t)clp_macro(i), val);
    } else {                             /* SOUND: mvane yard's saw */
        *label = SAW_N[i];
        int_text((int32_t)synth_param(SAW_P[i]), val);
    }
}

static void cc_turn(uint32_t cc, uint32_t now_v, int32_t e, const char *name)   /* a CC by e steps, recorded */
{
    char t[8];
    int32_t v = clamp((int32_t)now_v + e * 2, 0, 127);
    bus(0xB0u, cc, (uint32_t)v, LIVE);
    int_text(v, t);
    app_say(name, t);
}

static void play_set(uint32_t p)         /* POLY / MONO / +BREATH, both engines */
{
    set.play = (uint8_t)(p & 3u);
    set_dirty = 1;
    all_off();
    synth_mode(set.play & 1u, set.play >> 1);
    clp_mode(set.play & 1u, set.play >> 1);
    app_say("PLAY", PLAY_N[set.play]);
}

static void knob_turn(uint32_t i, int32_t e)
{
    int32_t l = layer_now(), d = e > 0 ? 1 : -1;
    const char *label;
    char val[16];
    if (l == B_EDIT) {
        if (i == 0)
            set.style = (uint8_t)((set.style + CHORD_STYLES + (uint32_t)d) % CHORD_STYLES);
        else if (i == 1)
            set.voices = (uint8_t)clamp(set.voices + d, 3, (int32_t)CHORD_MAX);
        else if (i == 2)
            set.bass = (uint8_t)(e > 0);
        else {
            set.bank = (uint8_t)(e > 0);
            set.type = (uint8_t)(set.bank * CHORD_BANK + set.type % CHORD_BANK);
        }
        harmony_changed();
    } else if (l == B_FX) {
        static const uint8_t CC[4] = {12, 93, 94, 91};   /* DRIVE (mvane yard's CC 12), CHORUS, DELAY, REVERB */
        static const char *const FX_N[4] = {"DRIVE", "CHORUS", "DELAY", "REVERB"};
        if (!set.sound) {
            app_say("FX", "CLIPPER SOUNDS");
            return;
        }
        cc_turn(CC[i], clp_fx(i), e, FX_N[i]);
        return;
    } else if (l == B_GLO) {
        if (i == 0) {
            play_set((set.play + 4u + (uint32_t)d) & 3u);
            return;
        } else if (i == 1) {
            set.usb_full = (uint8_t)(e > 0);
            plat_usb_audio_full(set.usb_full);
        } else if (i == 2) {
            all_off();
            set.mpe_out = (uint8_t)(e > 0);
            mpe_config();
        } else {
            set.bend = (uint8_t)clamp(set.bend + d, 0, 3);
            mpe_config();
        }
        set_dirty = 1;
    } else if (l == B_SEQ) {
        if (i == 0)
            slot_step(d);
        return;
    } else if (!set.page) {
        if (i == 0)
            set.quant = (uint8_t)clamp(set.quant + d, 0, 2);
        else if (i == 1) {
            set.tonic = (uint8_t)((set.tonic + 12u + (uint32_t)d) % 12u);
            harmony_changed();
        } else if (i == 2) {
            set.scale = (uint8_t)((set.scale + CHORD_NSCALES + (uint32_t)d) % CHORD_NSCALES);
            harmony_changed();
        } else {
            turn_knob(SP_GLIDE, SYNTH_CC_GLIDE, e, "GLIDE");
            return;
        }
        set_dirty = 1;
    } else if (set.sound) {
        static const char *const MAC_N[4] = {"BRITE", "TIMBR", "MOTN", "SPACE"};
        cc_turn(16u + i, clp_macro(i), e, MAC_N[i]);   /* Clipper's macros, CC 16..19: recorded */
        return;
    } else {
        static const uint8_t P[4] = {SP_TONE, SP_ATTACK, SP_REL, SP_GLIDE};
        static const uint8_t CC[4] = {SYNTH_CC_TONE, SYNTH_CC_ATTACK, SYNTH_CC_REL, SYNTH_CC_GLIDE};
        static const char *const N[4] = {"TONE", "ATTACK", "RELEASE", "GLIDE"};
        turn_knob(P[i], CC[i], e, N[i]);
        return;
    }
    knob_info(i, &label, val);
    app_say(label, val);
}

static void select_turn(int32_t e)       /* SELECT: what the context lists */
{
    int32_t l = layer_now(), d = e > 0 ? 1 : -1;
    if (l == B_EDIT) {                   /* the chord type, in its bank */
        set.type = (uint8_t)(set.bank * CHORD_BANK + (set.type % CHORD_BANK + CHORD_BANK + (uint32_t)d) % CHORD_BANK);
        set_dirty = 1;
        if (ui.chord_n)
            chord_sound();
        app_say("TYPE", CHORD_TYPE_NAMES[set.type]);
    } else if (l == B_FX) {              /* the reverb's model */
        if (set.sound)
            app_say("REVERB", clp_reverb_step(d));
    } else if (l == B_GLO) {             /* USB: a computer, or host to a MIDI device (the Diosynth) */
        plat_usb_host(e > 0);
        app_say("USB", e > 0 ? "HOST: A DEVICE" : "TO A COMPUTER");
    } else if (l == B_SEQ)
        slot_step(d);
    else {                               /* the play pages: a jump to the next / previous engine's sounds */
        uint32_t ns = clp_sounds() + 1u, k = set.sound, n;
        const char *eng = set.sound ? clp_sound_engine(set.sound - 1u) : "";
        for (n = 0; n < ns; n++) {
            k = (k + ns + (uint32_t)d) % ns;
            if (!k || clp_sound_engine(k - 1u) != eng)
                break;
        }
        if (d < 0 && k) {                /* back: to that engine's first sound */
            const char *e2 = clp_sound_engine(k - 1u);
            while (k > 1u && clp_sound_engine(k - 2u) == e2)
                k--;
        }
        set.sound = (uint16_t)k;
        set_dirty = 1;
        sound_change_hold();
        app_say(set.sound ? clp_sound_engine(set.sound - 1u) : "YARD", sound_label());
    }
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
    {                                    /* USB host (GLO + SELECT): what happened, once */
        static const char *const M[7] = {"", "LOOKING", "OFF", "DEVICE", "NOT MIDI", "NO POWER", "LOST"};
        int m = plat_usb_host_msg();
        if (m > USBH_MSG_NONE && m <= USBH_MSG_LOST)
            app_say("USB HOST", m == USBH_MSG_MIDI && plat_usb_host_name()[0] ? plat_usb_host_name() : M[m]);
        if (plat_usb_lost())
            all_off();
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
                ui.key_note[k] = (uint8_t)out_note((int32_t)(set.quant ? scale_snap(53u + k) : 53u + k));
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
            uint32_t other = k == B_OCTDN ? B_OCTUP : B_OCTDN;
            if (btn >> other & 1u) {     /* both: panic, everything off */
                all_off();
                bus(0xB0u, 123u, 0u, TO_SYNTH | TO_OUT);
                app_say("PANIC", "ALL OFF");
            } else {
                int32_t o = clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
                char t[4];
                if (o != set.octave) {
                    all_off();
                    set.octave = (int8_t)o;
                    set_dirty = 1;
                    t[0] = o < 0 ? '-' : '+', t[1] = (char)('0' + (o < 0 ? -o : o)), t[2] = 0;
                    app_say("OCTAVE", t);
                }
            }
        } else if (k == B_EDIT || k == B_FX || k == B_GLO || k == B_SEQ) {   /* a layer: held, or a tap opens it */
            ui.held = (int8_t)k;
            ui.held_used = 0;
        } else if (k == B_HOME) {        /* the knobs' page: PLAY / SOUND */
            ui.latched = -1;
            set.page ^= 1u;
            set_dirty = 1;
            app_say("KNOBS", set.page ? "SOUND" : "PLAY");
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
    if (ui.held >= 0 && !(btn >> ui.held & 1u)) {   /* a layer button let go: a tap opens or closes its page */
        if (!ui.held_used)
            ui.latched = ui.latched == ui.held ? -1 : ui.held;
        ui.held = -1;
        ui.redraw = 1;
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
        ui.knob_at[k] = now ? now : 1u;
        if (ui.held >= 0)
            ui.held_used = 1;
        if (k == EN_ALGO) {
            all_off();
            set.mode = (uint8_t)(e > 0);
            set_dirty = 1;
            app_say("MODE", set.mode ? "CHORDS" : "NOTES");
        } else if (k == EN_PRESET) {     /* PRESETS: the sound (0 mvane yard's saw, then Clipper's) */
            uint32_t ns = clp_sounds() + 1u;
            int32_t d = e > 0 ? (e > 4 ? 8 : 1) : (e < -4 ? -8 : -1);   /* a fast turn: eight at a time */
            set.sound = (uint16_t)(((int32_t)set.sound + d + 8 * (int32_t)ns) % (int32_t)ns);
            set_dirty = 1;
            sound_change_hold();
            app_say(set.sound ? clp_sound_engine(set.sound - 1u) : "YARD", sound_label());
        } else if (k == EN_SELECT)
            select_turn(e);
        else
            knob_turn(k - EN_K1, e);
        ui.redraw = 1;
    }
    if (ui.resound_at && (int32_t)(now - ui.resound_at) >= 0) {   /* the held chord, on the new sound */
        ui.resound_at = 0;
        if (ui.chord_key >= 0 && !ui.chord_n)
            chord_sound();
    }
    if (player.playing && !rec_play_poll(&player, now, play_emit, 0))
        ui.redraw = 1;
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds(btn | (set_dirty || take_dirty ? 1u << B_SAVE : 0u) | (player.playing ? 1u << B_PLAY : 0u)
                  | (layer_now() >= 0 ? 1u << layer_now() : 0u) | (set.page ? 1u << B_HOME : 0u),
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
    int32_t lay = layer_now();
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        const char *sn = sound_name();
        if (lay >= 0)                    /* a layer open: its name, lit */
            cv_text(10, 6, &FONT_B, layer_name(lay), C_WHITE);
        else
            cv_text(10, 6, &FONT_B, set.mode ? "CHORDS" : set.page ? "NOTES" : "NOTES", C_HI);
        cv_text(230 - text_w(&FONT_B, sn), 6, &FONT_B, sn, C_WHITE);
    }
    cv_blit(0, 0);

    cv_begin(240, 64, C_BLACK);          /* what sounds: the chord's name, or the notes held; a layer's own page */
    if (lay == B_SEQ) {                  /* TAKES: the eight slots, the current one lit, the full ones filled */
        for (i = 0; i < TAKE_SLOTS; i++) {
            int32_t x = 10 + (int32_t)i * 28, full = i == set.slot ? take.count != 0 : takes_size(i) > 16;
            char d[3] = {(char)('1' + i), 0, 0};
            cv_rect(x, 6, 24, 30, i == set.slot ? C_HI : C_LINE);
            if (!full)
                cv_rect(x + 2, 8, 20, 26, C_BLACK);
            cv_text(x + 8, 13, &FONT_S, d, full && i == set.slot ? C_BLACK : i == set.slot ? C_HI : C_GRAY);
        }
        cv_text(10, 44, &FONT_XS, "SELECT: THE SLOT   REC, PLAY, SAVE AS EVER", C_GRAY);
    } else if (lay == B_EDIT) {          /* CHORD: the black keys' types in this bank */
        int32_t x = 10;
        for (i = 0; i < CHORD_BANK; i++) {
            uint32_t ty = set.bank * CHORD_BANK + i;
            x = cv_text(x, i < 5u ? 4 : 22, &FONT_S, CHORD_TYPE_NAMES[ty], ty == set.type ? C_WHITE : C_GRAY) + 10;
            if (i == 4u)
                x = 10;
        }
        cv_text(10, 44, &FONT_XS, set.bank ? "BLACK KEYS: QUALITIES   SELECT: TYPE" : "BLACK KEYS: FROM THE SCALE   SELECT: TYPE",
                C_GRAY);
    } else if (set.mode) {
        if (ui.chord_name[0])
            cv_text((240 - text_w(&FONT_L, ui.chord_name)) / 2, 2, &FONT_L, ui.chord_name,
                    ui.chord_n ? C_WHITE : C_DIM);
        t[0] = 0;
        text_cat(t, NOTE_N[set.tonic]);
        text_cat(t, " ");
        text_cat(t, CHORD_SCALES[set.scale].name);
        text_cat(t, "  ");
        text_cat(t, CHORD_TYPE_NAMES[set.type]);
        text_cat(t, " ");
        text_cat(t, CHORD_STYLE_NAMES[set.style]);
        {
            char nv[4] = {' ', ' ', (char)('0' + set.voices), 0};
            text_cat(t, nv);
        }
        text_cat(t, set.bass ? "+BASS" : "");
        {
            char pcs[8] = " ";
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

    cv_begin(240, 36, C_BLACK);          /* the knobs: what they are here, the one turning lit */
    for (i = 0; i < 4u; i++) {
        const char *lb;
        int lit = ui.knob_at[EN_K1 + i] && now - ui.knob_at[EN_K1 + i] < 1200u;
        knob_info(i, &lb, t);
        cv_text(8 + (int32_t)i * 58, 0, &FONT_XS, lb, lit ? C_WHITE : C_GRAY);
        cv_text(8 + (int32_t)i * 58, 14, &FONT_S, t, lit ? C_WHITE : C_HI);
    }
    cv_blit(0, 100);

    cv_begin(240, 100, C_BLACK);         /* the history and the take */
    cv_rect(10, 0, 220, 1, C_LINE);
    time_text(ui.hist_ms, t);
    label_value(10, 6, "HISTORY", t);
    int_text((int32_t)(rec_count(&hist) * 100u / HIST_WORDS), t);
    text_cat(t, "% FULL");
    cv_text(80, 20, &FONT_XS, t, C_DIM);
    if (hist.full)                       /* the history reached a capture not yet saved */
        cv_text(150, 20, &FONT_XS, "FULL: SAVE", C_HI);
    else if (rec_sleeping(&hist, now) && rec_count(&hist))
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
    text_cat(t, "%  XR ");
    {
        char x[12];
        int_text((int32_t)plat_xruns(), x);
        text_cat(t, x);
    }
    cv_text(10, 82, &FONT_XS, t, plat_xruns() ? C_HI : C_DIM);
    if (set.sound) {                     /* the sound's measured cost (tools/clp_cost.c) */
        static const char *const COST_N[3] = {"LIGHT", "MID", "HEAVY"};
        uint32_t c = clp_sound_cost(set.sound - 1u);
        cv_text(104, 82, &FONT_XS, COST_N[c], c == 2u ? C_HI : C_DIM);
    }
    {
        static const char *const PLAY_S[4] = {"POLY", "MONO", "POLY+BR", "MONO+BR"};
        const char *ps = sound_mono() && !(set.play & 1u) ? "MONO SOUND" : PLAY_S[set.play];
        cv_text(230 - text_w(&FONT_XS, ps), 82, &FONT_XS, ps, set.play || sound_mono() ? C_HI : C_DIM);
    }
    if (plat_usb_host_state())
        cv_text(150, 82, &FONT_XS, plat_usb_host_state() == USBH_CONNECTED ? "HOST" : "HOST..", C_HI);
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
    ui.held = ui.latched = -1;
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
    f->room = 4u * (REC_HEAD_WORDS + TAKE_MAX);
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
    if (size > 4u * (REC_HEAD_WORDS + TAKE_MAX))
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
    else if (!strcmp(what, "sound"))
        *got = set.sound;
    else if (!strcmp(what, "scale_n"))
        *got = (int)clp_sounds();
    else if (!strcmp(what, "slot"))
        *got = set.slot;
    else if (!strcmp(what, "note_last"))   /* the last key's note (NOTES) */
        *got = ui.key_note[1];
    else if (!strcmp(what, "chord_key"))
        *got = ui.chord_key;
    else if (!strcmp(what, "layer"))
        *got = layer_now();
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "mpe_out"))
        *got = set.mpe_out;
    else if (!strcmp(what, "quant"))
        *got = set.quant;
    else if (!strcmp(what, "bass"))
        *got = set.bass;
    else if (!strcmp(what, "bank"))
        *got = set.bank;
    else if (!strcmp(what, "nvoices2"))
        *got = set.voices;
    else if (!strcmp(what, "play"))
        *got = set.play;
    else if (!strcmp(what, "nvoices"))
        *got = set.voices;
    else if (!strcmp(what, "hold"))
        *got = ui.hold;
    else if (!strcmp(what, "chord_n"))
        *got = ui.chord_n;
    else if (!strcmp(what, "voices"))   /* the engine's that plays */
        *got = (int)(eng_now ? clp_voices() : synth_voices());
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
