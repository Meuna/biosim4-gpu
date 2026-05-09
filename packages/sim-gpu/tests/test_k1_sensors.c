#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/io_catalogue.h"
#include "biosim/core/log.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/registry.h"
#include "biosim/sim-gpu/runner.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_sim_t g_sim;
static biosim_gpu_runner_t g_runner;
static cl_program g_program;
static cl_kernel g_kernel;
static cl_mem g_buf_locx;
static cl_mem g_buf_locy;
static cl_mem g_buf_dir;
static cl_mem g_buf_rng;
static cl_mem g_buf_out;
static float *g_host_out;
static int g_opencl_ok;

/* Populate a small but representative sim used across all tests. */
static void make_test_sim(biosim_sim_t *sim) {
    memset(sim, 0, sizeof(*sim));
    sim->population = 32U;
    sim->size_x = 16;
    sim->size_y = 16;
    sim->genome_max_len = 4U;
    sim->max_neurons = 2U;
    sim->long_probe_dist = 4U;
    sim->steps_per_gen = 100U;
    sim->step = 25U;
    sim->gen_rng = biosim_rng_seed(0U, 1U);
    sim->challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    sim->challenge.x_band.x_min = 0.0F;
    sim->challenge.x_band.x_max = 1.0F;
}

/* ── OpenCL availability probe ──────────────────────────────────────────── */

static int opencl_available(void) {
    cl_uint n = 0U;
    return (clGetPlatformIDs(0U, NULL, &n) == CL_SUCCESS && n > 0U) ? 1 : 0;
}

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── helpers ────────────────────────────────────────────────────────────── */

/* Run the k_sensor_eval kernel for one sensor_id and return a freshly
 * allocated array of pop results. Caller must free.
 * Writes initial rng state from rng_init (size pop) before the kernel runs
 * so that RANDOM results are reproducible.
 * Returns NULL on any OpenCL error. */
static float *run_sensor_kernel(int sensor_id, const uint64_t *rng_init) {
    uint32_t pop = g_sim.population;

    if (rng_init) {
        if (clEnqueueWriteBuffer(g_runner.queue, g_buf_rng, CL_TRUE, 0U,
                                 (size_t)pop * sizeof(uint64_t), rng_init, 0U, NULL,
                                 NULL) != CL_SUCCESS) {
            return NULL;
        }
    }

    cl_int size_x = (cl_int)g_sim.size_x;
    cl_int size_y = (cl_int)g_sim.size_y;
    cl_uint step = (cl_uint)g_sim.step;
    cl_uint steps_gen = (cl_uint)g_sim.steps_per_gen;
    cl_int sid = (cl_int)sensor_id;

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_locx);
    (void)clSetKernelArg(g_kernel, 1U, sizeof(cl_mem), (const void *)&g_buf_locy);
    (void)clSetKernelArg(g_kernel, 2U, sizeof(cl_mem), (const void *)&g_buf_dir);
    (void)clSetKernelArg(g_kernel, 3U, sizeof(cl_mem), (const void *)&g_buf_rng);
    (void)clSetKernelArg(g_kernel, 4U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(g_kernel, 5U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(g_kernel, 6U, sizeof(cl_uint), &step);
    (void)clSetKernelArg(g_kernel, 7U, sizeof(cl_uint), &steps_gen);
    (void)clSetKernelArg(g_kernel, 8U, sizeof(cl_int), &sid);
    (void)clSetKernelArg(g_kernel, 9U, sizeof(cl_mem), (const void *)&g_buf_out);

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(g_runner.queue, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL,
                               NULL) != CL_SUCCESS) {
        return NULL;
    }

    float *out = malloc((size_t)pop * sizeof(float));
    if (!out) {
        return NULL;
    }
    if (clEnqueueReadBuffer(g_runner.queue, g_buf_out, CL_TRUE, 0U, (size_t)pop * sizeof(float),
                            out, 0U, NULL, NULL) != CL_SUCCESS) {
        free(out);
        return NULL;
    }
    return out;
}

