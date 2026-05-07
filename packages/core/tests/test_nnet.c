#include "biosim/core/genome.h"
#include "biosim/core/nnet.h"
#include "biosim/core/status.h"
#include "unity.h"

#define CAP         4U
#define MAX_CONN    16U
#define MAX_NEURONS 8U
#define MAX_GENES   8U
#define NUM_SENSORS 4U
#define NUM_ACTIONS 4U
#define AGENT_IDX   0U

static biosim_nnet_t nnet;
static biosim_genome_t genome;

static void set_gene(uint32_t agent, uint16_t j, uint8_t st, uint8_t sn, uint8_t dt, uint8_t dn,
                     int16_t w) {
    genome.conn[(size_t)j * CAP + agent] = BIOSIM_GENE_PACK(st, sn, dt, dn);
    genome.wgt[(size_t)j * CAP + agent] = w;
}

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_create(CAP, MAX_CONN, MAX_NEURONS, &nnet));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_genome_create(CAP, MAX_GENES, &genome));
}

void tearDown(void) {
    biosim_nnet_free(&nnet);
    biosim_genome_free(&genome);
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

void test_create_returns_ok(void) {
    biosim_nnet_t local;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_create(4, 16, 8, &local));
    biosim_nnet_free(&local);
}

void test_create_pointers_non_null(void) {
    TEST_ASSERT_NOT_NULL(nnet.genome_conn);
    TEST_ASSERT_NOT_NULL(nnet.genome_wgt);
    TEST_ASSERT_NOT_NULL(nnet.conn_length);
    TEST_ASSERT_NOT_NULL(nnet.neuron_output);
    TEST_ASSERT_NOT_NULL(nnet.neuron_driven);
    TEST_ASSERT_NOT_NULL(nnet.neuron_count);
}

void test_create_metadata_stored(void) {
    TEST_ASSERT_EQUAL_UINT32(CAP, nnet.population);
    TEST_ASSERT_EQUAL_UINT16(MAX_CONN, nnet.max_conn);
    TEST_ASSERT_EQUAL_UINT8(MAX_NEURONS, nnet.max_neurons);
}

void test_free_zeroes_struct(void) {
    biosim_nnet_t local;
    biosim_nnet_create(4, 16, 8, &local);
    biosim_nnet_free(&local);
    TEST_ASSERT_NULL(local.genome_conn);
    TEST_ASSERT_EQUAL_UINT32(0, local.population);
}

/* ── empty and trivial genomes ──────────────────────────────────────────── */

