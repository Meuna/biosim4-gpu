#ifndef BIOSIM_STEPPER_CLI_H
#define BIOSIM_STEPPER_CLI_H

#include "biosim/core/params.h"
#include "biosim/core/status.h"

/* Build-time identity passed in by main; keeps injected macros out of cli.c */
typedef struct {
    const char *progname;
    const char *version;
    const char *build_timestamp;
    const char *build_type;
} biosim_build_info_t;

/*
 * Extend p with sim-stepper-specific params, then apply three-pass resolution:
 *   Pass 1 — already done by caller via biosim_params_init()
 *   Pass 2 — TOML file (if --config present in argv)
 *   Pass 3 — CLI flags (override TOML)
 */
biosim_status_t stepper_cli_and_toml(biosim_params_t *p, const biosim_build_info_t *info, int argc,
                                     char **argv);

#endif /* BIOSIM_STEPPER_CLI_H */
