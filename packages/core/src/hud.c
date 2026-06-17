#include "biosim/core/hud.h"

#include "biosim/core/log.h"

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

#define BAR_WIDTH    20U
#define BAR_FILLED   "\xe2\x96\x88" /* █ */
#define BAR_EMPTY    "\xe2\x96\x91" /* ░ */
#define HUD_LINES    3
#define CURSOR_UP_3  "\033[3A"
#define CLEAR_LINE   "\r\033[K"

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

/* Write a [████░░░░] style bar of BAR_WIDTH cells into buf (NUL-terminated).
 * Each cell is a 3-byte UTF-8 glyph; buf must hold BAR_WIDTH * 3 + 1 bytes. */
static void format_bar(char *buf, float fraction) {
    uint32_t filled = (uint32_t)(clamp_unit(fraction) * (float)BAR_WIDTH + 0.5F);
    size_t pos = 0U;
    for (uint32_t i = 0U; i < BAR_WIDTH; i++) {
        const char *cell = (i < filled) ? BAR_FILLED : BAR_EMPTY;
        buf[pos++] = cell[0];
        buf[pos++] = cell[1];
        buf[pos++] = cell[2];
    }
    buf[pos] = '\0';
}

static void render_tty(biosim_hud_t *hud, const biosim_census_t *c) {
    float survival_rate = ratio(c->survivors, c->population);
    float kill_rate = ratio(c->kills, c->population);
    uint32_t completed = c->gen + 1U;
    float progress = ratio(completed, hud->max_generations);

    char bar[(BAR_WIDTH * 3U) + 1U];
    format_bar(bar, progress);

    if (hud->started) {
        (void)fputs(CURSOR_UP_3, hud->stream);
    }
    hud->started = true;

    (void)fprintf(
        hud->stream,
        CLEAR_LINE "%s Generation %u / %u\n",
        spinner_frames[hud->spinner_index],
        completed,
        hud->max_generations
    );
    (void)fprintf(
        hud->stream, CLEAR_LINE "  [%s] %.0f%%\n", bar, (double)(clamp_unit(progress) * 100.0F)
    );
    (void)fprintf(
        hud->stream,
        CLEAR_LINE "  surv %.1f%% (%u/%u)   max %.1f%%   kills %.1f%% (%u)\n",
        (double)(survival_rate * 100.0F),
        c->survivors,
        c->population,
        (double)(hud->max_survival_rate * 100.0F),
        (double)(kill_rate * 100.0F),
        c->kills
    );
    (void)fflush(hud->stream);

    hud->spinner_index = (uint8_t)((hud->spinner_index + 1U) % SPINNER_FRAMES);
}

/* ── public API ─────────────────────────────────────────────────────────── */

void biosim_hud_init(biosim_hud_t *hud, FILE *stream, uint32_t max_generations) {
    hud->stream = stream;
    hud->max_generations = max_generations;
    hud->is_tty = biosim_isatty(stream);
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
