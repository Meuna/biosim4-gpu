#include "biosim/params/challenges.h"
#include "biosim/core/log.h"

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
    BIOSIM_ERRORF("invalid challenge kind '%s'. Accepted values are: x_band, disc, corners, "
                  "neighbor_count, center_sparse, against_wall, migrate_distance, touch_any_wall, "
                  "radioactive_walls, pairs, location_sequence, near_barrier, altruism",
                  s);
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

    case BIOSIM_CHALLENGE_DISC:
        out->disc.x = (float)biosim_params_get_float(p, "x");
        out->disc.y = (float)biosim_params_get_float(p, "y");
        out->disc.radius = (float)biosim_params_get_float(p, "radius");
        out->disc.weighted = biosim_params_get_bool(p, "weighted");
        break;

    case BIOSIM_CHALLENGE_CORNERS:
        out->corners.radius = (float)biosim_params_get_float(p, "radius");
        out->corners.weighted = biosim_params_get_bool(p, "weighted");
        break;

    case BIOSIM_CHALLENGE_NEIGHBOR_COUNT:
        out->neighbor_count.radius = (float)biosim_params_get_float(p, "radius");
        out->neighbor_count.min_n = (float)biosim_params_get_float(p, "min-n");
        out->neighbor_count.max_n = (float)biosim_params_get_float(p, "max-n");
        out->neighbor_count.exclude_border = biosim_params_get_bool(p, "exclude-border");
        break;

    case BIOSIM_CHALLENGE_CENTER_SPARSE:
        out->center_sparse.x = (float)biosim_params_get_float(p, "x");
        out->center_sparse.y = (float)biosim_params_get_float(p, "y");
        out->center_sparse.outer_r = (float)biosim_params_get_float(p, "outer-r");
        out->center_sparse.inner_r = (float)biosim_params_get_float(p, "inner-r");
        out->center_sparse.min_n = (float)biosim_params_get_float(p, "min-n");
        out->center_sparse.max_n = (float)biosim_params_get_float(p, "max-n");
        out->center_sparse.weighted = biosim_params_get_bool(p, "weighted");
        break;

    case BIOSIM_CHALLENGE_NEAR_BARRIER:
        out->near_barrier.radius = (float)biosim_params_get_float(p, "radius");
        break;

    case BIOSIM_CHALLENGE_LOCATION_SEQUENCE:
        out->location_sequence.radius = (float)biosim_params_get_float(p, "radius");
        break;

    case BIOSIM_CHALLENGE_AGAINST_WALL:
    case BIOSIM_CHALLENGE_MIGRATE_DISTANCE:
    case BIOSIM_CHALLENGE_TOUCH_ANY_WALL:
    case BIOSIM_CHALLENGE_RADIOACTIVE_WALLS:
    case BIOSIM_CHALLENGE_PAIRS:
    case BIOSIM_CHALLENGE_ALTRUISM:
        break;
    }

    return BIOSIM_OK;
}
