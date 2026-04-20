#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include "unity.h"

static biosim_params_t p;

void setUp(void) {
    biosim_params_init(&p);
}

void tearDown(void) {
    biosim_params_free(&p);
}

void test_init_population_default(void) {
    TEST_ASSERT_EQUAL_INT(3000, biosim_params_get_int(&p, "population"));
}

void test_init_is_set_false(void) {
    const biosim_param_entry_t *e = biosim_params_find(&p, "population");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->is_set);
}

void test_set_int_roundtrip(void) {
    biosim_params_set_int(&p, "population", 42);
    TEST_ASSERT_EQUAL_INT(42, biosim_params_get_int(&p, "population"));
}

void test_set_int_marks_is_set(void) {
    biosim_params_set_int(&p, "population", 1);
    const biosim_param_entry_t *e = biosim_params_find(&p, "population");
    TEST_ASSERT_TRUE(e->is_set);
}

void test_set_wrong_type_returns_err(void) {
    biosim_status_t st = biosim_params_set_string(&p, "population", "bad");
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_TYPE, st);
}

void test_unknown_key_returns_warn(void) {
    biosim_status_t st = biosim_params_set_int(&p, "no-such-key", 1);
    TEST_ASSERT_EQUAL_INT(BIOSIM_WARN_UNKNOWN_KEY, st);
}

void test_set_float_roundtrip(void) {
    biosim_params_set_float(&p, "mutation-rate", 0.5);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, biosim_params_get_float(&p, "mutation-rate"));
}

void test_extend_adds_entries(void) {
    size_t before = biosim_params_count(&p);

    biosim_param_entry_t extra = {
        "sim-name", {.s = "unnamed"}, {.s = "unnamed"}, PARAM_STRING, false};
    biosim_params_extend(&p, &extra, 1);
    TEST_ASSERT_EQUAL_UINT(before + 1, biosim_params_count(&p));
}

void test_get_string_default_after_extend(void) {
    biosim_param_entry_t extra = {
        "sim-name", {.s = "unnamed"}, {.s = "unnamed"}, PARAM_STRING, false};
    biosim_params_extend(&p, &extra, 1);
    TEST_ASSERT_EQUAL_STRING("unnamed", biosim_params_get_string(&p, "sim-name"));
}

void test_set_string_roundtrip(void) {
    biosim_param_entry_t extra = {
        "sim-name", {.s = "unnamed"}, {.s = "unnamed"}, PARAM_STRING, false};
    biosim_params_extend(&p, &extra, 1);
    biosim_params_set_string(&p, "sim-name", "myrun");
    TEST_ASSERT_EQUAL_STRING("myrun", biosim_params_get_string(&p, "sim-name"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_population_default);
    RUN_TEST(test_init_is_set_false);
    RUN_TEST(test_set_int_roundtrip);
    RUN_TEST(test_set_int_marks_is_set);
    RUN_TEST(test_set_wrong_type_returns_err);
    RUN_TEST(test_unknown_key_returns_warn);
    RUN_TEST(test_set_float_roundtrip);
    RUN_TEST(test_extend_adds_entries);
    RUN_TEST(test_get_string_default_after_extend);
    RUN_TEST(test_set_string_roundtrip);
    return UNITY_END();
}
