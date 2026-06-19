/* isatty/fileno are POSIX; enable them on non-Windows before any system header */
#ifndef _WIN32
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#define _POSIX_C_SOURCE 200809L
#endif

#include "biosim/core/log.h"

#include "biosim/core/terminal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define BIOSIM_ISATTY(fd) _isatty(fd)
#define BIOSIM_FILENO(f)  _fileno(f)
#else
#include <unistd.h>
#define BIOSIM_ISATTY(fd) isatty(fd)
#define BIOSIM_FILENO(f)  fileno(f)
#endif

/* ── level metadata ─────────────────────────────────────────────────────────── */

static const char *level_labels[] = {
    "",      /* OFF   */
    "ERROR", /* 1 */
    "WARN ", /* 2 */
    "INFO ", /* 3 */
    "DEBUG", /* 4 */
    "TRACE", /* 5 */
};

static const char *level_colors[] = {
    "",           /* OFF   */
    "\033[0;31m", /* ERROR: red    */
    "\033[0;33m", /* WARN:  yellow */
    "\033[0;32m", /* INFO:  green  */
    "\033[0;36m", /* DEBUG: cyan   */
    "\033[0;37m", /* TRACE: white  */
};

#define COLOR_RESET "\033[0m"
#define LEVEL_MAX   5

/* ── public API ─────────────────────────────────────────────────────────────── */

biosim_log_ctx_t biosim_log_default_ctx = {
    .threshold = BIOSIM_LOG_WARN,
    .sink = NULL,
    .use_color = 0,
};

bool biosim_isatty(FILE *stream) {
    return BIOSIM_ISATTY(BIOSIM_FILENO(stream)) != 0;
}

void biosim_log_init(biosim_log_ctx_t *ctx) {
    ctx->threshold = BIOSIM_LOG_WARN;
    ctx->sink = NULL;
    ctx->use_color = biosim_term_detect(stderr).color;
}

void biosim_log_emit(
    const biosim_log_ctx_t *ctx,
    biosim_log_level_t level,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    FILE *sink = (ctx->sink != NULL) ? ctx->sink : stderr;
    int lvl = ((int)level >= 1 && (int)level <= LEVEL_MAX) ? (int)level : 0;
    const char *label = level_labels[lvl];

    if (ctx->use_color) {
        (void)fprintf(
            sink, "%s[%s]%s %s:%d %s: ", level_colors[lvl], label, COLOR_RESET, file, line, func
        );
    } else {
        (void)fprintf(sink, "[%s] %s:%d %s: ", label, file, line, func);
    }

    va_list ap;
    va_start(ap, fmt);
    (void)vfprintf(sink, fmt, ap); // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(ap);
    (void)fputc('\n', sink);
}

_Noreturn void biosim_die(
    const biosim_log_ctx_t *ctx,
    biosim_status_t code,
    int saved_errno,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
) {
    FILE *sink = (ctx != NULL && ctx->sink != NULL) ? ctx->sink : stderr;

    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(msg, sizeof msg, fmt, ap); // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(ap);

    if (saved_errno != 0) {
#ifdef _WIN32
        char errbuf[128];
        (void)strerror_s(errbuf, sizeof errbuf, saved_errno);
        (void)fprintf(
            sink,
            "[FATAL] %s:%d %s: %s [%s] (status=%d)\n",
            file,
            line,
            func,
            msg,
            errbuf,
            (int)code
        );
#else
        (void)fprintf(
            sink,
            "[FATAL] %s:%d %s: %s [%s] (status=%d)\n",
            file,
            line,
            func,
            msg,
            strerror(saved_errno),
            (int)code
        );
#endif
    } else {
        (void)fprintf(sink, "[FATAL] %s:%d %s: %s (status=%d)\n", file, line, func, msg, (int)code);
    }

    exit(EXIT_FAILURE);
}
