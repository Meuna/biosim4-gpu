#include "biosim/core/params.h"
#include "biosim/core/status.h"

#include "unity.h"

// clang-format off
static const biosim_param_entry_t test_entries[] = {
    {"toplevel-param", NULL,         {.s = "default"}, PARAM_STRING, false, true,  NULL,         NULL},
    {"table-param",    "test-table", {.i = 1111},      PARAM_INT,    false, true,  NULL,         NULL},
    {"flag-param",     "test-table", {.i = 2222},      PARAM_INT,    false, true,  "long-flag",  "s"},
};
// clang-format on
#define TEST_ENTRIES_COUNT (sizeof(test_entries) / sizeof(test_entries[0]))

static biosim_params_t p;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_init(&p, test_entries, TEST_ENTRIES_COUNT));
}

void tearDown(void) {
    biosim_params_free(&p);
}

/* ── pass 1: defaults ──────────────────────────────────────────────────── */

void test_defaults(void) {
    TEST_ASSERT_EQUAL_STRING("default", biosim_params_get_string(&p, "toplevel-param"));
    TEST_ASSERT_EQUAL_INT(1111, biosim_params_get_int(&p, "table-param"));
    TEST_ASSERT_EQUAL_INT(2222, biosim_params_get_int(&p, "flag-param"));
}

/* ── pass 2: toml file only ────────────────────────────────────────────── */

void test_toml_file_sets_values(void) {
    char *argv[] = {"test-prog", "--config", TEST_FIXTURES_DIR "/basic.toml", NULL};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_parse(&p, "test-prog", "test-version", 3, argv));
    TEST_ASSERT_EQUAL_STRING("from-toml", biosim_params_get_string(&p, "toplevel-param"));
    TEST_ASSERT_EQUAL_INT(3333, biosim_params_get_int(&p, "table-param"));
}

/* ── pass 3: cli flags only ────────────────────────────────────────────── */

void test_cli_sets_values(void) {
    char *argv[] = {
        "test-prog", "--toplevel-param", "from-cli", "--test-table-table-param", "4444", NULL
    };
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_parse(&p, "test-prog", "test-version", 5, argv));
    TEST_ASSERT_EQUAL_STRING("from-cli", biosim_params_get_string(&p, "toplevel-param"));
    TEST_ASSERT_EQUAL_INT(4444, biosim_params_get_int(&p, "table-param"));
}

/* ── precedence: cli overrides TOML ────────────────────────────────────── */

void test_cli_overrides_toml(void) {
    char *argv[] = {
        "test-prog",
        "--config",
        TEST_FIXTURES_DIR "/basic.toml", // NOLINT(bugprone-suspicious-missing-comma)
        "--toplevel-param",
        "from-cli",
        "--test-table-table-param",
        "5555",
        NULL
    };
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_parse(&p, "test-prog", "test-version", 7, argv));
    TEST_ASSERT_EQUAL_STRING("from-cli", biosim_params_get_string(&p, "toplevel-param"));
    TEST_ASSERT_EQUAL_INT(5555, biosim_params_get_int(&p, "table-param"));
}

/* ── short flag ────────────────────────────────────────────────────────── */

void test_cli_short_flag(void) {
    char *argv[] = {"test-prog", "-s", "6666", NULL};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_parse(&p, "test-prog", "test-version", 3, argv));
    TEST_ASSERT_EQUAL_INT(6666, biosim_params_get_int(&p, "flag-param"));
}

void test_cli_long_flag(void) {
    char *argv[] = {"test-prog", "--long-flag", "7777", NULL};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_params_parse(&p, "test-prog", "test-version", 3, argv));
    TEST_ASSERT_EQUAL_INT(7777, biosim_params_get_int(&p, "flag-param"));
}

/* ── error: bad toml file ───────────────────────────────────────────────── */

void test_bad_path_returns_notfound(void) {
    char *argv[] = {"test-prog", "--config", TEST_FIXTURES_DIR "/nonexistent/path.toml", NULL};
    biosim_status_t st = biosim_params_parse(&p, "test-prog", "test-version", 3, argv);
    TEST_ASSERT_EQUAL(BIOSIM_ERR_INVALID, st);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_toml_file_sets_values);
    RUN_TEST(test_cli_sets_values);
    RUN_TEST(test_cli_overrides_toml);
    RUN_TEST(test_cli_short_flag);
    RUN_TEST(test_cli_long_flag);
    RUN_TEST(test_bad_path_returns_notfound);
    return UNITY_END();
}
