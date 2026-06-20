#include "biosim/core/nnet.h"

#include "biosim/core/io_defs.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_nnet_create(
    uint32_t population, uint16_t max_genes, uint8_t max_neurons, biosim_nnet_t *out
) {
    assert(out != NULL);
    assert(population > 0 && max_genes > 0 && max_neurons > 0);

    memset(out, 0, sizeof(*out));
    out->population = population;
    out->max_genes = max_genes;
    out->max_neurons = max_neurons;

    size_t conn_slots = (size_t)max_genes * (size_t)population;
    size_t neuron_slots = (size_t)max_neurons * (size_t)population;

    /* alloc start here, free on exit label */
    biosim_status_t returncode = BIOSIM_OK;

    out->genome_conn = calloc(conn_slots, sizeof(uint16_t));
    if (!out->genome_conn) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    out->genome_wgt = calloc(conn_slots, sizeof(int16_t));
    if (!out->genome_wgt) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    out->conn_length = calloc(population, sizeof(uint16_t));
    if (!out->conn_length) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    out->neuron_output = calloc(neuron_slots, sizeof(float));
    if (!out->neuron_output) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    out->neuron_driven = calloc(neuron_slots, sizeof(uint8_t));
    if (!out->neuron_driven) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    out->neuron_count = calloc(population, sizeof(uint8_t));
    if (!out->neuron_count) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
exit:
    if (returncode != BIOSIM_OK) {
        biosim_nnet_free(out);
    }
    return returncode;
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
    uint16_t packed;  /* compiled connection word — valid only when survives */
    uint16_t bucket;  /* sink bucket index      — valid only when survives */
    uint8_t survives; /* 1 if emitted, 0 if culled (set by classify_conn) */
} remapped_gene_t;

