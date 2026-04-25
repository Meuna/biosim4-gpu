#include "biosim/core/genome.h"
#include "biosim/core/status.h"
#include "unity.h"

static biosim_genome_t g;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_genome_create(8, 16, &g));
}

void tearDown(void) {
    biosim_genome_free(&g);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void test_create_returns_ok(void) {
    biosim_genome_t local;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_genome_create(4, 8, &local));
    biosim_genome_free(&local);
}

void test_create_pointers_non_null(void) {
    TEST_ASSERT_NOT_NULL(g.conn);
    TEST_ASSERT_NOT_NULL(g.wgt);
    TEST_ASSERT_NOT_NULL(g.length);
}

void test_create_metadata_stored(void) {
    TEST_ASSERT_EQUAL_UINT32(8, g.capacity);
    TEST_ASSERT_EQUAL_UINT16(16, g.max_length);
}

void test_free_zeroes_struct(void) {
    biosim_genome_t local;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_genome_create(4, 8, &local));
    biosim_genome_free(&local);
    TEST_ASSERT_NULL(local.conn);
    TEST_ASSERT_EQUAL_UINT32(0, local.capacity);
}

/* ── Slot Operations ────────────────────────────────────────────────────── */

void test_init_slot_sets_length(void) {
    uint64_t rng = 42ULL;
    biosim_genome_init_slot(&g, 0, 5, &rng);
    TEST_ASSERT_EQUAL_UINT16(5, g.length[0]);
}

void test_init_slot_different_rngs_differ(void) {
    uint64_t rng_a = 1ULL;
    uint64_t rng_b = 2ULL;
    biosim_genome_init_slot(&g, 0, 4, &rng_a);
    biosim_genome_init_slot(&g, 1, 4, &rng_b);
    TEST_ASSERT_NOT_EQUAL_UINT16(g.conn[0 * g.capacity + 0], g.conn[0 * g.capacity + 1]);
}

void test_copy_slot_matches_source(void) {
    uint64_t rng = 7ULL;
    biosim_genome_init_slot(&g, 0, 6, &rng);
    biosim_genome_copy_slot(&g, 1, 0);
    TEST_ASSERT_EQUAL_UINT16(g.length[0], g.length[1]);
    /* Spot-check first and last active gene */
    TEST_ASSERT_EQUAL_UINT16(g.conn[0 * g.capacity + 0], g.conn[0 * g.capacity + 1]);
    TEST_ASSERT_EQUAL_INT16(g.wgt[5 * g.capacity + 0], g.wgt[5 * g.capacity + 1]);
}

void test_copy_slot_independence(void) {
    uint64_t rng = 99ULL;
    biosim_genome_init_slot(&g, 0, 4, &rng);
    biosim_genome_copy_slot(&g, 1, 0);
    uint16_t original = g.conn[0 * g.capacity + 0];
    g.conn[0 * g.capacity + 1] = 0;
    TEST_ASSERT_EQUAL_UINT16(original, g.conn[0 * g.capacity + 0]);
}

/* ── Mutation ───────────────────────────────────────────────────────────── */

void test_mutate_rate_zero_unchanged(void) {
    uint64_t rng = 11ULL;
    biosim_genome_init_slot(&g, 0, 8, &rng);
    uint16_t saved_conn = g.conn[0 * g.capacity + 0];
    int16_t saved_wgt = g.wgt[0 * g.capacity + 0];
    uint16_t saved_len = g.length[0];
    biosim_genome_mutate(&g, 0, 0.0F, &rng);
    TEST_ASSERT_EQUAL_UINT16(saved_conn, g.conn[0 * g.capacity + 0]);
    TEST_ASSERT_EQUAL_INT16(saved_wgt, g.wgt[0 * g.capacity + 0]);
    TEST_ASSERT_EQUAL_UINT16(saved_len, g.length[0]);
}

void test_mutate_rate_one_changed(void) {
    /* With 1 < length < max_length, structural mutation always fires at rate=1.0
     * and always produces a length change (neither insert nor delete returns early). */
    uint64_t rng = 13ULL;
    biosim_genome_init_slot(&g, 0, 8, &rng);
    uint16_t original_len = g.length[0];
    biosim_genome_mutate(&g, 0, 1.0F, &rng);
    TEST_ASSERT_NOT_EQUAL_UINT16(original_len, g.length[0]);
}

void test_mutate_length_never_zero(void) {
    uint64_t rng = 17ULL;
    biosim_genome_init_slot(&g, 0, 1, &rng);
    for (int i = 0; i < 200; i++) {
        biosim_genome_mutate(&g, 0, 1.0F, &rng);
        TEST_ASSERT_TRUE(g.length[0] >= 1U);
    }
}

void test_mutate_length_never_exceeds_max(void) {
    uint64_t rng = 19ULL;
    biosim_genome_init_slot(&g, 0, 8, &rng);
    for (int i = 0; i < 200; i++) {
        biosim_genome_mutate(&g, 0, 1.0F, &rng);
        TEST_ASSERT_TRUE(g.length[0] <= g.max_length);
    }
}

/* ── Crossover ──────────────────────────────────────────────────────────── */

void test_crossover_child_length_equals_parent_b(void) {
    uint64_t rng_a = 5ULL;
    uint64_t rng_b = 6ULL;
    uint64_t rng_cross = 7ULL;
    biosim_genome_init_slot(&g, 0, 4, &rng_a);
    biosim_genome_init_slot(&g, 1, 6, &rng_b);
    biosim_genome_crossover(&g, 2, 0, 1, &rng_cross);
    TEST_ASSERT_EQUAL_UINT16(6, g.length[2]);
}

