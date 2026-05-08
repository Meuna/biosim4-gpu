#include "biosim/core/grid.h"
#include "biosim/core/log.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_grid_create(int16_t size_x, int16_t size_y, biosim_grid_t *out) {
    assert(out != NULL);
    assert(size_x > 0 && size_y > 0);

    uint16_t *cells = calloc((size_t)size_x * (size_t)size_y, sizeof(uint16_t));
    if (!cells) {
        return BIOSIM_ERR_NOMEM;
    }

    out->cells = cells;
    out->size_x = size_x;
    out->size_y = size_y;
    return BIOSIM_OK;
}

void biosim_grid_free(biosim_grid_t *grid) {
    if (grid) {
        free(grid->cells);
        grid->cells = NULL;
        grid->size_x = 0;
        grid->size_y = 0;
    }
}

void biosim_grid_zero_fill(biosim_grid_t *grid) {
    assert(grid && grid->cells);
    memset(grid->cells, 0, (size_t)grid->size_x * (size_t)grid->size_y * sizeof(uint16_t));
}

/* ── bounds and access ──────────────────────────────────────────────────── */

bool biosim_grid_in_bounds(const biosim_grid_t *grid, biosim_coord_t coord) {
    return coord.x >= 0 && coord.x < grid->size_x && coord.y >= 0 && coord.y < grid->size_y;
}

uint16_t biosim_grid_at(const biosim_grid_t *grid, biosim_coord_t coord) {
    assert(biosim_grid_in_bounds(grid, coord));
    return grid->cells[(size_t)coord.y * (size_t)grid->size_x + (size_t)coord.x];
}

void biosim_grid_set(biosim_grid_t *grid, biosim_coord_t coord, uint16_t value) {
    assert(biosim_grid_in_bounds(grid, coord));
    grid->cells[(size_t)coord.y * (size_t)grid->size_x + (size_t)coord.x] = value;
}

/* ── predicates ─────────────────────────────────────────────────────────── */

bool biosim_grid_is_empty(const biosim_grid_t *grid, biosim_coord_t coord) {
    return biosim_grid_at(grid, coord) == BIOSIM_GRID_EMPTY;
}

bool biosim_grid_is_barrier(const biosim_grid_t *grid, biosim_coord_t coord) {
    return biosim_grid_at(grid, coord) == BIOSIM_GRID_BARRIER;
}

bool biosim_grid_is_occupied(const biosim_grid_t *grid, biosim_coord_t coord) {
    uint16_t cell = biosim_grid_at(grid, coord);
    return cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER;
}

/* ── search ─────────────────────────────────────────────────────────────── */

biosim_status_t biosim_grid_find_empty(const biosim_grid_t *grid, uint64_t *rng_state,
                                       biosim_coord_t *out) {
    assert(grid && rng_state && out);
    int32_t total = (int32_t)grid->size_x * (int32_t)grid->size_y;

    /* Random probe phase — succeeds quickly on a sparse grid */
    for (int i = 0; i < 200; i++) {
        int32_t idx = (int32_t)(biosim_rng_next(rng_state) % (uint64_t)total);
        if (grid->cells[idx] == BIOSIM_GRID_EMPTY) {
            out->x = (int16_t)(idx % grid->size_x);
            out->y = (int16_t)(idx / grid->size_x);
            return BIOSIM_OK;
        }
    }

    /* Linear scan fallback — guaranteed to find one if any empty cell exists */
    for (int32_t idx = 0; idx < total; idx++) {
        if (grid->cells[idx] == BIOSIM_GRID_EMPTY) {
            out->x = (int16_t)(idx % grid->size_x);
            out->y = (int16_t)(idx / grid->size_x);
            return BIOSIM_OK;
        }
    }

    BIOSIM_ERRORF("grid is full, increase grid size or decrease population");
    return BIOSIM_ERR_NOTFOUND;
}

/* ── neighborhood ───────────────────────────────────────────────────────── */

void biosim_grid_visit_neighborhood(const biosim_grid_t *grid, biosim_coord_t center,
                                    int16_t radius, biosim_grid_visitor_t visitor, void *sim) {
    assert(grid && visitor);

    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > (int32_t)radius * radius) {
                continue;
            }
            biosim_coord_t c = {(int16_t)(center.x + dx), (int16_t)(center.y + dy)};
            if (!biosim_grid_in_bounds(grid, c)) {
                continue;
            }
            visitor(c, biosim_grid_at(grid, c), sim);
        }
    }
}
