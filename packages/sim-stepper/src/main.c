#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/core/challenges.h"
#include "biosim/core/gen.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "biosim/core/step.h"
#include "biosim/params/barriers.h"
#include "biosim/params/challenges.h"
#include "biosim/params/params.h"

// clang-format off
static const biosim_param_entry_t sim_params[] = {
    {"population",               "simulation", {.i = 3000},      PARAM_INT,    false, true, "pop",            "p"},
    {"grid-size-x",              "simulation", {.i = 128},       PARAM_INT,    false, true, "grid-size-x",    "x"},
    {"grid-size-y",              "simulation", {.i = 128},       PARAM_INT,    false, true, "grid-size-y",    "y"},
    {"steps-per-gen",            "simulation", {.i = 300},       PARAM_INT,    false, true, "steps-per-gen",  NULL},
    {"max-generations",          "simulation", {.i = 1000},      PARAM_INT,    false, true, "max-gen",        NULL},
    {"max-genome-length",        "genome",     {.i = 24},        PARAM_INT,    false, true, "max-genome-len", NULL},
    {"max-neurons",              "genome",     {.i = 5},         PARAM_INT,    false, true, "max-neurons",    NULL},
    {"point-mutation-rate",      "genome",     {.f = 0.001},     PARAM_FLOAT,  false, true, "point-mut-rate", NULL},
    {"long-probe-dist",          "sensors",    {.i = 16},        PARAM_INT,    false, true, NULL,             NULL},
    {"population-sensor-radius", "sensors",    {.i = 2},         PARAM_INT,    false, true, NULL,             NULL},
    {"kind",                     "challenge",  {.s = "x_band"},  PARAM_STRING, false, true, NULL,             NULL},
    {"x-min",                    "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"x-max",                    "challenge",  {.f = 1.0},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"mirror",                   "challenge",  {.b = false},     PARAM_BOOL,   false, true, NULL,             NULL},
    {"x",                        "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"y",                        "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"radius",                   "challenge",  {.f = 0.333},     PARAM_FLOAT,  false, true, NULL,             NULL},
    {"weighted",                 "challenge",  {.b = true},      PARAM_BOOL,   false, true, NULL,             NULL},
    {"min-n",                    "challenge",  {.f = 5.0},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"max-n",                    "challenge",  {.f = 8.0},       PARAM_FLOAT,  false, true, NULL,             NULL},
    {"exclude-border",           "challenge",  {.b = false},     PARAM_BOOL,   false, true, NULL,             NULL},
    {"outer-r",                  "challenge",  {.f = 0.25},      PARAM_FLOAT,  false, true, NULL,             NULL},
    {"inner-r",                  "challenge",  {.f = 0.012},     PARAM_FLOAT,  false, true, NULL,             NULL},
    {"enable-kill",              "actions",    {.b = false},     PARAM_BOOL,   false, true, "enable-kill",    NULL},
};
// clang-format on
#define SIM_PARAMS_COUNT (sizeof(sim_params) / sizeof(sim_params[0]))

/* ── statistics printing ────────────────────────────────────────────────── */

static void print_stats_header(void) {
    (void)printf("%5s %7s %7s %6s %8s %8s %7s %9s\n", "gen", "surv", "kills", "surv%", "glen.m",
                 "glen.s", "pdiv%", "score.m");
}

static void print_stats(const biosim_gen_stats_t *s) {
    (void)printf("%5u %7u %7u %5.1f%% %8.2f %8.2f %6.1f%% %9.4f\n", s->gen, s->survivors, s->kills,
                 (double)(s->survival_rate * 100.0F), (double)s->genome_len_mean,
                 (double)s->genome_len_std, (double)(s->phenotype_div * 100.0F),
                 (double)s->score_mean);
}

/* ── entry point ────────────────────────────────────────────────────────── */

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

    biosim_sim_t sim;
    memset(&sim, 0, sizeof(sim));
    sim.population = (uint32_t)biosim_params_get_int(&p, "population");
    sim.size_x = (int16_t)biosim_params_get_int(&p, "grid-size-x");
    sim.size_y = (int16_t)biosim_params_get_int(&p, "grid-size-y");
    sim.max_gen_len = (uint16_t)biosim_params_get_int(&p, "max-genome-length");
    sim.max_neurons = (uint8_t)biosim_params_get_int(&p, "max-neurons");
    sim.long_probe_dist = (uint8_t)biosim_params_get_int(&p, "long-probe-dist");
    sim.steps_per_gen = biosim_params_get_int(&p, "steps-per-gen");
    sim.population_sensor_radius = biosim_params_get_int(&p, "population-sensor-radius");
    sim.challenge = challenge;
    sim.enable_kill = biosim_params_get_bool(&p, "enable-kill");
    sim.mutation_rate = (float)biosim_params_get_float(&p, "point-mutation-rate");
    sim.gen_rng = biosim_rng_seed(0, 1);

    st = biosim_sim_create(&sim, barriers, n_barriers);
    free(barriers);
    if (st != BIOSIM_OK) {
        (void)fprintf(stderr, "biosim-stepper: init failed (status %d)\n", (int)st);
        biosim_params_free(&p);
        return st;
    }

    const int max_gens = biosim_params_get_int(&p, "max-generations");

    print_stats_header();
    for (int g = 0; g < max_gens; g++) {
        for (int s = 0; s < sim.steps_per_gen; s++) {
            for (uint32_t i = 0; i < sim.agents.population; i++) {
                if (!sim.agents.alive[i]) {
                    continue;
                }
                step_agent(&sim, i);
            }

            // Fade the signals
            for (size_t j = 0; j < sim.signal_len; j++) {
                sim.signal[j]--;
            }

            biosim_challenge_step(&sim.challenge, &sim, (int)sim.step, sim.steps_per_gen);

            sim.step++;
        }
        biosim_gen_stats_t stats;
        biosim_sim_advance_gen(&sim, &stats);
        print_stats(&stats);
    }

    biosim_sim_free(&sim);
    biosim_params_free(&p);
    return 0;
}
