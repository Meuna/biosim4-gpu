#include "biosim/core/challenges.h"

#include "biosim/core/rng.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* ── shared geometry helpers ────────────────────────────────────────────── */

static bool on_border(int16_t x, int16_t y, int16_t w, int16_t h) {
    return x == 0 || x == (int16_t)(w - 1) || y == 0 || y == (int16_t)(h - 1);
}

static float euclid(float dx, float dy) {
    return sqrtf(dx * dx + dy * dy);
}

/* ── neighbor counting for neighborhood challenges ──────────────────────── */

static void count_occupied_cb(biosim_coord_t coord, uint16_t cell, void *sim) {
    (void)coord;
    if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
        (*(int *)sim)++;
    }
}

/* ── x_band ─────────────────────────────────────────────────────────────── */

static bool in_x_band(int16_t x, int16_t size_x, float x_min, float x_max) {
    int16_t lo = (int16_t)(x_min * (float)size_x);
    int16_t hi = (int16_t)(x_max * (float)size_x);
    return x >= lo && x < hi;
}

static biosim_challenge_result_t eval_x_band(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                             int16_t size_x) {
    biosim_challenge_result_t r = {false, 0.0F};
    float x_min = spec->x_band.x_min;
    float x_max = spec->x_band.x_max;

    r.passed = in_x_band(loc_x, size_x, x_min, x_max);
    if (!r.passed && spec->x_band.mirror) {
        r.passed = in_x_band(loc_x, size_x, 1.0F - x_max, 1.0F - x_min);
    }
    if (r.passed) {
        r.score = 1.0F;
    }
    return r;
}

/* ── disc ────────────────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_disc(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                           int16_t loc_y, int16_t size_x, int16_t size_y) {
    biosim_challenge_result_t r = {false, 0.0F};
    float x = spec->disc.x * (float)size_x;
    float y = spec->disc.y * (float)size_y;
    float radius = spec->disc.radius * (float)size_x;
    float dist = euclid((float)loc_x - x, (float)loc_y - y);
    if (dist <= radius) {
        r.passed = true;
        r.score = spec->disc.weighted ? (radius - dist) / radius : 1.0F;
    }
    return r;
}

/* ── corners ─────────────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_corners(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                              int16_t loc_y, int16_t size_x, int16_t size_y) {
    biosim_challenge_result_t r = {false, 0.0F};
    float radius = spec->corners.radius * (float)size_x;
    float w = (float)(size_x - 1);
    float h = (float)(size_y - 1);
    float lx = (float)loc_x;
    float ly = (float)loc_y;

    float d0 = euclid(lx, ly);
    float d1 = euclid(lx, ly - h);
    float d2 = euclid(lx - w, ly);
    float d3 = euclid(lx - w, ly - h);

    float min_d = d0;
    if (d1 < min_d) {
        min_d = d1;
    }
    if (d2 < min_d) {
        min_d = d2;
    }
    if (d3 < min_d) {
        min_d = d3;
    }

    if (min_d <= radius) {
        r.passed = true;
        r.score = spec->corners.weighted ? (radius - min_d) / radius : 1.0F;
    }
    return r;
}

/* ── neighbor_count ──────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_neighbor_count(const biosim_challenge_spec_t *spec,
                                                     int16_t loc_x, int16_t loc_y, int16_t size_x,
                                                     int16_t size_y, const biosim_grid_t *grid) {
    biosim_challenge_result_t r = {false, 0.0F};
    if (spec->neighbor_count.exclude_border && on_border(loc_x, loc_y, size_x, size_y)) {
        return r;
    }
    int16_t rpx = (int16_t)(spec->neighbor_count.radius * (float)size_x);
    int count = 0;
    biosim_coord_t centre = {loc_x, loc_y};
    biosim_grid_visit_neighborhood(grid, centre, rpx, count_occupied_cb, &count);
    int min_n = (int)spec->neighbor_count.min_n;
    int max_n = (int)spec->neighbor_count.max_n;
    if (count >= min_n && count <= max_n) {
        r.passed = true;
        r.score = 1.0F;
    }
    return r;
}

/* ── center_sparse ───────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_center_sparse(const biosim_challenge_spec_t *spec,
                                                    int16_t loc_x, int16_t loc_y, int16_t size_x,
                                                    int16_t size_y, const biosim_grid_t *grid) {
    biosim_challenge_result_t r = {false, 0.0F};
    float x = spec->center_sparse.x * (float)size_x;
    float y = spec->center_sparse.y * (float)size_y;
    float outer_px = spec->center_sparse.outer_r * (float)size_x;
    int16_t inner_rpx = (int16_t)(spec->center_sparse.inner_r * (float)size_x);

    float dist_out = euclid((float)loc_x - x, (float)loc_y - y);
    if (dist_out > outer_px) {
        return r;
    }

    int count = 0;
    biosim_coord_t centre = {loc_x, loc_y};
    biosim_grid_visit_neighborhood(grid, centre, inner_rpx, count_occupied_cb, &count);

    int min_n = (int)spec->center_sparse.min_n;
    int max_n = (int)spec->center_sparse.max_n;
    if (count >= min_n && count <= max_n) {
        r.passed = true;
        r.score = spec->center_sparse.weighted ? (outer_px - dist_out) / outer_px : 1.0F;
    }
    return r;
}

/* ── against_wall ────────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_against_wall(int16_t loc_x, int16_t loc_y, int16_t size_x,
                                                   int16_t size_y) {
    biosim_challenge_result_t r = {on_border(loc_x, loc_y, size_x, size_y), 0.0F};
    if (r.passed) {
        r.score = 1.0F;
    }
    return r;
}

/* ── migrate_distance ────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_migrate_distance(int16_t loc_x, int16_t loc_y,
                                                       int16_t birth_x, int16_t birth_y,
                                                       int16_t size_x, int16_t size_y) {
    float dx = (float)(loc_x - birth_x);
    float dy = (float)(loc_y - birth_y);
    float dist = euclid(dx, dy);
    float max_d = (float)(size_x > size_y ? size_x : size_y);
    biosim_challenge_result_t r = {true, dist / max_d};
    return r;
}

/* ── pairs ───────────────────────────────────────────────────────────────── */

