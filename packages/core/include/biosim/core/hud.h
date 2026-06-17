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
 * started           — whether the TTY block has been drawn at least once
 * spinner_index     — current spinner frame, advances each update
 * max_survival_rate — running maximum survival rate (0..1) across generations
 */
typedef struct biosim_hud {
    FILE *stream;
    uint32_t max_generations;
    bool is_tty;
    bool started;
    uint8_t spinner_index;
    float max_survival_rate;
} biosim_hud_t;

/*
 * Initialise the HUD for stream. Detects whether stream is a TTY and selects
 * the rendering mode. In non-TTY mode, prints the census column header.
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

#endif /* BIOSIM_CORE_HUD_H */
