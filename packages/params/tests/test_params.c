#include "biosim/core/status.h"
#include "biosim/params/params.h"
#include "unity.h"

// clang-format off
static const biosim_param_entry_t test_entries[] = {
    {"population",   "simulation", {.i = 3000},      PARAM_INT,    false, true,  "pop",      "p"},
    {"mutation-rate","simulation", {.f = 0.001},     PARAM_FLOAT,  false, true,  "mut-rate", NULL},
    {"sim-name",     "simulation", {.s = "unnamed"}, PARAM_STRING, false, true,  "sim-name", NULL},
    {"enable-kill",  "simulation", {.b = false},     PARAM_BOOL,   false, true,  "violent",  NULL},
};
// clang-format on
#define TEST_ENTRIES_COUNT (sizeof(test_entries) / sizeof(test_entries[0]))

static biosim_params_t p;

void setUp(void) {
    biosim_params_init(&p, test_entries, TEST_ENTRIES_COUNT);
}

void tearDown(void) {
    biosim_params_free(&p);
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_init_default(void) {
    TEST_ASSERT_EQUAL_INT(3000, biosim_params_get_int(&p, "population"));
    TEST_ASSERT_EQUAL_FLOAT(0.001F, biosim_params_get_float(&p, "mutation-rate"));
    TEST_ASSERT_EQUAL_STRING("unnamed", biosim_params_get_string(&p, "sim-name"));
    TEST_ASSERT_FALSE(biosim_params_get_bool(&p, "enable-kill"));
}

void test_init_is_set_false(void) {
    const biosim_param_entry_t *e = biosim_params_find(&p, "population");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->is_set);
}

void test_set_roundtrip(void) {
    biosim_params_set_int(&p, "population", 42);
    biosim_params_set_float(&p, "mutation-rate", 3.14F);
    biosim_params_set_string(&p, "sim-name", "testsimu");
    biosim_params_set_bool(&p, "enable-kill", true);
    TEST_ASSERT_EQUAL_INT(42, biosim_params_get_int(&p, "population"));
    TEST_ASSERT_EQUAL_FLOAT(3.14F, biosim_params_get_float(&p, "mutation-rate"));
    TEST_ASSERT_EQUAL_STRING("testsimu", biosim_params_get_string(&p, "sim-name"));
    TEST_ASSERT_TRUE(biosim_params_get_bool(&p, "enable-kill"));
}

void test_set_marks_is_set(void) {
    biosim_params_set_int(&p, "population", 1);
    biosim_params_set_float(&p, "mutation-rate", 1.0F);
    biosim_params_set_string(&p, "sim-name", "1");
    biosim_params_set_bool(&p, "enable-kill", true);

    char *keys[] = {"population", "mutation-rate", "sim-name", "enable-kill"};
    int len = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < len; i++) {
        const biosim_param_entry_t *e = biosim_params_find(&p, keys[i]);
        TEST_ASSERT_TRUE(e->is_set);
    }
}

void test_set_wrong_type_returns_err(void) {
    biosim_status_t st;
    st = biosim_params_set_int(&p, "enable-kill", 1);
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_TYPE, st);
    st = biosim_params_set_float(&p, "population", 1.0);
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_TYPE, st);
    st = biosim_params_set_string(&p, "mutation-rate", "bad");
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_TYPE, st);
    st = biosim_params_set_bool(&p, "sim-name", false);
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_TYPE, st);
}

void test_unknown_key_returns_warn(void) {
    biosim_status_t st;
    st = biosim_params_set_int(&p, "no-such-key", 1);
    TEST_ASSERT_EQUAL_INT(BIOSIM_WARN_UNKNOWN_KEY, st);
    st = biosim_params_set_float(&p, "no-such-key", 1.0);
    TEST_ASSERT_EQUAL_INT(BIOSIM_WARN_UNKNOWN_KEY, st);
    st = biosim_params_set_string(&p, "no-such-key", "bad");
    TEST_ASSERT_EQUAL_INT(BIOSIM_WARN_UNKNOWN_KEY, st);
    st = biosim_params_set_bool(&p, "no-such-key", false);
    TEST_ASSERT_EQUAL_INT(BIOSIM_WARN_UNKNOWN_KEY, st);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_default);
    RUN_TEST(test_init_is_set_false);
    RUN_TEST(test_set_roundtrip);
    RUN_TEST(test_set_marks_is_set);
    RUN_TEST(test_set_wrong_type_returns_err);
    RUN_TEST(test_unknown_key_returns_warn);
    return UNITY_END();
}
