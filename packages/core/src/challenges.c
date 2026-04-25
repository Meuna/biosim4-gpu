#include "biosim/core/challenges.h"

/* ── x_band ─────────────────────────────────────────────────────────────── */

static bool in_x_band(int16_t x, int16_t size_x, float x_min, float x_max) {
    int16_t lo = (int16_t)(x_min * (float)size_x);
    int16_t hi = (int16_t)(x_max * (float)size_x);
    return x >= lo && x < hi;
}

static biosim_challenge_result_t eval_x_band(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                             int16_t size_x) {
    biosim_challenge_result_t r = {false, 0.0F};
    float x_min = spec->x_band.x_min;
    float x_max = spec->x_band.x_max;

    r.passed = in_x_band(loc_x, size_x, x_min, x_max);
    if (!r.passed && spec->x_band.mirror) {
        r.passed = in_x_band(loc_x, size_x, 1.0F - x_max, 1.0F - x_min);
    }
    if (r.passed) {
        r.score = 1.0F;
    }
    return r;
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_challenge_result_t biosim_challenge_eval(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                                int16_t loc_y, int16_t size_x, int16_t size_y,
                                                uint32_t challenge_bits) {
    (void)loc_y;
    (void)size_y;
    (void)challenge_bits;

    switch (spec->kind) {
    case BIOSIM_CHALLENGE_X_BAND:
        return eval_x_band(spec, loc_x, size_x);

    case BIOSIM_CHALLENGE_DISC:
    case BIOSIM_CHALLENGE_CORNERS:
    case BIOSIM_CHALLENGE_NEIGHBOR_COUNT:
    case BIOSIM_CHALLENGE_CENTER_SPARSE:
    case BIOSIM_CHALLENGE_AGAINST_WALL:
    case BIOSIM_CHALLENGE_MIGRATE_DISTANCE:
    case BIOSIM_CHALLENGE_TOUCH_ANY_WALL:
    case BIOSIM_CHALLENGE_RADIOACTIVE_WALLS:
    case BIOSIM_CHALLENGE_PAIRS:
    case BIOSIM_CHALLENGE_LOCATION_SEQUENCE:
    case BIOSIM_CHALLENGE_NEAR_BARRIER:
    case BIOSIM_CHALLENGE_ALTRUISM: {
        biosim_challenge_result_t stub = {false, 0.0F};
        return stub;
    }
    }

    biosim_challenge_result_t unreachable = {false, 0.0F};
    return unreachable;
}
