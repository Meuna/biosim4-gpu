#include "biosim/sim-gpu/benchmark.h"

#include <string.h>

const char *const biosim_gpu_kernel_names[BIOSIM_GPU_KERNEL_COUNT] = {
    "k1_feedforward",
    "k2_kill_marked",
    "k3_movement_resolution",
    "k4_signal_fade",
    "k5_challenge_step_eval",
};

void biosim_gpu_bench_compute(
    const biosim_gpu_profile_t *raw,
    uint64_t wall_ns,
    uint32_t population,
    uint32_t steps_per_gen,
    uint32_t gens,
    biosim_gpu_bench_metrics_t *out
) {
    memset(out, 0, sizeof(*out));

    uint64_t total_kernel_ns = 0U;
    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        total_kernel_ns += raw->kernel_ns[i];
    }
    out->total_kernel_ms = (double)total_kernel_ns / 1.0e6;

    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        biosim_gpu_kernel_metric_t *k = &out->kernels[i];
        k->total_ms = (double)raw->kernel_ns[i] / 1.0e6;
        k->pct =
            total_kernel_ns ? 100.0 * (double)raw->kernel_ns[i] / (double)total_kernel_ns : 0.0;
        k->us_per_step = raw->kernel_count[i]
                             ? (double)raw->kernel_ns[i] / (double)raw->kernel_count[i] / 1.0e3
                             : 0.0;
    }

    out->wall_ms = (double)wall_ns / 1.0e6;
    out->sync_to_ms = (double)raw->sync_to_ns / 1.0e6;
    out->sync_from_ms = (double)raw->sync_from_ns / 1.0e6;

    if (wall_ns != 0U) {
        double wall_s = (double)wall_ns / 1.0e9;
        double total_steps = (double)steps_per_gen * (double)gens;
        out->steps_per_s = total_steps / wall_s;
        out->agent_steps_per_s = (double)population * total_steps / wall_s;
        out->gens_per_s = (double)gens / wall_s;
    }
}

void biosim_gpu_bench_report_print(const biosim_gpu_bench_metrics_t *m, FILE *out) {
    (void)fprintf(out, "%-24s %10s %7s %10s\n", "kernel", "total ms", "%", "us/step");
    for (size_t i = 0U; i < BIOSIM_GPU_KERNEL_COUNT; i++) {
        const biosim_gpu_kernel_metric_t *k = &m->kernels[i];
        (void)fprintf(
            out,
            "%-24s %10.3f %6.1f%% %10.3f\n",
            biosim_gpu_kernel_names[i],
            k->total_ms,
            k->pct,
            k->us_per_step
        );
    }
    (void)fprintf(out, "%-24s %10.3f\n", "total kernel", m->total_kernel_ms);
    (void)fprintf(out, "\n");
    (void)fprintf(
        out,
        "throughput: %.0f steps/s, %.3g agent-steps/s, %.2f gens/s\n",
        m->steps_per_s,
        m->agent_steps_per_s,
        m->gens_per_s
    );
    (void)fprintf(
        out, "transfer:   sync_to %.3f ms, sync_from %.3f ms\n", m->sync_to_ms, m->sync_from_ms
    );
    (void)fprintf(out, "wall:       %.3f ms\n", m->wall_ms);
}
