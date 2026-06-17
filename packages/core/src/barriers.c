#include "biosim/core/barriers.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/rng.h"

#include <stddef.h>
#include <stdint.h>

/* ── rng helpers ────────────────────────────────────────────────────────── */

/* Return a random int in [lo, hi] inclusive. */
static int rand_range_i(uint64_t *rng, int lo, int hi) {
    return lo + (int)(biosim_rng_next(rng) % (uint64_t)(hi - lo + 1));
}

/* Return a random float in [lo, hi]. */
static float rand_range_f(uint64_t *rng, float lo, float hi) {
    uint64_t r = biosim_rng_next(rng);
    return lo + (float)(r % 10000U) / 10000.0F * (hi - lo);
}

/* Clamp v to [lo, hi]. */
static int32_t clamp_i(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* ── ratio resolution ───────────────────────────────────────────────────── */

/* Resolve a [0,1] ratio to a cell index in [0, size-1], rounded to nearest. */
static int ratio_to_cell(float ratio, int size) {
    return (int)(ratio * (float)(size - 1) + 0.5F);
}

/* Resolve a [0,1] ratio to a length in cells of the grid's smaller axis. The
 * caller applies its own truncation or rounding to match per-shape behaviour. */
static float ratio_to_dim(float ratio, int gmin) {
    return ratio * (float)gmin;
}

/* ── shape helpers ──────────────────────────────────────────────────────── */

static void fill_box(biosim_grid_t *grid, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    x0 = clamp_i(x0, 0, grid->size_x - 1);
    x1 = clamp_i(x1, 0, grid->size_x - 1);
    y0 = clamp_i(y0, 0, grid->size_y - 1);
    y1 = clamp_i(y1, 0, grid->size_y - 1);

    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            biosim_coord_t c = {x, y};
            biosim_grid_set(grid, c, BIOSIM_GRID_BARRIER);
        }
    }
}

static void barrier_visitor(biosim_coord_t coord, uint32_t cell, void *sim) {
    (void)cell;
    biosim_grid_set((biosim_grid_t *)sim, coord, BIOSIM_GRID_BARRIER);
}

/* Each place_* function returns the resolved centre. */

static biosim_coord_t place_hbar(
    biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng
) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int len = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->length, gmin)
                                                         : rand_range_i(rng, sx / 4, sx / 2);
    int w = (spec->width != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->width, gmin)
                                                      : rand_range_i(rng, 1, 3);

    int half_len = len / 2;
    int half_w = w / 2;

    int x = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->x, sx)
                : rand_range_i(rng, margin_x + half_len, sx - margin_x - half_len);
    int y = (spec->y != BIOSIM_BARRIER_POS_UNSET) ? ratio_to_cell(spec->y, sy)
                                                  : rand_range_i(rng, margin_y, sy - margin_y);

    fill_box(grid, x - half_len, y - half_w, x + half_len, y + half_w);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_vbar(
    biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng
) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int len = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->length, gmin)
                                                         : rand_range_i(rng, sy / 4, sy / 2);
    int w = (spec->width != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->width, gmin)
                                                      : rand_range_i(rng, 1, 3);

    int half_len = len / 2;
    int half_w = w / 2;

    int x = (spec->x != BIOSIM_BARRIER_POS_UNSET) ? ratio_to_cell(spec->x, sx)
                                                  : rand_range_i(rng, margin_x, sx - margin_x);
    int y = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->y, sy)
                : rand_range_i(rng, margin_y + half_len, sy - margin_y - half_len);

    fill_box(grid, x - half_w, y - half_len, x + half_w, y + half_len);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_square(
    biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng
) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int side = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->length, gmin)
                                                          : rand_range_i(rng, sx / 8, sx / 4);

    int half = side / 2;

    int x = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->x, sx)
                : rand_range_i(rng, margin_x + half, sx - margin_x - half);
    int y = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->y, sy)
                : rand_range_i(rng, margin_y + half, sy - margin_y - half);

    fill_box(grid, x - half, y - half, x + half, y + half);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_circle(
    biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng
) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    float radius = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? ratio_to_dim(spec->length, gmin)
                                                              : rand_range_f(rng, 3.0F, 10.0F);
    int r = (int)(radius + 0.5F);

    int x = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->x, sx)
                : rand_range_i(rng, margin_x + r, sx - margin_x - r);
    int y = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                ? ratio_to_cell(spec->y, sy)
                : rand_range_i(rng, margin_y + r, sy - margin_y - r);

    biosim_coord_t centre = {x, y};
    biosim_grid_visit_neighborhood(grid, centre, (int32_t)r, barrier_visitor, grid);
    return centre;
}

static biosim_coord_t place_corner(
    biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng
) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    /* Arms extend a full len from the junction, so the random range keeps len
     * of clearance on every side (range stays valid for gmin/8..gmin/4 arms). */
    int len = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->length, gmin)
                                                         : rand_range_i(rng, gmin / 8, gmin / 4);
    int w = (spec->width != BIOSIM_BARRIER_DIM_UNSET) ? (int)ratio_to_dim(spec->width, gmin)
                                                      : rand_range_i(rng, 1, 3);
    int half_w = w / 2;

    int jx = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                 ? ratio_to_cell(spec->x, sx)
                 : rand_range_i(rng, margin_x + len, sx - margin_x - len);
    int jy = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                 ? ratio_to_cell(spec->y, sy)
                 : rand_range_i(rng, margin_y + len, sy - margin_y - len);

    /* Arm directions per quadrant (index = quadrant): NE, NW, SE, SW. Cardinals
     * follow the io_defs.h direction table (north is -y): NE → (+x,-y),
     * NW → (-x,-y), SE → (+x,+y), SW → (-x,+y). */
    static const int dx_of[] = {1, -1, 1, -1};
    static const int dy_of[] = {-1, -1, 1, 1};
    int dx = dx_of[spec->quadrant];
    int dy = dy_of[spec->quadrant];

    /* Horizontal arm reaches from the junction along x; vertical along y. They
     * overlap at the w×w junction block, which fill_box paints idempotently. */
    int hx0 = jx + (dx < 0 ? dx * len : 0);
    int hx1 = jx + (dx > 0 ? dx * len : 0);
    int vy0 = jy + (dy < 0 ? dy * len : 0);
    int vy1 = jy + (dy > 0 ? dy * len : 0);
    fill_box(grid, hx0, jy - half_w, hx1, jy + half_w);
    fill_box(grid, jx - half_w, vy0, jx + half_w, vy1);

    biosim_coord_t centre = {jx, jy};
    return centre;
}

/* ── public API ─────────────────────────────────────────────────────────── */

void biosim_barriers_place(
    biosim_grid_t *grid,
    const biosim_barrier_spec_t *specs,
    uint32_t n,
    uint64_t *rng_state,
    biosim_coord_t *centers_out
) {
    for (uint32_t i = 0; i < n; i++) {
        const biosim_barrier_spec_t *s = &specs[i];
        biosim_coord_t centre;
        switch (s->kind) {
        case BIOSIM_BARRIER_HBAR:
            centre = place_hbar(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_VBAR:
            centre = place_vbar(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_SQUARE:
            centre = place_square(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_CIRCLE:
            centre = place_circle(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_CORNER:
            centre = place_corner(grid, s, rng_state);
            break;
        default:
            centre.x = 0;
            centre.y = 0;
            break;
        }
        if (centers_out != NULL) {
            centers_out[i] = centre;
        }
    }
}