/* Count 8-connected occupied neighbours of (x,y); record the last one. */
static int count_8_neighbours(const biosim_grid_t *grid, int16_t x, int16_t y, int16_t *out_x,
                              int16_t *out_y) {
    int count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            biosim_coord_t cell = {(int16_t)(x + dx), (int16_t)(y + dy)};
            if (biosim_grid_in_bounds(grid, cell) && biosim_grid_is_occupied(grid, cell)) {
                count++;
                *out_x = cell.x;
                *out_y = cell.y;
            }
        }
    }
    return count;
}

/* Check whether (nb_x,nb_y) has any 8-connected occupied neighbour other than (skip_x,skip_y). */
static bool neighbour_has_extra(const biosim_grid_t *grid, int16_t nb_x, int16_t nb_y,
                                int16_t skip_x, int16_t skip_y) {
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            biosim_coord_t cell = {(int16_t)(nb_x + dx), (int16_t)(nb_y + dy)};
            if (cell.x == skip_x && cell.y == skip_y) {
                continue;
            }
            if (biosim_grid_in_bounds(grid, cell) && biosim_grid_is_occupied(grid, cell)) {
                return true;
            }
        }
    }
    return false;
}

static biosim_challenge_result_t eval_pairs(int16_t loc_x, int16_t loc_y, int16_t size_x,
                                            int16_t size_y, const biosim_grid_t *grid) {
    biosim_challenge_result_t r = {false, 0.0F};
    if (on_border(loc_x, loc_y, size_x, size_y)) {
        return r;
    }
    int16_t nb_x = 0;
    int16_t nb_y = 0;
    int count = count_8_neighbours(grid, loc_x, loc_y, &nb_x, &nb_y);
    if (count != 1) {
        return r;
    }
    if (neighbour_has_extra(grid, nb_x, nb_y, loc_x, loc_y)) {
        return r;
    }
    r.passed = true;
    r.score = 1.0F;
    return r;
}

/* ── location_sequence ───────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_location_sequence(uint32_t challenge_bits) {
    int count = 0;
    uint32_t bits = challenge_bits;
    while (bits) {
        count += (int)(bits & 1U);
        bits >>= 1;
    }
    biosim_challenge_result_t r = {count > 0, (float)count / 32.0F};
    return r;
}

/* ── near_barrier ────────────────────────────────────────────────────────── */

static biosim_challenge_result_t eval_near_barrier(const biosim_challenge_spec_t *spec,
                                                   int16_t loc_x, int16_t loc_y, int16_t size_x,
                                                   const biosim_coord_t *ctrs, uint32_t n_ctrs) {
    biosim_challenge_result_t r = {false, 0.0F};
    if (n_ctrs == 0U) {
        return r;
    }
    float radius = spec->near_barrier.radius * (float)size_x;
    float min_dist = euclid((float)(loc_x - ctrs[0].x), (float)(loc_y - ctrs[0].y));
    for (uint32_t i = 1U; i < n_ctrs; i++) {
        float d = euclid((float)(loc_x - ctrs[i].x), (float)(loc_y - ctrs[i].y));
        if (d < min_dist) {
            min_dist = d;
        }
    }
    if (min_dist <= radius) {
        r.passed = true;
        r.score = 1.0F - (min_dist / radius);
    }
    return r;
}

/* ── public API: eval ────────────────────────────────────────────────────── */

