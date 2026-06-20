#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/sim-gpu/info.h"
#include "biosim/sim-gpu/runner.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

// clang-format off
static const biosim_param_entry_t test_entries[] = {
    {"max-generations", "simulation", {.i = 5},    PARAM_INT, false, true, "max-gen",     NULL},
    {"population",      "simulation", {.i = 1024},  PARAM_INT, false, true, "pop",         "p"},
    {"max-genes",       "genome",     {.i = 16},    PARAM_INT, false, true, "max-genes",   NULL},
    {"max-neurons",     "genome",     {.i = 8},     PARAM_INT, false, true, "max-neurons", NULL},
};
// clang-format on
#define TEST_ENTRIES_COUNT (sizeof(test_entries) / sizeof(test_entries[0]))

static biosim_params_t params;
static biosim_gpu_runner_t runner;
static biosim_status_t fixture_status;

void setUp(void) {
    biosim_log_init(&biosim_log_default_ctx);
    memset(&runner, 0, sizeof(runner));

    fixture_status = biosim_params_init(&params, test_entries, TEST_ENTRIES_COUNT);
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    cl_uint n = 0U;
    if (clGetPlatformIDs(0U, NULL, &n) != CL_SUCCESS || n == 0U) {
        fixture_status = BIOSIM_ERR_OPENCL;
        return;
    }
    fixture_status = biosim_gpu_runner_create(0U, 0U, false, &runner);
}

void tearDown(void) {
    biosim_gpu_runner_free(&runner);
    biosim_params_free(&params);
}

/* Read the whole content of f into out (NUL-terminated). */
static void capture(FILE *f, char *out, size_t cap) {
    (void)fflush(f);
    TEST_ASSERT_EQUAL_INT(0, fseek(f, 0L, SEEK_SET));
    size_t n = fread(out, 1U, cap - 1U, f);
    out[n] = '\0';
}

/* Default scope: version line, platform/device, key perf params. */
void test_info_default_scope(void) {
    if (fixture_status != BIOSIM_OK) {
        TEST_IGNORE_MESSAGE("no OpenCL platform");
    }

    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, biosim_gpu_info_print(&params, &runner, "v1.2.3-test", false, f)
    );

    char buf[4096];
    capture(f, buf, sizeof(buf));
    (void)fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buf, "v1.2.3-test"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "platform:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "device:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "max-generations=5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "population=1024"));
    /* Default scope does not include the full parameter listing. */
    TEST_ASSERT_NULL(strstr(buf, "all parameters:"));
}

/* Verbose scope additionally dumps every parameter. */
void test_info_verbose_scope_lists_all_params(void) {
    if (fixture_status != BIOSIM_OK) {
        TEST_IGNORE_MESSAGE("no OpenCL platform");
    }

    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, biosim_gpu_info_print(&params, &runner, "v1.2.3-test", true, f)
    );

    char buf[4096];
    capture(f, buf, sizeof(buf));
    (void)fclose(f);

    TEST_ASSERT_NOT_NULL(strstr(buf, "all parameters:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "[genome] max-neurons = 8"));
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_info_default_scope);
    RUN_TEST(test_info_verbose_scope_lists_all_params);
    return UNITY_END();
}
