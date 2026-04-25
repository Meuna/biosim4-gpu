#ifndef BIOSIM_PARAMS_CHALLENGES_H
#define BIOSIM_PARAMS_CHALLENGES_H

#include "biosim/core/challenges.h"
#include "biosim/core/status.h"
#include "biosim/params/params.h"

/*
 * Build a biosim_challenge_spec_t from an already-resolved biosim_params_t.
 *
 * Reads the "kind" string key and the kind-specific float/bool keys from p.
 * All keys must be present in p (i.e. the simulator must have declared them
 * in its entry table with appropriate defaults before calling this function).
 *
 * Returns BIOSIM_ERR_NOTFOUND if the "kind" key is absent.
 * Returns BIOSIM_ERR_INVALID  if the "kind" value is not a recognised kind.
 * Returns BIOSIM_OK on success, populating *out.
 */
biosim_status_t biosim_challenge_spec_from_params(const biosim_params_t *p,
                                                  biosim_challenge_spec_t *out);

#endif /* BIOSIM_PARAMS_CHALLENGES_H */
