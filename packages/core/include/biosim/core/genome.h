/* biosim/core/genome.h — genome SoA buffers and lifecycle/operator API.
 *
 * HOST-ONLY: this header uses heap pointers and host standard types.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */

#ifndef BIOSIM_CORE_GENOME_H
#define BIOSIM_CORE_GENOME_H

#include "biosim/core/status.h"
#include <stdint.h>

/* Transposed SoA: gene slot j of agent i lives at index j * population + i.
 * This layout coalesces reads when all work-items walk gene slot j in lock-step. */
typedef struct {
    uint32_t population; /* number of agent slots (N) */
    uint16_t max_len;    /* genes allocated per agent slot (GENOME_MAX_LEN) */
    uint16_t *conn;      /* packed connectivity [gene_slot * population + agent_idx] */
    int16_t *wgt;        /* raw signed weight  [gene_slot * population + agent_idx] */
    uint16_t *len;       /* active gene count per agent [agent_idx] */
} biosim_genome_t;

/* Lifecycle */
biosim_status_t biosim_genome_create(uint32_t population, uint16_t max_len, biosim_genome_t *out);
void biosim_genome_free(biosim_genome_t *g);

/* Slot operations */
void biosim_genome_init_slot(biosim_genome_t *g, uint32_t idx, uint16_t len, uint64_t *rng);
void biosim_genome_copy_slot(biosim_genome_t *g, uint32_t dst, uint32_t src);

/* Operators — called at the generation boundary on the host */
void biosim_genome_mutate(biosim_genome_t *g, uint32_t idx, float rate, uint64_t *rng);
void biosim_genome_crossover(biosim_genome_t *g, uint32_t child, uint32_t parent_a,
                             uint32_t parent_b, uint64_t *rng);
/* Warp-divergence mitigation: sort agents by descending genome length.
 * Reorders conn/wgt/length buffers in-place; writes perm_out[new_idx] = old_idx.
 * perm_out must point to a caller-allocated array of population uint32_t values.
 * Returns BIOSIM_ERR_NOMEM if scratch buffers cannot be allocated; in that case
 * the genome buffers and perm_out are left unmodified. */
biosim_status_t biosim_genome_sort_by_length(biosim_genome_t *g, uint32_t *perm_out);

#endif /* BIOSIM_CORE_GENOME_H */
