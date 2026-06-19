/*
 * HOST-ONLY: uses FILE* and queries the OS console/locale.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_TERMINAL_H
#define BIOSIM_CORE_TERMINAL_H

#include <stdbool.h>
#include <stdio.h>

/*
 * Progressive terminal capabilities, used to pick the richest output a terminal
 * can render. Three tiers: plain ASCII, Unicode glyphs, and Unicode + color.
 *
 * is_tty  — stream is connected to an interactive terminal
 * unicode — safe to emit UTF-8 glyphs (em-dash, braille spinner, block bars)
 * color   — safe to emit ANSI color escapes (implies unicode)
 */
typedef struct {
    bool is_tty;
    bool unicode;
    bool color;
} biosim_term_caps_t;

/*
 * One-time, process-global console setup. On Windows, switches the console
 * output codepage to UTF-8 and enables virtual-terminal processing so UTF-8
 * glyphs and ANSI escapes render correctly. On POSIX this is a no-op. Call once
 * at the very start of main(), before any output and before argument parsing.
 */
void biosim_term_init(void);

/*
 * Compute capabilities given whether the target stream is a terminal. Honors
 * the locale (POSIX) / console codepage (Windows) for unicode, and the NO_COLOR
 * and TERM=dumb conventions for color. Exposed for testing without a real TTY.
 */
biosim_term_caps_t biosim_term_caps_for(bool is_tty);

/* Detect capabilities for stream (convenience wrapper over biosim_term_caps_for). */
biosim_term_caps_t biosim_term_detect(FILE *stream);

#endif /* BIOSIM_CORE_TERMINAL_H */