/* ── one-time fixture setup / teardown ──────────────────────────────────── */

/* Called once before all tests to set up the shared OpenCL context. */
static void fixture_setup(void) {
    g_opencl_ok = 0;

    if (!opencl_available()) {
        return;
    }

    make_test_sim(&g_sim);
    biosim_status_t rc = biosim_sim_create(&g_sim, NULL, 0U);
    if (rc != BIOSIM_OK) {
        return;
    }

    rc = biosim_gpu_runner_create(0U, 0U, &g_runner);
    if (rc != BIOSIM_OK) {
        biosim_sim_free(&g_sim);
        return;
    }

    biosim_gpu_kernel_sources_t sources;
    memset(&sources, 0, sizeof(sources));
    rc = biosim_gpu_registry_get("k1_sensors", NULL, &sources);
    if (rc != BIOSIM_OK) {
        biosim_gpu_runner_free(&g_runner);
        biosim_sim_free(&g_sim);
        return;
    }

    rc = biosim_gpu_program_build(&g_runner, sources.sources, sources.count, &g_program);
    biosim_gpu_kernel_sources_free(&sources);
    if (rc != BIOSIM_OK) {
        biosim_gpu_runner_free(&g_runner);
        biosim_sim_free(&g_sim);
        return;
    }

    cl_int cl_err = CL_SUCCESS;
    g_kernel = clCreateKernel(g_program, "k_sensor_eval", &cl_err);
    if (cl_err != CL_SUCCESS) {
        (void)clReleaseProgram(g_program);
        biosim_gpu_runner_free(&g_runner);
        biosim_sim_free(&g_sim);
        return;
    }

    uint32_t pop = g_sim.population;
    size_t bs16 = (size_t)pop * sizeof(int16_t);
    size_t bu8 = (size_t)pop * sizeof(uint8_t);
    size_t bu64 = (size_t)pop * sizeof(uint64_t);
    size_t bf32 = (size_t)pop * sizeof(float);

    g_buf_locx = clCreateBuffer(g_runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bs16,
                                g_sim.agents.loc_x, &cl_err);
    if (cl_err != CL_SUCCESS) {
        goto cl_fail;
    }

    g_buf_locy = clCreateBuffer(g_runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bs16,
                                g_sim.agents.loc_y, &cl_err);
    if (cl_err != CL_SUCCESS) {
        goto cl_fail;
    }

    g_buf_dir = clCreateBuffer(g_runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bu8,
                               g_sim.agents.last_move_dir, &cl_err);
    if (cl_err != CL_SUCCESS) {
        goto cl_fail;
    }

    g_buf_rng = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bu64,
                               g_sim.agents.rng_state, &cl_err);
    if (cl_err != CL_SUCCESS) {
        goto cl_fail;
    }

    g_buf_out = clCreateBuffer(g_runner.context, CL_MEM_WRITE_ONLY, bf32, NULL, &cl_err);
    if (cl_err != CL_SUCCESS) {
        goto cl_fail;
    }

    g_host_out = malloc(bf32);
    if (!g_host_out) {
        goto cl_fail;
    }

    g_opencl_ok = 1;
    return;

cl_fail:
    if (g_buf_out) {
        (void)clReleaseMemObject(g_buf_out);
    }
    if (g_buf_rng) {
        (void)clReleaseMemObject(g_buf_rng);
    }
    if (g_buf_dir) {
        (void)clReleaseMemObject(g_buf_dir);
    }
    if (g_buf_locy) {
        (void)clReleaseMemObject(g_buf_locy);
    }
    if (g_buf_locx) {
        (void)clReleaseMemObject(g_buf_locx);
    }
    (void)clReleaseKernel(g_kernel);
    (void)clReleaseProgram(g_program);
    biosim_gpu_runner_free(&g_runner);
    biosim_sim_free(&g_sim);
}

