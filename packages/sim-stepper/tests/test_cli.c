#include "biosim/core/params.h"
#include "biosim/stepper/cli.h"
#include "biosim/stepper/toml.h"
#include "unity.h"

static const biosim_build_info_t stub_info = {
    .progname = "biosim-stepper",
    .version = "test",
    .build_timestamp = "1970-01-01T00:00:00Z",
    .build_type = "Test",
};

static biosim_params_t p;

void setUp(void) {
    biosim_params_init(&p);
}

void tearDown(void) {
    biosim_params_free(&p);
}

/* ── Pass 1: defaults ──────────────────────────────────────────────────── */

void test_defaults(void) {
    static const biosim_param_entry_t extra[] = {
        {"someparam", {.s = "unset"}, {.s = "unset"}, PARAM_STRING, false},
    };
    biosim_params_extend(&p, extra, 1);
    TEST_ASSERT_EQUAL_STRING("unset", biosim_params_get_string(&p, "someparam"));
    TEST_ASSERT_EQUAL_INT(3000, biosim_params_get_int(&p, "population"));
}

/* ── Pass 2: TOML file ─────────────────────────────────────────────────── */

void test_toml_file_sets_values(void) {
    static const biosim_param_entry_t extra[] = {
        {"someparam", {.s = "unset"}, {.s = "unset"}, PARAM_STRING, false},
    };
    biosim_params_extend(&p, extra, 1);

    stepper_load_toml_file(&p, TEST_FIXTURES_DIR "/basic.toml");
    TEST_ASSERT_EQUAL_STRING("from-toml", biosim_params_get_string(&p, "someparam"));
    TEST_ASSERT_EQUAL_INT(42, biosim_params_get_int(&p, "population"));
}

/* ── Pass 3: CLI flags ─────────────────────────────────────────────────── */

void test_cli_sets_values(void) {
    static const biosim_param_entry_t extra[] = {
        {"someparam", {.s = "unset"}, {.s = "unset"}, PARAM_STRING, false},
    };
    biosim_params_extend(&p, extra, 1);
    char *argv[] = {"biosim-stepper", "--someparam", "from-cli", "--population", "9999", NULL};
    stepper_cli_and_toml(&p, &stub_info, 5, argv);
    TEST_ASSERT_EQUAL_STRING("from-cli", biosim_params_get_string(&p, "someparam"));
    TEST_ASSERT_EQUAL_INT(9999, biosim_params_get_int(&p, "population"));
}

/* ── Precedence: CLI overrides TOML ────────────────────────────────────── */

void test_cli_overrides_toml(void) {
    static const biosim_param_entry_t extra[] = {
        {"someparam", {.s = "unset"}, {.s = "unset"}, PARAM_STRING, false},
    };
    biosim_params_extend(&p, extra, 1);

    char *argv[] = {"biosim-stepper",
                    "--config",
                    TEST_FIXTURES_DIR "/basic.toml", // NOLINT(bugprone-suspicious-missing-comma)
                    "--someparam",
                    "from-cli",
                    NULL};
    stepper_cli_and_toml(&p, &stub_info, 5, argv);
    TEST_ASSERT_EQUAL_STRING("from-cli", biosim_params_get_string(&p, "someparam"));
    TEST_ASSERT_EQUAL_INT(42, biosim_params_get_int(&p, "population"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_toml_file_sets_values);
    RUN_TEST(test_cli_sets_values);
    RUN_TEST(test_cli_overrides_toml);
    return UNITY_END();
}
