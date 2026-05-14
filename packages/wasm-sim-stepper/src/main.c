#include "biosim/core/census.h"
#include "biosim/core/challenge_spec.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── hardcoded simulation parameters ────────────────────────────────────── */

/* clang-format off */
static const biosim_param_entry_t s_params[] = {
    {"max-generations",           "simulation", {.i = 20},    PARAM_INT,   false, true, NULL, NULL},
    {"population",                "simulation", {.i = 3000},  PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-x",               "simulation", {.i = 128},   PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-y",               "simulation", {.i = 128},   PARAM_INT,   false, true, NULL, NULL},
    {"max-genome-len",            "genome",     {.i = 24},    PARAM_INT,   false, true, NULL, NULL},
    {"max-neurons",               "genome",     {.i = 5},     PARAM_INT,   false, true, NULL, NULL},
    {"long-probe-dist",           "sensors",    {.i = 16},    PARAM_INT,   false, true, NULL, NULL},
    {"steps-per-gen",             "simulation", {.i = 300},   PARAM_INT,   false, true, NULL, NULL},
    {"population-sensor-radius",  "sensors",    {.i = 2},     PARAM_INT,   false, true, NULL, NULL},
    {"enable-kill",               "actions",    {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"point-mutation-rate",       "genome",     {.f = 0.001}, PARAM_FLOAT, false, true, NULL, NULL},
    {"sexual-reproduction",       "genome",     {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"choose-parents-by-fitness", "genome",     {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
};
/* clang-format on */
#define S_PARAMS_COUNT (sizeof(s_params) / sizeof(s_params[0]))

/* ── entry point ─────────────────────────────────────────────────────────── */

int main(void) {
    biosim_log_init(&biosim_log_default_ctx);

    biosim_status_t returncode = BIOSIM_OK;
    biosim_params_t p;
    biosim_sim_t sim;
    memset(&sim, 0, sizeof(sim));

    returncode = biosim_params_init(&p, s_params, S_PARAMS_COUNT);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    biosim_challenge_spec_t challenge;
    memset(&challenge, 0, sizeof(challenge));
    challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    challenge.x_band.x_min = 0.5F;
    challenge.x_band.x_max = 1.0F;
    challenge.x_band.mirror = false;

    returncode = biosim_sim_create(&sim, &p, &challenge, NULL, 0U);
    biosim_params_free(&p);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    /* ── main generation loop ────────────────────────────────────────────── */

    biosim_census_print_header(stdout);

    while (sim.gen < sim.max_generations) {
        while (sim.step < sim.steps_per_gen) {
            for (uint32_t i = 0U; i < sim.agents.population; i++) {
                if (sim.agents.alive[i]) {
                    biosim_sim_step_agent(&sim, i);
                }
            }
            biosim_sim_next_step(&sim);
        }

        biosim_census_t census;
        returncode = biosim_sim_next_generation(&sim, &census);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        biosim_census_print(stdout, &census);
        (void)fflush(stdout);
    }

exit:
    biosim_sim_free(&sim);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("simulation failed (%s)", biosim_strerror(returncode));
    }
    return (int)returncode;
}
