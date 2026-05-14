/*
 * HOST-ONLY: uses grid.h which carries a heap pointer.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_BARRIERS_H
#define BIOSIM_CORE_BARRIERS_H

#include "biosim/core/grid.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/status.h"
#include <stdint.h>

typedef enum {
    BIOSIM_BARRIER_HBAR,
    BIOSIM_BARRIER_VBAR,
    BIOSIM_BARRIER_SQUARE,
    BIOSIM_BARRIER_CIRCLE,
} biosim_barrier_kind_t;

/* Sentinel: use random placement / dimension */
#define BIOSIM_BARRIER_POS_UNSET ((int16_t)INT16_MIN)
#define BIOSIM_BARRIER_DIM_UNSET (0.0F)

/*
 * Describes one barrier shape.
 *
 * x, y    — centre of the barrier; BIOSIM_BARRIER_POS_UNSET picks a random position.
 * length  — primary dimension (bar length, square side, circle radius);
 *           BIOSIM_BARRIER_DIM_UNSET picks a random value.
 * width   — thickness perpendicular to the bar axis (bars only);
 *           BIOSIM_BARRIER_DIM_UNSET picks a random value.
 */
typedef struct {
    biosim_barrier_kind_t kind;
    int16_t x;
    int16_t y;
    float length;
    float width;
} biosim_barrier_spec_t;

/*
 * Write all barriers described by specs[0..n-1] onto grid as BIOSIM_GRID_BARRIER cells.
 * rng_state drives random position/dimension resolution; it is advanced in-place so
 * multiple calls with the same initial state produce the same layout (deterministic).
 * If centers_out is non-NULL it must point to an array of n biosim_coord_t elements;
 * centers_out[i] receives the resolved centre of barrier i after random draws.
 * Returns BIOSIM_OK; placement is tolerant of out-of-bounds — cells are clipped silently.
 */
void biosim_barriers_place(biosim_grid_t *grid, const biosim_barrier_spec_t *specs, uint32_t n,
                           uint64_t *rng_state, biosim_coord_t *centers_out);

#endif /* BIOSIM_CORE_BARRIERS_H */
