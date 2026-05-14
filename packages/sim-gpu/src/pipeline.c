#include "biosim/sim-gpu/pipeline.h"
#include "biosim/core/log.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/registry.h"
#include "biosim/sim-gpu/runner.h"

#include "cl_macros.h"

#include <string.h>

/* ── buffer helpers ─────────────────────────────────────────────────────── */

static void kernel_buffers_release(kernel_buffers_t *b) {
    CL_SAFE_RELEASE(clReleaseMemObject, b->barrier_ctrs);
    CL_SAFE_RELEASE(clReleaseMemObject, b->challenge_bits);
    CL_SAFE_RELEASE(clReleaseMemObject, b->grid);
    CL_SAFE_RELEASE(clReleaseMemObject, b->kill_marker);
    CL_SAFE_RELEASE(clReleaseMemObject, b->desired_y);
    CL_SAFE_RELEASE(clReleaseMemObject, b->desired_x);
    CL_SAFE_RELEASE(clReleaseMemObject, b->rng_state);
    CL_SAFE_RELEASE(clReleaseMemObject, b->signal);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_count);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_driven);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_output);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_length);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_weight);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_packed);
    CL_SAFE_RELEASE(clReleaseMemObject, b->long_probe_dist);
    CL_SAFE_RELEASE(clReleaseMemObject, b->responsiveness);
    CL_SAFE_RELEASE(clReleaseMemObject, b->last_move_dir);
    CL_SAFE_RELEASE(clReleaseMemObject, b->osc_period);
    CL_SAFE_RELEASE(clReleaseMemObject, b->loc_y);
    CL_SAFE_RELEASE(clReleaseMemObject, b->loc_x);
    CL_SAFE_RELEASE(clReleaseMemObject, b->alive);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static biosim_status_t kernel_buffers_create(const biosim_sim_t *sim, cl_context ctx,
                                             kernel_buffers_t *out) {
    memset(out, 0, sizeof(*out));

    const biosim_agents_t *a = &sim->agents;
    const biosim_nnet_t *n = &sim->nnet;
    const uint32_t pop = sim->population;
    const uint16_t max_conn = n->max_conn;
    const uint8_t max_neurons = n->max_neurons;

    biosim_status_t returncode = BIOSIM_OK;
    cl_int cl_err = CL_SUCCESS;

#define MKBUF_RO(field, ptr, elem_size, count)                                                     \
    CL_ASSIGN_OR_GOTO_EXIT(out->field,                                                             \
                           clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,            \
                                          (size_t)(count) * (elem_size), (void *)(ptr), &cl_err))

#define MKBUF_RW(field, ptr, elem_size, count)                                                     \
    CL_ASSIGN_OR_GOTO_EXIT(out->field,                                                             \
                           clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,           \
                                          (size_t)(count) * (elem_size), (void *)(ptr), &cl_err))

    MKBUF_RW(alive, a->alive, sizeof(uint8_t), pop);
    MKBUF_RW(loc_x, a->loc_x, sizeof(int32_t), pop);
    MKBUF_RW(loc_y, a->loc_y, sizeof(int32_t), pop);
    MKBUF_RW(osc_period, a->osc_period, sizeof(uint16_t), pop);
    MKBUF_RW(last_move_dir, a->last_move_dir, sizeof(uint8_t), pop);
    MKBUF_RW(responsiveness, a->responsiveness, sizeof(float), pop);
    MKBUF_RW(long_probe_dist, a->long_probe_dist, sizeof(uint8_t), pop);
    MKBUF_RO(conn_packed, n->genome_conn, sizeof(uint16_t), (size_t)max_conn * pop);
    MKBUF_RO(conn_weight, n->genome_wgt, sizeof(int16_t), (size_t)max_conn * pop);
    MKBUF_RO(conn_length, n->conn_length, sizeof(uint16_t), pop);
    MKBUF_RW(neuron_output, n->neuron_output, sizeof(float), (size_t)max_neurons * pop);
    MKBUF_RO(neuron_driven, n->neuron_driven, sizeof(uint8_t), (size_t)max_neurons * pop);
    MKBUF_RO(neuron_count, n->neuron_count, sizeof(uint8_t), pop);
    MKBUF_RW(signal, sim->signal, sizeof(uint32_t), sim->signal_len);
    MKBUF_RW(rng_state, a->rng_state, sizeof(uint64_t), pop);
    MKBUF_RW(desired_x, a->desired_x, sizeof(int32_t), pop);
    MKBUF_RW(desired_y, a->desired_y, sizeof(int32_t), pop);
    MKBUF_RW(kill_marker, a->kill_marker, sizeof(uint8_t), pop);
    size_t grid_cells = (size_t)sim->size_x * (size_t)sim->size_y;
    MKBUF_RW(grid, sim->grid.cells, sizeof(uint32_t), grid_cells);
    MKBUF_RW(challenge_bits, a->challenge_bits, sizeof(uint32_t), pop);
    /* barrier_ctrs: allocate at least 1 slot to avoid passing NULL to CL_MEM_COPY_HOST_PTR */
    {
        uint32_t n_ctrs = sim->n_barrier_ctrs;
        size_t n_alloc = (n_ctrs == 0U) ? 1U : (size_t)n_ctrs;
        int32_t dummy[2] = {0, 0};
        void *host_ptr = (n_ctrs == 0U) ? (void *)dummy : (void *)sim->barrier_ctrs;
        CL_ASSIGN_OR_GOTO_EXIT(out->barrier_ctrs,
                               clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                              n_alloc * 2U * sizeof(int32_t), host_ptr, &cl_err));
    }

