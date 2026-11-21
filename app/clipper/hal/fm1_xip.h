/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard's stand-in for Felucca's hal/fm1_xip.h: Clipper's SAMPLE engine reads its user slots through it, which
 * clp.c points at an empty slot instead (SMP_USER_XIP): nothing here reads the flash. */
#pragma once
#include <stdint.h>
static inline const uint8_t *fm1_xip_ptr(uint32_t off) { (void)off; return 0; }
