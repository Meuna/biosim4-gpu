/*
 * HOST-ONLY: uses FILE* and references biosim_sim_t which carries heap
 * pointers. Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_SNAPSHOT_H
#define BIOSIM_CORE_SNAPSHOT_H

#include "biosim/core/sim.h"
#include "biosim/core/snapshot_defs.h"
#include <stdint.h>

/* ── survivor snap lifecycle ─────────────────────────────────────────────── */

/*
 * Grow snap to hold at least n_survivors entries each with at least
 * g_max_len columns.  Uses a doubling policy on each dimension independently.
 * Tolerates zero-initialised snap (first-time alloc).
 * n_survivors == 0 is a no-op.
 * On realloc failure, the partially-grown snap is left intact; the caller
 * must free it with biosim_survivor_snap_free.
 */
biosim_status_t biosim_survivor_snap_grow(
    biosim_survivor_snap_t *snap, uint32_t n_survivors, uint16_t g_max_len
);

/*
 * Free all buffers in snap.  Tolerates NULL snap and NULL members.
 * Does NOT free snap itself (snap is typically stack-allocated by the caller).
 * Zeros all fields so snap is safe to pass to grow again.
 */
void biosim_survivor_snap_free(biosim_survivor_snap_t *snap);

/* ── high-level restore ──────────────────────────────────────────────────── */

/*
 * Open path, validate the header, run coherency checks (warns to stderr on
 * topology mismatches; fatal error on schema/catalogue mismatch), load the
 * last generation record into snap (growing it as needed), and update
 * sim->gen and sim->gen_rng from the loaded record.
 * The caller increments sim->gen if starting a new generation from the snap.
 * Does NOT call breed; the caller is responsible for calling
 * biosim_generation_spawn to produce the next population.
 * Returns BIOSIM_ERR_INVALID on file, format, or fatal compat errors.
 * Returns BIOSIM_ERR_NOMEM on allocation failure.
 */
biosim_status_t biosim_snapshot_load_survivors(
    const char *path, biosim_sim_t *sim, biosim_survivor_snap_t *snap
);

/*
 * Low-level FILE* loader. f must be open for reading; the function seeks to
 * byte 0 internally. Reads the header, seeks to the last generation record,
 * and loads every survivor in that record into snap (growing it as needed).
 * Sets snap->gen and snap->gen_rng from the loaded record.
 * Does NOT perform coherency checks and does NOT update any sim fields.
 * Does NOT fclose f.
 * Returns BIOSIM_ERR_INVALID on file or format errors.
 * Returns BIOSIM_ERR_NOMEM on allocation failure.
 * Returns BIOSIM_EOF when no complete generation record is found.
 */
biosim_status_t biosim_snapshot_load_survivors_f(FILE *f, biosim_survivor_snap_t *snap);

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

#endif /* BIOSIM_CORE_SNAPSHOT_H */
