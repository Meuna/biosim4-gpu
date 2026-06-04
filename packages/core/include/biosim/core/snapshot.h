/*
 * HOST-ONLY: uses FILE* and references biosim_sim_t which carries heap
 * pointers. Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_SNAPSHOT_H
#define BIOSIM_CORE_SNAPSHOT_H

#include "biosim/core/sim.h"
#include "biosim/core/survivor_snap.h"
#include <stdint.h>

/* Bump when the on-disk record layout changes. */
#define BIOSIM_SNAP_FORMAT_VERSION 2U

/*
 * Information read from the 32-byte file header.
 * Used by the caller to perform coherency checks before loading agent data.
 */
typedef struct {
    uint16_t format_version;
    uint16_t schema_version;
    uint16_t num_sensors;
    uint16_t num_actions;
    uint16_t genome_max_len;
    uint8_t max_neurons;
    uint32_t generation_count; /* 0 = unknown / streaming */
} biosim_snap_header_t;

/* ── high-level restore ──────────────────────────────────────────────────── */

/*
 * Open path, validate the header, run coherency checks (warns to stderr on
 * topology mismatches; fatal error on schema/catalogue mismatch), load the
 * last generation record into snap (growing it as needed), and update
 * sim->gen (set to gen_idx + 1) and sim->gen_rng.
 * Does NOT call breed; the caller is responsible for calling
 * biosim_generation_spawn to produce the next population.
 * Returns BIOSIM_ERR_INVALID on file, format, or fatal compat errors.
 * Returns BIOSIM_ERR_NOMEM on allocation failure.
 */
biosim_status_t biosim_snapshot_load_survivors(
    const char *path, biosim_sim_t *sim, biosim_survivor_snap_t *snap
);

/* ── output session ──────────────────────────────────────────────────────── */

/*
 * Refuse if path already exists, then open the file, write the header, and
 * store interval for scheduling in sim->snap_*.
 * Returns BIOSIM_ERR_INVALID if the file already exists or cannot be created.
 */
biosim_status_t biosim_snapshot_session_open(biosim_sim_t *sim, const char *path, int interval);

/*
 * Write a generation record if the current generation index satisfies the
 * schedule (interval > 0: every Nth gen; interval = 0: final gen only).
 * Skips silently when snap->count == 0 or sim->snap_f == NULL.
 * Returns BIOSIM_OK even on write failure (non-fatal); the error is logged to
 * stderr.
 */
biosim_status_t biosim_snapshot_session_write(
    biosim_sim_t *sim, const biosim_survivor_snap_t *snap
);

/*
 * Finalise (write generation_count) and close the session file.
 * No-op when sim->snap_f == NULL.
 */
biosim_status_t biosim_snapshot_session_close(biosim_sim_t *sim);

/* ── write side ─────────────────────────────────────────────────────────── */

/*
 * Write the 32-byte file header. Call once before any generation records.
 * Returns BIOSIM_ERR_INVALID on I/O failure.
 */
biosim_status_t biosim_snapshot_write_header(FILE *f, const biosim_sim_t *sim);

/*
 * Append one generation record to an open snapshot file.
 * Records sim->gen and sim->gen_rng at the moment of the call; call this
 * after biosim_generation_collect_survivors and before
 * biosim_generation_breed so gen_rng is captured at the correct point.
 * Returns BIOSIM_ERR_IO on I/O failure.
 * Returns BIOSIM_ERR_NOMEM if a temporary buffer cannot be allocated.
 */
biosim_status_t biosim_snapshot_write_genome(
    FILE *f, const biosim_sim_t *sim, const biosim_survivor_snap_t *snap
);

/*
 * Seek to offset 16 in f and write the final generation_count.
 * Call once after all biosim_snapshot_write_genome calls; f must support seeking.
 * Returns BIOSIM_ERR_INVALID on I/O failure.
 */
biosim_status_t biosim_snapshot_finalize(FILE *f, uint32_t generation_count);

/* ── read side ──────────────────────────────────────────────────────────── */

/*
 * Read and validate the 32-byte file header from the start of f.
 * Returns BIOSIM_ERR_INVALID on magic mismatch or unsupported format_version.
 * Does NOT validate num_sensors/num_actions against the compiled-in catalogue;
 * that coherency check is the caller's responsibility.
 * On return, f is positioned at the start of the first generation record.
 */
biosim_status_t biosim_snapshot_read_header(FILE *f, biosim_snap_header_t *header_out);

/*
 * Load survivor genomes from the 0-based generation entry at gen_idx into
 * sim->genome slots 0..n-1, where n = min(file_n_survivors, sim->genome.population).
 * sim must already be fully allocated via biosim_sim_create. scores_out receives the
 * n challenge scores; must point to at least sim->genome.population floats.
 * On success:
 *   *n_survivors_out  — number of survivors loaded
 *   *gen_idx_out      — the generation index stored in the entry
 *   *gen_rng_out      — gen_rng captured at snapshot-write time
 * Returns BIOSIM_ERR_NOTFOUND if gen_idx is out of range.
 * Returns BIOSIM_ERR_IO on I/O failure.
 * Returns BIOSIM_EOF on end-of-file.
 * Returns BIOSIM_ERR_NOMEM if a temporary buffer cannot be allocated.
 */
biosim_status_t biosim_snapshot_load(
    FILE *f,
    uint32_t gen_idx,
    const biosim_snap_header_t *header,
    biosim_sim_t *sim,
    float *scores_out,
    uint32_t *n_survivors_out,
    uint32_t *gen_idx_out,
    uint64_t *gen_rng_out
);

/*
 * Convenience: load the last generation entry in the file.
 * Uses header->generation_count if non-zero; otherwise scans to EOF.
 * Same output and return conventions as biosim_snapshot_load.
 */
biosim_status_t biosim_snapshot_load_last(
    FILE *f,
    const biosim_snap_header_t *header,
    biosim_sim_t *sim,
    float *scores_out,
    uint32_t *n_survivors_out,
    uint32_t *gen_idx_out,
    uint64_t *gen_rng_out
);

#endif /* BIOSIM_CORE_SNAPSHOT_H */
