#include "biosim/core/census.h"
#include "biosim/core/hud.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {
}
void tearDown(void) {
}

/* Read the whole content of f into out (NUL-terminated). */
static void capture(FILE *f, char *out, size_t cap) {
    (void)fflush(f);
    rewind(f);
    size_t n = fread(out, 1U, cap - 1U, f);
    out[n] = '\0';
}

/* ── non-TTY mode (legacy line output) ──────────────────────────────────── */

void test_non_tty_matches_legacy_census_output(void) {
    biosim_census_t a = {.gen = 0U, .population = 100U, .survivors = 40U, .kills = 3U};
    biosim_census_t b = {.gen = 1U, .population = 100U, .survivors = 55U, .kills = 1U};

    FILE *got = tmpfile();
    TEST_ASSERT_NOT_NULL(got);
    biosim_hud_t hud;
    biosim_hud_init(&hud, got, 10U);
    TEST_ASSERT_FALSE(hud.is_tty); /* a regular file is never a TTY */
    biosim_hud_update(&hud, &a);
    biosim_hud_update(&hud, &b);
    biosim_hud_finish(&hud);

    FILE *want = tmpfile();
    TEST_ASSERT_NOT_NULL(want);
    biosim_census_print_header(want);
    biosim_census_print(want, &a);
    biosim_census_print(want, &b);

    char got_buf[1024];
    char want_buf[1024];
    capture(got, got_buf, sizeof(got_buf));
    capture(want, want_buf, sizeof(want_buf));
    TEST_ASSERT_EQUAL_STRING(want_buf, got_buf);

    (void)fclose(got);
    (void)fclose(want);
}

/* ── running max survival rate ──────────────────────────────────────────── */

void test_max_survival_rate_tracks_running_maximum(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    biosim_hud_t hud;
    biosim_hud_init(&hud, f, 10U);

    biosim_census_t c = {.gen = 0U, .population = 100U, .survivors = 40U, .kills = 0U};
    biosim_hud_update(&hud, &c);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.40F, hud.max_survival_rate);

    c.gen = 1U;
    c.survivors = 60U;
    biosim_hud_update(&hud, &c);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.60F, hud.max_survival_rate);

    c.gen = 2U;
    c.survivors = 30U; /* lower than the max: must hold */
    biosim_hud_update(&hud, &c);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.60F, hud.max_survival_rate);

    (void)fclose(f);
}

/* ── spinner advances and wraps ─────────────────────────────────────────── */

void test_spinner_index_wraps_modulo_eight(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    biosim_hud_t hud;
    biosim_hud_init(&hud, f, 100U);
    hud.is_tty = true; /* force the HUD-rendering path */

    biosim_census_t c = {.gen = 0U, .population = 100U, .survivors = 10U, .kills = 0U};
    TEST_ASSERT_EQUAL_UINT8(0U, hud.spinner_index);
    biosim_hud_update(&hud, &c);
    TEST_ASSERT_EQUAL_UINT8(1U, hud.spinner_index);

    for (uint32_t i = 0U; i < 7U; i++) {
        biosim_hud_update(&hud, &c);
    }
    TEST_ASSERT_EQUAL_UINT8(0U, hud.spinner_index); /* 8 advances wrap to 0 */

    (void)fclose(f);
}

/* ── zero population is safe ─────────────────────────────────────────────── */

void test_zero_population_does_not_divide_by_zero(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    biosim_hud_t hud;
    biosim_hud_init(&hud, f, 10U);
    hud.is_tty = true;

    biosim_census_t c = {.gen = 0U, .population = 0U, .survivors = 0U, .kills = 0U};
    biosim_hud_update(&hud, &c);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, hud.max_survival_rate);

    (void)fclose(f);
}

/* ── TTY redraw uses cursor-up only after the first frame ───────────────── */

void test_tty_redraw_emits_cursor_up_after_first_frame(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);
    biosim_hud_t hud;
    biosim_hud_init(&hud, f, 10U);
    hud.is_tty = true;

    biosim_census_t c = {.gen = 0U, .population = 100U, .survivors = 50U, .kills = 2U};
    char buf[2048];

    biosim_hud_update(&hud, &c);
    capture(f, buf, sizeof(buf));
    TEST_ASSERT_NULL(strstr(buf, "\033[3A")); /* first frame: no cursor-up */

    c.gen = 1U;
    biosim_hud_update(&hud, &c);
    capture(f, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\033[3A")); /* redraw moves the cursor up */

    (void)fclose(f);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_non_tty_matches_legacy_census_output);
    RUN_TEST(test_max_survival_rate_tracks_running_maximum);
    RUN_TEST(test_spinner_index_wraps_modulo_eight);
    RUN_TEST(test_zero_population_does_not_divide_by_zero);
    RUN_TEST(test_tty_redraw_emits_cursor_up_after_first_frame);
    return UNITY_END();
}
