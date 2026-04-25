#include <stdio.h>
#include <stdlib.h>

#include "biosim/params/barriers.h"
#include "biosim/params/challenges.h"
#include "biosim/params/params.h"
#include "biosim/stepper/step.h"

// clang-format off
static const biosim_param_entry_t sim_params[] = {
    {"population",               "simulation", {.i = 3000},      PARAM_INT,    false, true, "pop",               "p"},
    {"grid-size-x",              "simulation", {.i = 128},       PARAM_INT,    false, true, "grid-size-x",       "x"},
    {"grid-size-y",              "simulation", {.i = 128},       PARAM_INT,    false, true, "grid-size-y",       "y"},
    {"steps-per-gen",            "simulation", {.i = 300},       PARAM_INT,    false, true, "steps-per-gen",     NULL},
    {"max-genome-length",        "genome",     {.i = 24},        PARAM_INT,    false, true, "max-genome-length", NULL},
    {"max-neurons",              "genome",     {.i = 5},         PARAM_INT,    false, true, "max-neurons",       NULL},
    {"long-probe-dist",          "sensors",    {.i = 16},        PARAM_INT,    false, true, "long-probe-dist",   NULL},
    {"population-sensor-radius", "sensors",    {.i = 2},         PARAM_INT,    false, true, "pop-sensor-radius", NULL},
    {"kind",                     "challenge",  {.s = "x_band"},  PARAM_STRING, false, true, "challenge-kind",    NULL},
    {"x-min",                    "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true, "challenge-x-min",   NULL},
    {"x-max",                    "challenge",  {.f = 1.0},       PARAM_FLOAT,  false, true, "challenge-x-max",   NULL},
    {"mirror",                   "challenge",  {.b = false},     PARAM_BOOL,   false, true, "challenge-mirror",  NULL},
};
// clang-format on
#define SIM_PARAMS_COUNT (sizeof(sim_params) / sizeof(sim_params[0]))

int main(int argc, char **argv) {
    biosim_params_t p;
    biosim_status_t st = biosim_params_init(&p, sim_params, SIM_PARAMS_COUNT);
    if (st != BIOSIM_OK) {
        return st;
    }

    char version_buf[256];
    (void)snprintf(version_buf, sizeof(version_buf), "%s (%s, %s)", BIOSIM_GIT_VERSION,
                   BIOSIM_BUILD_TYPE, BIOSIM_BUILD_TIMESTAMP);

    st = biosim_params_parse(&p, BIOSIM_PROGNAME, version_buf, argc, argv);
    if (st != BIOSIM_OK) {
        biosim_params_free(&p);
        return st;
    }

    biosim_barrier_spec_t *barriers = NULL;
    int n_barriers = 0;
    st = biosim_barrier_params_load(p.config_path, &barriers, &n_barriers);
    if (st != BIOSIM_OK) {
        (void)fprintf(stderr, "biosim-stepper: barrier config error (status %d)\n", (int)st);
        biosim_params_free(&p);
        return st;
    }

    biosim_challenge_spec_t challenge;
    st = biosim_challenge_spec_from_params(&p, &challenge);
    if (st != BIOSIM_OK) {
        (void)fprintf(stderr, "biosim-stepper: challenge config error (status %d)\n", (int)st);
        free(barriers);
        biosim_params_free(&p);
        return st;
    }

    biosim_stepper_t sim;
    st = biosim_stepper_create(&sim, &p, barriers, n_barriers, challenge);
    free(barriers);
    if (st != BIOSIM_OK) {
        (void)fprintf(stderr, "biosim-stepper: init failed (status %d)\n", (int)st);
        biosim_params_free(&p);
        return st;
    }
    sim.base.challenge = challenge;

    for (int s = 0; s < sim.base.steps_per_gen; s++) {
        biosim_stepper_step(&sim);
    }

    biosim_stepper_free(&sim);
    biosim_params_free(&p);
    return 0;
}
