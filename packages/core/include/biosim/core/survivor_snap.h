/*
 * HOST-ONLY: uses heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_SURVIVOR_SNAP_H
#define BIOSIM_CORE_SURVIVOR_SNAP_H

#include "biosim/core/status.h"
#include <stdint.h>

/*
 * Compact row-major snapshot of survivor genomes collected at a generation
 * boundary.  Index into conn/wgt as: snap->conn[s * stride_cap + j].
 *
 * Zero-initialise before first use: biosim_survivor_snap_t snap = {0};
 * grow and free tolerate zero-initialised state.
 */
typedef struct {
    uint16_t *conn;       /* compact row-major: s * stride_cap + j  */
    int16_t  *wgt;        /* compact row-major: s * stride_cap + j  */
    uint16_t *len;        /* genome length per survivor              */
    float    *scores;     /* challenge score per survivor            */
    uint32_t  count;      /* live survivor count (filled by collect) */
    uint32_t  pop_cap;    /* allocated survivor slots                */
    uint16_t  stride_cap; /* allocated cols per survivor (>= max_len)*/
} biosim_survivor_snap_t;

/*
 * Grow snap to hold at least n_survivors entries each with at least
 * g_max_len columns.  Uses a doubling policy on each dimension independently.
 * Tolerates zero-initialised snap (first-time alloc).
 * n_survivors == 0 is a no-op.
 * On realloc failure, the partially-grown snap is left intact; the caller
 * must free it with biosim_survivor_snap_free.
 */
biosim_status_t biosim_survivor_snap_grow(
    biosim_survivor_snap_t *snap, uint32_t n_survivors, uint16_t g_max_len
);

/*
 * Free all buffers in snap.  Tolerates NULL snap and NULL members.
 * Does NOT free snap itself (snap is typically stack-allocated by the caller).
 * Zeros all fields so snap is safe to pass to grow again.
 */
void biosim_survivor_snap_free(biosim_survivor_snap_t *snap);

#endif /* BIOSIM_CORE_SURVIVOR_SNAP_H */
