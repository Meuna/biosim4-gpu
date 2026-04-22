#include "biosim/core/genome.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_genome_create(uint32_t capacity, uint16_t max_length, biosim_genome_t *out) {
    assert(out != NULL);
    assert(capacity > 0 && max_length > 0);

    memset(out, 0, sizeof(*out));
    out->capacity = capacity;
    out->max_length = max_length;

    size_t gene_slots = (size_t)max_length * (size_t)capacity;

    out->conn = calloc(gene_slots, sizeof(uint16_t));
    if (!out->conn) {
        biosim_genome_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->wgt = calloc(gene_slots, sizeof(int16_t));
    if (!out->wgt) {
        biosim_genome_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->length = calloc(capacity, sizeof(uint16_t));
    if (!out->length) {
        biosim_genome_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    return BIOSIM_OK;
}

void biosim_genome_free(biosim_genome_t *g) {
    if (!g) {
        return;
    }
    free(g->conn);
    free(g->wgt);
    free(g->length);
    memset(g, 0, sizeof(*g));
}

/* ── slot operations ────────────────────────────────────────────────────── */

void biosim_genome_init_slot(biosim_genome_t *g, uint32_t idx, uint16_t length, uint64_t *rng) {
    assert(g != NULL && rng != NULL);
    assert(idx < g->capacity);
    assert(length > 0 && length <= g->max_length);

    g->length[idx] = length;
    for (uint32_t j = 0; j < length; j++) {
        size_t slot = (size_t)j * g->capacity + idx;
        g->conn[slot] = (uint16_t)biosim_rng_next(rng);
        g->wgt[slot] = (int16_t)biosim_rng_next(rng);
    }
}

void biosim_genome_copy_slot(biosim_genome_t *g, uint32_t dst, uint32_t src) {
    assert(g != NULL);
    assert(dst < g->capacity && src < g->capacity);

    g->length[dst] = g->length[src];
    for (uint32_t j = 0; j < g->max_length; j++) {
        size_t s = (size_t)j * g->capacity + src;
        size_t d = (size_t)j * g->capacity + dst;
        g->conn[d] = g->conn[s];
        g->wgt[d] = g->wgt[s];
    }
}

/* ── operators ──────────────────────────────────────────────────────────── */

/* Maps a uint64_t RNG output to [0.0, 1.0) with 24 bits of precision. */
static float rng_float(uint64_t *rng) {
    return (float)(biosim_rng_next(rng) >> 40) * (1.0F / 16777216.0F);
}

static void mutate_gene(biosim_genome_t *g, uint32_t idx, uint32_t j, uint64_t *rng) {
    size_t slot = (size_t)j * g->capacity + idx;
    if (biosim_rng_next(rng) & 1U) {
        uint64_t bit = biosim_rng_next(rng) & 15U;
        g->conn[slot] ^= (uint16_t)(1U << bit);
    } else {
        g->wgt[slot] = (int16_t)biosim_rng_next(rng);
    }
}

static void mutate_insert(biosim_genome_t *g, uint32_t idx, uint64_t *rng) {
    uint32_t len = g->length[idx];
    if (len >= g->max_length) {
        return;
    }
    uint32_t pos = (uint32_t)(biosim_rng_next(rng) % ((uint64_t)len + 1ULL));
    for (uint32_t j = len; j > pos; j--) {
        size_t dst_slot = (size_t)j * g->capacity + idx;
        size_t src_slot = (size_t)(j - 1U) * g->capacity + idx;
        g->conn[dst_slot] = g->conn[src_slot];
        g->wgt[dst_slot] = g->wgt[src_slot];
    }
    size_t new_slot = (size_t)pos * g->capacity + idx;
    g->conn[new_slot] = (uint16_t)biosim_rng_next(rng);
    g->wgt[new_slot] = (int16_t)biosim_rng_next(rng);
    g->length[idx] = (uint16_t)(len + 1U);
}

static void mutate_delete(biosim_genome_t *g, uint32_t idx, uint64_t *rng) {
    uint32_t len = g->length[idx];
    if (len <= 1U) {
        return;
    }
    uint32_t pos = (uint32_t)(biosim_rng_next(rng) % (uint64_t)len);
    for (uint32_t j = pos; j < len - 1U; j++) {
        size_t dst_slot = (size_t)j * g->capacity + idx;
        size_t src_slot = (size_t)(j + 1U) * g->capacity + idx;
        g->conn[dst_slot] = g->conn[src_slot];
        g->wgt[dst_slot] = g->wgt[src_slot];
    }
    g->length[idx] = (uint16_t)(len - 1U);
}

void biosim_genome_mutate(biosim_genome_t *g, uint32_t idx, float rate, uint64_t *rng) {
    assert(g != NULL && rng != NULL);
    assert(idx < g->capacity);

    uint32_t len = g->length[idx];
    for (uint32_t j = 0; j < len; j++) {
        if (rng_float(rng) < rate) {
            mutate_gene(g, idx, j, rng);
        }
    }
    if (rng_float(rng) < rate) {
        if (biosim_rng_next(rng) & 1U) {
            mutate_insert(g, idx, rng);
        } else {
            mutate_delete(g, idx, rng);
        }
    }
}

void biosim_genome_crossover(biosim_genome_t *g, uint32_t child, uint32_t parent_a,
                             uint32_t parent_b, uint64_t *rng) {
    assert(g != NULL && rng != NULL);
    assert(child < g->capacity && parent_a < g->capacity && parent_b < g->capacity);

    uint32_t len_a = g->length[parent_a];
    uint32_t len_b = g->length[parent_b];
    uint32_t min_len = len_a < len_b ? len_a : len_b;
    uint32_t k = (uint32_t)(biosim_rng_next(rng) % ((uint64_t)min_len + 1ULL));

    uint32_t child_len = len_b;
    if (child_len > g->max_length) {
        child_len = g->max_length;
    }

    for (uint32_t j = 0; j < k; j++) {
        size_t dst_slot = (size_t)j * g->capacity + child;
        size_t src_slot = (size_t)j * g->capacity + parent_a;
        g->conn[dst_slot] = g->conn[src_slot];
        g->wgt[dst_slot] = g->wgt[src_slot];
    }
    for (uint32_t j = k; j < child_len; j++) {
        size_t dst_slot = (size_t)j * g->capacity + child;
        size_t src_slot = (size_t)j * g->capacity + parent_b;
        g->conn[dst_slot] = g->conn[src_slot];
        g->wgt[dst_slot] = g->wgt[src_slot];
    }
    g->length[child] = (uint16_t)child_len;
}

/* ── warp-divergence mitigation ─────────────────────────────────────────── */

void biosim_genome_sort_by_length(biosim_genome_t *g, uint32_t *perm_out) {
    assert(g != NULL && perm_out != NULL);

    uint32_t cap = g->capacity;
    uint32_t max_len = g->max_length;
    size_t gene_slots = (size_t)max_len * (size_t)cap;
    size_t buckets = (size_t)max_len + 1U;

    /* Allocate all scratch buffers first; abort silently on OOM */
    uint16_t *new_conn = malloc(gene_slots * sizeof(uint16_t));
    int16_t *new_wgt = malloc(gene_slots * sizeof(int16_t));
    uint16_t *new_len = malloc((size_t)cap * sizeof(uint16_t));
    uint32_t *count = calloc(buckets, sizeof(uint32_t));
    uint32_t *start = calloc(buckets, sizeof(uint32_t));
    uint32_t *cur = malloc(buckets * sizeof(uint32_t));
    if (!new_conn || !new_wgt || !new_len || !count || !start || !cur) {
        free(new_conn);
        free(new_wgt);
        free(new_len);
        free(count);
        free(start);
        free(cur);
        return;
    }

    /* Counting sort (descending) on length[0..cap-1] in range [0..max_len] */
    for (uint32_t i = 0; i < cap; i++) {
        count[g->length[i]]++;
    }
    {
        uint32_t pos = 0;
        for (size_t b = buckets; b-- > 0U;) {
            start[b] = pos;
            pos += count[b];
        }
    }
    memcpy(cur, start, buckets * sizeof(uint32_t));
    for (uint32_t i = 0; i < cap; i++) {
        perm_out[cur[g->length[i]]++] = i;
    }

    /* Reorder all three genome buffers according to the permutation */
    for (uint32_t new_i = 0; new_i < cap; new_i++) {
        uint32_t old_i = perm_out[new_i];
        new_len[new_i] = g->length[old_i];
        for (uint32_t j = 0; j < max_len; j++) {
            size_t src_slot = (size_t)j * cap + old_i;
            size_t dst_slot = (size_t)j * cap + new_i;
            new_conn[dst_slot] = g->conn[src_slot];
            new_wgt[dst_slot] = g->wgt[src_slot];
        }
    }
    memcpy(g->conn, new_conn, gene_slots * sizeof(uint16_t));
    memcpy(g->wgt, new_wgt, gene_slots * sizeof(int16_t));
    memcpy(g->length, new_len, (size_t)cap * sizeof(uint16_t));
    free(new_conn);
    free(new_wgt);
    free(new_len);
    free(count);
    free(start);
    free(cur);
}
