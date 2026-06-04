#include "biosim/core/survivor_snap.h"
#include "unity.h"

void setUp(void) {
}
void tearDown(void) {
}

void test_grow_zero_init_first_alloc(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    TEST_ASSERT_TRUE(snap.pop_cap >= 4U);
    TEST_ASSERT_TRUE(snap.stride_cap >= 8U);
    TEST_ASSERT_NOT_NULL(snap.conn);
    TEST_ASSERT_NOT_NULL(snap.wgt);
    TEST_ASSERT_NOT_NULL(snap.len);
    TEST_ASSERT_NOT_NULL(snap.scores);
    biosim_survivor_snap_free(&snap);
}

void test_grow_noop_when_capacity_sufficient(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    uint32_t pop_before = snap.pop_cap;
    uint16_t stride_before = snap.stride_cap;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    TEST_ASSERT_EQUAL_UINT32(pop_before, snap.pop_cap);
    TEST_ASSERT_EQUAL_UINT16(stride_before, snap.stride_cap);
    biosim_survivor_snap_free(&snap);
}

void test_grow_doubles_pop_dimension(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    uint32_t first_cap = snap.pop_cap;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, first_cap + 1U, 8U));
    TEST_ASSERT_TRUE(snap.pop_cap >= first_cap * 2U);
    biosim_survivor_snap_free(&snap);
}

void test_grow_doubles_stride_dimension(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    uint16_t first_stride = snap.stride_cap;
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, (uint16_t)(first_stride + 1U))
    );
    TEST_ASSERT_TRUE(snap.stride_cap >= (uint16_t)(first_stride * 2U));
    biosim_survivor_snap_free(&snap);
}

void test_grow_zero_survivors_noop(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 0U, 8U));
    TEST_ASSERT_EQUAL_UINT32(0U, snap.pop_cap);
    TEST_ASSERT_NULL(snap.conn);
    biosim_survivor_snap_free(&snap);
}

void test_free_tolerates_null_snap(void) {
    biosim_survivor_snap_free(NULL);
}

void test_free_tolerates_null_members(void) {
    biosim_survivor_snap_t snap = {0};
    biosim_survivor_snap_free(&snap);
}

void test_free_clears_caps(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    biosim_survivor_snap_free(&snap);
    TEST_ASSERT_EQUAL_UINT32(0U, snap.pop_cap);
    TEST_ASSERT_EQUAL_UINT16(0U, snap.stride_cap);
    TEST_ASSERT_EQUAL_UINT32(0U, snap.count);
    TEST_ASSERT_NULL(snap.conn);
    TEST_ASSERT_NULL(snap.wgt);
    TEST_ASSERT_NULL(snap.len);
    TEST_ASSERT_NULL(snap.scores);
}

void test_grow_reuse_after_free(void) {
    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 4U, 8U));
    biosim_survivor_snap_free(&snap);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_survivor_snap_grow(&snap, 2U, 4U));
    TEST_ASSERT_NOT_NULL(snap.conn);
    biosim_survivor_snap_free(&snap);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_grow_zero_init_first_alloc);
    RUN_TEST(test_grow_noop_when_capacity_sufficient);
    RUN_TEST(test_grow_doubles_pop_dimension);
    RUN_TEST(test_grow_doubles_stride_dimension);
    RUN_TEST(test_grow_zero_survivors_noop);
    RUN_TEST(test_free_tolerates_null_snap);
    RUN_TEST(test_free_tolerates_null_members);
    RUN_TEST(test_free_clears_caps);
    RUN_TEST(test_grow_reuse_after_free);
    return UNITY_END();
}
