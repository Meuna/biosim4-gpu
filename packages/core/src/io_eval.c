#include "biosim/core/io_eval.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

/* ── direction tables: see BIOSIM_BIOSIM_DIR_DX / BIOSIM_BIOSIM_DIR_DY in io_defs.h ──── */

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

static void pop_visitor(biosim_coord_t coord, uint32_t cell, void *sim) {
    (void)coord;
    pop_count_t *pc = (pop_count_t *)sim;
    pc->visited++;
    if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
        pc->occupied++;
    }
}

/* ── compute direction ──────────────────────────────────────────────────── */

uint8_t biosim_get_dir(int dx, int dy) {
    uint8_t d;
    for (d = 0; d < 8U; d++) {
        if ((int)BIOSIM_DIR_DX[d] == dx && (int)BIOSIM_DIR_DY[d] == dy) {
            break;
        }
    }
    return d;
}

/* ── sensor evaluation ──────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
float biosim_sensor_eval(biosim_sensor_t sensor, uint32_t idx, const biosim_sim_t *sim) {
    assert(sim != NULL);

    const biosim_agents_t *agents = &sim->agents;
    const biosim_grid_t *grid = &sim->grid;
    const int32_t sx = grid->size_x;
    const int32_t sy = grid->size_y;
    const int32_t x = agents->loc_x[idx];
    const int32_t y = agents->loc_y[idx];

    switch (sensor) {
    case BIOSIM_SENSOR_LOC_X:
        return (float)x / (float)(sx - 1);

    case BIOSIM_SENSOR_LOC_Y:
        return (float)y / (float)(sy - 1);

    case BIOSIM_SENSOR_BOUNDARY_DIST_X: {
        int32_t tmp = sx - x - 1;
        int32_t d = x < tmp ? x : tmp;
        return 2.0F * (float)d / (float)sx;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST_Y: {
        int32_t tmp = sy - y - 1;
        int32_t d = y < tmp ? y : tmp;
        return 2.0F * (float)d / (float)sy;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST: {
        int32_t xtmp = sx - x - 1;
        int32_t ytmp = sy - y - 1;
        int32_t ddx = x < xtmp ? x : xtmp;
        int32_t ddy = y < ytmp ? y : ytmp;
        float fnx = 2.0F * (float)ddx / (float)sx;
        float fny = 2.0F * (float)ddy / (float)sy;
        return fnx < fny ? fnx : fny;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_X: {
        uint8_t dir = agents->last_move_dir[idx];
        return ((float)BIOSIM_DIR_DX[dir & 7U] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_Y: {
        uint8_t dir = agents->last_move_dir[idx];
        return ((float)BIOSIM_DIR_DY[dir & 7U] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_OSC1: {
        uint16_t period = agents->osc_period[idx];
        if (period == 0U) {
            period = 1U;
        }
        float phase = (float)(sim->step % (uint32_t)period) / (float)period;
        return (1.0F - cosf(phase * 6.28318530F)) * 0.5F;
    }

    case BIOSIM_SENSOR_AGE: {
        return (float)sim->step / (float)sim->steps_per_gen;
    }

    case BIOSIM_SENSOR_RANDOM:
        return rng_float(&agents->rng_state[idx]);

    case BIOSIM_SENSOR_POPULATION: {
        biosim_coord_t center = {x, y};
        int32_t r = sim->population_sensor_radius;
        pop_count_t pc = {0U, 0U};
        biosim_grid_visit_neighborhood(grid, center, r, pop_visitor, &pc);
        if (pc.visited == 0U) {
            return 0.0F;
        }
        return (float)pc.occupied / (float)pc.visited;
    }

    case BIOSIM_SENSOR_POPULATION_FWD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        int32_t r = sim->population_sensor_radius;
        uint32_t visited = 0U;
        uint32_t occupied = 0U;
        for (int32_t dy = -r; dy <= r; dy++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                if (dx * fwd_x + dy * fwd_y <= 0) {
                    continue;
                }
                biosim_coord_t c = {x + dx, y + dy};
                if (!biosim_grid_in_bounds(grid, c)) {
                    continue;
                }
                visited++;
                uint32_t cell = biosim_grid_at(grid, c);
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    occupied++;
                }
            }
        }
        if (visited == 0U) {
            return 0.0F;
        }
        return (float)occupied / (float)visited;
    }

    case BIOSIM_SENSOR_POPULATION_LR: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        int32_t r = sim->population_sensor_radius;
        uint32_t l_occ = 0U;
        uint32_t r_occ = 0U;
        for (int32_t dy = -r; dy <= r; dy++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int32_t lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                biosim_coord_t c = {x + dx, y + dy};
                if (!biosim_grid_in_bounds(grid, c)) {
                    continue;
                }
                uint32_t cell = biosim_grid_at(grid, c);
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    if (lateral > 0) {
                        l_occ++;
                    } else {
                        r_occ++;
                    }
                }
            }
        }
        if (l_occ == 0U && r_occ == 0U) {
            return 0.0F;
        }
        return ((float)l_occ - (float)r_occ) / (float)(l_occ + r_occ);
    }

    case BIOSIM_SENSOR_BARRIER_FWD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        int32_t r = sim->population_sensor_radius;
        uint32_t visited = 0U;
        uint32_t barrier = 0U;
        for (int32_t dy = -r; dy <= r; dy++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                if (dx * fwd_x + dy * fwd_y <= 0) {
                    continue;
                }
                biosim_coord_t c = {x + dx, y + dy};
                if (!biosim_grid_in_bounds(grid, c)) {
                    continue;
                }
                visited++;
                if (biosim_grid_at(grid, c) == BIOSIM_GRID_BARRIER) {
                    barrier++;
                }
            }
        }
        if (visited == 0U) {
            return 0.0F;
        }
        return (float)barrier / (float)visited;
    }

    case BIOSIM_SENSOR_BARRIER_LR: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        int32_t r = sim->population_sensor_radius;
        uint32_t l_bar = 0U;
        uint32_t r_bar = 0U;
        for (int32_t dy = -r; dy <= r; dy++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int32_t lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                biosim_coord_t c = {x + dx, y + dy};
                if (!biosim_grid_in_bounds(grid, c)) {
                    continue;
                }
                if (biosim_grid_at(grid, c) == BIOSIM_GRID_BARRIER) {
                    if (lateral > 0) {
                        l_bar++;
                    } else {
                        r_bar++;
                    }
                }
            }
        }
        if (l_bar == 0U && r_bar == 0U) {
            return 0.0F;
        }
        return ((float)l_bar - (float)r_bar) / (float)(l_bar + r_bar);
    }

    case BIOSIM_SENSOR_LONGPROBE_POP_FWD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        uint8_t dist = agents->long_probe_dist[idx];
        if (dist == 0U) {
            return 0.0F;
        }
        for (uint8_t i = 1U; i <= dist; i++) {
            biosim_coord_t c = {x + (int32_t)i * fwd_x, y + (int32_t)i * fwd_y};
            if (!biosim_grid_in_bounds(grid, c)) {
                break;
            }
            uint32_t cell = biosim_grid_at(grid, c);
            if (cell == BIOSIM_GRID_BARRIER) {
                break;
            }
            if (cell != BIOSIM_GRID_EMPTY) {
                return (float)i / (float)dist;
            }
        }
        return 0.0F;
    }

    case BIOSIM_SENSOR_LONGPROBE_BAR_FWD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        uint8_t dist = agents->long_probe_dist[idx];
        if (dist == 0U) {
            return 0.0F;
        }
        for (uint8_t i = 1U; i <= dist; i++) {
            biosim_coord_t c = {x + (int32_t)i * fwd_x, y + (int32_t)i * fwd_y};
            if (!biosim_grid_in_bounds(grid, c)) {
                break;
            }
            if (biosim_grid_at(grid, c) == BIOSIM_GRID_BARRIER) {
                return (float)i / (float)dist;
            }
        }
        return 0.0F;
    }

    case BIOSIM_SENSOR_SIGNAL0: {
        assert(sim->signal != NULL);
        uint32_t val = sim->signal[(size_t)y * (size_t)sx + (size_t)x];
        if (val > 255U) {
            val = 255U;
        }
        return (float)val / 255.0F;
    }

    case BIOSIM_SENSOR_SIGNAL0_FWD: {
        assert(sim->signal != NULL);
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        uint8_t dist = agents->long_probe_dist[idx];
        if (dist == 0U) {
            return 0.0F;
        }
        uint32_t total = 0U;
        uint32_t visited = 0U;
        for (uint8_t i = 1U; i <= dist; i++) {
            biosim_coord_t c = {x + (int32_t)i * fwd_x, y + (int32_t)i * fwd_y};
            if (!biosim_grid_in_bounds(grid, c)) {
                break;
            }
            visited++;
            uint32_t val = sim->signal[(size_t)c.y * (size_t)sx + (size_t)c.x];
            if (val > 255U) {
                val = 255U;
            }
            total += val;
        }
        if (visited == 0U) {
            return 0.0F;
        }
        return (float)total / (255.0F * (float)visited);
    }

    case BIOSIM_SENSOR_SIGNAL0_LR: {
        assert(sim->signal != NULL);
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        int32_t fwd_x = (int32_t)BIOSIM_DIR_DX[dir];
        int32_t fwd_y = (int32_t)BIOSIM_DIR_DY[dir];
        int32_t r = sim->population_sensor_radius;
        uint32_t l_sum = 0U;
        uint32_t r_sum = 0U;
        for (int32_t dy = -r; dy <= r; dy++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int32_t lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                biosim_coord_t c = {x + dx, y + dy};
                if (!biosim_grid_in_bounds(grid, c)) {
                    continue;
                }
                uint32_t val = sim->signal[(size_t)c.y * (size_t)sx + (size_t)c.x];
                if (val > 255U) {
                    val = 255U;
                }
                if (lateral > 0) {
                    l_sum += val;
                } else {
                    r_sum += val;
                }
            }
        }
        if (l_sum == 0U && r_sum == 0U) {
            return 0.0F;
        }
        return ((float)l_sum - (float)r_sum) / ((float)l_sum + (float)r_sum);
    }

    case BIOSIM_SENSOR_GENETIC_SIM_FWD: {
        return 0.5F;

        /* Remove implementation pending redesign of the
           genetic similarity sensor and its implementation on the GPU */

        // uint8_t dir = agents->last_move_dir[idx] & 7U;
        // biosim_coord_t fwd = {x + BIOSIM_DIR_DX[dir], y + BIOSIM_DIR_DY[dir]};
        // if (!biosim_grid_in_bounds(grid, fwd)) {
        //     return 0.0F;
        // }
        // uint32_t cell = biosim_grid_at(grid, fwd);
        // if (cell == BIOSIM_GRID_EMPTY || cell == BIOSIM_GRID_BARRIER) {
        //     return 0.0F;
        // }
        // uint32_t nbr = (uint32_t)(cell - 1U);
        // uint64_t xored = agents->genome_fingerprint[idx] ^ agents->genome_fingerprint[nbr];
        // return 1.0F - (float)popcount64(xored) / 64.0F;
    }

    default:
        assert(0 && "invalid biosim_sensor_t value");
        return 0.0F;
    }
}

