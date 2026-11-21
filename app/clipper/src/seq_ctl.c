/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; Clipper contributors */
/* From Clipper's seq.c (7bc15a9), verbatim: a part's performance controllers and the standard CC map */

/* SLOOP 2.5: MIDI CCs set track parameters, after Felucca 1.1.5's standard CC map (#103, Leo Kuroshita).
 * A CC acts on the track its channel plays, as the notes do (1-3 the synths, the drum channel the drum
 * track, 4-16 the selected track), and sets its parameter as a knob would: 0..127 over the parameter's
 * range, 64 the middle of a bipolar one. 5 GLIDE, 7 LEVEL, 10 PAN, 71 the engine's resonance (RES or Q;
 * an engine without one ignores it), 72 / 73 / 75 release / attack / decay, 74 the track's FILTER (64 off,
 * below a low-pass, above a high-pass: on every engine and the drums), 91 / 93 / 94 the reverb, chorus and
 * delay sends. The drum track takes 7, 91 and 94 as GLO > DRUMS LVL, REV and DLY (2.5), and 10 and 74. */
#define MCC_RES 0xFFu
#define MCC_CUT 0xFEu                             /* (0.7, after SLOOP 2.7) CC 74: the engine's own cutoff / brightness */
static const uint8_t MIDI_CC_MAP[][2] = {
    {5, P_GLIDE}, {7, P_LEVEL}, {10, P_PAN}, {71, MCC_RES}, {72, P_REL}, {73, P_ATK}, {74, MCC_CUT}, {75, P_DEC},
    {91, P_REV}, {93, P_CHOR}, {94, P_DLY}, {76, P_LRATE}, {77, P_LD_PIT}, {78, P_LFADE},   /* (1.0: 76..78 the LFO's
                                                                                     * rate, vibrato depth, delay) */
};
/* Clipper: the performance controllers of a part (eng_hybrid.c part_ctl: the matrix's MW, AT, BEND, EXPR):
 * pitch bend (En, every engine: voice.c), channel and polyphonic pressure (Dn, An), and the CCs 1 (mod
 * wheel), 11 (expression), 64 (sustain pedal), 16..19 (HYBRID: the macros M1..M4; 1.0, the others: MACROS) */
static void part_ctl_event(track_t *t, uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t p = (uint32_t)(t - trk), i;
    part_ctl_t *c;
    if (p >= NPART)
        return;
    c = &part_ctl[p];
    if (st == 0xE0u) {
        c->bend = (int16_t)((((int32_t)d2 << 7 | (int32_t)d1) - 8192) * 4);
    } else if (st == 0xD0u) {
        c->at = (uint8_t)(d1 & 127u);
    } else if (st == 0xA0u) {
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].gate && t->v[i].note == d1)
                t->v[i].pat = (uint8_t)(d2 & 127u);
    }
}
/* a project load or NEW (after SLOOP 2.6): the pitch wheel back to the middle, no pressure, the pedal up (what it held
 * lets go; the mod wheel stays where the controller has it),
 * MPE's per-channel bends, pressures and slides at rest: nothing of the last performance stays on the new sound */
static void part_ctl_reset(void)
{
    uint32_t p;
    for (p = 0; p < NPART; p++) {
        part_ctl_t *c = &part_ctl[p];
        c->bend = 0;
        c->at = 0;
        if (c->sus) {
            c->sus = 0;
            trk_sustain_up(&trk[p]);
        }
    }
#if CLIPPER_SYNTH
    mpe_reset();
#endif
}
static int part_ctl_cc(track_t *t, uint32_t cc, uint32_t value)   /* 1: taken */
{
    uint32_t p = (uint32_t)(t - trk);
    part_ctl_t *c;
    if (p >= NPART)
        return 0;
    c = &part_ctl[p];
    switch (cc) {
    case 1: c->mw = (uint8_t)value; return 1;
    case 11:                                          /* (1.0) expression: the part's volume too (fx.c), as GM */
        c->expr = (uint8_t)value;
        c->exoff = (uint8_t)(127u - value);
        return 1;
    case 0: midi_bank = (uint8_t)value; return 1;
    case 2:                                           /* (1.0) breath (GLO > MIDI 2 > EWI) */
        if (midi_breath)
            c->exoff = (uint8_t)(127u - value);
        if (midi_breath > 1u)
            c->at = (uint8_t)value;
        return 1;    /* (1.0) bank select, MSB: the next program change's */
    case 101: case 100: midi_rpn[cc & 1u] = (uint8_t)value; return 1;   /* (1.0) RPN; NRPN: none selected */
    case 99: case 98: midi_rpn[0] = midi_rpn[1] = 127; return 1;
    case 6:                                           /* (1.0) data entry: RPN 0, the pitch bend range in semitones */
        if (!midi_rpn[0] && !midi_rpn[1])
            midi_bend = (uint8_t)clamp((int32_t)value, 1, 24);
        return 1;
    case 120: case 123: case 124: case 125: case 126: case 127:   /* (1.0) all sound / notes off (and the mode
                                                                 * messages, which mean it too) */
        panic_req |= 1u << p;
        return 1;
    case 121:                                         /* (1.0) reset all controllers */
        c->bend = 0;
        c->at = c->mw = c->expr = c->exoff = 0;
        midi_rpn[0] = midi_rpn[1] = 127;
        value = 0;                                    /* (and the pedal up) */
        /* fall through */
    case 64:
        if (value >= 64u) {
            c->sus = 1;
        } else if (c->sus) {
            c->sus = 0;
            trk_sustain_up(t);
        }
        return 1;
    case 16: case 17: case 18: case 19:
        if (t->eng_req == ENGI_HYBRID)
            t->p[P_E4 + cc - 16u] = (int16_t)value;
#if CLIPPER_SYNTH
        else                                          /* (1.0) the other engines: the sound's MACROS (BRITE TIMBR: centred) */
            t->p[P_MAC1 + cc - 16u] = (int16_t)(cc < 18u ? (int32_t)value - 64 : (int32_t)value);
#endif
        return 1;
#if CLIPPER_SYNTH
    case 20:                                          /* Clipper 0.3: MORPH (ui_synth.c, the UI applies it) */
        syn_morph_req = (int16_t)(value & 127u);
        return 1;
#endif
    case 122:                                         /* (1.0) local control (GLO > MIDI > LOCAL) */
        midi_local_off = value < 64u;
        panic_req |= 1u << p;
        return 1;
    default:
        return 0;
    }
}

