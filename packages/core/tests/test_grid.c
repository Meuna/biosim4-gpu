#include "biosim/core/grid.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"
#include "biosim/core/types.h"
#include "unity.h"

static biosim_grid_t g;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_grid_create(8, 8, &g));
}

void tearDown(void) {
    biosim_grid_free(&g);
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static biosim_coord_t coord(int32_t x, int32_t y) {
    biosim_coord_t c = {x, y};
    return c;
}

static void count_visitor(biosim_coord_t c, uint32_t cell, void *sim) {
    (void)c;
    (void)cell;
    (*(int *)sim)++;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

void test_fresh_grid_all_empty(void) {
    for (int32_t y = 0; y < g.size_y; y++) {
        for (int32_t x = 0; x < g.size_x; x++) {
            TEST_ASSERT_TRUE(biosim_grid_is_empty(&g, coord(x, y)));
        }
    }
}

void test_set_and_at_agent(void) {
    biosim_grid_set(&g, coord(3, 4), 7);
    TEST_ASSERT_EQUAL_UINT32(7, biosim_grid_at(&g, coord(3, 4)));
}

void test_set_and_at_barrier(void) {
    biosim_grid_set(&g, coord(1, 2), BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_UINT32(BIOSIM_GRID_BARRIER, biosim_grid_at(&g, coord(1, 2)));
}

void test_predicates_empty(void) {
    biosim_coord_t c = coord(0, 0);
    TEST_ASSERT_TRUE(biosim_grid_is_empty(&g, c));
    TEST_ASSERT_FALSE(biosim_grid_is_barrier(&g, c));
    TEST_ASSERT_FALSE(biosim_grid_is_occupied(&g, c));
}

void test_predicates_barrier(void) {
    biosim_coord_t c = coord(2, 2);
    biosim_grid_set(&g, c, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_FALSE(biosim_grid_is_empty(&g, c));
    TEST_ASSERT_TRUE(biosim_grid_is_barrier(&g, c));
    TEST_ASSERT_FALSE(biosim_grid_is_occupied(&g, c));
}

void test_predicates_agent(void) {
    biosim_coord_t c = coord(5, 5);
    biosim_grid_set(&g, c, 42);
    TEST_ASSERT_FALSE(biosim_grid_is_empty(&g, c));
    TEST_ASSERT_FALSE(biosim_grid_is_barrier(&g, c));
    TEST_ASSERT_TRUE(biosim_grid_is_occupied(&g, c));
}

void test_in_bounds_corners(void) {
    TEST_ASSERT_TRUE(biosim_grid_in_bounds(&g, coord(0, 0)));
    TEST_ASSERT_TRUE(biosim_grid_in_bounds(&g, coord(7, 7)));
    TEST_ASSERT_TRUE(biosim_grid_in_bounds(&g, coord(7, 0)));
    TEST_ASSERT_TRUE(biosim_grid_in_bounds(&g, coord(0, 7)));
}

void test_in_bounds_outside(void) {
    TEST_ASSERT_FALSE(biosim_grid_in_bounds(&g, coord(-1, 0)));
    TEST_ASSERT_FALSE(biosim_grid_in_bounds(&g, coord(0, -1)));
    TEST_ASSERT_FALSE(biosim_grid_in_bounds(&g, coord(8, 0)));
    TEST_ASSERT_FALSE(biosim_grid_in_bounds(&g, coord(0, 8)));
}

void test_zero_fill_clears_values(void) {
    biosim_grid_set(&g, coord(1, 1), 99);
    biosim_grid_set(&g, coord(6, 6), BIOSIM_GRID_BARRIER);
    biosim_grid_zero_fill(&g);
    TEST_ASSERT_TRUE(biosim_grid_is_empty(&g, coord(1, 1)));
    TEST_ASSERT_TRUE(biosim_grid_is_empty(&g, coord(6, 6)));
}

void test_visit_neighborhood_radius0(void) {
    int count = 0;
    biosim_grid_visit_neighborhood(&g, coord(4, 4), 0, count_visitor, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
}

void test_visit_neighborhood_radius1_center(void) {
    /* Disc r=1: center + 4 cardinal neighbours = 5 cells (not the 3×3 square) */
    int count = 0;
    biosim_grid_visit_neighborhood(&g, coord(4, 4), 1, count_visitor, &count);
    TEST_ASSERT_EQUAL_INT(5, count);
}

void test_visit_neighborhood_clips_at_corner(void) {
    /* corner (0,0) with radius=2: only non-negative coords are in bounds.
     * Disc cells with dx>=0, dy>=0: (0,0),(0,1),(0,2),(1,0),(1,1),(2,0) = 6 */
    int count = 0;
    biosim_grid_visit_neighborhood(&g, coord(0, 0), 2, count_visitor, &count);
    TEST_ASSERT_EQUAL_INT(6, count);
}

void test_find_empty_returns_valid_coord(void) {
    uint64_t rng = biosim_rng_seed(0, 1);
    biosim_coord_t c;
    biosim_status_t st = biosim_grid_find_empty(&g, &rng, &c);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, st);
    TEST_ASSERT_TRUE(biosim_grid_in_bounds(&g, c));
    TEST_ASSERT_TRUE(biosim_grid_is_empty(&g, c));
}

void test_find_empty_full_grid_returns_notfound(void) {
    /* Fill every cell with an agent index */
    for (int32_t y = 0; y < g.size_y; y++) {
        for (int32_t x = 0; x < g.size_x; x++) {
            biosim_grid_set(&g, coord(x, y), 1);
        }
    }
    uint64_t rng = biosim_rng_seed(0, 1);
    biosim_coord_t c;
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_NOTFOUND, biosim_grid_find_empty(&g, &rng, &c));
}

void test_find_last_empty_cell_returns_valid_coord(void) {
    /* Occupy every cell except (3,5) — find_empty must return exactly that coord */
    for (int32_t y = 0; y < g.size_y; y++) {
        for (int32_t x = 0; x < g.size_x; x++) {
            biosim_grid_set(&g, coord(x, y), 1);
        }
    }
    biosim_grid_set(&g, coord(3, 5), BIOSIM_GRID_EMPTY);

    uint64_t rng = biosim_rng_seed(0, 1);
    biosim_coord_t c;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_grid_find_empty(&g, &rng, &c));
    TEST_ASSERT_EQUAL_INT32(3, c.x);
    TEST_ASSERT_EQUAL_INT32(5, c.y);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_grid_all_empty);
    RUN_TEST(test_set_and_at_agent);
    RUN_TEST(test_set_and_at_barrier);
    RUN_TEST(test_predicates_empty);
    RUN_TEST(test_predicates_barrier);
    RUN_TEST(test_predicates_agent);
    RUN_TEST(test_in_bounds_corners);
    RUN_TEST(test_in_bounds_outside);
    RUN_TEST(test_zero_fill_clears_values);
    RUN_TEST(test_visit_neighborhood_radius0);
    RUN_TEST(test_visit_neighborhood_radius1_center);
    RUN_TEST(test_visit_neighborhood_clips_at_corner);
    RUN_TEST(test_find_empty_returns_valid_coord);
    RUN_TEST(test_find_empty_full_grid_returns_notfound);
    RUN_TEST(test_find_last_empty_cell_returns_valid_coord);
    return UNITY_END();
}
