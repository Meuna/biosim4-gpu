#include <stdio.h>

#include "biosim/core/params.h"
#include "biosim/stepper/cli.h"

int main(int argc, char **argv) {
    biosim_params_t p;
    biosim_params_init(&p);
    stepper_params_resolve(&p, argc, argv);

    printf("sim-name:   %s\n", biosim_params_get_string(&p, "sim-name"));
    printf("population: %d\n", biosim_params_get_int(&p, "population"));

    biosim_params_free(&p);
    return 0;
}