/* ── action application ─────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void biosim_action_apply(biosim_action_t action, float val, uint32_t idx, biosim_sim_t *sim) {
    assert(sim != NULL);

    biosim_agents_t *agents = &sim->agents;
    const float resp = agents->responsiveness[idx];

    switch (action) {

        /* ── group A: self-field writers ──────────────────────────────────── */

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

        /* ── group B: movement accumulators ──────────────────────────────── */

    case BIOSIM_ACTION_MOVE_X:
        agents->dx_sum[idx] += resp * val;
        break;

    case BIOSIM_ACTION_MOVE_Y:
        agents->dy_sum[idx] += resp * val;
        break;

    case BIOSIM_ACTION_MOVE_FORWARD: {
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        agents->dx_sum[idx] += resp * val * (float)BIOSIM_DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)BIOSIM_DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_REVERSE: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 4U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)BIOSIM_DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)BIOSIM_DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_LEFT: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 2U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)BIOSIM_DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)BIOSIM_DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_RIGHT: {
        uint8_t dir = (uint8_t)((agents->last_move_dir[idx] + 6U) & 7U);
        agents->dx_sum[idx] += resp * val * (float)BIOSIM_DIR_DX[dir];
        agents->dy_sum[idx] += resp * val * (float)BIOSIM_DIR_DY[dir];
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
        agents->dx_sum[idx] +=
            resp * ((float)BIOSIM_DIR_DX[rdir] * rw + (float)BIOSIM_DIR_DX[ldir] * lw);
        agents->dy_sum[idx] +=
            resp * ((float)BIOSIM_DIR_DY[rdir] * rw + (float)BIOSIM_DIR_DY[ldir] * lw);
        break;
    }

    case BIOSIM_ACTION_MOVE_RANDOM: {
        uint8_t dir = (uint8_t)(biosim_rng_next(&agents->rng_state[idx]) % 8U);
        agents->dx_sum[idx] += resp * (float)BIOSIM_DIR_DX[dir];
        agents->dy_sum[idx] += resp * (float)BIOSIM_DIR_DY[dir];
        break;
    }

    case BIOSIM_ACTION_MOVE_EAST:
        agents->dx_sum[idx] += resp * (float)BIOSIM_DIR_DX[0];
        agents->dy_sum[idx] += resp * (float)BIOSIM_DIR_DY[0];
        break;

    case BIOSIM_ACTION_MOVE_WEST:
        agents->dx_sum[idx] += resp * (float)BIOSIM_DIR_DX[4];
        agents->dy_sum[idx] += resp * (float)BIOSIM_DIR_DY[4];
        break;

    case BIOSIM_ACTION_MOVE_NORTH:
        agents->dx_sum[idx] += resp * (float)BIOSIM_DIR_DX[2];
        agents->dy_sum[idx] += resp * (float)BIOSIM_DIR_DY[2];
        break;

    case BIOSIM_ACTION_MOVE_SOUTH:
        agents->dx_sum[idx] += resp * (float)BIOSIM_DIR_DX[6];
        agents->dy_sum[idx] += resp * (float)BIOSIM_DIR_DY[6];
        break;

        /* ── group C: signal emission ─────────────────────────────────────── */

    case BIOSIM_ACTION_EMIT_SIGNAL0: {
        assert(sim->signal != NULL);
        if (val < 0.5F) {
            break;
        }
        const int32_t ex = agents->loc_x[idx];
        const int32_t ey = agents->loc_y[idx];
        const int32_t gsz_x = sim->grid.size_x;
        const int32_t gsz_y = sim->grid.size_y;
        /* center cell: +2 */
        size_t ci = (size_t)ey * (size_t)gsz_x + (size_t)ex;
        uint32_t cv = sim->signal[ci] + 2U;
        sim->signal[ci] = cv > 255U ? 255U : cv;
        /* neighbours within radius 1.5: dx²+dy² ≤ 2 (all 8 immediate neighbours) */
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                int32_t nx = ex + dx;
                int32_t ny = ey + dy;
                if (nx < 0 || nx >= gsz_x || ny < 0 || ny >= gsz_y) {
                    continue;
                }
                size_t ni = (size_t)ny * (size_t)gsz_x + (size_t)nx;
                uint32_t nv = sim->signal[ni] + 1U;
                sim->signal[ni] = nv > 255U ? 255U : nv;
            }
        }
        break;
    }

        /* ── group D: kill ────────────────────────────────────────────────── */

    case BIOSIM_ACTION_KILL_FORWARD: {
        if (!sim->enable_kill || val < 0.5F) {
            break;
        }
        uint8_t dir = agents->last_move_dir[idx] & 7U;
        biosim_coord_t fwd = {
            agents->loc_x[idx] + BIOSIM_DIR_DX[dir], agents->loc_y[idx] + BIOSIM_DIR_DY[dir]
        };
        if (!biosim_grid_in_bounds(&sim->grid, fwd)) {
            break;
        }
        uint32_t cell = biosim_grid_at(&sim->grid, fwd);
        if (cell == BIOSIM_GRID_EMPTY || cell == BIOSIM_GRID_BARRIER) {
            break;
        }
        agents->kill_marker[(uint32_t)(cell - 1U)] = 1U;
        break;
    }

    default:
        assert(0 && "invalid biosim_action_t value");
        break;
    }
}

