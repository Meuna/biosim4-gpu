#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"
#include "biosim/core/types.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

/* ── direction tables (0=E, CCW: E NE N NW W SW S SE) ──────────────────── */

static const int8_t DIR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t DIR_DY[8] = {0, -1, -1, -1, 0, 1, 1, 1};

/* ── internal helpers ───────────────────────────────────────────────────── */

static float rng_float(uint64_t *rng) {
    return (float)(biosim_rng_next(rng) >> 40) * (1.0F / 16777216.0F);
}

static uint32_t popcount64(uint64_t x) {
    x = x - ((x >> 1U) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2U) & 0x3333333333333333ULL);
    x = (x + (x >> 4U)) & 0x0F0F0F0F0F0F0F0FULL;
    return (uint32_t)((x * 0x0101010101010101ULL) >> 56U);
}

/* Visitor state for the POPULATION sensor. */
typedef struct {
    uint32_t occupied;
    uint32_t visited;
} pop_count_t;

static void pop_visitor(biosim_coord_t coord, uint16_t cell, void *ctx) {
    (void)coord;
    pop_count_t *pc = (pop_count_t *)ctx;
    pc->visited++;
    if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
        pc->occupied++;
    }
}

/* ── compute direction ──────────────────────────────────────────────────── */

uint8_t biosim_get_dir(int dx, int dy) {
    uint8_t d;
    for (d = 0; d < 8U; d++) {
        if ((int)DIR_DX[d] == dx && (int)DIR_DY[d] == dy) {
            break;
        }
    }
    return d;
}

