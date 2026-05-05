#ifndef BIOSIM_PARAMS_BARRIERS_H
#define BIOSIM_PARAMS_BARRIERS_H

#include "biosim/core/barriers.h"
#include "biosim/core/status.h"

/*
 * Parse barrier specs from a TOML config file.
 *
 * Reads the [barriers] section for num-barriers and [barrier-1]..[barrier-N]
 * tables for individual specs.  On success *specs_out points to a heap-allocated
 * array of *n_out elements; the caller must free() it.
 *
 * Returns BIOSIM_OK with *n_out = 0 when:
 *   - toml_path is NULL
 *   - the file has no [barriers] section
 *   - num-barriers is 0
 * Returns BIOSIM_ERR_NOTFOUND if the file cannot be opened/parsed.
 * Returns BIOSIM_ERR_INVALID if a [barrier-N] table has an unknown kind string.
 * Returns BIOSIM_ERR_NOMEM on allocation failure.
 */
biosim_status_t biosim_barrier_params_load(const char *toml_path, biosim_barrier_spec_t **specs_out,
                                           uint32_t *n_out);

#endif /* BIOSIM_PARAMS_BARRIERS_H */
