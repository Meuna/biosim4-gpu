/* setenv/unsetenv are POSIX; enable them before any system header. */
#ifndef _WIN32
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#define _POSIX_C_SOURCE 200809L
#endif

#include "biosim/core/terminal.h"
#include "unity.h"

#include <stdlib.h>

#ifndef _WIN32
/* Restore the locale/color environment to a known UTF-8, color-capable state. */
static void reset_env(void) {
    (void)setenv("LC_ALL", "en_US.UTF-8", 1);
    (void)unsetenv("LC_CTYPE");
    (void)unsetenv("LANG");
    (void)unsetenv("NO_COLOR");
    (void)setenv("TERM", "xterm-256color", 1);
}
#endif

void setUp(void) {
#ifndef _WIN32
    reset_env();
#endif
}

void tearDown(void) {
#ifndef _WIN32
    reset_env();
#endif
}

/* ── caps gating ────────────────────────────────────────────────────────── */

/* A non-TTY never advertises unicode or color, on any platform. */
void test_non_tty_has_no_caps(void) {
    biosim_term_caps_t caps = biosim_term_caps_for(false);
    TEST_ASSERT_FALSE(caps.is_tty);
    TEST_ASSERT_FALSE(caps.unicode);
    TEST_ASSERT_FALSE(caps.color);
}

#ifndef _WIN32

void test_utf8_locale_enables_unicode(void) {
    (void)setenv("LC_ALL", "en_US.UTF-8", 1);
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_TRUE(caps.unicode);
}

void test_non_utf8_locale_disables_unicode_only(void) {
    (void)setenv("LC_ALL", "C", 1);
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_FALSE(caps.unicode);
    TEST_ASSERT_TRUE(caps.color); /* color is independent of unicode */
}

void test_lang_fallback_when_lc_unset(void) {
    (void)unsetenv("LC_ALL");
    (void)unsetenv("LC_CTYPE");
    (void)setenv("LANG", "fr_FR.utf8", 1);
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_TRUE(caps.unicode);
}

void test_no_color_disables_color_only(void) {
    (void)setenv("NO_COLOR", "1", 1);
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_TRUE(caps.unicode);
    TEST_ASSERT_FALSE(caps.color);
}

void test_term_dumb_disables_color_only(void) {
    (void)setenv("TERM", "dumb", 1);
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_TRUE(caps.unicode);
    TEST_ASSERT_FALSE(caps.color);
}

void test_utf8_and_color_enabled(void) {
    biosim_term_caps_t caps = biosim_term_caps_for(true);
    TEST_ASSERT_TRUE(caps.is_tty);
    TEST_ASSERT_TRUE(caps.unicode);
    TEST_ASSERT_TRUE(caps.color);
}

#endif /* !_WIN32 */

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_non_tty_has_no_caps);
#ifndef _WIN32
    RUN_TEST(test_utf8_locale_enables_unicode);
    RUN_TEST(test_non_utf8_locale_disables_unicode_only);
    RUN_TEST(test_lang_fallback_when_lc_unset);
    RUN_TEST(test_no_color_disables_color_only);
    RUN_TEST(test_term_dumb_disables_color_only);
    RUN_TEST(test_utf8_and_color_enabled);
#endif
    return UNITY_END();
}
