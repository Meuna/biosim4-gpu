#include "biosim/stepper/step.h"

#include "biosim/core/context.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"
#include "biosim/core/types.h"
#include "biosim/params/params.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_stepper_create(biosim_stepper_t *out, const biosim_params_t *params) {
    assert(out != NULL);
    assert(params != NULL);

    const uint32_t pop = (uint32_t)biosim_params_get_int(params, "population");
    const int16_t size_x = (int16_t)biosim_params_get_int(params, "grid-size-x");
    const int16_t size_y = (int16_t)biosim_params_get_int(params, "grid-size-y");
    const int steps_per_gen = biosim_params_get_int(params, "steps-per-gen");
    const uint16_t max_gen_len = (uint16_t)biosim_params_get_int(params, "max-genome-length");
    const uint8_t long_probe_dist = (uint8_t)biosim_params_get_int(params, "long-probe-dist");
    const uint8_t max_neurons = (uint8_t)biosim_params_get_int(params, "max-neurons");
    const int pop_sensor_radius = biosim_params_get_int(params, "population-sensor-radius");

    memset(out, 0, sizeof(*out));
    biosim_status_t st =
        biosim_context_create(pop, size_x, size_y, steps_per_gen, max_gen_len, max_neurons,
                              long_probe_dist, pop_sensor_radius, &out->base);
    if (st != BIOSIM_OK) {
        biosim_stepper_free(out);
        return st;
    }

    out->step = 0;
    return BIOSIM_OK;
}

void biosim_stepper_free(biosim_stepper_t *stepper) {
    if (stepper == NULL) {
        return;
    }
    biosim_context_free(&stepper->base);
}

/* ── step ───────────────────────────────────────────────────────────────── */

static void step_agent(biosim_stepper_t *stepper, uint32_t i) {
    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    biosim_context_t *ctx = (biosim_context_t *)stepper;

    for (uint32_t s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, i, ctx, stepper->step);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(&stepper->base.nnet, i, sensor_vals, BIOSIM_NUM_SENSORS, action_vals,
                            BIOSIM_NUM_ACTIONS);

    stepper->base.agents.dx_sum[i] = 0.0F;
    stepper->base.agents.dy_sum[i] = 0.0F;

    for (uint32_t a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        biosim_action_apply((biosim_action_t)a, action_vals[a], i, ctx);
    }

    /* KILL_FORWARD targets others, not self; still guard in case a prior agent
     * killed this one during action application. */
    if (!stepper->base.agents.alive[i]) {
        return;
    }

    biosim_action_finalize_movement(i, ctx);

    const int dx = (int)stepper->base.agents.desired_x[i] - (int)stepper->base.agents.loc_x[i];
    const int dy = (int)stepper->base.agents.desired_y[i] - (int)stepper->base.agents.loc_y[i];

    /* Agent don't want to move, early exit */
    if (dx == 0 && dy == 0) {
        return;
    }

    biosim_coord_t target;
    target.x = stepper->base.agents.desired_x[i];
    target.y = stepper->base.agents.desired_y[i];

    /* Agent can't move, early exit */
    if (biosim_grid_at(&stepper->base.grid, target) != BIOSIM_GRID_EMPTY) {
        return;
    }

    /* Agent can move, update agent and grid */
    biosim_coord_t old_loc;
    old_loc.x = stepper->base.agents.loc_x[i];
    old_loc.y = stepper->base.agents.loc_y[i];

    biosim_grid_set(&stepper->base.grid, old_loc, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&stepper->base.grid, target, (uint16_t)(i + 1U));
    stepper->base.agents.loc_x[i] = target.x;
    stepper->base.agents.loc_y[i] = target.y;
    stepper->base.agents.last_move_dir[i] = biosim_get_dir(dx, dy);
}

void biosim_stepper_step(biosim_stepper_t *stepper) {
    assert(stepper != NULL);

    const uint32_t pop = stepper->base.agents.capacity;

    for (uint32_t i = 0; i < pop; i++) {
        if (!stepper->base.agents.alive[i]) {
            continue;
        }
        step_agent(stepper, i);
    }

    for (size_t j = 0; j < stepper->base.signal_len; j++) {
        stepper->base.signal[j] >>= 1;
    }

    stepper->step++;
}
