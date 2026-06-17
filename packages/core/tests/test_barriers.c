#include "biosim/core/barriers.h"
#include "biosim/core/grid.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"
#include "unity.h"

static biosim_grid_t g;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_grid_create(64, 64, &g));
}

void tearDown(void) {
    biosim_grid_free(&g);
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static biosim_coord_t coord(int16_t x, int16_t y) {
    biosim_coord_t c = {x, y};
    return c;
}

static int count_barriers(const biosim_grid_t *grid) {
    int n = 0;
    for (int16_t y = 0; y < grid->size_y; y++) {
        for (int16_t x = 0; x < grid->size_x; x++) {
            if (biosim_grid_at(grid, coord(x, y)) == BIOSIM_GRID_BARRIER) {
                n++;
            }
        }
    }
    return n;
}

/* ── hbar ───────────────────────────────────────────────────────────────── */

void test_hbar_explicit(void) {
    /* ratios on a 64×64 grid: x/y 0.5 → cell 32; length 20/64, width 2/64 */
    biosim_barrier_spec_t spec = {BIOSIM_BARRIER_HBAR, 0.5F, 0.5F, 20.0F / 64.0F, 2.0F / 64.0F};
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);

    /* centre at (32,32), half_len=10, half_w=1 → rows 31..33, cols 22..42 */
    for (int16_t x = 22; x <= 42; x++) {
        for (int16_t y = 31; y <= 33; y++) {
            TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(x, y)));
        }
    }
    /* One cell outside the expected box must be empty */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(21, 32)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(43, 32)));
}

/* ── vbar ───────────────────────────────────────────────────────────────── */

void test_vbar_explicit(void) {
    biosim_barrier_spec_t spec = {BIOSIM_BARRIER_VBAR, 0.5F, 0.5F, 20.0F / 64.0F, 2.0F / 64.0F};
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);

    /* centre at (32,32), half_len=10, half_w=1 → cols 31..33, rows 22..42 */
    for (int16_t x = 31; x <= 33; x++) {
        for (int16_t y = 22; y <= 42; y++) {
            TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(x, y)));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 21)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 43)));
}

/* ── square ─────────────────────────────────────────────────────────────── */

void test_square_explicit(void) {
    /* x/y 30/63 → cell 30; side 10/64 → 10 */
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_SQUARE, 30.0F / 63.0F, 30.0F / 63.0F, 10.0F / 64.0F, BIOSIM_BARRIER_DIM_UNSET
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);

    /* side=10, half=5 → rows/cols 25..35 */
    for (int16_t x = 25; x <= 35; x++) {
        for (int16_t y = 25; y <= 35; y++) {
            TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(x, y)));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(24, 30)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(36, 30)));
}

/* ── circle ─────────────────────────────────────────────────────────────── */

void test_circle_explicit(void) {
    /* x/y 0.5 → cell 32; radius 5/64 → 5 */
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_CIRCLE, 0.5F, 0.5F, 5.0F / 64.0F, BIOSIM_BARRIER_DIM_UNSET
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);

    /* Centre must be a barrier */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(32, 32)));
    /* Cells clearly outside the radius must be empty */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 26)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 38)));
}

void test_circle_stays_in_bounds(void) {
    /* Spec right at the edge; visit_neighborhood clips silently */
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_CIRCLE, 0.0F, 0.0F, 5.0F / 64.0F, BIOSIM_BARRIER_DIM_UNSET
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);
    /* At least the origin is a barrier */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(0, 0)));
}

/* ── corner ─────────────────────────────────────────────────────────────── */

void test_corner_explicit(void) {
    /* junction (32,32); NE arms reach +x and +y by length 10/64 → 10 cells */
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_CORNER, 0.5F, 0.5F, 10.0F / 64.0F, 2.0F / 64.0F, BIOSIM_CORNER_NE
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);

    /* junction and both arm tips are barriers */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(32, 32)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(42, 32)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(32, 42)));
    /* arms do not extend the opposite way, nor past their length */
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(22, 32)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 22)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(43, 32)));
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_EMPTY, biosim_grid_at(&g, coord(32, 43)));
}

void test_corner_quadrants(void) {
    /* From junction (32,32) with arm length 10, each quadrant places its
     * horizontal arm tip at hx and its vertical arm tip at vy. */
    struct {
        biosim_corner_quadrant_t q;
        int16_t hx;
        int16_t vy;
    } cases[] = {
        {BIOSIM_CORNER_NE, 42, 42},
        {BIOSIM_CORNER_NW, 22, 42},
        {BIOSIM_CORNER_SE, 42, 22},
        {BIOSIM_CORNER_SW, 22, 22},
    };
    for (int i = 0; i < 4; i++) {
        biosim_grid_zero_fill(&g);
        biosim_barrier_spec_t spec = {
            BIOSIM_BARRIER_CORNER, 0.5F, 0.5F, 10.0F / 64.0F, 2.0F / 64.0F, cases[i].q
        };
        uint64_t rng = biosim_rng_seed(0, 0);
        biosim_barriers_place(&g, &spec, 1, &rng, NULL);
        TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(cases[i].hx, 32)));
        TEST_ASSERT_EQUAL_UINT16(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(32, cases[i].vy)));
    }
}

/* ── random placement ───────────────────────────────────────────────────── */

void test_random_hbar_places_barriers(void) {
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_HBAR,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_DIM_UNSET,
        BIOSIM_BARRIER_DIM_UNSET
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);
    TEST_ASSERT_GREATER_THAN(0, count_barriers(&g));
}

void test_random_corner_places_barriers(void) {
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_CORNER,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_DIM_UNSET,
        BIOSIM_BARRIER_DIM_UNSET,
        BIOSIM_CORNER_NE
    };
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng, NULL);
    TEST_ASSERT_GREATER_THAN(0, count_barriers(&g));
}

void test_random_deterministic(void) {
    biosim_barrier_spec_t spec = {
        BIOSIM_BARRIER_CIRCLE,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_POS_UNSET,
        BIOSIM_BARRIER_DIM_UNSET,
        BIOSIM_BARRIER_DIM_UNSET
    };

    uint64_t rng1 = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng1, NULL);
    int count1 = count_barriers(&g);

    biosim_grid_zero_fill(&g);
    uint64_t rng2 = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, &spec, 1, &rng2, NULL);
    int count2 = count_barriers(&g);

    TEST_ASSERT_EQUAL_INT(count1, count2);
}

void test_empty_spec_list(void) {
    uint64_t rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&g, NULL, 0, &rng, NULL);
    TEST_ASSERT_EQUAL_INT(0, count_barriers(&g));
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hbar_explicit);
    RUN_TEST(test_vbar_explicit);
    RUN_TEST(test_square_explicit);
    RUN_TEST(test_circle_explicit);
    RUN_TEST(test_circle_stays_in_bounds);
    RUN_TEST(test_corner_explicit);
    RUN_TEST(test_corner_quadrants);
    RUN_TEST(test_random_hbar_places_barriers);
    RUN_TEST(test_random_corner_places_barriers);
    RUN_TEST(test_random_deterministic);
    RUN_TEST(test_empty_spec_list);
    return UNITY_END();
}
