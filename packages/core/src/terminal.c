#include "biosim/core/terminal.h"

#include "biosim/core/log.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ── helpers ────────────────────────────────────────────────────────────── */

/* Case-insensitive search for "utf-8"/"utf8" in a locale string. */
static bool contains_utf8(const char *s) {
    for (; *s != '\0'; s++) {
        if ((*s == 'u' || *s == 'U') && (s[1] == 't' || s[1] == 'T') &&
            (s[2] == 'f' || s[2] == 'F')) {
            const char *tail = s + 3;
            if (*tail == '-') {
                tail++;
            }
            if (*tail == '8') {
                return true;
            }
        }
    }
    return false;
}

/* The active locale advertises UTF-8. On Windows, the console codepage (which
 * biosim_term_init switches to UTF-8) is authoritative instead of the locale. */
static bool locale_is_utf8(void) {
#ifdef _WIN32
    return GetConsoleOutputCP() == CP_UTF8;
#else
    const char *vars[] = {getenv("LC_ALL"), getenv("LC_CTYPE"), getenv("LANG")};
    for (size_t i = 0U; i < sizeof(vars) / sizeof(vars[0]); i++) {
        if (vars[i] != NULL && vars[i][0] != '\0') {
            return contains_utf8(vars[i]);
        }
    }
    return false;
#endif
}

/* Color is allowed unless NO_COLOR is set (non-empty) or TERM is "dumb". */
static bool color_allowed(void) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        return false;
    }
    const char *term = getenv("TERM");
    return term == NULL || strcmp(term, "dumb") != 0;
}

/* ── public API ─────────────────────────────────────────────────────────── */

void biosim_term_init(void) {
#ifdef _WIN32
    (void)SetConsoleOutputCP(CP_UTF8);
    HANDLE handles[] = {GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
    for (size_t i = 0U; i < sizeof(handles) / sizeof(handles[0]); i++) {
        DWORD mode = 0;
        if (handles[i] != INVALID_HANDLE_VALUE && GetConsoleMode(handles[i], &mode) != 0) {
            (void)SetConsoleMode(handles[i], mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

biosim_term_caps_t biosim_term_caps_for(bool is_tty) {
    biosim_term_caps_t caps = {.is_tty = is_tty, .unicode = false, .color = false};
    caps.unicode = is_tty && locale_is_utf8();
    caps.color = is_tty && color_allowed();
    return caps;
}

biosim_term_caps_t biosim_term_detect(FILE *stream) {
    return biosim_term_caps_for(biosim_isatty(stream));
}
