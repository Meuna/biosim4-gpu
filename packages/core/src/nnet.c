#include "biosim/core/nnet.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_nnet_create(uint32_t capacity, uint16_t max_conn, uint8_t max_neurons,
                                   biosim_nnet_t *out) {
    assert(out != NULL);
    assert(capacity > 0 && max_conn > 0 && max_neurons > 0);

    memset(out, 0, sizeof(*out));
    out->capacity = capacity;
    out->max_conn = max_conn;
    out->max_neurons = max_neurons;

    size_t conn_slots = (size_t)max_conn * (size_t)capacity;
    size_t neuron_slots = (size_t)max_neurons * (size_t)capacity;

    out->genome_conn = calloc(conn_slots, sizeof(uint16_t));
    if (!out->genome_conn) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->genome_wgt = calloc(conn_slots, sizeof(int16_t));
    if (!out->genome_wgt) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->conn_length = calloc(capacity, sizeof(uint16_t));
    if (!out->conn_length) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->neuron_output = calloc(neuron_slots, sizeof(float));
    if (!out->neuron_output) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->neuron_driven = calloc(neuron_slots, sizeof(uint8_t));
    if (!out->neuron_driven) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    out->neuron_count = calloc(capacity, sizeof(uint8_t));
    if (!out->neuron_count) {
        biosim_nnet_free(out);
        return BIOSIM_ERR_NOMEM;
    }
    return BIOSIM_OK;
}

void biosim_nnet_free(biosim_nnet_t *n) {
    if (!n) {
        return;
    }
    free(n->genome_conn);
    free(n->genome_wgt);
    free(n->conn_length);
    free(n->neuron_output);
    free(n->neuron_driven);
    free(n->neuron_count);
    memset(n, 0, sizeof(*n));
}

