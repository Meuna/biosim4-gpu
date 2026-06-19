#include <stdint.h>

#include "biosim/sim-gpu/benchmark.h"
#include "unity.h"

void setUp(void) {
}
void tearDown(void) {
}

/* ── biosim_gpu_bench_compute ────────────────────────────────────────────── */

/* Derived metrics from a representative set of raw accumulators. */
void test_bench_compute_basic(void) {
    biosim_gpu_profile_t raw = {0};
    raw.kernel_ns[0] = 400000000ULL; /* 400 ms */
    raw.kernel_ns[1] = 100000000ULL; /* 100 ms */
    raw.kernel_ns[2] = 300000000ULL; /* 300 ms */
    raw.kernel_ns[3] = 100000000ULL; /* 100 ms */
    raw.kernel_ns[4] = 100000000ULL; /* 100 ms; total 1000 ms */
    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        raw.kernel_count[i] = 100U;
    }
    raw.sync_to_ns = 50000000ULL;   /* 50 ms  */
    raw.sync_from_ns = 250000000ULL; /* 250 ms */

    biosim_gpu_bench_metrics_t m;
    /* wall 2 s, pop 1000, 10 steps/gen, 20 gens -> 200 timed steps. */
    biosim_gpu_bench_compute(&raw, 2000000000ULL, 1000U, 10U, 20U, &m);

    TEST_ASSERT_FLOAT_WITHIN(0.001F, 1000.0F, (float)m.total_kernel_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 40.0F, (float)m.kernels[0].pct);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 10.0F, (float)m.kernels[1].pct);
    /* 400 ms over 100 enqueues = 4000 us/step. */
    TEST_ASSERT_FLOAT_WITHIN(0.1F, 4000.0F, (float)m.kernels[0].us_per_step);

    TEST_ASSERT_FLOAT_WITHIN(0.01F, 100.0F, (float)m.steps_per_s);
    TEST_ASSERT_FLOAT_WITHIN(1.0F, 100000.0F, (float)m.agent_steps_per_s);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 10.0F, (float)m.gens_per_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 50.0F, (float)m.sync_to_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 250.0F, (float)m.sync_from_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 2000.0F, (float)m.wall_ms);
}

/* Per-kernel percentages sum to ~100 when any kernel time is present. */
void test_bench_compute_pct_sums_to_100(void) {
    biosim_gpu_profile_t raw = {0};
    raw.kernel_ns[0] = 13U;
    raw.kernel_ns[1] = 71U;
    raw.kernel_ns[2] = 5U;
    raw.kernel_ns[3] = 200U;
    raw.kernel_ns[4] = 42U;

    biosim_gpu_bench_metrics_t m;
    biosim_gpu_bench_compute(&raw, 1000000ULL, 10U, 10U, 1U, &m);

    float sum = 0.0F;
    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        sum += (float)m.kernels[i].pct;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 100.0F, sum);
}

/* Divide-by-zero inputs yield zeros, not NaN/inf. */
void test_bench_compute_zero_guards(void) {
    biosim_gpu_profile_t raw = {0}; /* all zero: no kernel time, no counts */

    biosim_gpu_bench_metrics_t m;
    /* wall_ns = 0 must not divide. */
    biosim_gpu_bench_compute(&raw, 0ULL, 1000U, 300U, 20U, &m);

    TEST_ASSERT_EQUAL_FLOAT(0.0F, (float)m.steps_per_s);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, (float)m.agent_steps_per_s);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, (float)m.gens_per_s);
    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0F, (float)m.kernels[i].pct);
        TEST_ASSERT_EQUAL_FLOAT(0.0F, (float)m.kernels[i].us_per_step);
    }
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bench_compute_basic);
    RUN_TEST(test_bench_compute_pct_sums_to_100);
    RUN_TEST(test_bench_compute_zero_guards);
    return UNITY_END();
}
