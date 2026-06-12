/* biosim/core/nnet.h — Neural network SoA buffers and compile-slot function.
 *
 * HOST-ONLY: this header uses heap pointers and host standard types.
 * Do NOT include from OpenCL kernel sources (.cl files).
 * GPU kernels receive these buffers (e.g. genome_conn/genome_wgt) as bare buffer arguments.
 */

#ifndef BIOSIM_CORE_NNET_H
#define BIOSIM_CORE_NNET_H

#include "biosim/core/gene.h"
#include "biosim/core/genome.h"
#include "biosim/core/status.h"
#include <stdint.h>

/* ── neural network SoA struct ──────────────────────────────────────────── */

/* Transposed SoA: connection slot j of agent i lives at j * population + i.
 * Mirrors the genome layout for coalesced GPU reads.
 *
 * Note: conn_length uses uint16_t (not uint8_t as in 05-gpu-data-model.md §6.1)
 * to safely accommodate up to MAX_GENES connections per agent. */
typedef struct {
    uint32_t population;    /* number of agent slots (N) */
    uint16_t max_genes;     /* connection slots per agent (MAX_GENES) */
    uint8_t max_neurons;    /* neuron slots per agent (MAX_NEURONS, ≤ 128) */
    uint16_t *genome_conn;  /* packed conn genes [conn_slot * pop + agent_idx] */
    int16_t *genome_wgt;    /* conn weights      [conn_slot * pop + agent_idx] */
    uint16_t *conn_length;  /* active connection count [agent_idx] */
    float *neuron_output;   /* [neuron_slot * pop + agent_idx], init 0.0f */
    uint8_t *neuron_driven; /* 1 if neuron has non-self input [neuron_slot * pop + agent_idx] */
    uint8_t *neuron_count;  /* active neuron count [agent_idx] */
} biosim_nnet_t;

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_nnet_create(
    uint32_t population, uint16_t max_genes, uint8_t max_neurons, biosim_nnet_t *out
);
void biosim_nnet_free(biosim_nnet_t *n);

/* ── compilation ────────────────────────────────────────────────────────── */

/* Compile agent idx's genome slot into the nnet SoA. Called once per agent
 * per generation at the generation boundary.
 *
 * Culls dead neurons (no non-self input, or no path to any action), renumbers
 * surviving neurons compactly 0..n-1, and writes connections ordered:
 * neuron-sink first, action-sink second (required for single-pass feedforward).
 *
 * num_sensors / num_actions are used to remap the raw 7-bit gene fields.
 * max_neurons must be ≤ 128 (upper bound of the 7-bit gene field).
 * Returns BIOSIM_ERR_NOMEM if a temporary parse buffer cannot be allocated. */
biosim_status_t biosim_nnet_compile_slot(
    biosim_nnet_t *n,
    const biosim_genome_t *genome,
    uint32_t idx,
    uint8_t num_sensors,
    uint8_t num_actions
);

/* ── fingerprint ────────────────────────────────────────────────────────── */

/* Phenotypic fingerprint: hash the compiled connection list for agent idx.
 * Two agents whose genomes compile to identical networks (same connections
 * after culling and compact renumbering) receive the same fingerprint.
 * biosim_nnet_compile_slot must be called before this function. */
uint64_t biosim_nnet_fingerprint(const biosim_nnet_t *n, uint32_t idx);

/* ── feedforward ────────────────────────────────────────────────────────── */

/* Run one feedforward pass for agent idx.
 *
 * sensor_vals[0..num_sensors-1]  — caller-evaluated sensor outputs in [0,1].
 * action_vals[0..num_actions-1]  — caller-zeroed; receives raw weighted sums.
 *   Responsiveness is NOT applied here; biosim_action_apply handles that.
 * neuron_output in the SoA is updated in-place; state carries across calls,
 * producing step-delayed recurrent behaviour.
 *
 * Relies on the neuron-sink-before-action-sink ordering invariant established
 * by biosim_nnet_compile_slot. */
void biosim_nnet_feedforward(
    biosim_nnet_t *n, uint32_t idx, const float *sensor_vals, float *action_vals
);

#endif /* BIOSIM_CORE_NNET_H */
