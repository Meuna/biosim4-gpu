#include "biosim/core/census.h"
#include "biosim/core/log.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"

#include <stdint.h>
#include <stdio.h>

/* ── hardcoded defaults (mirrors sim-stepper parameter table) ────────────── */

#define WASM_POPULATION               3000U
#define WASM_GRID_SIZE_X              128
#define WASM_GRID_SIZE_Y              128
#define WASM_MAX_GENERATIONS          20U
#define WASM_STEPS_PER_GEN            300U
#define WASM_GENOME_MAX_LEN           24U
#define WASM_MAX_NEURONS              5U
#define WASM_LONG_PROBE_DIST          16U
#define WASM_POPULATION_SENSOR_RADIUS 2
#define WASM_MUTATION_RATE            0.001F
#define WASM_RNG_SEED                 1U

/* ── entry point ─────────────────────────────────────────────────────────── */

int main(void) {
    biosim_log_init(&biosim_log_default_ctx);

    biosim_status_t returncode = BIOSIM_OK;
    biosim_sim_t sim = {0};

    sim.max_generations = WASM_MAX_GENERATIONS;
    sim.population = WASM_POPULATION;
    sim.size_x = WASM_GRID_SIZE_X;
    sim.size_y = WASM_GRID_SIZE_Y;
    sim.genome_max_len = WASM_GENOME_MAX_LEN;
    sim.max_neurons = WASM_MAX_NEURONS;
    sim.long_probe_dist = WASM_LONG_PROBE_DIST;
    sim.steps_per_gen = WASM_STEPS_PER_GEN;
    sim.population_sensor_radius = WASM_POPULATION_SENSOR_RADIUS;
    sim.mutation_rate = WASM_MUTATION_RATE;
    sim.sexual_reproduction = false;
    sim.choose_parents_by_fitness = false;
    sim.enable_kill = false;
    sim.challenge = (biosim_challenge_spec_t){
        .kind = BIOSIM_CHALLENGE_X_BAND,
        .x_band = {.x_min = 0.5F, .x_max = 1.0F, .mirror = false},
    };
    sim.gen_rng = biosim_rng_seed(0U, WASM_RNG_SEED);

    returncode = biosim_sim_create(&sim, NULL, 0U);
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