static void parse_genes(
    const biosim_genome_t *genome,
    uint32_t idx,
    uint8_t num_sensors,
    uint8_t num_actions,
    uint8_t max_neurons,
    remapped_gene_t *genes,
    uint16_t gene_count
) {
    uint32_t pop = genome->population;
    for (uint16_t j = 0; j < gene_count; j++) {
        size_t slot = (size_t)j * pop + idx;
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

static void propagate_output_paths(
    const remapped_gene_t *genes, uint16_t gene_count, uint8_t *ota
) {
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

/* Classify one parsed gene for the compiled SoA.  Sets g->survives to 0 if the
 * connection is culled (a neuron source or sink that is not alive), otherwise 1,
 * filling g->packed (the compiled connection word) and g->bucket (its sink
 * bucket).  Buckets order the compiled connections: neuron sinks occupy
 * [0, max_neurons) by remapped sink id, action sinks occupy
 * [max_neurons, max_neurons + num_actions) by action number — so every sink's
 * connections are contiguous, neuron sinks ahead of action sinks, each ascending
 * by number.  This is the contiguity the scalar-accumulator feedforward relies on. */
static void classify_conn(
    remapped_gene_t *g, const uint8_t *alive, const uint8_t *remap, uint8_t max_neurons
) {
    if (g->src_type == BIOSIM_GENE_NEURON && !alive[g->src_num]) {
        g->survives = 0;
        return;
    }
    uint8_t new_sn = (g->src_type == BIOSIM_GENE_NEURON) ? remap[g->src_num] : g->src_num;
    if (g->sink_type == BIOSIM_GENE_NEURON) {
        if (!alive[g->sink_num]) {
            g->survives = 0;
            return;
        }
        uint8_t new_dn = remap[g->sink_num];
        g->packed = BIOSIM_GENE_PACK(g->src_type, new_sn, BIOSIM_GENE_NEURON, new_dn);
        g->bucket = new_dn;
    } else {
        g->packed = BIOSIM_GENE_PACK(g->src_type, new_sn, BIOSIM_GENE_IO, g->sink_num);
        g->bucket = (uint16_t)max_neurons + g->sink_num;
    }
    g->survives = 1;
}

/* Emit the compiled connections for one agent in sink-bucketed order using a
 * linear counting sort.  One classify pass culls each gene and records its packed
 * word and sink bucket; counting those buckets, then exclusive-prefix-summing them,
 * yields a per-bucket insertion offset; a final pass places each surviving gene at
 * its bucket offset and advances it.  Both data passes walk genome order, so
 * connections within a bucket keep genome order and the float summation order is
 * identical to a stable sort.  Writes are dropped once the SoA capacity (max_genes)
 * is reached; the returned count is clamped to it. */
static uint16_t emit_conns_bucketed(
    biosim_nnet_t *n,
    remapped_gene_t *genes,
    uint16_t gene_count,
    uint32_t idx,
    const uint8_t *alive,
    const uint8_t *remap
) {
    uint32_t pop = n->population;
    uint8_t max_neurons = n->max_neurons;
    uint16_t bucket_count = (uint16_t)max_neurons + BIOSIM_NUM_ACTIONS;

    uint16_t offset[128 + BIOSIM_NUM_ACTIONS];
    for (uint16_t b = 0; b < bucket_count; b++) {
        offset[b] = 0;
    }
    for (uint16_t j = 0; j < gene_count; j++) {
        classify_conn(&genes[j], alive, remap, max_neurons);
        if (genes[j].survives) {
            offset[genes[j].bucket]++;
        }
    }

    uint16_t total = 0;
    for (uint16_t b = 0; b < bucket_count; b++) {
        uint16_t c = offset[b];
        offset[b] = total;
        total += c;
    }

    for (uint16_t j = 0; j < gene_count; j++) {
        if (!genes[j].survives) {
            continue;
        }
        uint16_t slot = offset[genes[j].bucket]++;
        if (slot < n->max_genes) {
            n->genome_conn[(size_t)slot * pop + idx] = genes[j].packed;
            n->genome_wgt[(size_t)slot * pop + idx] = genes[j].weight;
        }
    }

    return (total > n->max_genes) ? n->max_genes : total;
}

biosim_status_t biosim_nnet_compile_slot(
    biosim_nnet_t *n,
    const biosim_genome_t *genome,
    uint32_t idx,
    uint8_t num_sensors,
    uint8_t num_actions
) {
    assert(n != NULL && genome != NULL);
    assert(idx < n->population);
    assert(idx < genome->population);
    assert(n->population == genome->population);
    assert(num_sensors > 0 && num_actions > 0);
    assert(num_actions <= BIOSIM_NUM_ACTIONS);
    assert(n->max_neurons > 0 && n->max_neurons <= 128);

    uint16_t gene_count = genome->len[idx];
    uint8_t max_neurons = n->max_neurons;
    uint32_t pop = n->population;

    /* alloc start here, free on exit label */
    remapped_gene_t *genes = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    /* Phase 1: parse genome genes and remap raw 7-bit fields modulo valid range */
    if (gene_count > 0) {
        genes = malloc((size_t)gene_count * sizeof(remapped_gene_t));
        if (!genes) {
            returncode = BIOSIM_ERR_NOMEM;
            goto exit;
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

    /* Phase 5: emit culled connections, bucketed by sink so every sink's
     * connections are contiguous and appear in ascending sink order (neuron sinks
     * before action sinks). */
    uint16_t out_slot = emit_conns_bucketed(n, genes, gene_count, idx, alive, remap);

    /* Phase 6: write metadata and initialise neuron state */
    n->conn_length[idx] = out_slot;
    n->neuron_count[idx] = alive_count;
    for (uint8_t k = 0; k < alive_count; k++) {
        n->neuron_output[(size_t)k * pop + idx] = 0.0F;
        n->neuron_driven[(size_t)k * pop + idx] = 1;
    }

exit:
    free(genes);
    return returncode;
}

/* ── fingerprint ────────────────────────────────────────────────────────── */

uint64_t biosim_nnet_fingerprint(const biosim_nnet_t *n, uint32_t idx) {
    assert(n != NULL && idx < n->population);

    uint64_t h = 0x9e3779b97f4a7c15ULL;
    uint32_t pop = n->population;
    uint16_t len = n->conn_length[idx];
    for (uint16_t j = 0; j < len; j++) {
        size_t slot = (size_t)j * pop + idx;
        h ^= (uint64_t)n->genome_conn[slot];
        h *= 0x9e3779b97f4a7c15ULL;
        h ^= (uint64_t)(uint16_t)n->genome_wgt[slot];
        h *= 0x9e3779b97f4a7c15ULL;
    }
    return h;
}

/* ── feedforward ────────────────────────────────────────────────────────── */

/* Commit one sink's accumulated input.  A neuron sink applies tanh to driven
 * neurons (undriven neurons hold the quiescent baseline 0.5F); an action sink
 * stores the raw accumulator.  Each sink owns one contiguous run of sorted
 * connections, so a plain store (not +=) is correct. */
static void flush_sink(
    biosim_nnet_t *n, uint32_t idx, float *action_vals, uint8_t cur_type, uint8_t cur_num, float acc
) {
    uint32_t pop = n->population;
    if (cur_type == BIOSIM_GENE_NEURON) {
        size_t nslot = (size_t)cur_num * pop + idx;
        n->neuron_output[nslot] = n->neuron_driven[nslot] ? tanhf(acc) : 0.5F;
    } else {
        action_vals[cur_num] = acc;
    }
}

void biosim_nnet_feedforward(
    biosim_nnet_t *n, uint32_t idx, const float *sensor_vals, float *action_vals
) {
    assert(n != NULL);
    assert(idx < n->population);
    assert(sensor_vals != NULL);
    assert(action_vals != NULL);

    uint32_t pop = n->population;
    uint16_t nconn = n->conn_length[idx];
    if (nconn == 0) {
        return;
    }

    /* Single pass over connections sorted by sink (neuron sinks first, then
     * actions, each ascending by number).  A scalar accumulator collects one
     * sink's inputs and is flushed when the sink changes, so neuron outputs are
     * committed before later connections — including every action sink — read
     * them.  Neuron sources whose sink group has already flushed therefore read
     * the freshly committed value. */
    float acc = 0.0F;
    uint16_t packed0 = n->genome_conn[(size_t)idx];
    uint8_t cur_type = (uint8_t)BIOSIM_GENE_SINK_TYPE(packed0);
    uint8_t cur_num = (uint8_t)BIOSIM_GENE_SINK_NUM(packed0);

    for (uint16_t j = 0; j < nconn; j++) {
        size_t slot = (size_t)j * pop + idx;
        uint16_t packed = n->genome_conn[slot];
        int16_t raw_wgt = n->genome_wgt[slot];

        uint8_t src_type = (uint8_t)BIOSIM_GENE_SRC_TYPE(packed);
        uint8_t src_num = (uint8_t)BIOSIM_GENE_SRC_NUM(packed);
        uint8_t sink_type = (uint8_t)BIOSIM_GENE_SINK_TYPE(packed);
        uint8_t sink_num = (uint8_t)BIOSIM_GENE_SINK_NUM(packed);

        if (sink_type != cur_type || sink_num != cur_num) {
            flush_sink(n, idx, action_vals, cur_type, cur_num, acc);
            acc = 0.0F;
            cur_type = sink_type;
            cur_num = sink_num;
        }

        float source = (src_type == BIOSIM_GENE_IO) ? sensor_vals[src_num]
                                                    : n->neuron_output[(size_t)src_num * pop + idx];

        acc += source * ((float)raw_wgt / BIOSIM_GENE_WEIGHT_SCALE);
    }

    flush_sink(n, idx, action_vals, cur_type, cur_num, acc);
}
