#include "biosim/stepper/step.h"

#include "biosim/core/agents.h"
#include "biosim/core/context.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"
#include "biosim/core/types.h"
#include "biosim/params/params.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── direction table ────────────────────────────────────────────────────── */

/* 0=E, counter-clockwise: E NE N NW W SW S SE.
 * Duplicated from io_catalogue.c — not exported by core. */
static const int8_t S_DIR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t S_DIR_DY[8] = {0, -1, -1, -1, 0, 1, 1, 1};

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_stepper_create(biosim_stepper_t *out, const biosim_params_t *params) {
    assert(out != NULL);
    assert(params != NULL);

    memset(out, 0, sizeof(*out));
    out->ctx.steps_per_gen = biosim_params_get_int(params, "steps-per-gen");
    out->ctx.population_sensor_radius = biosim_params_get_int(params, "population-sensor-radius");

    const int population = biosim_params_get_int(params, "population");
    const int16_t size_x = (int16_t)biosim_params_get_int(params, "grid-size-x");
    const int16_t size_y = (int16_t)biosim_params_get_int(params, "grid-size-y");
    const uint16_t max_gen_len = (uint16_t)biosim_params_get_int(params, "max-genome-length");
    const uint8_t long_probe_dist = (uint8_t)biosim_params_get_int(params, "long-probe-dist");
    const uint8_t max_neurons = (uint8_t)biosim_params_get_int(params, "max-neurons");
    const uint32_t pop = (uint32_t)population;

    biosim_status_t st;

    st = biosim_grid_create(size_x, size_y, &out->grid);
    if (st != BIOSIM_OK) {
        biosim_stepper_free(out);
        return st;
    }

    st = biosim_agents_create(pop, &out->agents);
    if (st != BIOSIM_OK) {
        biosim_stepper_free(out);
        return st;
    }

    st = biosim_genome_create(pop, max_gen_len, &out->genome);
    if (st != BIOSIM_OK) {
        biosim_stepper_free(out);
        return st;
    }

    /* max_conn = max_gen_len: worst case every gene survives culling */
    st = biosim_nnet_create(pop, max_gen_len, max_neurons, &out->nnet);
    if (st != BIOSIM_OK) {
        biosim_stepper_free(out);
        return st;
    }

    out->signal_len = (size_t)size_x * (size_t)size_y;
    out->signal = (uint32_t *)calloc(out->signal_len, sizeof(uint32_t));
    if (out->signal == NULL) {
        biosim_stepper_free(out);
        return BIOSIM_ERR_NOMEM;
    }

    for (uint32_t i = 0; i < pop; i++) {
        uint64_t rng = biosim_rng_seed(i, 0);

        biosim_genome_init_slot(&out->genome, i, max_gen_len, &rng);
        biosim_nnet_compile_slot(&out->nnet, &out->genome, i, BIOSIM_NUM_SENSORS,
                                 BIOSIM_NUM_ACTIONS);

        biosim_coord_t loc;
        st = biosim_grid_find_empty(&out->grid, &rng, &loc);
        if (st != BIOSIM_OK) {
            biosim_stepper_free(out);
            return st;
        }

        biosim_agents_init_slot(&out->agents, i, loc, long_probe_dist, 0);
        /* Overwrite the seed stored by init_slot with the already-advanced rng,
         * preserving continuity across genome init and grid placement. */
        out->agents.rng_state[i] = rng;

        biosim_grid_set(&out->grid, loc, (uint16_t)(i + 1U));
        out->agents.genome_fingerprint[i] = biosim_nnet_fingerprint(&out->nnet, i);
    }

    out->step = 0;
    return BIOSIM_OK;
}

void biosim_stepper_free(biosim_stepper_t *stepper) {
    if (stepper == NULL) {
        return;
    }
    biosim_nnet_free(&stepper->nnet);
    biosim_genome_free(&stepper->genome);
    biosim_agents_free(&stepper->agents);
    biosim_grid_free(&stepper->grid);
    free(stepper->signal);
    stepper->signal = NULL;
    stepper->signal_len = 0;
    stepper->step = 0;
}

/* ── step ───────────────────────────────────────────────────────────────── */

static void step_agent(biosim_stepper_t *stepper, uint32_t i) {
    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    biosim_sense_ctx_t sense_ctx;
    sense_ctx.idx = i;
    sense_ctx.agents = &stepper->agents;
    sense_ctx.grid = &stepper->grid;
    sense_ctx.signal = stepper->signal;
    sense_ctx.sim_step = stepper->step;

    for (uint32_t s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, &sense_ctx, &stepper->ctx);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(&stepper->nnet, i, sensor_vals, BIOSIM_NUM_SENSORS, action_vals,
                            BIOSIM_NUM_ACTIONS);

    biosim_act_ctx_t act_ctx;
    act_ctx.idx = i;
    act_ctx.agents = &stepper->agents;
    act_ctx.grid = &stepper->grid;
    act_ctx.signal = stepper->signal;
    act_ctx.dx_sum = 0.0F;
    act_ctx.dy_sum = 0.0F;

    for (uint32_t a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        biosim_action_apply((biosim_action_t)a, action_vals[a], &act_ctx);
    }

    /* KILL_FORWARD targets others, not self; still guard in case a prior agent
     * killed this one during action application. */
    if (!stepper->agents.alive[i]) {
        return;
    }

    biosim_action_finalize_movement(&act_ctx);

    const int16_t dx = (int16_t)(stepper->agents.desired_x[i] - stepper->agents.loc_x[i]);
    const int16_t dy = (int16_t)(stepper->agents.desired_y[i] - stepper->agents.loc_y[i]);

    if (dx == 0 && dy == 0) {
        return;
    }

    biosim_coord_t target;
    target.x = stepper->agents.desired_x[i];
    target.y = stepper->agents.desired_y[i];

    /* Dead agents retain their grid cell (v1 artifact); only move into EMPTY cells. */
    if (biosim_grid_at(&stepper->grid, target) != BIOSIM_GRID_EMPTY) {
        return;
    }

    biosim_coord_t old_loc;
    old_loc.x = stepper->agents.loc_x[i];
    old_loc.y = stepper->agents.loc_y[i];

    biosim_grid_set(&stepper->grid, old_loc, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&stepper->grid, target, (uint16_t)(i + 1U));
    stepper->agents.loc_x[i] = target.x;
    stepper->agents.loc_y[i] = target.y;

    for (uint8_t d = 0; d < 8U; d++) {
        if ((int)S_DIR_DX[d] == (int)dx && (int)S_DIR_DY[d] == (int)dy) {
            stepper->agents.last_move_dir[i] = d;
            break;
        }
    }
}

void biosim_stepper_step(biosim_stepper_t *stepper) {
    assert(stepper != NULL);

    const uint32_t pop = stepper->agents.capacity;

    for (uint32_t i = 0; i < pop; i++) {
        if (!stepper->agents.alive[i]) {
            continue;
        }
        step_agent(stepper, i);
    }

    for (size_t j = 0; j < stepper->signal_len; j++) {
        stepper->signal[j] >>= 1;
    }

    stepper->step++;
}
