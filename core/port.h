/* SPDX-License-Identifier: GPL-3.0-only */
/* What core/ needs from C's library, on a desktop (hosted) and on the FM-1 (freestanding: felucca-bsp's chip/libc.c
 * has memset and memcpy, and there are no libc headers). */
#ifndef MVANE_PORT_H
#define MVANE_PORT_H

#if defined(__has_include)
#if __has_include(<string.h>)
#define MVANE_HOSTED 1
#endif
#endif

#ifdef MVANE_HOSTED
#include <string.h>
#else
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
#endif

#define MVANE_INT_MAX 0x7FFFFFFF
static inline int mvane_abs(int v) { return v < 0 ? -v : v; }

#endif
