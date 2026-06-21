#include "biosim/core/barriers.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/io_defs.h"

#include <stddef.h>
#include <stdint.h>

/* ── default resolution ─────────────────────────────────────────────────── */

/*
 * Default ratios substituted for omitted (BIOSIM_BARRIER_*_UNSET) fields. The
 * position default centres the barrier; the per-kind dimension defaults sit
 * near the middle of each shape's useful range and scale with the grid via the
 * same ratio_to_dim path the explicit values use.
 */
#define DEFAULT_BARRIER_POS   (0.5F)
#define DEFAULT_BAR_LENGTH    (0.375F)
#define DEFAULT_BAR_WIDTH     (0.03F)
#define DEFAULT_SQUARE_SIDE   (0.1875F)
#define DEFAULT_CIRCLE_RADIUS (0.10F)
#define DEFAULT_CORNER_LENGTH (0.1875F)
#define DEFAULT_CORNER_WIDTH  (0.03F)

/* Per-kind dimension defaults. width is ignored where the shape has none. */
static void kind_dim_defaults(biosim_barrier_kind_t kind, float *length, float *width) {
    switch (kind) {
    case BIOSIM_BARRIER_HBAR:
    case BIOSIM_BARRIER_VBAR:
        *length = DEFAULT_BAR_LENGTH;
        *width = DEFAULT_BAR_WIDTH;
        break;
    case BIOSIM_BARRIER_SQUARE:
        *length = DEFAULT_SQUARE_SIDE;
        *width = DEFAULT_BAR_WIDTH;
        break;
    case BIOSIM_BARRIER_CIRCLE:
        *length = DEFAULT_CIRCLE_RADIUS;
        *width = DEFAULT_BAR_WIDTH;
        break;
    case BIOSIM_BARRIER_CORNER:
        *length = DEFAULT_CORNER_LENGTH;
        *width = DEFAULT_CORNER_WIDTH;
        break;
    default:
        *length = DEFAULT_BAR_LENGTH;
        *width = DEFAULT_BAR_WIDTH;
        break;
    }
}

/* Return a copy of spec with every omitted field replaced by its default. */
static biosim_barrier_spec_t resolve_defaults(const biosim_barrier_spec_t *spec) {
    biosim_barrier_spec_t out = *spec;
    float default_length = 0.0F;
    float default_width = 0.0F;
    kind_dim_defaults(spec->kind, &default_length, &default_width);

    if (out.x == BIOSIM_BARRIER_POS_UNSET) {
        out.x = DEFAULT_BARRIER_POS;
    }
    if (out.y == BIOSIM_BARRIER_POS_UNSET) {
        out.y = DEFAULT_BARRIER_POS;
    }
    if (out.length == BIOSIM_BARRIER_DIM_UNSET) {
        out.length = default_length;
    }
    if (out.width == BIOSIM_BARRIER_DIM_UNSET) {
        out.width = default_width;
    }
    return out;
}

/* ── math helpers ───────────────────────────────────────────────────────── */

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

/* Each place_* function takes a spec with defaults already resolved (no omitted
 * fields) and returns the resolved centre. */

static biosim_coord_t place_hbar(biosim_grid_t *grid, const biosim_barrier_spec_t *spec) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;

    int len = (int)ratio_to_dim(spec->length, gmin);
    int w = (int)ratio_to_dim(spec->width, gmin);

    int half_len = len / 2;
    int half_w = w / 2;

    int x = ratio_to_cell(spec->x, sx);
    int y = ratio_to_cell(spec->y, sy);

    fill_box(grid, x - half_len, y - half_w, x + half_len, y + half_w);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_vbar(biosim_grid_t *grid, const biosim_barrier_spec_t *spec) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;

    int len = (int)ratio_to_dim(spec->length, gmin);
    int w = (int)ratio_to_dim(spec->width, gmin);

    int half_len = len / 2;
    int half_w = w / 2;

    int x = ratio_to_cell(spec->x, sx);
    int y = ratio_to_cell(spec->y, sy);

    fill_box(grid, x - half_w, y - half_len, x + half_w, y + half_len);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_square(biosim_grid_t *grid, const biosim_barrier_spec_t *spec) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;

    int side = (int)ratio_to_dim(spec->length, gmin);

    int half = side / 2;

    int x = ratio_to_cell(spec->x, sx);
    int y = ratio_to_cell(spec->y, sy);

    fill_box(grid, x - half, y - half, x + half, y + half);

    biosim_coord_t centre = {x, y};
    return centre;
}

static biosim_coord_t place_circle(biosim_grid_t *grid, const biosim_barrier_spec_t *spec) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;

    float radius = ratio_to_dim(spec->length, gmin);
    int r = (int)(radius + 0.5F);

    int x = ratio_to_cell(spec->x, sx);
    int y = ratio_to_cell(spec->y, sy);

    biosim_coord_t centre = {x, y};
    biosim_grid_visit_neighborhood(grid, centre, (int32_t)r, barrier_visitor, grid);
    return centre;
}

static biosim_coord_t place_corner(biosim_grid_t *grid, const biosim_barrier_spec_t *spec) {
    int sx = (int)grid->size_x;
    int sy = (int)grid->size_y;
    int gmin = sx < sy ? sx : sy;

    int len = (int)ratio_to_dim(spec->length, gmin);
    int w = (int)ratio_to_dim(spec->width, gmin);
    int half_w = w / 2;

    int jx = ratio_to_cell(spec->x, sx);
    int jy = ratio_to_cell(spec->y, sy);

    /* Arm directions come straight from the io_defs.h direction table so they
     * cannot drift from the canonical cardinal vectors. quadrant_dir maps each
     * quadrant (NE, NW, SE, SW) onto its table index (E NE N NW W SW S SE),
     * yielding NE → (+x,-y), NW → (-x,-y), SE → (+x,+y), SW → (-x,+y). */
    static const int quadrant_dir[] = {1, 3, 7, 5};
    int dir = quadrant_dir[spec->quadrant];
    int dx = (int)BIOSIM_DIR_DX[dir];
    int dy = (int)BIOSIM_DIR_DY[dir];

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
    biosim_grid_t *grid, const biosim_barrier_spec_t *specs, uint32_t n, biosim_coord_t *centers_out
) {
    for (uint32_t i = 0; i < n; i++) {
        biosim_barrier_spec_t s = resolve_defaults(&specs[i]);
        biosim_coord_t centre;
        switch (s.kind) {
        case BIOSIM_BARRIER_HBAR:
            centre = place_hbar(grid, &s);
            break;
        case BIOSIM_BARRIER_VBAR:
            centre = place_vbar(grid, &s);
            break;
        case BIOSIM_BARRIER_SQUARE:
            centre = place_square(grid, &s);
            break;
        case BIOSIM_BARRIER_CIRCLE:
            centre = place_circle(grid, &s);
            break;
        case BIOSIM_BARRIER_CORNER:
            centre = place_corner(grid, &s);
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
