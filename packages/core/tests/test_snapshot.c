#include "biosim/core/snapshot.h"
#include "sim_test_utils.h"

#include "biosim/core/census.h"
#include "biosim/core/generation.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <stdio.h>

/* ── tests ──────────────────────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

void test_write_header_correct_magic(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
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

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    TEST_ASSERT_EQUAL_UINT16(BIOSIM_SNAP_FORMAT_VERSION, hdr.format_version);
    TEST_ASSERT_EQUAL_UINT16(BIOSIM_IO_SCHEMA_VERSION, hdr.schema_version);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)BIOSIM_NUM_SENSORS, hdr.num_sensors);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)BIOSIM_NUM_ACTIONS, hdr.num_actions);
    TEST_ASSERT_EQUAL_UINT16(sim.genome.max_len, hdr.genome_max_len);
    TEST_ASSERT_EQUAL_UINT8(sim.nnet.max_neurons, hdr.max_neurons);
    TEST_ASSERT_EQUAL_UINT32(0U, hdr.generation_count); /* finalize not called */

    (void)fclose(f);
    biosim_sim_free(&sim);
}

void test_bad_magic_returns_invalid(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
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

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    /* Use all 4 agents as "survivors" */
    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    uint32_t n_surv = 4U;
    float scores[4] = {0.25F, 0.50F, 0.75F, 1.00F};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_write_genome(f, &sim, survivors, scores, n_surv));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(1U, hdr.generation_count);

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_load_last(f, &hdr, &sim2, loaded_scores, &loaded_n,
                                                    &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(n_surv, loaded_n);
    TEST_ASSERT_EQUAL_UINT32(sim.gen, loaded_gen);
    TEST_ASSERT_EQUAL_UINT64(sim.gen_rng, loaded_rng);
    for (uint32_t s = 0U; s < loaded_n; s++) {
        TEST_ASSERT_EQUAL_FLOAT(scores[s], loaded_scores[s]);
    }

    assert_genome_equal(&sim.genome, &sim2.genome);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_multi_gen_load_last(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    float scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};

    /* Write 3 records, each with sim.gen different */
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                              biosim_snapshot_write_genome(f, &sim, survivors, scores, 4U));
    }
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 3U));

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_load_last(f, &hdr, &sim2, loaded_scores, &loaded_n,
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

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    float scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                              biosim_snapshot_write_genome(f, &sim, survivors, scores, 4U));
    }
    /* Deliberately do NOT call finalize, so generation_count stays 0 */

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(0U, hdr.generation_count);

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_load_last(f, &hdr, &sim2, loaded_scores, &loaded_n,
                                                    &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(2U, loaded_gen);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_load_gen_index(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    float scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};
    for (uint32_t g = 0U; g < 3U; g++) {
        sim.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                              biosim_snapshot_write_genome(f, &sim, survivors, scores, 4U));
    }
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 3U));

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    /* Load entry at index 1 (gen=1) */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load(f, 1U, &hdr, &sim2, loaded_scores,
                                                          &loaded_n, &loaded_gen, &loaded_rng));
    TEST_ASSERT_EQUAL_UINT32(1U, loaded_gen);

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

void test_load_gen_out_of_range(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    float scores[4] = {0.25F, 0.50F, 0.75F, 1.00F};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, survivors, scores, 4U));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_EOF, biosim_snapshot_load(f, 5U, &hdr, &sim2, loaded_scores,
                                                           &loaded_n, &loaded_gen, &loaded_rng));

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