/* ── sensor evaluation ──────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
float biosim_sensor_eval(biosim_sensor_t sensor, uint32_t idx, const biosim_context_t *ctx,
                         uint32_t sim_step) {
    assert(ctx != NULL);

    const biosim_agents_t *agents = &ctx->agents;
    const biosim_grid_t *grid = &ctx->grid;
    const int16_t sx = grid->size_x;
    const int16_t sy = grid->size_y;
    const int16_t x = agents->loc_x[idx];
    const int16_t y = agents->loc_y[idx];

    switch (sensor) {
    case BIOSIM_SENSOR_LOC_X:
        return (float)x / (float)(sx - 1);

    case BIOSIM_SENSOR_LOC_Y:
        return (float)y / (float)(sy - 1);

    case BIOSIM_SENSOR_BOUNDARY_DIST_X: {
        int16_t tmp = (int16_t)(sx - x - 1);
        int16_t d = (int16_t)(x < tmp ? x : tmp);
        return 2.0F * (float)d / (float)sx;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST_Y: {
        int16_t tmp = (int16_t)(sy - y - 1);
        int16_t d = (int16_t)(y < tmp ? y : tmp);
        return 2.0F * (float)d / (float)sy;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST: {
        int16_t xtmp = (int16_t)(sx - x - 1);
        int16_t ytmp = (int16_t)(sy - y - 1);
        int16_t ddx = (int16_t)(x < xtmp ? x : xtmp);
        int16_t ddy = (int16_t)(y < ytmp ? y : ytmp);
        float fnx = 2.0F * (float)ddx / (float)sx;
        float fny = 2.0F * (float)ddy / (float)sy;
        return fnx < fny ? fnx : fny;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_X: {
        uint8_t dir = agents->last_move_dir[idx];
        return ((float)DIR_DX[dir & 7U] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_Y: {
        uint8_t dir = agents->last_move_dir[idx];
        return ((float)DIR_DY[dir & 7U] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_OSC1: {
        uint16_t period = agents->osc_period[idx];
        if (period == 0U) {
            period = 1U;
        }
        float phase = (float)(sim_step % (uint32_t)period) / (float)period;
        return (1.0F - cosf(phase * 6.28318530F)) * 0.5F;
    }

    case BIOSIM_SENSOR_AGE: {
        int steps = ctx->steps_per_gen;
        if (steps <= 0) {
            steps = 1;
        }
        return (float)sim_step / (float)steps;
    }

    case BIOSIM_SENSOR_RANDOM:
        return rng_float(&agents->rng_state[idx]);

    case BIOSIM_SENSOR_POPULATION: {
        int r = ctx->population_sensor_radius;
        if (r <= 0) {
            r = 1;
        }
        biosim_coord_t center = {x, y};
        pop_count_t pc = {0U, 0U};
        biosim_grid_visit_neighborhood(grid, center, (int16_t)r, pop_visitor, &pc);
        if (pc.visited == 0U) {
            return 0.0F;
        }
        return (float)pc.occupied / (float)pc.visited;
    }

    case BIOSIM_SENSOR_POPULATION_FWD:
    case BIOSIM_SENSOR_POPULATION_LR:
    case BIOSIM_SENSOR_BARRIER_FWD:
    case BIOSIM_SENSOR_BARRIER_LR:
    case BIOSIM_SENSOR_LONGPROBE_POP_FWD:
    case BIOSIM_SENSOR_LONGPROBE_BAR_FWD:
    case BIOSIM_SENSOR_SIGNAL0_FWD:
    case BIOSIM_SENSOR_SIGNAL0_LR:
        return 0.5F;

    case BIOSIM_SENSOR_SIGNAL0: {
        assert(ctx->signal != NULL);
        uint32_t val = ctx->signal[(size_t)y * (size_t)sx + (size_t)x];
        if (val > 255U) {
            val = 255U;
        }
        return (float)val / 255.0F;
    }

    case BIOSIM_SENSOR_GENETIC_SIM_FWD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        biosim_coord_t fwd = {(int16_t)(x + DIR_DX[dir]), (int16_t)(y + DIR_DY[dir])};
        if (!biosim_grid_in_bounds(grid, fwd)) {
            return 0.0F;
        }
        uint16_t cell = biosim_grid_at(grid, fwd);
        if (cell == BIOSIM_GRID_EMPTY || cell == BIOSIM_GRID_BARRIER) {
            return 0.0F;
        }
        uint32_t nbr = (uint32_t)(cell - 1U);
        uint64_t xored = agents->genome_fingerprint[idx] ^ agents->genome_fingerprint[nbr];
        return 1.0F - (float)popcount64(xored) / 64.0F;
    }

    default:
        assert(0 && "invalid biosim_sensor_t value");
        return 0.0F;
    }
}

/* ── action application ─────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void biosim_action_apply(biosim_action_t action, float val, uint32_t idx, biosim_context_t *ctx) {
    assert(ctx != NULL);

    biosim_agents_t *agents = &ctx->agents;
    const float resp = agents->responsiveness[idx];

    switch (action) {

        /* ── Group A: self-field writers ──────────────────────────────────── */

    case BIOSIM_ACTION_SET_RESPONSIVENESS:
        agents->responsiveness[idx] = tanhf(val) * 0.5F + 0.5F;
        break;

    case BIOSIM_ACTION_SET_OSCILLATOR_PERIOD: {
        float t = tanhf(val);
        float f = 2.0F * powf(1024.0F, (t + 1.0F) * 0.5F);
        if (f < 2.0F) {
            f = 2.0F;
        }
        if (f > 2048.0F) {
            f = 2048.0F;
        }
        agents->osc_period[idx] = (uint16_t)f;
        break;
    }

    case BIOSIM_ACTION_SET_LONGPROBE_DIST: {
        float t = tanhf(val);
        float f = 1.0F + 31.0F * (t + 1.0F) * 0.5F;
        if (f < 1.0F) {
            f = 1.0F;
        }
        if (f > 32.0F) {
            f = 32.0F;
        }
        agents->long_probe_dist[idx] = (uint8_t)f;
        break;
    }

        /* ── Group B: movement accumulators ──────────────────────────────── */

    case BIOSIM_ACTION_MOVE_X:
        agents->dx_sum[idx] += resp * val;
        break;

    case BIOSIM_ACTION_MOVE_Y:
        agents->dy_sum[idx] += resp * val;
        break;

    case BIOSIM_ACTION_MOVE_FORWARD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        agents->dx_sum[idx] += resp * val * (float)DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_REVERSE: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 4U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_LEFT: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 2U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_RIGHT: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 6U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_RL: {
        /* val determines the left/right bias: positive → more right, negative → more left.
         * tanhf maps to (-1,1) to bound the bias regardless of accumulated action magnitude. */
        uint8_t ldir = (uint8_t)((agents->last_move_dir[idx] + 2U) & 7U);
        uint8_t rdir = (uint8_t)((agents->last_move_dir[idx] + 6U) & 7U);
        float t = tanhf(val);
        float rw = (t + 1.0F) * 0.5F;
        float lw = 1.0F - rw;
        agents->dx_sum[idx] += resp * ((float)DIR_DX[rdir] * rw + (float)DIR_DX[ldir] * lw);
        agents->dy_sum[idx] += resp * ((float)DIR_DY[rdir] * rw + (float)DIR_DY[ldir] * lw);
        break;
    }

    case BIOSIM_ACTION_MOVE_RANDOM: {
        uint8_t dir = (uint8_t)(biosim_rng_next(&agents->rng_state[idx]) % 8U);
        agents->dx_sum[idx] += resp * (float)DIR_DX[dir];
        agents->dy_sum[idx] += resp * (float)DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_EAST:
        agents->dx_sum[idx] += resp * (float)DIR_DX[0];
        agents->dy_sum[idx] += resp * (float)DIR_DY[0];
        break;

    case BIOSIM_ACTION_MOVE_WEST:
        agents->dx_sum[idx] += resp * (float)DIR_DX[4];
        agents->dy_sum[idx] += resp * (float)DIR_DY[4];
        break;

    case BIOSIM_ACTION_MOVE_NORTH:
        agents->dx_sum[idx] += resp * (float)DIR_DX[2];
        agents->dy_sum[idx] += resp * (float)DIR_DY[2];
        break;

    case BIOSIM_ACTION_MOVE_SOUTH:
        agents->dx_sum[idx] += resp * (float)DIR_DX[6];
        agents->dy_sum[idx] += resp * (float)DIR_DY[6];
        break;

        /* ── Group C: signal emission ─────────────────────────────────────── */

    case BIOSIM_ACTION_EMIT_SIGNAL0: {
        assert(ctx->signal != NULL);
        if (val < 0.5F) {
            break;
        }
        const int16_t ex = agents->loc_x[idx];
        const int16_t ey = agents->loc_y[idx];
        const int16_t gsz_x = ctx->grid.size_x;
        const int16_t gsz_y = ctx->grid.size_y;
        /* center cell: +2 */
        size_t ci = (size_t)ey * (size_t)gsz_x + (size_t)ex;
        uint32_t cv = ctx->signal[ci] + 2U;
        ctx->signal[ci] = cv > 255U ? 255U : cv;
        /* neighbours within radius 1.5: dx²+dy² ≤ 2 (all 8 immediate neighbours) */
        for (int16_t dy = -1; dy <= 1; dy++) {
            for (int16_t dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                int16_t nx = (int16_t)(ex + dx);
                int16_t ny = (int16_t)(ey + dy);
                if (nx < 0 || nx >= gsz_x || ny < 0 || ny >= gsz_y) {
                    continue;
                }
                size_t ni = (size_t)ny * (size_t)gsz_x + (size_t)nx;
                uint32_t nv = ctx->signal[ni] + 1U;
                ctx->signal[ni] = nv > 255U ? 255U : nv;
            }
        }
        break;
    }

        /* ── Group D: kill ────────────────────────────────────────────────── */

    case BIOSIM_ACTION_KILL_FORWARD: {
        if (val < 0.5F) {
            break;
        }
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        biosim_coord_t fwd = {(int16_t)(agents->loc_x[idx] + DIR_DX[dir]),
                              (int16_t)(agents->loc_y[idx] + DIR_DY[dir])};
        if (!biosim_grid_in_bounds(&ctx->grid, fwd)) {
            break;
        }
        uint16_t cell = biosim_grid_at(&ctx->grid, fwd);
        if (cell == BIOSIM_GRID_EMPTY || cell == BIOSIM_GRID_BARRIER) {
            break;
        }
        agents->alive[(uint32_t)(cell - 1U)] = 0U;
        break;
    }

    default:
        assert(0 && "invalid biosim_action_t value");
        break;
    }
}