static void fixture_teardown(void) {
    if (!g_opencl_ok) {
        return;
    }
    free(g_host_out);
    (void)clReleaseMemObject(g_buf_out);
    (void)clReleaseMemObject(g_buf_rng);
    (void)clReleaseMemObject(g_buf_dir);
    (void)clReleaseMemObject(g_buf_locy);
    (void)clReleaseMemObject(g_buf_locx);
    (void)clReleaseKernel(g_kernel);
    (void)clReleaseProgram(g_program);
    biosim_gpu_runner_free(&g_runner);
    biosim_sim_free(&g_sim);
}

/* ── test: compare each Group-A-minus-OSC1 sensor against host reference ── */

/* Tolerance for float comparison.  These sensors use only integer → float
 * casts and simple division; results should be bit-exact across host and
 * OpenCL CPU devices.  Use a small epsilon to tolerate denorm differences. */
#define FLOAT_EPS 1e-6F

static void assert_sensor_matches_reference(int sensor_id) {
    uint32_t pop = g_sim.population;

    float *gpu = run_sensor_kernel(sensor_id, NULL);
    TEST_ASSERT_NOT_NULL(gpu);

    for (uint32_t i = 0U; i < pop; i++) {
        float ref = biosim_sensor_eval((biosim_sensor_t)sensor_id, i, &g_sim);
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_EPS, ref, gpu[i]);
    }
    free(gpu);
}

void test_sensor_loc_x(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_LOC_X);
}

void test_sensor_loc_y(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_LOC_Y);
}

void test_sensor_boundary_dist_x(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_BOUNDARY_DIST_X);
}

void test_sensor_boundary_dist_y(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_BOUNDARY_DIST_Y);
}

void test_sensor_boundary_dist(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_BOUNDARY_DIST);
}

void test_sensor_last_move_dir_x(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_LAST_MOVE_DIR_X);
}

void test_sensor_last_move_dir_y(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_LAST_MOVE_DIR_Y);
}

void test_sensor_age(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }
    assert_sensor_matches_reference(BIOSIM_SENSOR_AGE);
}

void test_sensor_random(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("No OpenCL platform/device available");
    }

    uint32_t pop = g_sim.population;

    /* Save the initial rng state so we can give the kernel the same starting
     * state that the host reference will use. */
    uint64_t *rng_saved = malloc((size_t)pop * sizeof(uint64_t));
    TEST_ASSERT_NOT_NULL(rng_saved);
    memcpy(rng_saved, g_sim.agents.rng_state, (size_t)pop * sizeof(uint64_t));

    /* Host reference — mutates g_sim.agents.rng_state */
    float *ref = malloc((size_t)pop * sizeof(float));
    TEST_ASSERT_NOT_NULL(ref);
    for (uint32_t i = 0U; i < pop; i++) {
        ref[i] = biosim_sensor_eval(BIOSIM_SENSOR_RANDOM, i, &g_sim);
    }

    /* Kernel — reset to the original rng_state before running */
    float *gpu = run_sensor_kernel(BIOSIM_SENSOR_RANDOM, rng_saved);
    TEST_ASSERT_NOT_NULL(gpu);

    for (uint32_t i = 0U; i < pop; i++) {
        TEST_ASSERT_FLOAT_WITHIN(FLOAT_EPS, ref[i], gpu[i]);
    }

    free(gpu);
    free(ref);
    free(rng_saved);
}

/* ── entry point ────────────────────────────────────────────────────────── */

int main(void) {
    biosim_log_init(&biosim_log_default_ctx);

    fixture_setup();

    UNITY_BEGIN();
    RUN_TEST(test_sensor_loc_x);
    RUN_TEST(test_sensor_loc_y);
    RUN_TEST(test_sensor_boundary_dist_x);
    RUN_TEST(test_sensor_boundary_dist_y);
    RUN_TEST(test_sensor_boundary_dist);
    RUN_TEST(test_sensor_last_move_dir_x);
    RUN_TEST(test_sensor_last_move_dir_y);
    RUN_TEST(test_sensor_age);
    RUN_TEST(test_sensor_random);
    int result = UNITY_END();

    fixture_teardown();
    return result;
}
