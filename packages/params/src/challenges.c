#include "biosim/params/challenges.h"

#include <string.h>

/* ── kind parsing ───────────────────────────────────────────────────────── */

static biosim_status_t parse_kind(const char *s, biosim_challenge_kind_t *out) {
    if (strcmp(s, "x_band") == 0) {
        *out = BIOSIM_CHALLENGE_X_BAND;
        return BIOSIM_OK;
    }
    if (strcmp(s, "disc") == 0) {
        *out = BIOSIM_CHALLENGE_DISC;
        return BIOSIM_OK;
    }
    if (strcmp(s, "corners") == 0) {
        *out = BIOSIM_CHALLENGE_CORNERS;
        return BIOSIM_OK;
    }
    if (strcmp(s, "neighbor_count") == 0) {
        *out = BIOSIM_CHALLENGE_NEIGHBOR_COUNT;
        return BIOSIM_OK;
    }
    if (strcmp(s, "center_sparse") == 0) {
        *out = BIOSIM_CHALLENGE_CENTER_SPARSE;
        return BIOSIM_OK;
    }
    if (strcmp(s, "against_wall") == 0) {
        *out = BIOSIM_CHALLENGE_AGAINST_WALL;
        return BIOSIM_OK;
    }
    if (strcmp(s, "migrate_distance") == 0) {
        *out = BIOSIM_CHALLENGE_MIGRATE_DISTANCE;
        return BIOSIM_OK;
    }
    if (strcmp(s, "touch_any_wall") == 0) {
        *out = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
        return BIOSIM_OK;
    }
    if (strcmp(s, "radioactive_walls") == 0) {
        *out = BIOSIM_CHALLENGE_RADIOACTIVE_WALLS;
        return BIOSIM_OK;
    }
    if (strcmp(s, "pairs") == 0) {
        *out = BIOSIM_CHALLENGE_PAIRS;
        return BIOSIM_OK;
    }
    if (strcmp(s, "location_sequence") == 0) {
        *out = BIOSIM_CHALLENGE_LOCATION_SEQUENCE;
        return BIOSIM_OK;
    }
    if (strcmp(s, "near_barrier") == 0) {
        *out = BIOSIM_CHALLENGE_NEAR_BARRIER;
        return BIOSIM_OK;
    }
    if (strcmp(s, "altruism") == 0) {
        *out = BIOSIM_CHALLENGE_ALTRUISM;
        return BIOSIM_OK;
    }
    return BIOSIM_ERR_INVALID;
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_status_t biosim_challenge_spec_from_params(const biosim_params_t *p,
                                                  biosim_challenge_spec_t *out) {
    const biosim_param_entry_t *kind_entry = biosim_params_find(p, "kind");
    if (kind_entry == NULL) {
        return BIOSIM_ERR_NOTFOUND;
    }

    biosim_challenge_kind_t kind;
    biosim_status_t st = parse_kind(kind_entry->value.s, &kind);
    if (st != BIOSIM_OK) {
        return st;
    }

    out->kind = kind;

    switch (kind) {
    case BIOSIM_CHALLENGE_X_BAND:
        out->x_band.x_min = (float)biosim_params_get_float(p, "x-min");
        out->x_band.x_max = (float)biosim_params_get_float(p, "x-max");
        out->x_band.mirror = biosim_params_get_bool(p, "mirror");
        break;

    default:
        break;
    }

    return BIOSIM_OK;
}
