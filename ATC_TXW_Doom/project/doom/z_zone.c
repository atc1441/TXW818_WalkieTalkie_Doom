/*
 * PrBoom zone allocator — TXW81x static PSRAM backend
 *
 * One static buffer in .psram.data acts as the zone.  All blocks live
 * inside it; no external allocator is called after Z_Init().
 *
 * Free blocks are kept in an address-sorted doubly-linked list.
 * Allocated blocks are kept in per-tag circular doubly-linked lists
 * (blockbytag[]).  The same prev/next pointers serve both purposes:
 * free blocks use them as free-list links; allocated blocks use them
 * as tag-list links.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "z_zone.h"
#include "doomstat.h"
#include "i_system.h"
#include "lprintf.h"

/* ------------------------------------------------------------------ */
/* Zone storage — static, placed in PSRAM by the linker               */
/* ------------------------------------------------------------------ */
#define DOOM_ZONE_SIZE  (700 * 1024)

static uint8_t zone_storage[DOOM_ZONE_SIZE]
    __attribute__((aligned(4), section(".psram.data")));

/* ------------------------------------------------------------------ */
/* Block header                                                        */
/* ------------------------------------------------------------------ */
#define CHUNK_SIZE 4

typedef struct memblock {
    size_t           size;   /* payload bytes (not counting this header) */
    int              tag;
    void           **user;
    struct memblock *prev;   /* tag-list (allocated) / free-list (free) */
    struct memblock *next;
} memblock_t;

/* Round header up to CHUNK_SIZE alignment */
static const size_t HEADER_SIZE =
    (sizeof(memblock_t) + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);

/* Per-tag circular lists (allocated blocks only) */
static memblock_t *blockbytag[PU_MAX];

/* Free list head — address-sorted, non-circular, NULL-terminated */
static memblock_t *free_head;

/* ------------------------------------------------------------------ */
/* Stub functions                                                      */
/* ------------------------------------------------------------------ */
void Z_DumpHistory(char *buf) { (void)buf; }
void Z_Close(void) {}
void (Z_CheckHeap)(void) {}

/* ------------------------------------------------------------------ */
/* Z_Init                                                              */
/* ------------------------------------------------------------------ */
void Z_Init(void)
{
    for (int i = 0; i < PU_MAX; i++)
        blockbytag[i] = NULL;

    memblock_t *b = (memblock_t *)zone_storage;
    b->size  = DOOM_ZONE_SIZE - HEADER_SIZE;
    b->tag   = PU_FREE;
    b->user  = NULL;
    b->prev  = NULL;
    b->next  = NULL;
    free_head = b;

    lprintf(LO_INFO, "Z_Init: zone %u bytes at %p\n",
            (unsigned)DOOM_ZONE_SIZE, (void *)zone_storage);
}

/* ------------------------------------------------------------------ */
/* Free-list helpers                                                   */
/* ------------------------------------------------------------------ */

/* Insert block b into the address-sorted free list */
static void free_insert(memblock_t *b)
{
    b->tag = PU_FREE;
    b->user = NULL;

    if (!free_head || b < free_head) {
        b->prev = NULL;
        b->next = free_head;
        if (free_head) free_head->prev = b;
        free_head = b;
        return;
    }
    memblock_t *cur = free_head;
    while (cur->next && cur->next < b)
        cur = cur->next;
    b->next = cur->next;
    b->prev = cur;
    if (cur->next) cur->next->prev = b;
    cur->next = b;
}

/* Remove block b from the free list */
static void free_remove(memblock_t *b)
{
    if (b->prev) b->prev->next = b->next;
    else         free_head     = b->next;
    if (b->next) b->next->prev = b->prev;
    b->prev = b->next = NULL;
}

/* Merge b with its next free neighbour if physically adjacent */
static void coalesce_next(memblock_t *b)
{
    if (!b->next) return;
    memblock_t *nx = b->next;
    if ((char *)b + HEADER_SIZE + b->size == (char *)nx) {
        b->size += HEADER_SIZE + nx->size;
        b->next = nx->next;
        if (nx->next) nx->next->prev = b;
    }
}