/* 0.7, after SLOOP 2.7 (renebohne's pull request #71, Felucca 1.1.5's standard CC map): CC 74 ("brightness", the cutoff
 * knob of most controllers) sweeps the engine's own cutoff or brightness over its range, from dark to bright: its EDIT
 * value named CUT, else the one that brightens it (PHASE's DCW, FM6's and FMX's modulation, PHYS's BRIT...); DIGITAL,
 * VOICE, WHEEL and the drum track keep the track FILTER (64 off, as before) */
static uint32_t cc74_slot(const engine_t *e)      /* 0..7, or 8: none */
{
    static const struct { const char *name; uint8_t slot; } T[] = {
        {"PHASE", 2}, {"LOFI", 7}, {"GRAIN", 7}, {"FM6", 2}, {"PHYS", 2}, {"NOISE", 2}, {"SLICE", 7}, {"FMX", 2}};
    uint32_t i;
    for (i = 0; i < 8u; i++)
        if (e->edit[i].label && str_eq(e->edit[i].label, "CUT") && e->edit[i].max > e->edit[i].min && !e->edit[i].names)
            return i;
    for (i = 0; i < NELEM(T); i++)
        if (str_eq(e->name, T[i].name) && e->edit[T[i].slot].max > e->edit[T[i].slot].min &&
            !e->edit[T[i].slot].names)
            return T[i].slot;
    return 8u;
}
/* the parameter a mapped CC sets on track t: its range, *slotp its value; 0 none (1.0: shared with CC OUT) */
static const param_desc_t *__attribute__((noinline)) cc_target(track_t *t, uint32_t cc, int16_t **slotp)
{
    const param_desc_t *d = 0;
    int16_t *slot = 0;
    uint32_t i, id = 0xFFFFu;
    for (i = 0; i < NELEM(MIDI_CC_MAP); i++)
        if (MIDI_CC_MAP[i][0] == cc)
            id = MIDI_CC_MAP[i][1];
    if (id == 0xFFFFu)
        return 0;
    if (id == MCC_CUT) {                               /* (0.7) the engine's cutoff, else the track FILTER */
        uint32_t k = is_drum(t) ? 8u : cc74_slot(ENGINES[t->eng_req % NENGINES]);
        if (k < 8u) {
            d = &ENGINES[t->eng_req % NENGINES]->edit[k];
            slot = &t->p[P_E0 + k];
        } else {
            id = P_TFLT;
        }
    }
    if (d) {
    } else if (is_drum(t)) {
        if (id == P_LEVEL || id == P_REV || id == P_DLY) {
            id = id == P_LEVEL ? G_DRLVL : id == P_REV ? G_DRREV : G_DRDLY;
            d = &GP[id];
            slot = &song.g[id];
        } else if (id == P_PAN || id == P_TFLT) {
            d = &TP[id];
            slot = &t->p[id];
        }
    } else if (id == MCC_RES) {
        const engine_t *e = ENGINES[t->eng_req % NENGINES];
        for (i = 0; i < 8u && !d; i++)
            if (str_eq(e->edit[i].label, "RES") || str_eq(e->edit[i].label, "Q")) {
                d = &e->edit[i];
                slot = &t->p[P_E0 + i];
            }
    } else {
        d = &TP[id];
        slot = &t->p[id];
    }
    *slotp = slot;
    return d && d->max > d->min ? d : 0;
}
static void midi_cc(track_t *t, uint32_t cc, uint32_t value)
{
    int16_t *slot;
    const param_desc_t *d;
    if (!part_ctl_cc(t, cc, value) && (d = cc_target(t, cc, &slot)) != 0)
        *slot = (int16_t)(d->min + ((int32_t)value * (d->max - d->min) + 63) / 127);
}
