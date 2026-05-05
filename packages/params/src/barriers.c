#include "biosim/params/barriers.h"
#include "tomlc17.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── kind parsing ───────────────────────────────────────────────────────── */

static biosim_status_t parse_kind(const char *s, biosim_barrier_kind_t *out) {
    if (strcmp(s, "hbar") == 0) {
        *out = BIOSIM_BARRIER_HBAR;
        return BIOSIM_OK;
    }
    if (strcmp(s, "vbar") == 0) {
        *out = BIOSIM_BARRIER_VBAR;
        return BIOSIM_OK;
    }
    if (strcmp(s, "square") == 0) {
        *out = BIOSIM_BARRIER_SQUARE;
        return BIOSIM_OK;
    }
    if (strcmp(s, "circle") == 0) {
        *out = BIOSIM_BARRIER_CIRCLE;
        return BIOSIM_OK;
    }
    return BIOSIM_ERR_INVALID;
}

/* ── single barrier table parser ────────────────────────────────────────── */

static biosim_status_t parse_barrier_table(toml_datum_t tab, biosim_barrier_spec_t *out) {
    out->x = BIOSIM_BARRIER_POS_UNSET;
    out->y = BIOSIM_BARRIER_POS_UNSET;
    out->length = BIOSIM_BARRIER_DIM_UNSET;
    out->width = BIOSIM_BARRIER_DIM_UNSET;

    toml_datum_t kind_val = toml_get(tab, "kind");
    if (kind_val.type != TOML_STRING) {
        return BIOSIM_ERR_INVALID;
    }
    biosim_status_t st = parse_kind(kind_val.u.s, &out->kind);
    if (st != BIOSIM_OK) {
        return st;
    }

    toml_datum_t x_val = toml_get(tab, "x");
    if (x_val.type == TOML_INT64) {
        out->x = (int16_t)x_val.u.int64;
    }

    toml_datum_t y_val = toml_get(tab, "y");
    if (y_val.type == TOML_INT64) {
        out->y = (int16_t)y_val.u.int64;
    }

    toml_datum_t len_val = toml_get(tab, "length");
    if (len_val.type == TOML_FP64) {
        out->length = (float)len_val.u.fp64;
    } else if (len_val.type == TOML_INT64) {
        out->length = (float)len_val.u.int64;
    }

    /* circle alias: "radius" maps to length */
    toml_datum_t rad_val = toml_get(tab, "radius");
    if (out->length == BIOSIM_BARRIER_DIM_UNSET) {
        if (rad_val.type == TOML_FP64) {
            out->length = (float)rad_val.u.fp64;
        } else if (rad_val.type == TOML_INT64) {
            out->length = (float)rad_val.u.int64;
        }
    }

    toml_datum_t w_val = toml_get(tab, "width");
    if (w_val.type == TOML_FP64) {
        out->width = (float)w_val.u.fp64;
    } else if (w_val.type == TOML_INT64) {
        out->width = (float)w_val.u.int64;
    }

    return BIOSIM_OK;
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_status_t biosim_barrier_params_load(const char *toml_path, biosim_barrier_spec_t **specs_out,
                                           int *n_out) {
    *specs_out = NULL;
    *n_out = 0;

    if (toml_path == NULL) {
        return BIOSIM_OK;
    }

    toml_result_t result = toml_parse_file_ex(toml_path);
    if (!result.ok) {
        return BIOSIM_ERR_NOTFOUND;
    }

    /* alloc start here, freed on exit label */
    biosim_barrier_spec_t *specs = NULL;
    biosim_status_t returncode = BIOSIM_OK;
    int n = 0;

    toml_datum_t toptab = result.toptab;
    toml_datum_t barriers_tab = toml_get(toptab, "barriers");
    if (barriers_tab.type != TOML_TABLE) {
        goto exit;
    }

    toml_datum_t num_val = toml_get(barriers_tab, "num-barriers");
    if (num_val.type != TOML_INT64 || num_val.u.int64 <= 0) {
        goto exit;
    }

    n = (int)num_val.u.int64;
    specs = (biosim_barrier_spec_t *)malloc((size_t)n * sizeof(*specs));
    if (specs == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    for (int i = 0; i < n; i++) {
        char key[32];
        (void)snprintf(key, sizeof(key), "barrier-%d", i + 1);
        toml_datum_t tab = toml_get(toptab, key);
        if (tab.type != TOML_TABLE) {
            returncode = BIOSIM_ERR_INVALID;
            goto exit;
        }
        biosim_status_t st = parse_barrier_table(tab, &specs[i]);
        if (st != BIOSIM_OK) {
            returncode = st;
            goto exit;
        }
    }

    *specs_out = specs;
    *n_out = n;
    specs = NULL;

exit:
    free(specs);
    toml_free(result);
    return returncode;
}