#undef MKBUF_RO
#undef MKBUF_RW

exit:
    if (returncode != BIOSIM_OK) {
        kernel_buffers_release(out);
    }
    return returncode;
}

/* ── kernel argument setup ──────────────────────────────────────────────── */

static void k1_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = sim->size_x;
    cl_int size_y = sim->size_y;
    cl_uint step = (cl_uint)sim->step;
    cl_uint steps_gen = (cl_uint)sim->steps_per_gen;
    cl_uint pop = (cl_uint)sim->population;
    cl_int enable_kill_val = (cl_int)(sim->enable_kill ? 1 : 0);

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->osc_period);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&b->last_move_dir);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&b->responsiveness);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&b->long_probe_dist);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_mem), (const void *)&b->conn_packed);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_mem), (const void *)&b->conn_weight);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_mem), (const void *)&b->conn_length);
    (void)clSetKernelArg(kernel, 10U, sizeof(cl_mem), (const void *)&b->neuron_output);
    (void)clSetKernelArg(kernel, 11U, sizeof(cl_mem), (const void *)&b->neuron_driven);
    (void)clSetKernelArg(kernel, 12U, sizeof(cl_mem), (const void *)&b->neuron_count);
    (void)clSetKernelArg(kernel, 13U, sizeof(cl_mem), (const void *)&b->signal);
    (void)clSetKernelArg(kernel, 14U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 15U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 16U, sizeof(cl_uint), (const void *)&step);
    (void)clSetKernelArg(kernel, 17U, sizeof(cl_uint), (const void *)&steps_gen);
    (void)clSetKernelArg(kernel, 18U, sizeof(cl_uint), (const void *)&pop);
    (void)clSetKernelArg(kernel, 19U, sizeof(cl_mem), (const void *)&b->rng_state);
    (void)clSetKernelArg(kernel, 20U, sizeof(cl_mem), (const void *)&b->desired_x);
    (void)clSetKernelArg(kernel, 21U, sizeof(cl_mem), (const void *)&b->desired_y);
    (void)clSetKernelArg(kernel, 22U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 23U, sizeof(cl_int), (const void *)&enable_kill_val);
    (void)clSetKernelArg(kernel, 24U, sizeof(cl_mem), (const void *)&b->kill_marker);
}

static void k2_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = sim->size_x;
    cl_uint pop = (cl_uint)sim->population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->kill_marker);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_uint), (const void *)&pop);
}

static void k3_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = sim->size_x;
    cl_int size_y = sim->size_y;
    cl_uint pop = (cl_uint)sim->population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->desired_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->desired_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&b->last_move_dir);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_uint), (const void *)&pop);
}

static void k4_set_args(cl_kernel kernel, const kernel_buffers_t *b) {
    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->signal);
}

static void k5_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = sim->size_x;
    cl_int size_y = sim->size_y;
    cl_uint step = (cl_uint)sim->step;
    cl_uint steps_gen = (cl_uint)sim->steps_per_gen;
    cl_uint kind = (cl_uint)sim->challenge.kind;
    cl_float radius = sim->challenge.location_sequence.radius;
    cl_uint n_ctrs = sim->n_barrier_ctrs;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->challenge_bits);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&b->rng_state);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_uint), (const void *)&step);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_uint), (const void *)&steps_gen);
    (void)clSetKernelArg(kernel, 10U, sizeof(cl_uint), (const void *)&kind);
    (void)clSetKernelArg(kernel, 11U, sizeof(cl_float), (const void *)&radius);
    (void)clSetKernelArg(kernel, 12U, sizeof(cl_mem), (const void *)&b->barrier_ctrs);
    (void)clSetKernelArg(kernel, 13U, sizeof(cl_uint), (const void *)&n_ctrs);
}

static void set_static_args(biosim_gpu_pipeline_t *p, const biosim_sim_t *sim) {
    k1_set_args(p->k1, &p->bufs, sim);
    k2_set_args(p->k2, &p->bufs, sim);
    k3_set_args(p->k3, &p->bufs, sim);
    k4_set_args(p->k4, &p->bufs);
    k5_set_args(p->k5, &p->bufs, sim);
}

