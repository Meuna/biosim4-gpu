#include "biosim/core/hud.h"

#include "biosim/core/terminal.h"

/* ── rendering constants ────────────────────────────────────────────────── */

/* Growing-braille spinner: an orbiting blob, advanced one frame per update. */
static const char *const spinner_frames[] = {
    "\xe2\xa3\xbe", /* ⣾ */
    "\xe2\xa3\xbd", /* ⣽ */
    "\xe2\xa3\xbb", /* ⣻ */
    "\xe2\xa2\xbf", /* ⢿ */
    "\xe2\xa1\xbf", /* ⡿ */
    "\xe2\xa3\x9f", /* ⣟ */
    "\xe2\xa3\xaf", /* ⣯ */
    "\xe2\xa3\xb7", /* ⣷ */
};
#define SPINNER_FRAMES 8U

/* ASCII fallback spinner for terminals that cannot render UTF-8. */
static const char *const spinner_frames_ascii[] = {"|", "/", "-", "\\"};
#define SPINNER_FRAMES_ASCII 4U

#define BAR_WIDTH 20U
/* UTF-8 block glyphs, with single-byte ASCII fallbacks. */
#define BAR_FILLED       "\xe2\x96\x88" /* █ */
#define BAR_EMPTY        "\xe2\x96\x91" /* ░ */
#define BAR_FILLED_ASCII "#"
#define BAR_EMPTY_ASCII  "-"
/* The HUD occupies 3 lines; CURSOR_UP_3 rewinds to the top of the block. */
#define CURSOR_UP_3 "\033[3A"
#define CLEAR_LINE  "\r\033[K"

/* ANSI colors, applied only when hud->color. Same convention as log.c. */
#define ANSI_SPINNER "\033[0;36m" /* cyan   */
#define ANSI_BAR     "\033[0;32m" /* green  */
#define ANSI_MAX     "\033[0;33m" /* yellow */
#define ANSI_RESET   "\033[0m"

/* ── helpers ────────────────────────────────────────────────────────────── */

static float clamp_unit(float v) {
    if (v < 0.0F) {
        return 0.0F;
    }
    if (v > 1.0F) {
        return 1.0F;
    }
    return v;
}

static float ratio(uint32_t num, uint32_t den) {
    return den > 0U ? (float)num / (float)den : 0.0F;
}

/* Append cell (a NUL-terminated 1- or 3-byte glyph) into buf at *pos. */
static void append_cell(char *buf, size_t *pos, const char *cell) {
    for (size_t i = 0U; cell[i] != '\0'; i++) {
        buf[(*pos)++] = cell[i];
    }
}

/* Write a [████░░░░] style bar of BAR_WIDTH cells into buf (NUL-terminated).
 * Each cell is at most a 3-byte UTF-8 glyph; buf must hold BAR_WIDTH * 3 + 1. */
static void format_bar(char *buf, float fraction, bool unicode) {
    uint32_t filled = (uint32_t)(clamp_unit(fraction) * (float)BAR_WIDTH + 0.5F);
    const char *fill = unicode ? BAR_FILLED : BAR_FILLED_ASCII;
    const char *empty = unicode ? BAR_EMPTY : BAR_EMPTY_ASCII;
    size_t pos = 0U;
    for (uint32_t i = 0U; i < BAR_WIDTH; i++) {
        append_cell(buf, &pos, (i < filled) ? fill : empty);
    }
    buf[pos] = '\0';
}

/* Current spinner glyph for the active charset. */
static const char *spinner_glyph(const biosim_hud_t *hud) {
    if (hud->unicode) {
        return spinner_frames[hud->spinner_index];
    }
    return spinner_frames_ascii[hud->spinner_index % SPINNER_FRAMES_ASCII];
}

/* Return code when hud->color, else "" — lets one format string serve both. */
static const char *col(const biosim_hud_t *hud, const char *code) {
    return hud->color ? code : "";
}

static void render_tty(biosim_hud_t *hud, const biosim_census_t *c) {
    float survival_rate = ratio(c->survivors, c->population);
    float kill_rate = ratio(c->kills, c->population);
    uint32_t completed = c->gen + 1U;
    float progress = ratio(completed, hud->max_generations);

    char bar[(BAR_WIDTH * 3U) + 1U];
    format_bar(bar, progress, hud->unicode);
    const char *reset = col(hud, ANSI_RESET);

    if (hud->started) {
        (void)fputs(CURSOR_UP_3, hud->stream);
    }
    hud->started = true;

    (void)fprintf(
        hud->stream,
        CLEAR_LINE "%s%s%s Generation %u / %u\n",
        col(hud, ANSI_SPINNER),
        spinner_glyph(hud),
        reset,
        completed,
        hud->max_generations
    );
    (void)fprintf(
        hud->stream,
        CLEAR_LINE "  [%s%s%s] %.0f%%\n",
        col(hud, ANSI_BAR),
        bar,
        reset,
        (double)(clamp_unit(progress) * 100.0F)
    );
    (void)fprintf(
        hud->stream,
        CLEAR_LINE "  surv %.1f%% (%u/%u)   max %s%.1f%%%s   kills %.1f%% (%u)\n",
        (double)(survival_rate * 100.0F),
        c->survivors,
        c->population,
        col(hud, ANSI_MAX),
        (double)(hud->max_survival_rate * 100.0F),
        reset,
        (double)(kill_rate * 100.0F),
        c->kills
    );
    (void)fflush(hud->stream);

    hud->spinner_index = (uint8_t)((hud->spinner_index + 1U) % SPINNER_FRAMES);
}

/* ── public API ─────────────────────────────────────────────────────────── */

void biosim_hud_init(biosim_hud_t *hud, FILE *stream, uint32_t max_generations) {
    biosim_term_caps_t caps = biosim_term_detect(stream);
    hud->stream = stream;
    hud->max_generations = max_generations;
    hud->is_tty = caps.is_tty;
    hud->unicode = caps.unicode;
    /* The HUD interleaves color with glyphs, so its top tier needs both. */
    hud->color = caps.color && caps.unicode;
    hud->started = false;
    hud->spinner_index = 0U;
    hud->max_survival_rate = 0.0F;

    if (!hud->is_tty) {
        biosim_census_print_header(stream);
    }
}

void biosim_hud_update(biosim_hud_t *hud, const biosim_census_t *c) {
    float survival_rate = ratio(c->survivors, c->population);
    if (survival_rate > hud->max_survival_rate) {
        hud->max_survival_rate = survival_rate;
    }

    if (hud->is_tty) {
        render_tty(hud, c);
    } else {
        biosim_census_print(hud->stream, c);
    }
}

void biosim_hud_finish(biosim_hud_t *hud) {
    if (hud->is_tty && hud->started) {
        (void)fflush(hud->stream);
    }
}
