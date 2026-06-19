/*
 * GPU profiling accumulators and benchmark metric computation.
 *
 * biosim_gpu_profile_t holds raw cl_event-derived timings collected by the
 * pipeline when the runner has profiling enabled.  The bench_compute /
 * report_print pair turns those raw counters into human-readable performance
 * metrics; both are pure (no OpenCL) and therefore unit-testable without a GPU.
 */
#ifndef BIOSIM_SIM_GPU_BENCHMARK_H
#define BIOSIM_SIM_GPU_BENCHMARK_H

#include <stdint.h>
#include <stdio.h>

/* Number of kernels in the per-step pipeline (K1..K5). */
#define BIOSIM_GPU_KERNEL_COUNT 5U

/*
 * Raw profiling accumulators.  Kernel times are GPU-side nanoseconds taken from
 * CL_PROFILING_COMMAND_START/END; counts are the number of enqueues sampled.
 * Zero-initialise before use.
 */
typedef struct {
    uint64_t kernel_ns[BIOSIM_GPU_KERNEL_COUNT];
    uint64_t kernel_count[BIOSIM_GPU_KERNEL_COUNT];
    uint64_t sync_to_ns;   /* device->host reads  (clEnqueueReadBuffer)  */
    uint64_t sync_from_ns; /* host->device writes (clEnqueueWriteBuffer) */
} biosim_gpu_profile_t;

/* Human-readable kernel names, indexed 0..BIOSIM_GPU_KERNEL_COUNT-1. */
extern const char *const biosim_gpu_kernel_names[BIOSIM_GPU_KERNEL_COUNT];

/* Derived per-kernel metrics. */
typedef struct {
    double total_ms;    /* kernel_ns / 1e6                     */
    double pct;         /* share of total kernel time, 0..100  */
    double us_per_step; /* kernel_ns / count / 1e3             */
} biosim_gpu_kernel_metric_t;

/* Derived benchmark metrics for one timed run. */
typedef struct {
    biosim_gpu_kernel_metric_t kernels[BIOSIM_GPU_KERNEL_COUNT];
    double total_kernel_ms;
    double wall_ms;
    double steps_per_s;
    double agent_steps_per_s;
    double gens_per_s;
    double sync_to_ms;
    double sync_from_ms;
} biosim_gpu_bench_metrics_t;

/*
 * Compute derived metrics from raw accumulators and the timed wall-clock span.
 * Pure and deterministic.  population/steps_per_gen/gens describe the timed
 * workload.  Divide-by-zero inputs (zero counts, zero wall time, zero total
 * kernel time) yield 0 for the affected fields rather than NaN/inf.
 */
void biosim_gpu_bench_compute(
    const biosim_gpu_profile_t *raw,
    uint64_t wall_ns,
    uint32_t population,
    uint32_t steps_per_gen,
    uint32_t gens,
    biosim_gpu_bench_metrics_t *out
);

/* Print a formatted metrics report (kernel table, throughput, transfers). */
void biosim_gpu_bench_report_print(const biosim_gpu_bench_metrics_t *m, FILE *out);

#endif /* BIOSIM_SIM_GPU_BENCHMARK_H */
