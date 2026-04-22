#include <stdio.h>

#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include "biosim/stepper/cli.h"
#include "biosim/stepper/step.h"

static const biosim_build_info_t build_info = {
    .progname = BIOSIM_PROGNAME,
    .version = BIOSIM_GIT_VERSION,
    .build_timestamp = BIOSIM_BUILD_TIMESTAMP,
    .build_type = BIOSIM_BUILD_TYPE,
};

int main(int argc, char **argv) {
    biosim_params_t p;
    biosim_params_init(&p);

    biosim_status_t st = stepper_cli_and_toml(&p, &build_info, argc, argv);
    if (st != BIOSIM_OK) {
        biosim_params_free(&p);
        return 1;
    }

    biosim_stepper_t sim;
    st = biosim_stepper_create(&sim, &p);
    if (st != BIOSIM_OK) {
        (void)fprintf(stderr, "biosim-stepper: init failed (status %d)\n", (int)st);
        biosim_params_free(&p);
        return 1;
    }

    const int steps = biosim_params_get_int(&p, "steps-per-gen");
    for (int s = 0; s < steps; s++) {
        biosim_stepper_step(&sim);
    }

    biosim_stepper_free(&sim);
    biosim_params_free(&p);
    return 0;
}