/* pop_file (6) > pop_sim (4): load_last loads the first pop_sim entries. */
void test_population_load_file_larger_than_sim(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim_write = sim_test_make_light();
    sim_write.population = 6U;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim_write, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim_write));

    uint32_t survivors[6] = {0U, 1U, 2U, 3U, 4U, 5U};
    float scores[6] = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_write_genome(f, &sim_write, survivors, scores, 6U));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim_read = sim_test_make_light(); /* population = 4 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim_read, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_load_last(f, &hdr, &sim_read, loaded_scores, &loaded_n,
                                                    &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(4U, loaded_n);
    for (uint32_t s = 0U; s < loaded_n; s++) {
        TEST_ASSERT_EQUAL_FLOAT(scores[s], loaded_scores[s]);
    }
    assert_genome_slice_equal(&sim_write.genome, &sim_read.genome, loaded_n);

    (void)fclose(f);
    biosim_sim_free(&sim_write);
    biosim_sim_free(&sim_read);
}

/* pop_file (4) < pop_sim (6): load_last loads all pop_file entries into slots 0..3. */
void test_population_load_file_smaller_than_sim(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim_write = sim_test_make_light(); /* population = 4 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim_write, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim_write));

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    float scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_write_genome(f, &sim_write, survivors, scores, 4U));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim_read = sim_test_make_light();
    sim_read.population = 6U;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim_read, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));

    uint32_t loaded_n;
    uint32_t loaded_gen;
    uint64_t loaded_rng;
    float loaded_scores[6];
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK,
                          biosim_snapshot_load_last(f, &hdr, &sim_read, loaded_scores, &loaded_n,
                                                    &loaded_gen, &loaded_rng));

    TEST_ASSERT_EQUAL_UINT32(4U, loaded_n);
    for (uint32_t s = 0U; s < loaded_n; s++) {
        TEST_ASSERT_EQUAL_FLOAT(scores[s], loaded_scores[s]);
    }
    assert_genome_slice_equal(&sim_write.genome, &sim_read.genome, loaded_n);

    (void)fclose(f);
    biosim_sim_free(&sim_write);
    biosim_sim_free(&sim_read);
}

/*
 * Two generations on sim1; snapshot written after gen 0 via session API.
 * sim2 is restored and also runs gen 1.  After the second generation both sims
 * must be numerically identical across all sub-structs (agents, grid, genome, nnet).
 * Failure in assert_sim_equal is accepted — RNG reproducibility gap is noted.
 */
void test_session_restore_identical_second_generation(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_e2e_test.bsm4";
    (void)remove(path);

    biosim_census_t census;

    biosim_sim_t sim1 = sim_test_make_medium();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim1, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_session_open(&sim1, path, 1));

    /* gen 0: step×8, evaluate, snapshot_write, reproduce */
    sim_test_run_one_gen(&sim1);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_next_generation(&sim1, &census));

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_session_close(&sim1));

    biosim_sim_t sim2 = sim_test_make_medium();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_restore(path, &sim2));

    /* Run both sims to the end of gen 1 */
    sim_test_run_one_gen(&sim1); /* gen 1 */
    sim_test_run_one_gen(&sim2); /* gen 1 */

    assert_sim_equal(&sim1, &sim2);

    /* Advance to gen 2 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_next_generation(&sim1, &census));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_next_generation(&sim2, &census));

    assert_sim_equal(&sim1, &sim2);

    (void)remove(path);
    biosim_sim_free(&sim1);
    biosim_sim_free(&sim2);
}

/* generation_count=1 in header but file ends after the 8-byte entry_size field */
void test_load_last_truncated_last_gen(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    /* Write only the entry_size prefix — no payload follows */
    uint64_t fake_size = 1000U;
    (void)fwrite(&fake_size, 8U, 1U, f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(1U, hdr.generation_count);

    uint32_t n = 0U;
    uint32_t gen = 0U;
    uint64_t rng = 0U;
    float scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_EOF,
                          biosim_snapshot_load_last(f, &hdr, &sim2, scores, &n, &gen, &rng));

    (void)fclose(f);
    biosim_sim_free(&sim);
    biosim_sim_free(&sim2);
}

/* generation_count=0 (scan mode) with header only — no complete generation */
void test_load_last_no_complete_gen(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    biosim_sim_t sim = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim, NULL, 0));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    /* No genome entries; generation_count stays 0 */

    biosim_sim_t sim2 = sim_test_make_light();
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_create(&sim2, NULL, 0));

    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_read_header(f, &hdr));
    TEST_ASSERT_EQUAL_UINT32(0U, hdr.generation_count);

    uint32_t n = 0U;
    uint32_t gen = 0U;
    uint64_t rng = 0U;
    float scores[4];
    TEST_ASSERT_EQUAL_INT(BIOSIM_EOF,
                          biosim_snapshot_load_last(f, &hdr, &sim2, scores, &n, &gen, &rng));

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
    RUN_TEST(test_load_last_truncated_last_gen);
    RUN_TEST(test_load_last_no_complete_gen);
    RUN_TEST(test_population_load_file_larger_than_sim);
    RUN_TEST(test_population_load_file_smaller_than_sim);
    RUN_TEST(test_session_restore_identical_second_generation);
    return UNITY_END();
}
