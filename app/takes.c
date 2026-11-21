/* SPDX-License-Identifier: GPL-3.0-only */
/* mvane yard's takes in the flash: TAKE_SLOTS slots sharing the data area, each in the sectors it needs. A directory
 * (two copies, A and B: the newer whole one is current) says where each slot's take is: its first sector, how many,
 * its bytes and their CRC-32. A save writes the new take into free sectors (never over a slot's current copy), reads
 * it back, then writes the directory: a power cut leaves the previous take. Main loop only (plat_data_*: the sound
 * stops for each erase). Part of the unity build, included by app.c. */

#define TAKE_SLOTS 8u
#define TD_A PLAT_DATA_LO                /* the directory's copies */
#define TD_B (PLAT_DATA_LO + 0x1000u)
#define TD_DATA (PLAT_DATA_LO + 0x2000u) /* the takes' sectors, up to the panel's store (APP_STORE_MAP) */
#define TD_END 0xDC000u
#define TD_NSECT ((TD_END - TD_DATA) / 0x1000u)
#define TD_MAGIC 0x4454564Du             /* "MVTD" */

typedef struct {
    uint16_t start, nsect;               /* sector index from TD_DATA; nsect 0: empty */
    uint32_t size, crc;
} td_entry_t;
typedef struct {
    uint32_t magic, seq;
    td_entry_t e[TAKE_SLOTS];
    uint32_t crc;                        /* of everything before it */
} td_dir_t;

static td_dir_t td;                      /* the current directory, in RAM */
static uint32_t td_at;                   /* where it was read from (TD_A or TD_B; 0: none yet) */
static struct {
    int slot;                            /* -1: no write going on */
    uint32_t start, nsect, size, pos;
} tw = {-1, 0, 0, 0, 0};

static int td_ok(const td_dir_t *d)
{
    return d->magic == TD_MAGIC && d->crc == rec_crc32(0, (const uint8_t *)d, (uint32_t)((const uint8_t *)&d->crc - (const uint8_t *)d));
}

/* the directory from the flash: 1 if there is one */
static int takes_init(void)
{
    const td_dir_t *a = (const td_dir_t *)plat_data(TD_A), *b = (const td_dir_t *)plat_data(TD_B);
    int oa = a && td_ok(a), ob = b && td_ok(b);
    mvane_zero(&td, sizeof td);
    td.magic = TD_MAGIC;
    td_at = 0;
    if (oa && (!ob || a->seq - b->seq < 0x80000000u))
        td = *a, td_at = TD_A;
    else if (ob)
        td = *b, td_at = TD_B;
    return td_at != 0;
}

static int takes_size(uint32_t k) { return k < TAKE_SLOTS && td.e[k].nsect ? (int)td.e[k].size : -1; }
static const uint8_t *takes_data(uint32_t k)
{
    return k < TAKE_SLOTS && td.e[k].nsect ? plat_data(TD_DATA + 0x1000u * td.e[k].start) : 0;
}

static uint32_t takes_free_sectors(void)
{
    uint32_t k, used = 0;
    for (k = 0; k < TAKE_SLOTS; k++)
        used += td.e[k].nsect;
    return TD_NSECT - used;
}

/* a new take of size bytes for slot k: free sectors found (first fit, past every slot's current copy). 0 = ready */
static int takes_begin(uint32_t k, uint32_t size)
{
    uint32_t need = (size + 0xFFFu) / 0x1000u, s, j;
    tw.slot = -1;
    if (k >= TAKE_SLOTS || !size || need > TD_NSECT)
        return -1;
    for (s = 0; s + need <= TD_NSECT; s++) {
        int clash = 0;
        for (j = 0; j < TAKE_SLOTS && !clash; j++) {
            const td_entry_t *e = &td.e[j];
            clash = e->nsect && s < (uint32_t)e->start + e->nsect && e->start < s + need;
        }
        if (!clash)
            break;
    }
    if (s + need > TD_NSECT)
        return -8;                       /* no run of free sectors that long */
    tw.slot = (int)k, tw.start = s, tw.nsect = need, tw.size = size, tw.pos = 0;
    return 0;
}

/* the take's bytes, in order: each sector erased as the data reaches it. 0 = written */
static int takes_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)src;
    if (tw.slot < 0 || off != tw.pos || off + n > tw.size)
        return -7;
    while (n) {
        uint32_t at = TD_DATA + 0x1000u * tw.start + tw.pos, room = 0x1000u - (tw.pos & 0xFFFu), k = n < room ? n : room;
        if (!(tw.pos & 0xFFFu) && plat_data_erase(at))
            return -6;
        if (plat_data_write(at, p, k))
            return -6;
        tw.pos += k, p += k, n -= k;
    }
    return 0;
}

/* all of it came: read back, checked against crc, then the directory says so. 0 = the slot's take now */
static int takes_commit(uint32_t crc)
{
    td_dir_t d = td;
    uint32_t to;
    const uint8_t *data;
    if (tw.slot < 0 || tw.pos != tw.size)
        return -7;
    data = plat_data(TD_DATA + 0x1000u * tw.start);
    if (!data || rec_crc32(0, data, tw.size) != crc) {
        tw.slot = -1;
        return -5;
    }
    d.e[tw.slot].start = (uint16_t)tw.start, d.e[tw.slot].nsect = (uint16_t)tw.nsect;
    d.e[tw.slot].size = tw.size, d.e[tw.slot].crc = crc;
    d.magic = TD_MAGIC;
    d.seq = td.seq + 1u;
    d.crc = rec_crc32(0, (const uint8_t *)&d, (uint32_t)((uint8_t *)&d.crc - (uint8_t *)&d));
    to = td_at == TD_A ? TD_B : TD_A;    /* the other copy: the current one stays whole until this one is */
    tw.slot = -1;
    if (plat_data_erase(to) || plat_data_write(to, &d, sizeof d))
        return -6;
    if (!td_ok((const td_dir_t *)plat_data(to)))
        return -6;
    td = d, td_at = to;
    return 0;
}

static int takes_delete(uint32_t k)
{
    td_dir_t d = td;
    uint32_t to = td_at == TD_A ? TD_B : TD_A;
    if (k >= TAKE_SLOTS)
        return -1;
    if (!td.e[k].nsect)
        return 0;
    d.e[k].start = d.e[k].nsect = 0, d.e[k].size = d.e[k].crc = 0;
    d.seq = td.seq + 1u;
    d.crc = rec_crc32(0, (const uint8_t *)&d, (uint32_t)((uint8_t *)&d.crc - (uint8_t *)&d));
    if (plat_data_erase(to) || plat_data_write(to, &d, sizeof d))
        return -6;
    td = d, td_at = to;
    return 0;
}
