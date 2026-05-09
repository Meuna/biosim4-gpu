/*
 * HOST-ONLY: uses FILE* and dynamic allocation. Do NOT include from kernel sources.
 */
#ifndef BIOSIM_SIM_GPU_REGISTRY_H
#define BIOSIM_SIM_GPU_REGISTRY_H

#include "biosim/core/status.h"
#include <stddef.h>

/* ── kernel source bundle ─────────────────────────────────────────────────── */

/* Preamble strings prepended to every kernel program: types.h, rng.h, gene.h */
#define BIOSIM_GPU_PREAMBLE_COUNT 3U

/* Maximum source strings per program: preamble + one kernel source */
#define BIOSIM_GPU_MAX_SOURCES (BIOSIM_GPU_PREAMBLE_COUNT + 1U)

/*
 * Bundle of source strings ready to pass to clCreateProgramWithSource.
 *
 * sources[0..BIOSIM_GPU_PREAMBLE_COUNT-1] always point to embedded preamble
 * strings (types.h, rng.h, gene.h).
 * sources[BIOSIM_GPU_PREAMBLE_COUNT] points to the kernel source — either the
 * embedded fallback or a buffer loaded from the filesystem override.
 */
typedef struct {
    const char *sources[BIOSIM_GPU_MAX_SOURCES];
    size_t count;       /* always BIOSIM_GPU_MAX_SOURCES */
    char *override_buf; /* non-NULL: filesystem override; caller must free */
} biosim_gpu_kernel_sources_t;

/* Free override_buf if present; zero the struct. Safe to call on a
 * zero-initialised struct or after a failed biosim_gpu_registry_get. */
void biosim_gpu_kernel_sources_free(biosim_gpu_kernel_sources_t *s);

/* ── two-level lookup ─────────────────────────────────────────────────────── */

/*
 * Fill *out with source strings for the named kernel.
 *
 * Lookup order:
 *   1. Filesystem override — looks for <exec_dir>/<kernel_name>.cl alongside
 *      the running binary. exec_dir may be NULL to skip this level.
 *   2. Embedded fallback  — C string literal compiled into the binary.
 *
 * Returns:
 *   BIOSIM_OK         — *out is valid; call biosim_gpu_kernel_sources_free when done
 *   BIOSIM_ERR_NOTFOUND — kernel_name is not registered
 *   BIOSIM_ERR_IO       — filesystem override file found but could not be read
 *   BIOSIM_ERR_NOMEM    — filesystem override buffer allocation failed
 */
biosim_status_t biosim_gpu_registry_get(const char *kernel_name, const char *exec_dir,
                                        biosim_gpu_kernel_sources_t *out);

#endif /* BIOSIM_SIM_GPU_REGISTRY_H */
