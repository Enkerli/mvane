/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard's settings record (OBJ_SETTINGS, the file "settings"): the FM-1's (app.c) and the VIBE FM port's
 * (vibe/mvane-yard), the same 52 bytes, so a settings file moves between the two */
#ifndef MVANE_SETTINGS_H
#define MVANE_SETTINGS_H
#include <stdint.h>
#include "synth.h"

#define SET_MAGIC 0x4D56413Au            /* "MVA:" (0.11: SPREAD, and room for more); MVA9 (PHONES MAX, 0.11-beta), MVA8
                                          * (YARD's thirteen, a 0.11 build), 0.10's MVA7 and 0.9's MVA6 are read and
                                          * moved on */
#define SET_MAGIC_11B 0x4D564139u
#define SET_MAGIC_11 0x4D564138u
#define SET_MAGIC_10 0x4D564137u
#define SET_MAGIC_09 0x4D564136u
typedef struct {
    uint32_t magic;
    uint8_t mode;                        /* 0 notes, 1 chords */
    uint8_t tonic;                       /* 0 C .. 11 B */
    uint8_t scale;                       /* CHORD_SCALES */
    int8_t octave;                       /* -2 .. +2 */
    uint8_t p[SP_N_SET];                 /* YARD's: tone, release, glide, attack, decay, res, detune, sub, pluck, wave,
                                          * pw, noise, filter: 0..127 */
    uint8_t type, style, bass, usb_full;
    uint8_t voices;                      /* upper voices, 3..6 */
    uint8_t bank;                        /* black keys: 0 types from the scale, 1 fixed qualities */
    uint8_t play;                        /* 0 POLY, 1 MONO, 2 POLY+BREATH, 3 MONO+BREATH (synth_mode) */
    uint8_t slot;                        /* the take's slot (multiple takes) */
    uint16_t sound;                      /* 0 mvane yard's saw; 1.. Clipper's factory sounds (clp.h), their place in the
                                          * list here; stored as 0x8000 | its key (engine << 8 | preset: the list's
                                          * order may change), before 0.10 as its place in the order as made */
    uint8_t quant;                       /* QUANT: 0 off, 1 basic, 2 full (the PLAY page; not acting yet) */
    uint8_t mpe_out;                     /* 1: a channel a note (MPE); 0: everything on channel 1 */
    uint8_t bend;                        /* the MPE members' bend range sent: BEND_RANGES[bend] */
    uint8_t page;                        /* the knobs' page: 0 PLAY, 1 SOUND */
    uint8_t phones;                      /* PHONES MAX: MASTER's top, 2..20 in 5% (20: 100%, as MASTER was) */
    uint8_t spread;                      /* MPE glide's SPREAD, 0..127 (0: the engines' own glide, as before) */
    uint16_t pcs;                        /* the scale: 0x8000 | its pitch-class set's number (0..4095, bit 0 the tonic,
                                          * always counted in: C major 2741); 0, a record from before 0.12: the named
                                          * scale's (set.scale) */
    uint8_t loop;                        /* 1: the take plays again from its start at its end (SEQ's LOOP) */
    uint8_t arp_mode, arp_rate, arp_gate, arp_oct;   /* the ARP's (ARP held): ARP_MODES, ARP_RATES, 5..100 %, 1..4 */
    uint8_t arp_bpm;                     /* its tempo, 40..240 (below 40: a record from before it, the defaults) */
    uint8_t arp_latch;                   /* 1: ARP on keeps the notes let go (its LATCH, the layer's second page) */
    uint8_t onset, onset_lvl;            /* (0.15) YARD's ONSET and ONSET LVL (its p[]'s fourteenth and fifteenth) */
    uint8_t level;                       /* (0.16) YARD's LEVEL (its p[]'s sixteenth; 0: from before it, 0 dB) */
    uint8_t loud;                        /* (0.16) LOUD, the output's compressor: 0 OFF, 1 +3 DB, 2 +6 DB (app/loud.h) */
    uint8_t room[1];                    /* (for settings to come: 0 their default, so no new record is needed) */
} settings_t;
typedef char settings_same_size[sizeof(settings_t) == 52u ? 1 : -1];   /* (MVA: as 0.11 wrote it: pcs took room) */

#endif
