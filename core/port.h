/* SPDX-License-Identifier: GPL-3.0-only */
/* What core/ needs from C's library, written out, so it builds anywhere: on a desktop, on the FM-1 (freestanding:
 * no libc headers, felucca-bsp's chip/libc.c) and in a WebAssembly converter with its own libc. core/ declares no
 * library function itself, so it can never clash with one. */
#ifndef MVANE_PORT_H
#define MVANE_PORT_H

#define MVANE_INT_MAX 0x7FFFFFFF
static inline int mvane_abs(int v) { return v < 0 ? -v : v; }
static inline void mvane_copy(void *d, const void *src, unsigned n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)src;
    while (n--)
        *p++ = *q++;
}
static inline void mvane_zero(void *d, unsigned n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--)
        *p++ = 0;
}

#endif
