/* SPDX-License-Identifier: GPL-3.0-only */
/* Clipper's synth (app/clipper, the FM-1's vendored copy), its own unit as on the FM-1: mvane_yard.c reaches it
 * through clp.h. vibe/build.sh gives the include paths (Clipper's sources include each other by name). */
#include "clp.c"

/* the FM-1's second core (fm1-base's plat_cpu2_*), which Clipper asks for: none here (one core first, as the Hub's
 * guide says; the SDK's VIBE_JOBS later), so it renders every voice itself */
int plat_cpu2_on(void) { return 0; }
int plat_cpu2_run(void (*fn)(uint32_t), uint32_t arg) { (void)fn, (void)arg; return 0; }
int plat_cpu2_wait(void) { return -1; }