void test_crossover_length_within_max(void) {
    uint64_t rng_a = 8ULL;
    uint64_t rng_b = 9ULL;
    uint64_t rng_cross = 10ULL;
    biosim_genome_init_slot(&g, 0, 16, &rng_a);
    biosim_genome_init_slot(&g, 1, 16, &rng_b);
    biosim_genome_crossover(&g, 2, 0, 1, &rng_cross);
    TEST_ASSERT_TRUE(g.length[2] <= g.max_length);
}

void test_crossover_child_has_parents_genome(void) {
    /* Single-point crossover: gene j of child equals gene j of parent_a (j < k)
     * or gene j of parent_b (j >= k).  Every child gene must match one parent. */
    uint64_t rng_a = 51ULL;
    uint64_t rng_b = 53ULL;
    uint64_t rng_cross = 59ULL;
    biosim_genome_init_slot(&g, 0, 8, &rng_a);
    biosim_genome_init_slot(&g, 1, 8, &rng_b);
    biosim_genome_crossover(&g, 2, 0, 1, &rng_cross);
    uint16_t child_len = g.length[2];
    for (uint32_t j = 0; j < child_len; j++) {
        uint16_t c_conn = g.conn[j * g.capacity + 2];
        int16_t c_wgt = g.wgt[j * g.capacity + 2];
        int from_a = (c_conn == g.conn[j * g.capacity + 0]) && (c_wgt == g.wgt[j * g.capacity + 0]);
        int from_b = (c_conn == g.conn[j * g.capacity + 1]) && (c_wgt == g.wgt[j * g.capacity + 1]);
        TEST_ASSERT_TRUE(from_a || from_b);
    }
}

/* ── Sort ───────────────────────────────────────────────────────────────── */

void test_sort_descending_order(void) {
    uint64_t rng = 37ULL;
    biosim_genome_init_slot(&g, 0, 4, &rng);
    biosim_genome_init_slot(&g, 1, 2, &rng);
    biosim_genome_init_slot(&g, 2, 6, &rng);
    biosim_genome_init_slot(&g, 3, 1, &rng);
    uint32_t perm[8];
    biosim_genome_sort_by_length(&g, perm);
    TEST_ASSERT_EQUAL_UINT16(6, g.length[0]);
    TEST_ASSERT_EQUAL_UINT16(4, g.length[1]);
    TEST_ASSERT_EQUAL_UINT16(2, g.length[2]);
    TEST_ASSERT_EQUAL_UINT16(1, g.length[3]);
}

void test_sort_permutation_valid(void) {
    uint64_t rng = 41ULL;
    for (uint32_t i = 0; i < 8; i++) {
        biosim_genome_init_slot(&g, i, (i % 4U) + 1U, &rng);
    }
    uint32_t perm[8];
    biosim_genome_sort_by_length(&g, perm);
    uint8_t seen[8] = {0};
    for (uint32_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(perm[i] < 8U);
        seen[perm[i]] = 1;
    }
    for (uint32_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(seen[i]);
    }
}

void test_sort_preserves_genes(void) {
    uint64_t rng_a = 1ULL;
    uint64_t rng_b = 2ULL;
    uint64_t rng_c = 3ULL;
    uint64_t rng_d = 4ULL;
    biosim_genome_init_slot(&g, 0, 3, &rng_a);
    biosim_genome_init_slot(&g, 1, 5, &rng_b);
    biosim_genome_init_slot(&g, 2, 1, &rng_c);
    biosim_genome_init_slot(&g, 3, 7, &rng_d);
    uint16_t saved_conn = g.conn[0 * g.capacity + 2];
    int16_t saved_wgt = g.wgt[0 * g.capacity + 2];
    uint32_t perm[8];
    biosim_genome_sort_by_length(&g, perm);
    uint32_t new_pos = 0;
    for (uint32_t i = 0; i < 8; i++) {
        if (perm[i] == 2U) {
            new_pos = i;
            break;
        }
    }
    TEST_ASSERT_EQUAL_UINT16(saved_conn, g.conn[0 * g.capacity + new_pos]);
    TEST_ASSERT_EQUAL_INT16(saved_wgt, g.wgt[0 * g.capacity + new_pos]);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_returns_ok);
    RUN_TEST(test_create_pointers_non_null);
    RUN_TEST(test_create_metadata_stored);
    RUN_TEST(test_free_zeroes_struct);
    RUN_TEST(test_init_slot_sets_length);
    RUN_TEST(test_init_slot_different_rngs_differ);
    RUN_TEST(test_copy_slot_matches_source);
    RUN_TEST(test_copy_slot_independence);
    RUN_TEST(test_mutate_rate_zero_unchanged);
    RUN_TEST(test_mutate_rate_one_changed);
    RUN_TEST(test_mutate_length_never_zero);
    RUN_TEST(test_mutate_length_never_exceeds_max);
    RUN_TEST(test_crossover_child_length_equals_parent_b);
    RUN_TEST(test_crossover_length_within_max);
    RUN_TEST(test_crossover_child_has_parents_genome);
    RUN_TEST(test_sort_descending_order);
    RUN_TEST(test_sort_permutation_valid);
    RUN_TEST(test_sort_preserves_genes);
    return UNITY_END();
}