/* ── movement finalisation ──────────────────────────────────────────────── */

void biosim_action_finalize_movement(uint32_t idx, biosim_context_t *ctx) {
    assert(ctx != NULL);

    biosim_agents_t *agents = &ctx->agents;
    const int16_t size_x = ctx->grid.size_x;
    const int16_t size_y = ctx->grid.size_y;

    /* Squash accumulated sums to a probability magnitude in (-1, 1). */
    float lx = tanhf(agents->dx_sum[idx] * 0.5F);
    float ly = tanhf(agents->dy_sum[idx] * 0.5F);

    /* Compare magnitude against a uniform random; take a step if it wins. */
    int16_t step_x;
    if (fabsf(lx) > rng_float(&agents->rng_state[idx])) {
        step_x = (int16_t)(lx >= 0.0F ? 1 : -1);
    } else {
        step_x = (int16_t)0;
    }
    int16_t step_y;
    if (fabsf(ly) > rng_float(&agents->rng_state[idx])) {
        step_y = (int16_t)(ly >= 0.0F ? 1 : -1);
    } else {
        step_y = (int16_t)0;
    }

    int16_t nx = (int16_t)(agents->loc_x[idx] + step_x);
    int16_t ny = (int16_t)(agents->loc_y[idx] + step_y);

    if (nx < 0) {
        nx = 0;
    }
    if (nx >= size_x) {
        nx = (int16_t)(size_x - 1);
    }
    if (ny < 0) {
        ny = 0;
    }
    if (ny >= size_y) {
        ny = (int16_t)(size_y - 1);
    }

    agents->desired_x[idx] = nx;
    agents->desired_y[idx] = ny;
}
