#include <string.h>
#include "unity.h"
#include "biosim/core/params.h"
#include "biosim/stepper/cli.h"
#include "biosim/stepper/toml.h"

static biosim_params_t p;

void setUp(void) {
    biosim_params_init(&p);
}

void tearDown(void) {
    biosim_params_free(&p);
}

/* ── Pass 1: defaults ──────────────────────────────────────────────────── */

void test_defaults_population(void) {
    /* extend manually to inspect stepper params without going through resolve */
    static const biosim_param_entry_t extra[] = {
        {"sim-name", PARAM_STRING, {.s = "unnamed"}, {.s = "unnamed"}, false},
    };
    biosim_params_extend(&p, extra, 1);
    TEST_ASSERT_EQUAL_STRING("unnamed", biosim_params_get_string(&p, "sim-name"));
    TEST_ASSERT_EQUAL_INT(3000, biosim_params_get_int(&p, "population"));
}

/* ── Pass 2: TOML string ───────────────────────────────────────────────── */

void test_toml_str_sets_sim_name(void) {
    static const biosim_param_entry_t extra[] = {
        {"sim-name", PARAM_STRING, {.s = "unnamed"}, {.s = "unnamed"}, false},
    };
    biosim_params_extend(&p, extra, 1);

    const char *src = "sim-name = \"from-toml\"\n";
    stepper_load_toml_str(&p, src, (int)strlen(src));
    TEST_ASSERT_EQUAL_STRING("from-toml", biosim_params_get_string(&p, "sim-name"));
}

void test_toml_str_sets_population(void) {
    const char *src = "population = 99\n";
    stepper_load_toml_str(&p, src, (int)strlen(src));
    TEST_ASSERT_EQUAL_INT(99, biosim_params_get_int(&p, "population"));
}

void test_toml_str_unknown_key_is_ignored(void) {
    const char *src = "no-such-key = 1\n";
    biosim_status_t st = stepper_load_toml_str(&p, src, (int)strlen(src));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, st);
    TEST_ASSERT_EQUAL_INT(3000, biosim_params_get_int(&p, "population"));
}

/* ── Pass 2: TOML file ─────────────────────────────────────────────────── */

void test_toml_file_sets_sim_name(void) {
    static const biosim_param_entry_t extra[] = {
        {"sim-name", PARAM_STRING, {.s = "unnamed"}, {.s = "unnamed"}, false},
    };
    biosim_params_extend(&p, extra, 1);

    stepper_load_toml_file(&p, TEST_FIXTURES_DIR "/basic.toml");
    TEST_ASSERT_EQUAL_STRING("fixture-run", biosim_params_get_string(&p, "sim-name"));
    TEST_ASSERT_EQUAL_INT(42, biosim_params_get_int(&p, "population"));
}

/* ── Pass 3: CLI flags ─────────────────────────────────────────────────── */

void test_cli_sets_sim_name(void) {
    char *argv[] = {"biosim-stepper", "--sim-name", "cli-run", NULL};
    stepper_params_resolve(&p, 3, argv);
    TEST_ASSERT_EQUAL_STRING("cli-run", biosim_params_get_string(&p, "sim-name"));
}

void test_cli_sets_population(void) {
    char *argv[] = {"biosim-stepper", "--population", "9999", NULL};
    stepper_params_resolve(&p, 3, argv);
    TEST_ASSERT_EQUAL_INT(9999, biosim_params_get_int(&p, "population"));
}

/* ── Precedence: CLI overrides TOML ────────────────────────────────────── */

void test_cli_overrides_toml(void) {
    static const biosim_param_entry_t extra[] = {
        {"sim-name", PARAM_STRING, {.s = "unnamed"}, {.s = "unnamed"}, false},
    };
    biosim_params_extend(&p, extra, 1);

    /* Simulate pass 2 inline (TOML string) */
    const char *src = "sim-name = \"from-toml\"\n";
    stepper_load_toml_str(&p, src, (int)strlen(src));
    TEST_ASSERT_EQUAL_STRING("from-toml", biosim_params_get_string(&p, "sim-name"));

    /* Now pass 3 via set_string (CLI wins) */
    biosim_params_set_string(&p, "sim-name", "from-cli");
    TEST_ASSERT_EQUAL_STRING("from-cli", biosim_params_get_string(&p, "sim-name"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_population);
    RUN_TEST(test_toml_str_sets_sim_name);
    RUN_TEST(test_toml_str_sets_population);
    RUN_TEST(test_toml_str_unknown_key_is_ignored);
    RUN_TEST(test_toml_file_sets_sim_name);
    RUN_TEST(test_cli_sets_sim_name);
    RUN_TEST(test_cli_sets_population);
    RUN_TEST(test_cli_overrides_toml);
    return UNITY_END();
}