biosim_challenge_result_t biosim_challenge_eval(const biosim_challenge_spec_t *spec,
                                                uint32_t agent_idx, const biosim_sim_t *sim) {
    int16_t loc_x = sim->agents.loc_x[agent_idx];
    int16_t loc_y = sim->agents.loc_y[agent_idx];
    int16_t size_x = sim->grid.size_x;
    int16_t size_y = sim->grid.size_y;

    switch (spec->kind) {
    case BIOSIM_CHALLENGE_X_BAND:
        return eval_x_band(spec, loc_x, size_x);

    case BIOSIM_CHALLENGE_DISC:
        return eval_disc(spec, loc_x, loc_y, size_x, size_y);

    case BIOSIM_CHALLENGE_CORNERS:
        return eval_corners(spec, loc_x, loc_y, size_x, size_y);

    case BIOSIM_CHALLENGE_NEIGHBOR_COUNT:
        return eval_neighbor_count(spec, loc_x, loc_y, size_x, size_y, &sim->grid);

    case BIOSIM_CHALLENGE_CENTER_SPARSE:
        return eval_center_sparse(spec, loc_x, loc_y, size_x, size_y, &sim->grid);

    case BIOSIM_CHALLENGE_AGAINST_WALL:
        return eval_against_wall(loc_x, loc_y, size_x, size_y);

    case BIOSIM_CHALLENGE_MIGRATE_DISTANCE:
        return eval_migrate_distance(loc_x, loc_y, sim->agents.birth_x[agent_idx],
                                     sim->agents.birth_y[agent_idx], size_x, size_y);

    case BIOSIM_CHALLENGE_TOUCH_ANY_WALL: {
        biosim_challenge_result_t r = {sim->agents.challenge_bits[agent_idx] != 0U, 0.0F};
        if (r.passed) {
            r.score = 1.0F;
        }
        return r;
    }

    case BIOSIM_CHALLENGE_RADIOACTIVE_WALLS: {
        biosim_challenge_result_t r = {true, 1.0F};
        return r;
    }

    case BIOSIM_CHALLENGE_PAIRS:
        return eval_pairs(loc_x, loc_y, size_x, size_y, &sim->grid);

    case BIOSIM_CHALLENGE_LOCATION_SEQUENCE:
        return eval_location_sequence(sim->agents.challenge_bits[agent_idx]);

    case BIOSIM_CHALLENGE_NEAR_BARRIER:
        return eval_near_barrier(spec, loc_x, loc_y, size_x, sim->barrier_ctrs,
                                 sim->n_barrier_ctrs);

    case BIOSIM_CHALLENGE_ALTRUISM: {
        biosim_challenge_result_t stub = {false, 0.0F};
        return stub;
    }
    }

    biosim_challenge_result_t unreachable = {false, 0.0F};
    return unreachable;
}

/* ── public API: step hook ───────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void biosim_challenge_step(biosim_sim_t *sim) {
    uint32_t n = sim->agents.population;
    int16_t w = sim->grid.size_x;
    int16_t h = sim->grid.size_y;

    switch (sim->challenge.kind) {

    case BIOSIM_CHALLENGE_TOUCH_ANY_WALL:
        for (uint32_t i = 0; i < n; i++) {
            if (!sim->agents.alive[i]) {
                continue;
            }
            if (on_border(sim->agents.loc_x[i], sim->agents.loc_y[i], w, h)) {
                sim->agents.challenge_bits[i] = 1U;
            }
        }
        break;

    case BIOSIM_CHALLENGE_RADIOACTIVE_WALLS: {
        int16_t radioactive_x = (int16_t)((int)sim->step < sim->steps_per_gen / 2 ? 0 : w - 1);
        for (uint32_t i = 0; i < n; i++) {
            if (!sim->agents.alive[i]) {
                continue;
            }
            int dist = abs((int)sim->agents.loc_x[i] - (int)radioactive_x);
            if (dist == 0) {
                sim->agents.alive[i] = 0;
                continue;
            }
            if (dist < w / 2) {
                uint64_t roll = biosim_rng_next(&sim->agents.rng_state[i]);
                if (roll < UINT64_MAX / (uint64_t)dist) {
                    sim->agents.alive[i] = 0;
                }
            }
        }
        break;
    }

    case BIOSIM_CHALLENGE_LOCATION_SEQUENCE: {
        int rpx = (int)(sim->challenge.location_sequence.radius * (float)w);
        int rpx_sq = rpx * rpx;
        for (uint32_t i = 0; i < n; i++) {
            if (!sim->agents.alive[i]) {
                continue;
            }
            for (uint32_t b = 0U; b < sim->n_barrier_ctrs; b++) {
                uint32_t bit = 1U << b;
                if (sim->agents.challenge_bits[i] & bit) {
                    continue;
                }
                int dx = (int)sim->agents.loc_x[i] - (int)sim->barrier_ctrs[b].x;
                int dy = (int)sim->agents.loc_y[i] - (int)sim->barrier_ctrs[b].y;
                if (dx * dx + dy * dy <= rpx_sq) {
                    sim->agents.challenge_bits[i] |= bit;
                }
                break;
            }
        }
        break;
    }

    default:
        break;
    }
}
