/* biosim/core/genome.h — gene encoding macros and genome SoA buffers.
 *
 * PORTABILITY: The gene-encoding macros and constants before the
 * #ifndef __OPENCL_VERSION__ guard compile under both C11 and OpenCL C.
 * Do NOT add <stdio.h>, <stdlib.h>, <string.h>, or any other host-only
 * include above that guard.
 */

#ifndef BIOSIM_CORE_GENOME_H
#define BIOSIM_CORE_GENOME_H

#ifdef __OPENCL_VERSION__
typedef uchar uint8_t;
typedef ushort uint16_t;
typedef short int16_t;
typedef uint uint32_t;
typedef ulong uint64_t;
#else
#include <stdint.h>
#endif

/* ── gene field encoding ─────────────────────────────────────────────────── */
/* conn layout: [srcType:1][srcNum:7][sinkType:1][sinkNum:7]
 * srcType  1 = SENSOR, 0 = NEURON
 * sinkType 1 = ACTION, 0 = NEURON
 * Source/sink numbers are raw 7-bit values remapped modulo valid range
 * by the feedforward kernel.
 */

#define BIOSIM_GENE_SRC_TYPE(c)  (((c) >> 15) & 1U)
#define BIOSIM_GENE_SRC_NUM(c)   (((c) >> 8) & 0x7FU)
#define BIOSIM_GENE_SINK_TYPE(c) (((c) >> 7) & 1U)
#define BIOSIM_GENE_SINK_NUM(c)  ((c) & 0x7FU)
#define BIOSIM_GENE_PACK(st, sn, dt, dn)                                                           \
    ((uint16_t)(((st) & 1U) << 15 | ((sn) & 0x7FU) << 8 | ((dt) & 1U) << 7 | ((dn) & 0x7FU)))

#ifndef __OPENCL_VERSION__

/* ── host-only: genome SoA struct ───────────────────────────────────────── */

#include "biosim/core/status.h"

/* Transposed SoA: gene slot j of agent i lives at index j * capacity + i.
 * This layout coalesces reads when all work-items walk gene slot j in lock-step. */
typedef struct {
    uint32_t capacity;   /* number of agent slots (N) */
    uint16_t max_length; /* genes allocated per agent slot (GENOME_MAX_LENGTH) */
    uint16_t *conn;      /* packed connectivity [gene_slot * capacity + agent_idx] */
    int16_t *wgt;        /* raw signed weight  [gene_slot * capacity + agent_idx] */
    uint16_t *length;    /* active gene count per agent [agent_idx] */
} biosim_genome_t;

/* Lifecycle */
biosim_status_t biosim_genome_create(uint32_t capacity, uint16_t max_length, biosim_genome_t *out);
void biosim_genome_free(biosim_genome_t *g);

/* Slot operations */
void biosim_genome_init_slot(biosim_genome_t *g, uint32_t idx, uint16_t length, uint64_t *rng);
void biosim_genome_copy_slot(biosim_genome_t *g, uint32_t dst, uint32_t src);

/* Operators — called at the generation boundary on the host */
void biosim_genome_mutate(biosim_genome_t *g, uint32_t idx, float rate, uint64_t *rng);
void biosim_genome_crossover(biosim_genome_t *g, uint32_t child, uint32_t parent_a,
                             uint32_t parent_b, uint64_t *rng);
uint64_t biosim_genome_fingerprint(const biosim_genome_t *g, uint32_t idx);

/* Warp-divergence mitigation: sort agents by descending genome length.
 * Reorders conn/wgt/length buffers in-place; writes perm_out[new_idx] = old_idx.
 * perm_out must point to a caller-allocated array of capacity uint32_t values. */
void biosim_genome_sort_by_length(biosim_genome_t *g, uint32_t *perm_out);

#endif /* !__OPENCL_VERSION__ */

#endif /* BIOSIM_CORE_GENOME_H */
