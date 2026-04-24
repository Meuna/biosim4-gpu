#include "biosim/core/barriers.h"
#include "biosim/core/rng.h"
#include "biosim/core/types.h"

#include <stdint.h>

/* ── RNG helpers ────────────────────────────────────────────────────────── */

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
static int16_t clamp_i(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* ── Shape helpers ──────────────────────────────────────────────────────── */

static void fill_box(biosim_grid_t *grid, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    x0 = clamp_i(x0, 0, (int16_t)(grid->size_x - 1));
    x1 = clamp_i(x1, 0, (int16_t)(grid->size_x - 1));
    y0 = clamp_i(y0, 0, (int16_t)(grid->size_y - 1));
    y1 = clamp_i(y1, 0, (int16_t)(grid->size_y - 1));

    for (int16_t y = y0; y <= y1; y++) {
        for (int16_t x = x0; x <= x1; x++) {
            biosim_coord_t c = {x, y};
            biosim_grid_set(grid, c, BIOSIM_GRID_BARRIER);
        }
    }
}

static void barrier_visitor(biosim_coord_t coord, uint16_t cell, void *ctx) {
    (void)cell;
    biosim_grid_set((biosim_grid_t *)ctx, coord, BIOSIM_GRID_BARRIER);
}

static void place_hbar(biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int len = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)spec->length
                                                         : rand_range_i(rng, sx / 4, sx / 2);
    int w = (spec->width != BIOSIM_BARRIER_DIM_UNSET) ? (int)spec->width : rand_range_i(rng, 1, 3);

    int half_len = len / 2;
    int half_w = w / 2;

    int cx = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->x
                 : rand_range_i(rng, margin_x + half_len, sx - margin_x - half_len);
    int cy = (spec->y != BIOSIM_BARRIER_POS_UNSET) ? (int)spec->y
                                                   : rand_range_i(rng, margin_y, sy - margin_y);

    fill_box(grid, (int16_t)(cx - half_len), (int16_t)(cy - half_w), (int16_t)(cx + half_len),
             (int16_t)(cy + half_w));
}

static void place_vbar(biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int len = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)spec->length
                                                         : rand_range_i(rng, sy / 4, sy / 2);
    int w = (spec->width != BIOSIM_BARRIER_DIM_UNSET) ? (int)spec->width : rand_range_i(rng, 1, 3);

    int half_len = len / 2;
    int half_w = w / 2;

    int cx = (spec->x != BIOSIM_BARRIER_POS_UNSET) ? (int)spec->x
                                                   : rand_range_i(rng, margin_x, sx - margin_x);
    int cy = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->y
                 : rand_range_i(rng, margin_y + half_len, sy - margin_y - half_len);

    fill_box(grid, (int16_t)(cx - half_w), (int16_t)(cy - half_len), (int16_t)(cx + half_w),
             (int16_t)(cy + half_len));
}

static void place_square(biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    int side = (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? (int)spec->length
                                                          : rand_range_i(rng, sx / 8, sx / 4);

    int half = side / 2;

    int cx = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->x
                 : rand_range_i(rng, margin_x + half, sx - margin_x - half);
    int cy = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->y
                 : rand_range_i(rng, margin_y + half, sy - margin_y - half);

    fill_box(grid, (int16_t)(cx - half), (int16_t)(cy - half), (int16_t)(cx + half),
             (int16_t)(cy + half));
}

static void place_circle(biosim_grid_t *grid, const biosim_barrier_spec_t *spec, uint64_t *rng) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int margin_x = sx / 10;
    int margin_y = sy / 10;

    float radius =
        (spec->length != BIOSIM_BARRIER_DIM_UNSET) ? spec->length : rand_range_f(rng, 3.0F, 10.0F);
    int r = (int)(radius + 0.5F);

    int cx = (spec->x != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->x
                 : rand_range_i(rng, margin_x + r, sx - margin_x - r);
    int cy = (spec->y != BIOSIM_BARRIER_POS_UNSET)
                 ? (int)spec->y
                 : rand_range_i(rng, margin_y + r, sy - margin_y - r);

    biosim_coord_t center = {(int16_t)cx, (int16_t)cy};
    biosim_grid_visit_neighborhood(grid, center, (int16_t)r, barrier_visitor, grid);
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_status_t biosim_barriers_place(biosim_grid_t *grid, const biosim_barrier_spec_t *specs,
                                      int n, uint64_t *rng_state) {
    for (int i = 0; i < n; i++) {
        const biosim_barrier_spec_t *s = &specs[i];
        switch (s->kind) {
        case BIOSIM_BARRIER_HBAR:
            place_hbar(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_VBAR:
            place_vbar(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_SQUARE:
            place_square(grid, s, rng_state);
            break;
        case BIOSIM_BARRIER_CIRCLE:
            place_circle(grid, s, rng_state);
            break;
        }
    }
    return BIOSIM_OK;
}
