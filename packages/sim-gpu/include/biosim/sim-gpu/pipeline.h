/*
 * HOST-ONLY: wraps OpenCL objects. Do NOT include from kernel sources.
 */
#ifndef BIOSIM_SIM_GPU_PIPELINE_H
#define BIOSIM_SIM_GPU_PIPELINE_H

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/runner.h"
#include <stddef.h>
#include <stdint.h>

/* ── GPU buffer layout ──────────────────────────────────────────────────── */

/*
 * All cl_mem handles for one simulation session.
 * Zero-initialise before use; kernel_buffers_release tolerates NULL entries.
 */
typedef struct {
    /* Per-agent fixed fields */
    cl_mem alive;
    cl_mem loc_x; /* RW: K1 reads, K3 writes */
    cl_mem loc_y;
    cl_mem osc_period;
    cl_mem last_move_dir; /* RW: K1 reads, K3 writes */
    cl_mem responsiveness;
    cl_mem los_range;
    /* Neural network (transposed SoA) */
    cl_mem conn_packed;
    cl_mem conn_weight;
    cl_mem conn_length;
    cl_mem neuron_output;
    cl_mem neuron_driven;
    cl_mem neuron_count;
    /* Signal layer */
    cl_mem signal;
    /* K1 outputs */
    cl_mem rng_state;
    cl_mem desired_x; /* K1 writes, K3 reads */
    cl_mem desired_y;
    cl_mem kill_marker; /* K1 writes, K2 reads; host zeros at gen boundary */
    /* Grid: K2/K3/K5 in/out */
    cl_mem grid;
    /* K5 challenge evaluation */
    cl_mem challenge_bits; /* K5 accumulates per-agent challenge state */
    cl_mem barrier_ctrs;   /* RO: flat [x0,y0,x1,y1,...] for LOCATION_SEQUENCE */
} kernel_buffers_t;

/* ── GPU pipeline ───────────────────────────────────────────────────────── */

/*
 * Owns programs, kernels, and GPU buffers for one simulation session.
 * Zero-initialise before use; biosim_gpu_pipeline_free tolerates a zero struct.
 */
typedef struct {
    const biosim_gpu_runner_t *runner; /* borrowed; must outlive this struct */
    /* Compiled programs */
    cl_program k1_program;
    cl_program k2_program;
    cl_program k3_program;
    cl_program k4_program;
    cl_program k5_program;
    /* Kernel handles */
    cl_kernel k1;
    cl_kernel k2;
    cl_kernel k3;
    cl_kernel k4;
    cl_kernel k5;
    /* GPU buffers */
    kernel_buffers_t bufs;
    /* Cached config for buffer-size arithmetic in sync calls */
    uint32_t population;
    int32_t size_x;
    int32_t size_y;
    size_t signal_len;
    uint16_t max_conn;
    uint8_t max_neurons;
    uint32_t n_barrier_ctrs;
} biosim_gpu_pipeline_t;

/*
 * Build 5 OpenCL programs, create kernels, allocate GPU buffers, and upload
 * initial simulation state from *sim.  exec_dir is passed to
 * biosim_gpu_registry_get for filesystem kernel overrides; may be NULL.
 *
 * All static kernel args (cl_mem handles and scalars that do not change within
 * a generation) are set once here.  Only the per-step `step` arg is updated
 * by biosim_gpu_pipeline_step.
 *
 * Returns BIOSIM_OK on success.  On failure, *out is fully cleaned up.
 */
biosim_status_t biosim_gpu_pipeline_create(
    const biosim_sim_t *sim,
    const biosim_gpu_runner_t *runner,
    const char *exec_dir,
    biosim_gpu_pipeline_t *out
);

/*
 * Enqueue one complete step: K1 → K2 → K3 → K4 → K5.
 * No implicit clFinish; the caller controls synchronisation.
 * Updates only the `step` scalar arg on K1 (arg 16) and K5 (arg 8)
 * before enqueuing.
 */
biosim_status_t biosim_gpu_pipeline_step(biosim_gpu_pipeline_t *p, const biosim_sim_t *sim);

/*
 * clFinish then download the subset of GPU state needed for
 * biosim_sim_next_generation into *sim:
 *   alive, loc_x, loc_y, challenge_bits, signal.
 */
biosim_status_t biosim_gpu_pipeline_sync_to_host(const biosim_gpu_pipeline_t *p, biosim_sim_t *sim);

/*
 * Re-upload all mutable state from *sim after biosim_sim_next_generation:
 *   alive, loc_x, loc_y, osc_period, last_move_dir, responsiveness,
 *   los_range, conn_packed, conn_weight, conn_length,
 *   neuron_output, neuron_driven, neuron_count, signal, rng_state,
 *   kill_marker, challenge_bits, grid.
 * barrier_ctrs is static and is never re-uploaded.
 */
biosim_status_t biosim_gpu_pipeline_sync_from_host(
    biosim_gpu_pipeline_t *p, const biosim_sim_t *sim
);

/* Release all GPU resources.  Safe on a zero-initialised struct. */
void biosim_gpu_pipeline_free(biosim_gpu_pipeline_t *p);

#endif /* BIOSIM_SIM_GPU_PIPELINE_H */