void test_empty_genome_zero_connections(void) {
    genome.len[AGENT_IDX] = 0;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(0, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

void test_all_self_loops_culled(void) {
    /* NEURON 0→NEURON 0, NEURON 1→NEURON 1: self-loops do not count as non-self
     * input, so both neurons are dead and all connections are removed */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 1, BIOSIM_GENE_NEURON, 1, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(0, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

/* ── sensor-to-action ───────────────────────────────────────────────────── */

void test_sensor_to_action_direct(void) {
    /* SENSOR 0 → ACTION 0: one connection, no internal neurons */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(1, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

void test_sensor_to_action_correct_packed_bits(void) {
    /* SENSOR 2 → ACTION 3, weight 200: verify packed field values are preserved */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 2, BIOSIM_GENE_IO, 3, 200);
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    uint16_t packed = nnet.genome_conn[0 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(BIOSIM_GENE_IO, BIOSIM_GENE_SRC_TYPE(packed));
    TEST_ASSERT_EQUAL_UINT8(2, BIOSIM_GENE_SRC_NUM(packed));
    TEST_ASSERT_EQUAL_UINT8(BIOSIM_GENE_IO, BIOSIM_GENE_SINK_TYPE(packed));
    TEST_ASSERT_EQUAL_UINT8(3, BIOSIM_GENE_SINK_NUM(packed));
    TEST_ASSERT_EQUAL_INT16(200, nnet.genome_wgt[0 * CAP + AGENT_IDX]);
}

/* ── alive neuron ───────────────────────────────────────────────────────── */

void test_one_neuron_alive(void) {
    /* SENSOR 0 → NEURON 0, NEURON 0 → ACTION 0 */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(2, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_count[AGENT_IDX]);
}

void test_neuron_sink_before_action_sink(void) {
    /* The neuron-sink connection (SENSOR→NEURON) must occupy slot 0 and the
     * action-sink connection (NEURON→ACTION) must occupy slot 1 */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    uint16_t p0 = nnet.genome_conn[0 * CAP + AGENT_IDX];
    uint16_t p1 = nnet.genome_conn[1 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(BIOSIM_GENE_NEURON, BIOSIM_GENE_SINK_TYPE(p0));
    TEST_ASSERT_EQUAL_UINT8(BIOSIM_GENE_IO, BIOSIM_GENE_SINK_TYPE(p1));
}

void test_neuron_driven_set(void) {
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_driven[0 * CAP + AGENT_IDX]);
}

void test_neuron_output_zeroed(void) {
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_FLOAT(0.0F, nnet.neuron_output[0 * CAP + AGENT_IDX]);
}

/* ── dead neuron culling ────────────────────────────────────────────────── */

void test_no_input_neuron_culled(void) {
    /* NEURON 0 → ACTION 0 only: N0 has output but no input → dead */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(0, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

void test_no_output_path_neuron_culled(void) {
    /* SENSOR 0 → NEURON 0 only: N0 has input but no path to any action → dead */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(0, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

void test_self_loop_only_neuron_culled(void) {
    /* NEURON 0→NEURON 0 (self-loop) plus NEURON 0→ACTION 0.
     * has_non_self_input[0] = false because the only input is the self-loop,
     * so N0 is dead despite having an action output. */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT16(0, nnet.conn_length[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT8(0, nnet.neuron_count[AGENT_IDX]);
}

void test_indirect_output_path_alive(void) {
    /* SENSOR 0→NEURON 0, NEURON 0→NEURON 1, NEURON 1→ACTION 0.
     * ota propagates: N1 has direct action output → N0 inherits it.
     * Both N0 and N1 are alive. */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_NEURON, 1, 100);
    set_gene(AGENT_IDX, 2, BIOSIM_GENE_NEURON, 1, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 3;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(2, nnet.neuron_count[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT16(3, nnet.conn_length[AGENT_IDX]);
}

/* ── neuron renumbering ──────────────────────────────────────────────────── */

void test_renumber_gap_filled(void) {
    /* N0 alive (S0→N0, N0→A0), N1 unreferenced (dead), N2 alive (S0→N2, N2→A0).
     * After culling: two neurons, compact IDs 0 and 1. */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    set_gene(AGENT_IDX, 2, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 2, 100);
    set_gene(AGENT_IDX, 3, BIOSIM_GENE_NEURON, 2, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 4;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(2, nnet.neuron_count[AGENT_IDX]);
}

void test_remapped_src_num(void) {
    /* N0 dead (never referenced as sink), N2 alive: S0→N2, N2→A0.
     * N2 gets compact ID 0.  The action-sink connection's src_num must be 0. */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 2, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 2, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    /* slot 1 is the action-sink connection (N→A) */
    uint16_t action_conn = nnet.genome_conn[1 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(0, BIOSIM_GENE_SRC_NUM(action_conn));
}

void test_remapped_sink_num(void) {
    /* Same setup as above: S0→N2, N2→A0.  N2 → compact ID 0.
     * The neuron-sink connection's sink_num must be 0 after remapping. */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 2, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 2, BIOSIM_GENE_IO, 0, 100);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    /* slot 0 is the neuron-sink connection (S→N) */
    uint16_t neuron_conn = nnet.genome_conn[0 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(0, BIOSIM_GENE_SINK_NUM(neuron_conn));
}

/* ── modulo remapping ───────────────────────────────────────────────────── */

void test_src_sensor_modulo(void) {
    /* Raw srcNum=5, num_sensors=4: 5%4=1.
     * Compiled connection must encode sensor 1 as the source. */
    genome.conn[0 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 5, BIOSIM_GENE_IO, 0);
    genome.wgt[0 * CAP + AGENT_IDX] = 0;
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    uint16_t packed = nnet.genome_conn[0 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(1, BIOSIM_GENE_SRC_NUM(packed));
}

void test_src_neuron_modulo(void) {
    /* Gene 0: S0→N(raw sinkNum=10), Gene 1: N(raw srcNum=10)→A0.
     * With modulo 10%8=2: both genes reference N2 → N2 alive → 1 neuron, 2 conns.
     * Without modulo: gene 0 feeds N10; gene 1 reads from N10 which has no sink input
     * reaching an action independently, so the networks would diverge. */
    genome.conn[0 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 10);
    genome.wgt[0 * CAP + AGENT_IDX] = 100;
    genome.conn[1 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_NEURON, 10, BIOSIM_GENE_IO, 0);
    genome.wgt[1 * CAP + AGENT_IDX] = 100;
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_count[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT16(2, nnet.conn_length[AGENT_IDX]);
}

void test_sink_action_modulo(void) {
    /* Raw sinkNum=6, num_actions=4: 6%4=2.
     * Compiled connection must encode action 2 as the sink. */
    genome.conn[0 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 6);
    genome.wgt[0 * CAP + AGENT_IDX] = 0;
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    uint16_t packed = nnet.genome_conn[0 * CAP + AGENT_IDX];
    TEST_ASSERT_EQUAL_UINT8(2, BIOSIM_GENE_SINK_NUM(packed));
}

void test_sink_neuron_modulo(void) {
    /* Gene 0: S0→N(raw sinkNum=9), Gene 1: N(raw srcNum=9)→A0.
     * With modulo 9%8=1: both reference N1 → N1 alive → 1 neuron, 2 conns. */
    genome.conn[0 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 9);
    genome.wgt[0 * CAP + AGENT_IDX] = 100;
    genome.conn[1 * CAP + AGENT_IDX] = BIOSIM_GENE_PACK(BIOSIM_GENE_NEURON, 9, BIOSIM_GENE_IO, 0);
    genome.wgt[1 * CAP + AGENT_IDX] = 100;
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_count[AGENT_IDX]);
    TEST_ASSERT_EQUAL_UINT16(2, nnet.conn_length[AGENT_IDX]);
}

/* ── soa indexing ───────────────────────────────────────────────────────── */

void test_transposed_conn_indexing(void) {
    /* Slot 0 of agent 0 is at buffer[0*CAP+0]=buffer[0].
     * Slot 0 of agent 1 is at buffer[0*CAP+1]=buffer[1].
     * Adjacent agents' same-slot data is adjacent in memory. */
    genome.conn[0 * CAP + 0] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0);
    genome.wgt[0 * CAP + 0] = 10;
    genome.len[0] = 1;
    genome.conn[0 * CAP + 1] = BIOSIM_GENE_PACK(BIOSIM_GENE_IO, 1, BIOSIM_GENE_IO, 0);
    genome.wgt[0 * CAP + 1] = 20;
    genome.len[1] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(0, BIOSIM_GENE_SRC_NUM(nnet.genome_conn[0]));
    TEST_ASSERT_EQUAL_UINT8(1, BIOSIM_GENE_SRC_NUM(nnet.genome_conn[1]));
}

void test_transposed_neuron_indexing(void) {
    /* Neuron k of agent i at neuron_driven[k*CAP+i].
     * For k=0: agent 0 is at [0], agent 1 is at [1]. */
    set_gene(0, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(0, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[0] = 2;
    set_gene(1, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(1, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[1] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_driven[0 * CAP + 0]);
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_driven[0 * CAP + 1]);
}

void test_adjacent_agents_independent(void) {
    /* Compiling agent 1 must not corrupt agent 0's buffers */
    set_gene(0, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 42);
    set_gene(0, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 42);
    genome.len[0] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));

    int16_t saved_weight = nnet.genome_wgt[0 * CAP + 0];

    set_gene(1, 0, BIOSIM_GENE_IO, 1, BIOSIM_GENE_IO, 1, 999);
    genome.len[1] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));

    TEST_ASSERT_EQUAL_INT16(saved_weight, nnet.genome_wgt[0 * CAP + 0]);
    TEST_ASSERT_EQUAL_UINT16(2, nnet.conn_length[0]);
    TEST_ASSERT_EQUAL_UINT8(1, nnet.neuron_count[0]);
}

/* ── overflow guard ─────────────────────────────────────────────────────── */

void test_max_conn_not_exceeded(void) {
    /* Create nnet with max_conn=2; genome has 4 SENSOR→ACTION genes that
     * all survive culling.  conn_length must be capped at 2. */
    biosim_nnet_t small;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_create(CAP, 2, MAX_NEURONS, &small));

    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 100);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_IO, 1, BIOSIM_GENE_IO, 1, 100);
    set_gene(AGENT_IDX, 2, BIOSIM_GENE_IO, 2, BIOSIM_GENE_IO, 2, 100);
    set_gene(AGENT_IDX, 3, BIOSIM_GENE_IO, 3, BIOSIM_GENE_IO, 3, 100);
    genome.len[AGENT_IDX] = 4;

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&small, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_TRUE(small.conn_length[AGENT_IDX] <= 2);

    biosim_nnet_free(&small);
}

/* ── fingerprint ─────────────────────────────────────────────────────────── */

void test_fingerprint_compiled_nnet_deterministic(void) {
    /* Compile the same genome slot to two different nnet slots; fingerprints
     * must be equal regardless of which slot index holds the compiled network. */
    set_gene(0, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 100);
    set_gene(0, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[0] = 2;
    biosim_genome_copy_slot(&genome, 1, 0);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT64(biosim_nnet_fingerprint(&nnet, 0), biosim_nnet_fingerprint(&nnet, 1));
}

void test_fingerprint_differs_for_different_nnets(void) {
    /* Two genomes that compile to different networks produce different fingerprints. */
    set_gene(0, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[0] = 1;
    set_gene(1, 0, BIOSIM_GENE_IO, 1, BIOSIM_GENE_IO, 1, 200);
    genome.len[1] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_NOT_EQUAL_UINT64(biosim_nnet_fingerprint(&nnet, 0),
                                 biosim_nnet_fingerprint(&nnet, 1));
}

void test_fingerprint_phenotypic_equivalence(void) {
    /* Genome 0: S0→A0 (one gene, survives culling).
     * Genome 1: S0→A0 + N0→A0 (N0 has no non-self input so it is dead and
     * culled; only S0→A0 remains).  Both compile to the same network. */
    set_gene(0, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[0] = 1;
    set_gene(1, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 100);
    set_gene(1, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 100);
    genome.len[1] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 0, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_nnet_compile_slot(&nnet, &genome, 1, NUM_SENSORS, NUM_ACTIONS));
    TEST_ASSERT_EQUAL_UINT64(biosim_nnet_fingerprint(&nnet, 0), biosim_nnet_fingerprint(&nnet, 1));
}

/* feedforward ─────────────────────────────────────────────────────────── */

void test_feedforward_sensor_to_action_direct(void) {
    /* S0→A0, weight=8192 (one scale unit): sensor=1.0 → action_vals[0]=1.0 */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 8192);
    genome.len[AGENT_IDX] = 1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    float sensor_vals[NUM_SENSORS] = {1.0F, 0.0F, 0.0F, 0.0F};
    float action_vals[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sensor_vals, NUM_SENSORS, action_vals, NUM_ACTIONS);

    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.0F, action_vals[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, action_vals[1]);
}

void test_feedforward_sensor_neuron_action_chain(void) {
    /* S0→N0 (w=8192), N0→A0 (w=8192).
     * On the first call neuron_output[N0] starts at 0, so A0 receives 0.
     * After the call neuron_output[N0] = tanhf(1.0). */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 8192);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 8192);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    float sensor_vals[NUM_SENSORS] = {1.0F, 0.0F, 0.0F, 0.0F};
    float action_vals[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sensor_vals, NUM_SENSORS, action_vals, NUM_ACTIONS);

    TEST_ASSERT_FLOAT_WITHIN(1e-5F, tanhf(1.0F), nnet.neuron_output[0 * CAP + AGENT_IDX]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 0.0F, action_vals[0]);
}

void test_feedforward_neuron_state_carry(void) {
    /* Same S0→N0→A0 chain.  After a warm-up call with sensor=1.0, a second
     * call with sensor=0.0 reads the stored neuron_output as the source for
     * the N0→A0 connection, so action_vals[0] ≈ tanhf(1.0). */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 8192);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 8192);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    float sv1[NUM_SENSORS] = {1.0F, 0.0F, 0.0F, 0.0F};
    float av1[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sv1, NUM_SENSORS, av1, NUM_ACTIONS);

    float sv2[NUM_SENSORS] = {0.0F, 0.0F, 0.0F, 0.0F};
    float av2[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sv2, NUM_SENSORS, av2, NUM_ACTIONS);

    TEST_ASSERT_FLOAT_WITHIN(1e-5F, tanhf(1.0F), av2[0]);
}

void test_feedforward_undriven_neuron_defaults_to_half(void) {
    /* compile_slot only keeps neurons with driven=1.  Forcing driven=0 after
     * compilation exercises the undriven branch (output fixed to 0.5F). */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_NEURON, 0, 8192);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_NEURON, 0, BIOSIM_GENE_IO, 0, 8192);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    nnet.neuron_driven[0 * CAP + AGENT_IDX] = 0;

    float sensor_vals[NUM_SENSORS] = {1.0F, 0.0F, 0.0F, 0.0F};
    float action_vals[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sensor_vals, NUM_SENSORS, action_vals, NUM_ACTIONS);

    TEST_ASSERT_EQUAL_FLOAT(0.5F, nnet.neuron_output[0 * CAP + AGENT_IDX]);
}

void test_feedforward_multiple_sensors_sum_to_action(void) {
    /* S0→A0 (w=8192) + S1→A0 (w=4096), both sensors=1.0:
     * action_vals[0] = 1.0*(8192/8192) + 1.0*(4096/8192) = 1.5 */
    set_gene(AGENT_IDX, 0, BIOSIM_GENE_IO, 0, BIOSIM_GENE_IO, 0, 8192);
    set_gene(AGENT_IDX, 1, BIOSIM_GENE_IO, 1, BIOSIM_GENE_IO, 0, 4096);
    genome.len[AGENT_IDX] = 2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    float sensor_vals[NUM_SENSORS] = {1.0F, 1.0F, 0.0F, 0.0F};
    float action_vals[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sensor_vals, NUM_SENSORS, action_vals, NUM_ACTIONS);

    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.5F, action_vals[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, action_vals[1]);
}

void test_feedforward_no_connections_noop(void) {
    /* Empty genome: no connections, no neurons.  action_vals must stay zero
     * and any pre-existing neuron_output value must remain untouched. */
    genome.len[AGENT_IDX] = 0;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_nnet_compile_slot(&nnet, &genome, AGENT_IDX,
                                                              NUM_SENSORS, NUM_ACTIONS));

    nnet.neuron_output[0 * CAP + AGENT_IDX] = 99.0F;

    float sensor_vals[NUM_SENSORS] = {0.5F, 0.5F, 0.5F, 0.5F};
    float action_vals[NUM_ACTIONS] = {0.0F, 0.0F, 0.0F, 0.0F};
    biosim_nnet_feedforward(&nnet, AGENT_IDX, sensor_vals, NUM_SENSORS, action_vals, NUM_ACTIONS);

    for (uint8_t a = 0; a < NUM_ACTIONS; a++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0F, action_vals[a]);
    }
    TEST_ASSERT_EQUAL_FLOAT(99.0F, nnet.neuron_output[0 * CAP + AGENT_IDX]);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_returns_ok);
    RUN_TEST(test_create_pointers_non_null);
    RUN_TEST(test_create_metadata_stored);
    RUN_TEST(test_free_zeroes_struct);
    RUN_TEST(test_empty_genome_zero_connections);
    RUN_TEST(test_all_self_loops_culled);
    RUN_TEST(test_sensor_to_action_direct);
    RUN_TEST(test_sensor_to_action_correct_packed_bits);
    RUN_TEST(test_one_neuron_alive);
    RUN_TEST(test_neuron_sink_before_action_sink);
    RUN_TEST(test_neuron_driven_set);
    RUN_TEST(test_neuron_output_zeroed);
    RUN_TEST(test_no_input_neuron_culled);
    RUN_TEST(test_no_output_path_neuron_culled);
    RUN_TEST(test_self_loop_only_neuron_culled);
    RUN_TEST(test_indirect_output_path_alive);
    RUN_TEST(test_renumber_gap_filled);
    RUN_TEST(test_remapped_src_num);
    RUN_TEST(test_remapped_sink_num);
    RUN_TEST(test_src_sensor_modulo);
    RUN_TEST(test_src_neuron_modulo);
    RUN_TEST(test_sink_action_modulo);
    RUN_TEST(test_sink_neuron_modulo);
    RUN_TEST(test_transposed_conn_indexing);
    RUN_TEST(test_transposed_neuron_indexing);
    RUN_TEST(test_adjacent_agents_independent);
    RUN_TEST(test_max_conn_not_exceeded);
    RUN_TEST(test_fingerprint_compiled_nnet_deterministic);
    RUN_TEST(test_fingerprint_differs_for_different_nnets);
    RUN_TEST(test_fingerprint_phenotypic_equivalence);
    RUN_TEST(test_feedforward_sensor_to_action_direct);
    RUN_TEST(test_feedforward_sensor_neuron_action_chain);
    RUN_TEST(test_feedforward_neuron_state_carry);
    RUN_TEST(test_feedforward_undriven_neuron_defaults_to_half);
    RUN_TEST(test_feedforward_multiple_sensors_sum_to_action);
    RUN_TEST(test_feedforward_no_connections_noop);
    return UNITY_END();
}