/* ── compilation ────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t src_type;
    uint8_t src_num;
    uint8_t sink_type;
    uint8_t sink_num;
    int16_t weight;
} remapped_gene_t;

static void parse_genes(const biosim_genome_t *genome, uint32_t idx, uint8_t num_sensors,
                        uint8_t num_actions, uint8_t max_neurons, remapped_gene_t *genes,
                        uint16_t gene_count) {
    uint32_t cap = genome->capacity;
    for (uint16_t j = 0; j < gene_count; j++) {
        size_t slot = (size_t)j * cap + idx;
        uint16_t c = genome->conn[slot];
        uint8_t st = (uint8_t)BIOSIM_GENE_SRC_TYPE(c);
        uint8_t sn = (uint8_t)BIOSIM_GENE_SRC_NUM(c);
        uint8_t dt = (uint8_t)BIOSIM_GENE_SINK_TYPE(c);
        uint8_t dn = (uint8_t)BIOSIM_GENE_SINK_NUM(c);

        sn = st ? (uint8_t)(sn % num_sensors) : (uint8_t)(sn % max_neurons);
        dn = dt ? (uint8_t)(dn % num_actions) : (uint8_t)(dn % max_neurons);

        genes[j] = (remapped_gene_t){st, sn, dt, dn, genome->wgt[slot]};
    }
}

static void compute_has_nsi(const remapped_gene_t *genes, uint16_t gene_count, uint8_t *has_nsi) {
    for (uint16_t j = 0; j < gene_count; j++) {
        if (genes[j].sink_type == BIOSIM_GENE_NEURON) {
            uint8_t dn = genes[j].sink_num;
            int self_loop = (genes[j].src_type == BIOSIM_GENE_NEURON) && (genes[j].src_num == dn);
            if (!self_loop) {
                has_nsi[dn] = 1;
            }
        }
    }
}

static void propagate_output_paths(const remapped_gene_t *genes, uint16_t gene_count,
                                   uint8_t *ota) {
    for (uint16_t j = 0; j < gene_count; j++) {
        if (genes[j].src_type == BIOSIM_GENE_NEURON && genes[j].sink_type == BIOSIM_GENE_IO) {
            ota[genes[j].src_num] = 1;
        }
    }
    int changed = 1;
    while (changed) {
        changed = 0;
        for (uint16_t j = 0; j < gene_count; j++) {
            if (genes[j].src_type == BIOSIM_GENE_NEURON &&
                genes[j].sink_type == BIOSIM_GENE_NEURON) {
                uint8_t a = genes[j].src_num;
                uint8_t b = genes[j].sink_num;
                if (ota[b] && !ota[a]) {
                    ota[a] = 1;
                    changed = 1;
                }
            }
        }
    }
}

static uint16_t emit_neuron_sink(biosim_nnet_t *n, const remapped_gene_t *genes,
                                 uint16_t gene_count, uint32_t idx, const uint8_t *alive,
                                 const uint8_t *remap, uint16_t out_slot) {
    uint32_t cap = n->capacity;
    for (uint16_t j = 0; j < gene_count && out_slot < n->max_conn; j++) {
        if (genes[j].sink_type != BIOSIM_GENE_NEURON) {
            continue;
        }
        uint8_t dn = genes[j].sink_num;
        if (!alive[dn]) {
            continue;
        }
        if (genes[j].src_type == BIOSIM_GENE_NEURON && !alive[genes[j].src_num]) {
            continue;
        }
        uint8_t new_sn =
            (genes[j].src_type == BIOSIM_GENE_NEURON) ? remap[genes[j].src_num] : genes[j].src_num;
        uint8_t new_dn = remap[dn];
        uint16_t packed = BIOSIM_GENE_PACK(genes[j].src_type, new_sn, BIOSIM_GENE_NEURON, new_dn);
        n->genome_conn[(size_t)out_slot * cap + idx] = packed;
        n->genome_wgt[(size_t)out_slot * cap + idx] = genes[j].weight;
        out_slot++;
    }
    return out_slot;
}

static uint16_t emit_action_sink(biosim_nnet_t *n, const remapped_gene_t *genes,
                                 uint16_t gene_count, uint32_t idx, const uint8_t *alive,
                                 const uint8_t *remap, uint16_t out_slot) {
    uint32_t cap = n->capacity;
    for (uint16_t j = 0; j < gene_count && out_slot < n->max_conn; j++) {
        if (genes[j].sink_type != BIOSIM_GENE_IO) {
            continue;
        }
        if (genes[j].src_type == BIOSIM_GENE_NEURON && !alive[genes[j].src_num]) {
            continue;
        }
        uint8_t new_sn =
            (genes[j].src_type == BIOSIM_GENE_NEURON) ? remap[genes[j].src_num] : genes[j].src_num;
        uint16_t packed =
            BIOSIM_GENE_PACK(genes[j].src_type, new_sn, BIOSIM_GENE_IO, genes[j].sink_num);
        n->genome_conn[(size_t)out_slot * cap + idx] = packed;
        n->genome_wgt[(size_t)out_slot * cap + idx] = genes[j].weight;
        out_slot++;
    }
    return out_slot;
}

void biosim_nnet_compile_slot(biosim_nnet_t *n, const biosim_genome_t *genome, uint32_t idx,
                              uint8_t num_sensors, uint8_t num_actions) {
    assert(n != NULL && genome != NULL);
    assert(idx < n->capacity);
    assert(idx < genome->capacity);
    assert(n->capacity == genome->capacity);
    assert(num_sensors > 0 && num_actions > 0);
    assert(n->max_neurons > 0 && n->max_neurons <= 128);

    uint16_t gene_count = genome->length[idx];
    uint8_t max_neurons = n->max_neurons;
    uint32_t cap = n->capacity;

    /* Phase 1: parse genome genes and remap raw 7-bit fields modulo valid range */
    remapped_gene_t *genes = NULL;
    if (gene_count > 0) {
        genes = malloc((size_t)gene_count * sizeof(remapped_gene_t));
        if (!genes) {
            n->conn_length[idx] = 0;
            n->neuron_count[idx] = 0;
            return;
        }
        parse_genes(genome, idx, num_sensors, num_actions, max_neurons, genes, gene_count);
    }

    /* Phase 2a: mark neurons with at least one non-self input as driven */
    uint8_t has_nsi[128] = {0};
    compute_has_nsi(genes, gene_count, has_nsi);

    /* Phase 2b: propagate output-path reachability from each neuron to its
     * predecessors until no new neurons gain a path to an action */
    uint8_t ota[128] = {0};
    propagate_output_paths(genes, gene_count, ota);

    /* Phase 3: a neuron is alive if it is driven AND has a path to an action */
    /* Phase 4: assign compact sequential IDs to alive neurons (ascending order) */
    uint8_t remap[128];
    uint8_t alive[128];
    memset(remap, 0xFFU, sizeof(remap));
    uint8_t next_id = 0;
    for (uint8_t k = 0; k < max_neurons; k++) {
        alive[k] = (uint8_t)(has_nsi[k] && ota[k]);
        if (alive[k]) {
            remap[k] = next_id++;
        }
    }
    uint8_t alive_count = next_id;

    /* Phase 5: emit culled connections — neuron-sink first, action-sink second */
    uint16_t out_slot = emit_neuron_sink(n, genes, gene_count, idx, alive, remap, 0);
    out_slot = emit_action_sink(n, genes, gene_count, idx, alive, remap, out_slot);

    /* Phase 6: write metadata and initialise neuron state */
    n->conn_length[idx] = out_slot;
    n->neuron_count[idx] = alive_count;
    for (uint8_t k = 0; k < alive_count; k++) {
        n->neuron_output[(size_t)k * cap + idx] = 0.0F;
        n->neuron_driven[(size_t)k * cap + idx] = 1;
    }

    free(genes);
}

/* ── fingerprint ────────────────────────────────────────────────────────── */

uint64_t biosim_nnet_fingerprint(const biosim_nnet_t *n, uint32_t idx) {
    assert(n != NULL && idx < n->capacity);

    uint64_t h = 0x9e3779b97f4a7c15ULL;
    uint32_t cap = n->capacity;
    uint16_t len = n->conn_length[idx];
    for (uint16_t j = 0; j < len; j++) {
        size_t slot = (size_t)j * cap + idx;
        h ^= (uint64_t)n->genome_conn[slot];
        h *= 0x9e3779b97f4a7c15ULL;
        h ^= (uint64_t)(uint16_t)n->genome_wgt[slot];
        h *= 0x9e3779b97f4a7c15ULL;
    }
    return h;
}
