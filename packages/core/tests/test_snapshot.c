#include "biosim/core/snapshot.h"

#include "biosim/core/generation.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <string.h>

/* ── helpers ────────────────────────────────────────────────────────────── */

/* Minimal sim used by all tests: pop=4, 4x4 grid, genome_max_len=4, max_neurons=2 */
static biosim_sim_t make_sim(void) {
    biosim_sim_t s;
    memset(&s, 0, sizeof(s));
    s.population = 4U;
    s.size_x = 4;
    s.size_y = 4;
    s.genome_max_len = 4U;
    s.max_neurons = 2U;
    s.long_probe_dist = 4U;
    s.steps_per_gen = 1;
    s.population_sensor_radius = 1;
    s.challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    s.challenge.x_band.x_min = 0.0F;
    s.challenge.x_band.x_max = 1.0F;
    s.challenge.x_band.mirror = false;
    s.mutation_rate = 0.0F;
    s.gen_rng = biosim_rng_seed(0U, 1U);
    return s;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

void test_write_header_correct_magic(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    (void)fseek(f, 0L, SEEK_SET);
    TEST_ASSERT_EQUAL_UINT8(0x42U, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x53U, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x4DU, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x34U, (uint8_t)fgetc(f));

    (void)fclose(f);
    biosim_sim_free(&sim);
}

void test_read_header_roundtrip(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    TEST_ASSERT_EQUAL_UINT16(BIOSIM_SNAP_FORMAT_VERSION, hdr.format_version);
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_IO_SCHEMA_VERSION, hdr.schema_version);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)BIOSIM_NUM_SENSORS, hdr.num_sensors);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)BIOSIM_NUM_ACTIONS, hdr.num_actions);
    TEST_ASSERT_EQUAL_UINT16(sim.genome.max_length, hdr.genome_max_len);
    TEST_ASSERT_EQUAL_UINT8(sim.nnet.max_neurons, hdr.max_neurons);
    TEST_ASSERT_EQUAL_UINT32(0U, hdr.generation_count); /* finalize not called */

    (void)fclose(f);
    biosim_sim_free(&sim);
}

void test_bad_magic_returns_invalid(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    (void)fseek(f, 0L, SEEK_SET);
    (void)fputc(0x00, f); /* corrupt magic */
    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_INVALID, biosim_snapshot_read_header(f, &hdr));

    (void)fclose(f);
    biosim_sim_free(&sim);
}

void test_genome_roundtrip_single_record(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    /* Use all 4 agents as "survivors" */
    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    uint32_t n_surv = 4U;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, n_surv));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    /* Save expected genome data before reading */
    const uint32_t pop = sim.genome.population;
    const uint16_t max_len = sim.genome.max_length;
    uint16_t expected_len[4];
    uint16_t expected_conn[4][4];
    int16_t expected_wgt[4][4];
    for (uint32_t s = 0U; s < n_surv; s++) {
        expected_len[s] = sim.genome.length[survivors[s]];
        for (uint16_t j = 0U; j < max_len; j++) {
            expected_conn[s][j] = sim.genome.conn[(size_t)j * pop + survivors[s]];
            expected_wgt[s][j] = sim.genome.wgt[(size_t)j * pop + survivors[s]];
        }
    }

    /* Create a fresh sim with the same config for the restore */
    biosim_sim_t sim2 = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(1U, hdr.generation_count);

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_last(f, &hdr, &sim2, &loaded_n,
                                                               &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(n_surv, loaded_n);
    TEST_ASSERT_EQUAL_UINT32(sim.gen, loaded_gen);
    TEST_ASSERT_EQUAL_UINT64(sim.gen_rng, loaded_rng);

    const uint32_t pop2 = sim2.genome.population;
    for (uint32_t s = 0U; s < loaded_n; s++) {
        TEST_ASSERT_EQUAL_UINT16(expected_len[s], sim2.genome.length[s]);
        for (uint16_t j = 0U; j < max_len; j++) {
            TEST_ASSERT_EQUAL_UINT16(expected_conn[s][j], sim2.genome.conn[(size_t)j * pop2 + s]);
            TEST_ASSERT_EQUAL_INT16(expected_wgt[s][j], sim2.genome.wgt[(size_t)j * pop2 + s]);
        }
    }

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_multi_gen_load_last(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};

    /* Write 3 records, each with sim.gen different */
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, 4U));
    }
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 3U));

    biosim_sim_t sim2 = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_last(f, &hdr, &sim2, &loaded_n,
                                                               &loaded_gen, &loaded_rng));

    /* load_last should give us the entry written with gen=2 */
    TEST_ASSERT_EQUAL_UINT32(2U, loaded_gen);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_multi_gen_scan_without_gen_count(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, 4U));
    }
    /* Deliberately do NOT call finalize, so generation_count stays 0 */

    biosim_sim_t sim2 = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(0U, hdr.generation_count);

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_last(f, &hdr, &sim2, &loaded_n,
                                                               &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(2U, loaded_gen);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_load_gen_index(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, 4U));
    }
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 3U));

    biosim_sim_t sim2 = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    /* Load entry at index 1 (gen=1) */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load(f, 1U, &hdr, &sim2, &loaded_n,
                                                          &loaded_gen, &loaded_rng));
    TEST_ASSERT_EQUAL_UINT32(1U, loaded_gen);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_load_gen_out_of_range(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, 4U));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim2 = make_sim();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_NOTFOUND, biosim_snapshot_load(f, 5U, &hdr, &sim2, &loaded_n,
                                                                    &loaded_gen, &loaded_rng));

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_header_correct_magic);
    RUN_TEST(test_read_header_roundtrip);
    RUN_TEST(test_bad_magic_returns_invalid);
    RUN_TEST(test_genome_roundtrip_single_record);
    RUN_TEST(test_multi_gen_load_last);
    RUN_TEST(test_multi_gen_scan_without_gen_count);
    RUN_TEST(test_load_gen_index);
    RUN_TEST(test_load_gen_out_of_range);
    return UNITY_END();
}