/* ── movement proposal ──────────────────────────────────────────────────── */

void biosim_action_propose_move(uint32_t idx, biosim_sim_t *sim) {
    assert(sim != NULL);

    biosim_agents_t *agents = &sim->agents;
    const int32_t size_x = sim->grid.size_x;
    const int32_t size_y = sim->grid.size_y;

    /* Squash accumulated sums to a probability magnitude in (-1, 1). */
    float lx = tanhf(agents->dx_sum[idx] * 0.5F);
    float ly = tanhf(agents->dy_sum[idx] * 0.5F);

    /* Compare magnitude against a uniform random; take a step if it wins. */
    int32_t step_x;
    if (fabsf(lx) > rng_float(&agents->rng_state[idx])) {
        step_x = lx >= 0.0F ? 1 : -1;
    } else {
        step_x = 0;
    }
    int32_t step_y;
    if (fabsf(ly) > rng_float(&agents->rng_state[idx])) {
        step_y = ly >= 0.0F ? 1 : -1;
    } else {
        step_y = 0;
    }

    int32_t nx = agents->loc_x[idx] + step_x;
    int32_t ny = agents->loc_y[idx] + step_y;

    if (nx < 0) {
        nx = 0;
    }
    if (nx >= size_x) {
        nx = size_x - 1;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (ny >= size_y) {
        ny = size_y - 1;
    }

    agents->desired_x[idx] = nx;
    agents->desired_y[idx] = ny;
}
