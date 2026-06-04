#include "biosim/core/survivor_snap.h"

#include <stdlib.h>

/* ── lifecycle ───────────────────────────────────────────────────────────── */

biosim_status_t biosim_survivor_snap_grow(
    biosim_survivor_snap_t *snap, uint32_t n_survivors, uint16_t g_max_len
) {
    if (n_survivors == 0U) {
        return BIOSIM_OK;
    }

    uint32_t new_pop = snap->pop_cap;
    uint16_t new_stride = snap->stride_cap;

    if (n_survivors > snap->pop_cap) {
        uint32_t doubled = snap->pop_cap == 0U ? n_survivors : snap->pop_cap * 2U;
        new_pop = doubled >= n_survivors ? doubled : n_survivors;
    }
    if (g_max_len > snap->stride_cap) {
        uint16_t doubled = snap->stride_cap == 0U ? g_max_len : (uint16_t)(snap->stride_cap * 2U);
        new_stride = doubled >= g_max_len ? doubled : g_max_len;
    }

    if (new_pop == snap->pop_cap && new_stride == snap->stride_cap) {
        return BIOSIM_OK;
    }

    size_t conn_slots = (size_t)new_pop * new_stride;

    uint16_t *nc = realloc(snap->conn, conn_slots * sizeof(uint16_t));
    if (nc == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    snap->conn = nc;

    int16_t *nw = realloc(snap->wgt, conn_slots * sizeof(int16_t));
    if (nw == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    snap->wgt = nw;

    uint16_t *nl = realloc(snap->len, (size_t)new_pop * sizeof(uint16_t));
    if (nl == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    snap->len = nl;

    float *ns = realloc(snap->scores, (size_t)new_pop * sizeof(float));
    if (ns == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    snap->scores = ns;

    snap->pop_cap = new_pop;
    snap->stride_cap = new_stride;
    return BIOSIM_OK;
}

void biosim_survivor_snap_free(biosim_survivor_snap_t *snap) {
    if (snap == NULL) {
        return;
    }
    free(snap->conn);
    free(snap->wgt);
    free(snap->len);
    free(snap->scores);
    snap->conn = NULL;
    snap->wgt = NULL;
    snap->len = NULL;
    snap->scores = NULL;
    snap->count = 0U;
    snap->pop_cap = 0U;
    snap->stride_cap = 0U;
}
