/*
 * HOST-ONLY: uses heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_SNAPSHOT_DEFS_H
#define BIOSIM_CORE_SNAPSHOT_DEFS_H

#include "biosim/core/status.h"
#include <stdint.h>

/* Bump when the on-disk record layout changes. */
#define BIOSIM_SNAP_FORMAT_VERSION 2U

/*
 * Information read from the 32-byte file header.
 * Used by the caller to perform coherency checks before loading agent data.
 */
typedef struct {
    uint16_t format_version;
    uint16_t schema_version;
    uint16_t num_sensors;
    uint16_t num_actions;
    uint16_t max_genes;
    uint8_t max_neurons;
    uint32_t generation_count; /* 0 = unknown / streaming */
} biosim_snap_header_t;

/*
 * Compact row-major snapshot of survivor genomes collected at a generation
 * boundary.  Index into conn/wgt as: snap->conn[s * stride_cap + j].
 *
 * Zero-initialise before first use: biosim_survivor_snap_t snap = {0};
 * grow and free tolerate zero-initialised state.
 */
typedef struct {
    uint16_t *conn;      /* compact row-major: s * stride_cap + j  */
    int16_t *wgt;        /* compact row-major: s * stride_cap + j  */
    uint16_t *len;       /* genome length per survivor              */
    float *scores;       /* challenge score per survivor            */
    uint32_t count;      /* live survivor count (filled by collect) */
    uint32_t pop_cap;    /* allocated survivor slots                */
    uint16_t stride_cap; /* allocated cols per survivor (>= max_genes)*/
    uint32_t gen;        /* generation index at collection time     */
    uint64_t gen_rng;    /* RNG state before breed (for replay)     */
    uint16_t max_genes;  /* genome-length cap of the originating config */
    uint8_t max_neurons; /* neuron cap of the originating config        */
} biosim_survivor_snap_t;

#endif /* BIOSIM_CORE_SNAPSHOT_DEFS_H */
