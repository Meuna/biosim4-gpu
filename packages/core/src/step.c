#include "biosim/core/step.h"

#include "biosim/core/challenges.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <string.h>

/* ── per-agent step ─────────────────────────────────────────────────────── */

void step_agent(biosim_context_t *ctx, uint32_t i) {
    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    for (uint32_t s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, i, ctx, ctx->step);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(&ctx->nnet, i, sensor_vals, BIOSIM_NUM_SENSORS, action_vals,
                            BIOSIM_NUM_ACTIONS);

    ctx->agents.dx_sum[i] = 0.0F;
    ctx->agents.dy_sum[i] = 0.0F;

    for (uint32_t a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        biosim_action_apply((biosim_action_t)a, action_vals[a], i, ctx);
    }

    /* KILL_FORWARD targets others, not self; still guard in case a prior agent
     * killed this one during action application. */
    if (!ctx->agents.alive[i]) {
        return;
    }

    biosim_action_finalize_movement(i, ctx);

    const int dx = (int)ctx->agents.desired_x[i] - (int)ctx->agents.loc_x[i];
    const int dy = (int)ctx->agents.desired_y[i] - (int)ctx->agents.loc_y[i];

    if (dx == 0 && dy == 0) {
        return;
    }

    biosim_coord_t target;
    target.x = ctx->agents.desired_x[i];
    target.y = ctx->agents.desired_y[i];

    if (biosim_grid_at(&ctx->grid, target) != BIOSIM_GRID_EMPTY) {
        return;
    }

    biosim_coord_t old_loc;
    old_loc.x = ctx->agents.loc_x[i];
    old_loc.y = ctx->agents.loc_y[i];

    biosim_grid_set(&ctx->grid, old_loc, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&ctx->grid, target, (uint16_t)(i + 1U));
    ctx->agents.loc_x[i] = target.x;
    ctx->agents.loc_y[i] = target.y;
    ctx->agents.last_move_dir[i] = biosim_get_dir(dx, dy);
}