/* ── build helper ───────────────────────────────────────────────────────── */

static biosim_status_t build_one_kernel(const biosim_gpu_runner_t *runner, const char *name,
                                        const char *exec_dir, const char *entry_point,
                                        cl_program *program_out, cl_kernel *kernel_out) {
    biosim_gpu_kernel_sources_t sources;
    memset(&sources, 0, sizeof(sources));

    biosim_status_t returncode = biosim_gpu_registry_get(name, exec_dir, &sources);
    if (returncode != BIOSIM_OK) {
        return returncode;
    }

    returncode = biosim_gpu_program_build(runner, sources.sources, sources.count, program_out);
    biosim_gpu_kernel_sources_free(&sources);
    if (returncode != BIOSIM_OK) {
        return returncode;
    }

    cl_int cl_err = CL_SUCCESS;
    *kernel_out = clCreateKernel(*program_out, entry_point, &cl_err);
    if (cl_err != CL_SUCCESS || !(*kernel_out)) {
        BIOSIM_ERRORF("clCreateKernel(%s) failed (OpenCL %d)", entry_point, (int)cl_err);
        return BIOSIM_ERR_OPENCL;
    }

    return BIOSIM_OK;
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_status_t biosim_gpu_pipeline_create(const biosim_sim_t *sim,
                                           const biosim_gpu_runner_t *runner, const char *exec_dir,
                                           biosim_gpu_pipeline_t *out) {
    memset(out, 0, sizeof(*out));
    out->runner = runner;

    out->population = sim->population;
    out->size_x = sim->size_x;
    out->size_y = sim->size_y;
    out->signal_len = sim->signal_len;
    out->max_conn = sim->nnet.max_conn;
    out->max_neurons = sim->nnet.max_neurons;
    out->n_barrier_ctrs = sim->n_barrier_ctrs;

    biosim_status_t returncode = BIOSIM_OK;

    returncode = kernel_buffers_create(sim, runner->context, &out->bufs);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = build_one_kernel(runner, "k1_feedforward", exec_dir, "k_feedforward",
                                  &out->k1_program, &out->k1);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = build_one_kernel(runner, "k2_kill_marked", exec_dir, "k_kill_marked",
                                  &out->k2_program, &out->k2);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = build_one_kernel(runner, "k3_movement_resolution", exec_dir,
                                  "k_movement_resolution", &out->k3_program, &out->k3);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = build_one_kernel(runner, "k4_signal_fade", exec_dir, "k_signal_fade",
                                  &out->k4_program, &out->k4);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = build_one_kernel(runner, "k5_challenge_step_eval", exec_dir,
                                  "k_challenge_step_eval", &out->k5_program, &out->k5);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    set_static_args(out, sim);

exit:
    if (returncode != BIOSIM_OK) {
        biosim_gpu_pipeline_free(out);
        BIOSIM_ERRORF("pipeline create failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

biosim_status_t biosim_gpu_pipeline_step(biosim_gpu_pipeline_t *p, const biosim_sim_t *sim) {
    biosim_status_t returncode = BIOSIM_OK;
    cl_command_queue q = p->runner->queue;
    size_t pop_size = (size_t)p->population;
    size_t grid_size = (size_t)p->size_x * (size_t)p->size_y;

    /* Only the per-step arg changes; all others were set in pipeline_create. */
    cl_uint step = (cl_uint)sim->step;
    (void)clSetKernelArg(p->k1, 16U, sizeof(cl_uint), &step);
    (void)clSetKernelArg(p->k5, 8U, sizeof(cl_uint), &step);

    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(q, p->k1, 1U, NULL, &pop_size, NULL, 0U, NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(q, p->k2, 1U, NULL, &pop_size, NULL, 0U, NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(q, p->k3, 1U, NULL, &pop_size, NULL, 0U, NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(q, p->k4, 1U, NULL, &grid_size, NULL, 0U, NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(q, p->k5, 1U, NULL, &pop_size, NULL, 0U, NULL, NULL));

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("step %u dispatch failed (%s)", sim->step, biosim_strerror(returncode));
    }
    return returncode;
}

biosim_status_t biosim_gpu_pipeline_sync_to_host(const biosim_gpu_pipeline_t *p,
                                                 biosim_sim_t *sim) {
    biosim_status_t returncode = BIOSIM_OK;
    cl_command_queue q = p->runner->queue;
    uint32_t pop = p->population;

    CL_GOTO_EXIT_ON_ERROR(clFinish(q));

    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(q, p->bufs.alive, CL_FALSE, 0U,
                                              (size_t)pop * sizeof(uint8_t), sim->agents.alive, 0U,
                                              NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(q, p->bufs.loc_x, CL_FALSE, 0U,
                                              (size_t)pop * sizeof(int32_t), sim->agents.loc_x, 0U,
                                              NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(q, p->bufs.loc_y, CL_FALSE, 0U,
                                              (size_t)pop * sizeof(int32_t), sim->agents.loc_y, 0U,
                                              NULL, NULL));
    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(q, p->bufs.challenge_bits, CL_FALSE, 0U,
                                              (size_t)pop * sizeof(uint32_t),
                                              sim->agents.challenge_bits, 0U, NULL, NULL));
    /* Block on the last read to ensure all transfers are complete. */
    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(q, p->bufs.signal, CL_TRUE, 0U,
                                              p->signal_len * sizeof(uint32_t), sim->signal, 0U,
                                              NULL, NULL));

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("sync to host failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
biosim_status_t biosim_gpu_pipeline_sync_from_host(biosim_gpu_pipeline_t *p,
                                                   const biosim_sim_t *sim) {
    biosim_status_t returncode = BIOSIM_OK;
    cl_command_queue q = p->runner->queue;
    uint32_t pop = p->population;
    const biosim_agents_t *a = &sim->agents;
    const biosim_nnet_t *n = &sim->nnet;

#define WRITE_BUF(buf_field, host_ptr, elem_sz, count)                                             \
    CL_GOTO_EXIT_ON_ERROR(clEnqueueWriteBuffer(q, p->bufs.buf_field, CL_FALSE, 0U,                 \
                                               (size_t)(count) * (size_t)(elem_sz),                \
                                               (const void *)(host_ptr), 0U, NULL, NULL))

    WRITE_BUF(alive, a->alive, sizeof(uint8_t), pop);
    WRITE_BUF(loc_x, a->loc_x, sizeof(int32_t), pop);
    WRITE_BUF(loc_y, a->loc_y, sizeof(int32_t), pop);
    WRITE_BUF(osc_period, a->osc_period, sizeof(uint16_t), pop);
    WRITE_BUF(last_move_dir, a->last_move_dir, sizeof(uint8_t), pop);
    WRITE_BUF(responsiveness, a->responsiveness, sizeof(float), pop);
    WRITE_BUF(long_probe_dist, a->long_probe_dist, sizeof(uint8_t), pop);
    WRITE_BUF(conn_packed, n->genome_conn, sizeof(uint16_t), (size_t)p->max_conn * pop);
    WRITE_BUF(conn_weight, n->genome_wgt, sizeof(int16_t), (size_t)p->max_conn * pop);
    WRITE_BUF(conn_length, n->conn_length, sizeof(uint16_t), pop);
    WRITE_BUF(neuron_output, n->neuron_output, sizeof(float), (size_t)p->max_neurons * pop);
    WRITE_BUF(neuron_driven, n->neuron_driven, sizeof(uint8_t), (size_t)p->max_neurons * pop);
    WRITE_BUF(neuron_count, n->neuron_count, sizeof(uint8_t), pop);
    WRITE_BUF(signal, sim->signal, sizeof(uint32_t), p->signal_len);
    WRITE_BUF(rng_state, a->rng_state, sizeof(uint64_t), pop);
    WRITE_BUF(kill_marker, a->kill_marker, sizeof(uint8_t), pop);
    WRITE_BUF(challenge_bits, a->challenge_bits, sizeof(uint32_t), pop);
    /* Block on the last write to ensure all transfers are complete. */
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueWriteBuffer(q, p->bufs.grid, CL_TRUE, 0U,
                             (size_t)p->size_x * (size_t)p->size_y * sizeof(uint32_t),
                             (const void *)sim->grid.cells, 0U, NULL, NULL));

#undef WRITE_BUF

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("sync from host failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

void biosim_gpu_pipeline_free(biosim_gpu_pipeline_t *p) {
    if (!p) {
        return;
    }
    kernel_buffers_release(&p->bufs);
    CL_SAFE_RELEASE(clReleaseKernel, p->k5);
    CL_SAFE_RELEASE(clReleaseProgram, p->k5_program);
    CL_SAFE_RELEASE(clReleaseKernel, p->k4);
    CL_SAFE_RELEASE(clReleaseProgram, p->k4_program);
    CL_SAFE_RELEASE(clReleaseKernel, p->k3);
    CL_SAFE_RELEASE(clReleaseProgram, p->k3_program);
    CL_SAFE_RELEASE(clReleaseKernel, p->k2);
    CL_SAFE_RELEASE(clReleaseProgram, p->k2_program);
    CL_SAFE_RELEASE(clReleaseKernel, p->k1);
    CL_SAFE_RELEASE(clReleaseProgram, p->k1_program);
    memset(p, 0, sizeof(*p));
}
