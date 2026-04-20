#ifndef BIOSIM_STEPPER_TOML_H
#define BIOSIM_STEPPER_TOML_H

#include "biosim/core/params.h"
#include "biosim/core/status.h"

biosim_status_t stepper_load_toml_file(biosim_params_t *p, const char *path);

#endif /* BIOSIM_STEPPER_TOML_H */
