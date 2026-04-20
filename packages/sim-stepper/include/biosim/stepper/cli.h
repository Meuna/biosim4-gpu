#ifndef BIOSIM_STEPPER_CLI_H
#define BIOSIM_STEPPER_CLI_H

#include "biosim/core/params.h"
#include "biosim/core/status.h"

/*
 * Extend p with sim-stepper-specific params, then apply three-pass resolution:
 *   Pass 1 — already done by caller via biosim_params_init()
 *   Pass 2 — TOML file (if --config present in argv)
 *   Pass 3 — CLI flags (override TOML)
 */
biosim_status_t stepper_params_resolve(biosim_params_t *p,
                                       int argc, char **argv);

#endif /* BIOSIM_STEPPER_CLI_H */
