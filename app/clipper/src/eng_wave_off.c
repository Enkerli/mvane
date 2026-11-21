/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard: Clipper's WAVE engine left out (eng_wave.c: its band-limited tables take 72 KB of RAM, which mvane
 * yard's recorder needs). Its place in ENGINES[] stays (the engine numbers are Clipper's), with no sound: nothing
 * selects it. CLP_WAVE 1 builds the real one. */
static volatile int16_t wt_have = -1;
static int16_t wt_tab_eff[NPART];
#define WT_NALL 1u
static void wt_user_scan(uint32_t k) { (void)k; }
static void wave_note_on(struct track *t, voice_t *v) { (void)t, (void)v; }
static void wave_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t, (void)v, (void)out, (void)n, (void)m;
}
static void wave_spread(track_t *t, voice_t *v, uint32_t r) { (void)t, (void)v, (void)r; }   /* (voice.c unison_spread) */
static const preset_t WAVE_OFF[1];
static const engine_t ENG_WAVE = {"WAVE", {"WAVE", "FLT"}, {{0}}, WAVE_OFF, 0, -1, wave_note_on, wave_render};
