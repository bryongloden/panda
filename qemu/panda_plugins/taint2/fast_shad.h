/* PANDABEGINCOMMENT
 *
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */

#ifndef __FAST_SHAD_H
#define __FAST_SHAD_H

#include <assert.h>
#include <stdint.h>

#include "defines.h"

#define CPU_LOG_TAINT_OPS (1 << 14)
#ifndef TAINTDEBUG
#define taint_log(...) {}
#define tassert(...) {}
#else
#define tassert(cond) assert((cond))
#define taint_log(...) qemu_log_mask(CPU_LOG_TAINT_OPS, ## __VA_ARGS__)
//#define taint_log(...) printf(__VA_ARGS__)
#endif

void *memset(void *dest, int val, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);

typedef struct LabelSet LabelSet;

typedef struct FastShad {
    LabelSet **labels;
    LabelSet **orig_labels;
    uint64_t size; // Number of labelsets contained.
} FastShad;

FastShad *fast_shad_new(uint64_t size);
void fast_shad_free(FastShad *fast_shad);

static inline void fast_shad_set(FastShad *fast_shad, uint64_t addr, LabelSet *ls);
static inline void fast_shad_move(FastShad *fast_shad_dest, uint64_t dest, FastShad *fast_shad_src, uint64_t src, uint64_t size);
static inline void fast_shad_copy(FastShad *fast_shad_dest, uint64_t dest, FastShad *fast_shad_src, uint64_t src, uint64_t size);
static inline void fast_shad_remove(FastShad *fast_shad, uint64_t addr, uint64_t size);
static inline LabelSet *fast_shad_query(FastShad *fast_shad, uint64_t addr);
static inline void fast_shad_reset_frame(FastShad *fast_shad);
static inline void fast_shad_push_frame(FastShad *fast_shad);
static inline void fast_shad_pop_frame(FastShad *fast_shad);

static inline LabelSet **get_ls_p(FastShad *fast_shad, uint64_t guest_addr) {
    tassert(guest_addr < fast_shad->size);
    return &fast_shad->labels[guest_addr];
}

// Taint an address with a labelset.
static inline void fast_shad_set(FastShad *fast_shad, uint64_t addr, LabelSet *ls) {
    *get_ls_p(fast_shad, addr) = ls;
}

static inline void fast_shad_copy(FastShad *fast_shad_dest, uint64_t dest, FastShad *fast_shad_src, uint64_t src, uint64_t size) {
    tassert(dest + size <= fast_shad_dest->size);
    tassert(src + size <= fast_shad_src->size);
    
#ifdef TAINTDEBUG
    unsigned i;
    for (i = 0; i < size; i++) {
        if (*get_ls_p(fast_shad_src, src + i) != NULL) {
            taint_log("TAINTED COPY: %lx[%lx] <- %lx[%lx] (%lx)\n",
                    (uint64_t)fast_shad_dest, dest + i,
                    (uint64_t)fast_shad_src, src + i,
                    (uint64_t)*get_ls_p(fast_shad_src, src + i));
            break;
        }
    }
#endif

    memcpy(get_ls_p(fast_shad_dest, dest), get_ls_p(fast_shad_src, src), size * sizeof(LabelSet *));
}

static inline void fast_shad_move(FastShad *fast_shad_dest, uint64_t dest, FastShad *fast_shad_src, uint64_t src, uint64_t size) {
    tassert(dest + size <= fast_shad_dest->size);
    tassert(src + size <= fast_shad_src->size);
    
#ifdef TAINTDEBUG
    unsigned i;
    for (i = 0; i < size; i++) {
        if (*get_ls_p(fast_shad_src, src + i) != NULL) {
            taint_log("TAINTED MOVE\n");
            break;
        }
    }
#endif

    memmove(get_ls_p(fast_shad_dest, dest), get_ls_p(fast_shad_src, src), size * sizeof(LabelSet *));
}

// Remove taint.
static inline void fast_shad_remove(FastShad *fast_shad, uint64_t addr, uint64_t size) {
    tassert(addr + size <= fast_shad->size);
    
#ifdef TAINTDEBUG
    unsigned i;
    for (i = 0; i < size; i++) {
        if (*get_ls_p(fast_shad, addr + i) != NULL) {
            taint_log("TAINTED DELETE\n");
            break;
        }
    }
#endif

    memset(get_ls_p(fast_shad, addr), 0, size * sizeof(LabelSet *));
}

// Query. NULL if untainted.
static inline LabelSet *fast_shad_query(FastShad *fast_shad, uint64_t addr) {
    return *get_ls_p(fast_shad, addr);
} 

static inline void fast_shad_reset_frame(FastShad *fast_shad) {
    fast_shad->labels = fast_shad->orig_labels;
    //printf("reset: %lx\n", (uint64_t)fast_shad->labels);
}

static inline void fast_shad_push_frame(FastShad *fast_shad) {
    fast_shad->labels += MAXREGSIZE * MAXFRAMESIZE;
    tassert(fast_shad->labels < fast_shad->orig_labels + fast_shad->size);
    //printf("push: %lx\n", (uint64_t)fast_shad->labels);
}

static inline void fast_shad_pop_frame(FastShad *fast_shad) {
    fast_shad->labels -= MAXREGSIZE * MAXFRAMESIZE;
    tassert(fast_shad->labels >= fast_shad->orig_labels);
    //printf("pop: %lx\n", (uint64_t)fast_shad->labels);
}

#endif