/* ------------------------------------------------------------------ */
/* Z_Malloc                                                            */
/* ------------------------------------------------------------------ */
void *Z_Malloc_verbose(size_t size, int tag, void **user,
                       const char *function, int line)
{
    if (!size)
        return user ? (*user = NULL) : NULL;

    size = (size + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);

retry:;
    /* First-fit search in free list */
    memblock_t *b = free_head;
    while (b && b->size < size)
        b = b->next;

    if (!b) {
        if (!blockbytag[PU_CACHE])
            I_Error("Z_Malloc: failed to allocate %lu bytes\n  (called from %s:%d)",
                    (unsigned long)size, function, line);
        Z_FreeTags(PU_CACHE, PU_CACHE);
        goto retry;
    }

    free_remove(b);

    /* Split if remainder is large enough to be useful */
    size_t remain = b->size - size;
    if (remain >= HEADER_SIZE + CHUNK_SIZE) {
        memblock_t *split = (memblock_t *)((char *)b + HEADER_SIZE + size);
        split->size = remain - HEADER_SIZE;
        b->size = size;
        free_insert(split);
    }

    /* Add to tag list */
    b->tag  = tag;
    b->user = user;
    if (!blockbytag[tag]) {
        blockbytag[tag] = b;
        b->next = b->prev = b;
    } else {
        blockbytag[tag]->prev->next = b;
        b->prev = blockbytag[tag]->prev;
        b->next = blockbytag[tag];
        blockbytag[tag]->prev = b;
    }

    void *ret = (char *)b + HEADER_SIZE;
    if (user) *user = ret;
    return ret;
}

/* ------------------------------------------------------------------ */
/* Z_Free                                                              */
/* ------------------------------------------------------------------ */
void (Z_Free)(void *p)
{
    if (!p) return;

    memblock_t *b = (memblock_t *)((char *)p - HEADER_SIZE);

    if (b->user) *b->user = NULL;

    /* Remove from tag list */
    if (b == b->next)
        blockbytag[b->tag] = NULL;
    else {
        if (blockbytag[b->tag] == b)
            blockbytag[b->tag] = b->next;
        b->prev->next = b->next;
        b->next->prev = b->prev;
    }

    /* Return to free list and coalesce */
    free_insert(b);
    coalesce_next(b);
    if (b->prev) coalesce_next(b->prev);
}

/* ------------------------------------------------------------------ */
/* Z_FreeTags                                                          */
/* ------------------------------------------------------------------ */
void (Z_FreeTags)(int lowtag, int hightag)
{
    if (lowtag <= PU_FREE)  lowtag  = PU_FREE + 1;
    if (hightag > PU_CACHE) hightag = PU_CACHE;

    for (; lowtag <= hightag; lowtag++) {
        memblock_t *block = blockbytag[lowtag];
        if (!block) continue;
        memblock_t *end = block->prev;
        while (1) {
            memblock_t *nx = block->next;
            (Z_Free)((char *)block + HEADER_SIZE);
            if (block == end) break;
            block = nx;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Z_ChangeTag                                                         */
/* ------------------------------------------------------------------ */
void (Z_ChangeTag)(void *ptr, int tag)
{
    if (!ptr) return;
    memblock_t *b = (memblock_t *)((char *)ptr - HEADER_SIZE);
    if (tag == b->tag) return;

    if (b == b->next)
        blockbytag[b->tag] = NULL;
    else {
        if (blockbytag[b->tag] == b)
            blockbytag[b->tag] = b->next;
        b->prev->next = b->next;
        b->next->prev = b->prev;
    }

    if (!blockbytag[tag]) {
        blockbytag[tag] = b;
        b->next = b->prev = b;
    } else {
        blockbytag[tag]->prev->next = b;
        b->prev = blockbytag[tag]->prev;
        b->next = blockbytag[tag];
        blockbytag[tag]->prev = b;
    }
    b->tag = tag;
}

/* ------------------------------------------------------------------ */
/* Utility wrappers                                                    */
/* ------------------------------------------------------------------ */
void *(Z_Realloc)(void *ptr, size_t n, int tag, void **user)
{
    void *p = Z_Malloc(n, tag, user);
    if (ptr) {
        memblock_t *b = (memblock_t *)((char *)ptr - HEADER_SIZE);
        memcpy(p, ptr, n <= b->size ? n : b->size);
        (Z_Free)(ptr);
        if (user) *user = p;
    }
    return p;
}

void *(Z_Calloc)(size_t n1, size_t n2, int tag, void **user)
{
    return (n1 *= n2) ? memset(Z_Malloc(n1, tag, user), 0, n1) : NULL;
}

char *(Z_Strdup)(const char *s, int tag, void **user)
{
    return strcpy(Z_Malloc(strlen(s) + 1, tag, user), s);
}
