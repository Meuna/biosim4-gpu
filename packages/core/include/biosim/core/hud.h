/*
 * HOST-ONLY: includes biosim/core/census.h and <stdio.h>.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_HUD_H
#define BIOSIM_CORE_HUD_H

#include "biosim/core/census.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Per-generation telemetry presenter, shared by sim-ref and sim-gpu.
 *
 * When the output stream is a TTY, the HUD draws a 3-line block (spinner,
 * progress bar, and survival/max/kill rates) and redraws it in place on every
 * update. When the stream is not a TTY (pipe, file, CI), it falls back to the
 * legacy one-line-per-generation census output, preceded by a column header.
 *
 * stream            — destination stream (typically stdout)
 * max_generations   — denominator for the progress bar / counter
 * is_tty            — chosen output mode, detected at init
 * unicode           — emit UTF-8 glyphs (braille spinner, block bar) vs ASCII
 * color             — wrap the HUD in ANSI color (implies unicode)
 * started           — whether the TTY block has been drawn at least once
 * spinner_index     — current spinner frame, advances each update
 * max_survival_rate — running maximum survival rate (0..1) across generations
 */
typedef struct biosim_hud {
    FILE *stream;
    uint32_t max_generations;
    bool is_tty;
    bool unicode;
    bool color;
    bool started;
    uint8_t spinner_index;
    float max_survival_rate;
} biosim_hud_t;

/*
 * Initialise the HUD for stream. Detects the terminal capabilities (TTY,
 * Unicode, color) and selects the rendering mode. In non-TTY mode, prints the
 * census column header.
 */
void biosim_hud_init(biosim_hud_t *hud, FILE *stream, uint32_t max_generations);

/*
 * Present one generation's census. In TTY mode, redraws the 3-line HUD in
 * place; in non-TTY mode, prints one census data row.
 */
void biosim_hud_update(biosim_hud_t *hud, const biosim_census_t *c);

/*
 * Finish the HUD. In TTY mode, moves the cursor below the block so subsequent
 * output starts on a clean line. In non-TTY mode, this is a no-op.
 */
void biosim_hud_finish(biosim_hud_t *hud);

/*
 * Lightweight progress widget: a single-line spinner + progress bar, with no
 * census statistics. Suitable for any "completed / total" loop (e.g. the GPU
 * benchmark, which runs the pipeline without per-generation census data).
 * Shares the HUD's spinner/bar rendering and terminal-capability detection.
 *
 * stream        — destination stream (typically stdout)
 * total         — denominator for the progress bar / counter
 * is_tty        — chosen output mode, detected at init (non-TTY stays silent)
 * unicode       — emit UTF-8 glyphs (braille spinner, block bar) vs ASCII
 * color         — wrap the line in ANSI color (implies unicode)
 * started       — whether at least one frame has been drawn
 * spinner_index — current spinner frame, advances each update
 */
typedef struct biosim_progress {
    FILE *stream;
    uint32_t total;
    bool is_tty;
    bool unicode;
    bool color;
    bool started;
    uint8_t spinner_index;
} biosim_progress_t;

/*
 * Initialise the progress widget for stream. Detects terminal capabilities and
 * selects the rendering mode (in non-TTY mode, updates are silent).
 */
void biosim_progress_init(biosim_progress_t *pg, FILE *stream, uint32_t total);

/*
 * Present progress as `completed` of `total`. In TTY mode, redraws the single
 * line in place; in non-TTY mode, this is a no-op.
 */
void biosim_progress_update(biosim_progress_t *pg, uint32_t completed);

/*
 * Finish the progress widget. In TTY mode, emits a trailing newline so
 * subsequent output starts on a clean line. In non-TTY mode, this is a no-op.
 */
void biosim_progress_finish(biosim_progress_t *pg);

#endif /* BIOSIM_CORE_HUD_H */
