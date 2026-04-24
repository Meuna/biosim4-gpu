#include <stdio.h>

#include "biosim/core/status.h"
#include "biosim/params/params.h"
#include "biosim/stepper/step.h"

// clang-format off
static const biosim_param_entry_t sim_params[] = {
    {"sim-name",                 "simulation", {.s = "unnamed"}, PARAM_STRING, false, true,  "sim-name",                 "s"},
    {"population",               "simulation", {.i = 3000},      PARAM_INT,    false, true,  "population",               "p"},
    {"grid-size-x",              "simulation", {.i = 128},       PARAM_INT,    false, true,  "grid-size-x",              NULL},
    {"grid-size-y",              "simulation", {.i = 128},       PARAM_INT,    false, true,  "grid-size-y",              NULL},
    {"steps-per-gen",            "simulation", {.i = 300},       PARAM_INT,    false, true,  "steps-per-gen",            NULL},
    {"max-genome-length",        "simulation", {.i = 24},        PARAM_INT,    false, true,  "max-genome-length",        NULL},
    {"mutation-rate",            "simulation", {.f = 0.001},     PARAM_FLOAT,  false, true,  "mutation-rate",            NULL},
    {"challenge",                "simulation", {.i = 0},         PARAM_INT,    false, true,  "challenge",                NULL},
    {"long-probe-dist",          "simulation", {.i = 16},        PARAM_INT,    false, true,  "long-probe-dist",          NULL},
    {"max-neurons",              "simulation", {.i = 5},         PARAM_INT,    false, true,  "max-neurons",              NULL},
    {"population-sensor-radius", "simulation", {.i = 2},         PARAM_INT,    false, true,  "population-sensor-radius", NULL},
    {"trace-out",                NULL,         {.s = ""},        PARAM_STRING, false, false, NULL,                       NULL},
};
// clang-format on
#define SIM_PARAMS_COUNT (sizeof(sim_params) / sizeof(sim_params[0]))

int main(int argc, char **argv) {
    biosim_params_t p;
    biosim_params_init(&p, sim_params, SIM_PARAMS_COUNT);

    char version_buf[256];
    (void)snprintf(version_buf, sizeof(version_buf), "%s (%s, %s)", BIOSIM_GIT_VERSION,
                   BIOSIM_BUILD_TYPE, BIOSIM_BUILD_TIMESTAMP);

    biosim_status_t st = biosim_params_parse(&p, BIOSIM_PROGNAME, version_buf, argc, argv);
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

    for (int s = 0; s < sim.base.steps_per_gen; s++) {
        biosim_stepper_step(&sim);
    }

    biosim_stepper_free(&sim);
    biosim_params_free(&p);
    return 0;
}
