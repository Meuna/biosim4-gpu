/*
 * HOST-ONLY: this header uses a heap pointer and standard library types.
 * Do NOT include it from OpenCL kernel sources (.cl files).
 * GPU kernels receive the raw uint16_t * buffer as a kernel argument.
 */
#ifndef BIOSIM_CORE_GRID_H
#define BIOSIM_CORE_GRID_H

#include "biosim/core/status.h"
#include "biosim/core/types.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Flat 2D grid buffer, row-major: cells[y * size_x + x].
 * Cell encoding: BIOSIM_GRID_EMPTY (0), BIOSIM_GRID_BARRIER (0xFFFF),
 * or a 1-based agent index in [1, 0xFFFE].
 */
typedef struct {
    uint16_t *cells;
    int16_t   size_x;
    int16_t   size_y;
} biosim_grid_t;

/* Lifecycle */
biosim_status_t biosim_grid_create(int16_t size_x, int16_t size_y, biosim_grid_t *out);
void            biosim_grid_free(biosim_grid_t *grid);
void            biosim_grid_zero_fill(biosim_grid_t *grid);

/* Bounds check and raw cell access — at/set assert in_bounds */
bool     biosim_grid_in_bounds(const biosim_grid_t *grid, biosim_coord_t coord);
uint16_t biosim_grid_at(const biosim_grid_t *grid, biosim_coord_t coord);
void     biosim_grid_set(biosim_grid_t *grid, biosim_coord_t coord, uint16_t value);

/* Cell predicates */
bool biosim_grid_is_empty(const biosim_grid_t *grid, biosim_coord_t coord);
bool biosim_grid_is_barrier(const biosim_grid_t *grid, biosim_coord_t coord);
bool biosim_grid_is_occupied(const biosim_grid_t *grid, biosim_coord_t coord);

/*
 * Visit every cell within a disc of the given radius centred on `center`.
 * Out-of-bounds cells are silently skipped. The callback receives the
 * coordinate and current cell value; pass arbitrary context via `ctx`.
 */
typedef void (*biosim_grid_visitor_t)(biosim_coord_t coord, uint16_t cell, void *ctx);
void biosim_grid_visit_neighborhood(const biosim_grid_t *grid, biosim_coord_t center,
                                    int16_t radius, biosim_grid_visitor_t visitor, void *ctx);

#endif /* BIOSIM_CORE_GRID_H */
