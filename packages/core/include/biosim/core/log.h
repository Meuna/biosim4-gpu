/*
 * HOST-ONLY: uses FILE* and errno. Do NOT include from OpenCL kernel sources.
 */
#ifndef BIOSIM_CORE_LOG_H
#define BIOSIM_CORE_LOG_H

#include "biosim/core/status.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/* Compile-time level gate. Override per preset:
 *   -DBIOSIM_LOG_MAX_LEVEL=3   (strips TRACE and DEBUG from release/ci) */
#ifndef BIOSIM_LOG_MAX_LEVEL
#define BIOSIM_LOG_MAX_LEVEL 5
#endif

typedef enum {
    BIOSIM_LOG_OFF = 0,
    BIOSIM_LOG_ERROR = 1,
    BIOSIM_LOG_WARN = 2,
    BIOSIM_LOG_INFO = 3,
    BIOSIM_LOG_DEBUG = 4,
    BIOSIM_LOG_TRACE = 5
} biosim_log_level_t;

typedef struct {
    biosim_log_level_t threshold; /* runtime cutoff; set from CLI -v/-vv */
    FILE *sink;                   /* NULL → stderr */
    int use_color;                /* set by biosim_log_init via terminal detection */
} biosim_log_ctx_t;

extern biosim_log_ctx_t biosim_log_default_ctx;

/* True when stream is connected to an interactive terminal. Portable wrapper
 * over POSIX isatty / Windows _isatty. */
bool biosim_isatty(FILE *stream);

void biosim_log_init(biosim_log_ctx_t *ctx);

void biosim_log_emit(
    const biosim_log_ctx_t *ctx,
    biosim_log_level_t level,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 6, 7)))
#endif
    ;

_Noreturn void biosim_die(
    const biosim_log_ctx_t *ctx,
    biosim_status_t code,
    int saved_errno,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...
)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 7, 8)))
#endif
    ;

/* Short-circuits before evaluating variadic args when the level is filtered. */
#define BIOSIM_LOG_TO(ctx, lvl, ...)                                                               \
    do {                                                                                           \
        if ((int)(lvl) <= BIOSIM_LOG_MAX_LEVEL && (ctx) != NULL &&                                 \
            (int)(lvl) <= (int)(ctx)->threshold) {                                                 \
            biosim_log_emit((ctx), (lvl), __FILE__, __LINE__, __func__, __VA_ARGS__);              \
        }                                                                                          \
    } while (0)

/* Convenience macros for the default context. */
#define BIOSIM_LOG(lvl, ...) BIOSIM_LOG_TO(&biosim_log_default_ctx, (lvl), __VA_ARGS__)
#define BIOSIM_ERRORF(...)   BIOSIM_LOG(BIOSIM_LOG_ERROR, __VA_ARGS__)
#define BIOSIM_WARNF(...)    BIOSIM_LOG(BIOSIM_LOG_WARN, __VA_ARGS__)
#define BIOSIM_INFOF(...)    BIOSIM_LOG(BIOSIM_LOG_INFO, __VA_ARGS__)
#define BIOSIM_DEBUGF(...)   BIOSIM_LOG(BIOSIM_LOG_DEBUG, __VA_ARGS__)
#define BIOSIM_TRACEF(...)   BIOSIM_LOG(BIOSIM_LOG_TRACE, __VA_ARGS__)

/* errno is captured at the macro call site before any internal call can
 * clobber it. */
#define BIOSIM_DIE(ctx, code, ...)                                                                 \
    do {                                                                                           \
        int biosim_saved_errno_ = errno;                                                           \
        biosim_die((ctx), (code), biosim_saved_errno_, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

#endif /* BIOSIM_CORE_LOG_H */
