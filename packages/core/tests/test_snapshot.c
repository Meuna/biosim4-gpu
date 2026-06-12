#include "biosim/core/snapshot.h"
#include "biosim/core/test_utils.h"

#include "biosim/core/census.h"
#include "biosim/core/generation.h"
#include "biosim/core/io_eval.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

/*
 * Build a snap from an explicit agent-index array + scores array so that
 * low-level tests can still call biosim_snapshot_write_genome directly.
 */
static biosim_status_t build_snap_from_agents(
    const biosim_sim_t *sim,
    const uint32_t *survivors,
    const float *scores,
    uint32_t n_surv,
    biosim_survivor_snap_t *snap
) {
    biosim_status_t st = biosim_survivor_snap_grow(snap, n_surv, sim->genome.max_len);
    if (st != BIOSIM_OK) {
        return st;
    }
    snap->gen = sim->gen;
    snap->gen_rng = sim->gen_rng;
    for (uint32_t s = 0U; s < n_surv; s++) {
        uint32_t src = survivors[s];
        snap->len[s] = sim->genome.len[src];
        snap->scores[s] = scores[s];
        for (uint16_t j = 0U; j < snap->len[s]; j++) {
            snap->conn[(size_t)s * snap->stride_cap + j] =
                sim->genome.conn[(size_t)j * sim->genome.population + src];
            snap->wgt[(size_t)s * snap->stride_cap + j] =
                sim->genome.wgt[(size_t)j * sim->genome.population + src];
        }
    }
    snap->count = n_surv;
    return BIOSIM_OK;
}

/* ── fixture ─────────────────────────────────────────────────────────────── */

static biosim_sim_t sim;
static biosim_survivor_snap_t snap;

static const uint32_t k_fix_survivors[4] = {0U, 1U, 2U, 3U};
static const float k_fix_scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, build_snap_from_agents(&sim, k_fix_survivors, k_fix_scores, 4U, &snap)
    );
}
void tearDown(void) {
    biosim_sim_free(&sim);
    biosim_survivor_snap_free(&snap);
}

/* ── tests ──────────────────────────────────────────────────────────────── */

void test_write_header_correct_magic(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    (void)fseek(f, 0L, SEEK_SET);
    TEST_ASSERT_EQUAL_UINT8(0x42U, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x53U, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x4DU, (uint8_t)fgetc(f));
    TEST_ASSERT_EQUAL_UINT8(0x34U, (uint8_t)fgetc(f));

    (void)fclose(f);
}

void test_read_header_roundtrip(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

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
}

void test_bad_magic_returns_invalid(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    (void)fseek(f, 0L, SEEK_SET);
    (void)fputc(0x00, f); /* corrupt magic */
    (void)fseek(f, 0L, SEEK_SET);
    biosim_snap_header_t hdr;
    TEST_ASSERT_EQUAL_INT(BIOSIM_ERR_INVALID, biosim_snapshot_read_header(f, &hdr));

    (void)fclose(f);
}

void test_genome_roundtrip_single_record(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_roundtrip.bsm4";
    (void)remove(path);

    uint32_t survivors[4] = {0U, 1U, 2U, 3U};
    uint32_t n_surv = 4U;
    float scores[4] = {0.1F, 0.2F, 0.3F, 0.4F};

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, &snap));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));
    (void)fclose(f);

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim2, &rsnap));

    TEST_ASSERT_EQUAL_UINT32(n_surv, rsnap.count);
    TEST_ASSERT_EQUAL_UINT32(sim.gen, sim2.gen);
    TEST_ASSERT_EQUAL_UINT64(sim.gen_rng, sim2.gen_rng);
    TEST_ASSERT_EQUAL_UINT32(snap.gen, rsnap.gen);
    TEST_ASSERT_EQUAL_UINT64(snap.gen_rng, rsnap.gen_rng);
    /* The originating config's caps are restored from the file header. */
    TEST_ASSERT_EQUAL_UINT16(sim.genome.max_len, rsnap.genome_max_len);
    TEST_ASSERT_EQUAL_UINT8(sim.nnet.max_neurons, rsnap.max_neurons);
    for (uint32_t s = 0U; s < n_surv; s++) {
        TEST_ASSERT_EQUAL_FLOAT(scores[s], rsnap.scores[s]);
        TEST_ASSERT_EQUAL_UINT16(sim.genome.len[survivors[s]], rsnap.len[s]);
        for (uint16_t j = 0U; j < rsnap.len[s]; j++) {
            TEST_ASSERT_EQUAL_UINT16(
                sim.genome.conn[(size_t)j * sim.genome.population + survivors[s]],
                rsnap.conn[(size_t)s * rsnap.stride_cap + j]
            );
            TEST_ASSERT_EQUAL_INT16(
                sim.genome.wgt[(size_t)j * sim.genome.population + survivors[s]],
                rsnap.wgt[(size_t)s * rsnap.stride_cap + j]
            );
        }
    }

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim2);
}

void test_multi_gen_load_last(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_multi_gen.bsm4";
    (void)remove(path);

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    for (uint32_t g = 0U; g < 3U; g++) {
        snap.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, &snap));
    }
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 3U));
    (void)fclose(f);

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim2, &rsnap));

    /* load_survivors should return the last gen (gen_idx=2) */
    TEST_ASSERT_EQUAL_UINT32(2U, sim2.gen);
    TEST_ASSERT_EQUAL_UINT32(2U, rsnap.gen);
    TEST_ASSERT_EQUAL_UINT32(4U, rsnap.count);

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim2);
}

void test_multi_gen_scan_without_gen_count(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_scan.bsm4";
    (void)remove(path);

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    for (uint32_t g = 0U; g < 3U; g++) {
        snap.gen = g;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, &snap));
    }
    /* Deliberately do NOT call finalize, so generation_count stays 0 */
    (void)fclose(f);

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim2, &rsnap));

    TEST_ASSERT_EQUAL_UINT32(2U, sim2.gen);

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim2);
}

/*
 * Two generations on sim1; snapshot written after gen 0 via session API.
 * sim2 is restored and also runs gen 1.  After the second generation both sims
 * must be numerically identical across all sub-structs (agents, grid, genome, nnet).
 */
void test_session_restore_identical_second_generation(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_e2e_test.bsm4";
    (void)remove(path);

    biosim_census_t census;

    biosim_sim_t sim1;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_32x32(&sim1));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_session_open(&sim1, path, 1));

    /* gen 0: step×8, retire, spawn */
    sim_test_run_one_gen(&sim1);

    biosim_survivor_snap_t snap1 = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_retire_generation(&sim1, &snap1, &census));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim1, &snap1));
    /* sim1.gen == 1 */

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_session_close(&sim1));

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_32x32(&sim2));

    biosim_survivor_snap_t snap2 = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim2, &snap2));
    sim2.gen++; /* advance past the loaded generation, matching sim1 after retire */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim2, &snap2));

    /* Run both sims to the end of gen 1 */
    sim_test_run_one_gen(&sim1);
    sim_test_run_one_gen(&sim2);

    assert_sim_equal(&sim1, &sim2);

    /* Advance to gen 2 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_retire_generation(&sim1, &snap1, &census));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim1, &snap1));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_retire_generation(&sim2, &snap2, &census));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim2, &snap2));

    assert_sim_equal(&sim1, &sim2);

    (void)remove(path);
    biosim_survivor_snap_free(&snap1);
    biosim_survivor_snap_free(&snap2);
    biosim_sim_free(&sim1);
    biosim_sim_free(&sim2);
}

/* generation_count=1 in header but file ends after the 8-byte entry_size field */
void test_load_last_truncated_last_gen(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_truncated.bsm4";
    (void)remove(path);

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));

    /* Write only the entry_size prefix — no payload follows */
    uint64_t fake_size = 1000U;
    (void)fwrite(&fake_size, 8U, 1U, f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));
    (void)fclose(f);

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_EOF, biosim_snapshot_load_survivors(path, &sim2, &rsnap));

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim2);
}

/* generation_count=0 (scan mode) with header only — no complete generation */
void test_load_last_no_complete_gen(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_no_gen.bsm4";
    (void)remove(path);

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    /* No genome entries; generation_count stays 0 */
    (void)fclose(f);

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_EOF, biosim_snapshot_load_survivors(path, &sim2, &rsnap));

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim2);
}

/* load_survivors with pop_file (6) > pop_sim (4): snap->count must equal pop_sim. */
void test_load_survivors_pop_file_larger(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_load_surv_large.bsm4";
    (void)remove(path);

    biosim_sim_t sim_write;
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK,
        sim_test_create(
            &sim_write,
            &(sim_test_scn_t){
                .population = 6U,
                .size_x = 4,
                .size_y = 4,
                .genome_max_len = 4U,
                .max_neurons = 2U,
                .los_range = 4U,
                .steps_per_gen = 1U,
                .sensor_radius = 1,
            }
        )
    );

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim_write));

    uint32_t survivors[6] = {0U, 1U, 2U, 3U, 4U, 5U};
    float scores[6] = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F};
    biosim_survivor_snap_t wsnap = {0};
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, build_snap_from_agents(&sim_write, survivors, scores, 6U, &wsnap)
    );
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim_write, &wsnap));
    biosim_survivor_snap_free(&wsnap);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));
    (void)fclose(f);

    biosim_sim_t sim_read;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim_read)); /* pop = 4 */

    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim_read, &rsnap));

    TEST_ASSERT_EQUAL_UINT32(4U, rsnap.count); /* truncated to pop_sim */
    for (uint32_t s = 0U; s < rsnap.count; s++) {
        TEST_ASSERT_EQUAL_FLOAT(scores[s], rsnap.scores[s]);
    }

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim_write);
    biosim_sim_free(&sim_read);
}

/* load_survivors with pop_file (4) < pop_sim (6): snap->count must equal pop_file. */
void test_load_survivors_pop_file_smaller(void) {
    static const char path[] = BIOSIM_TEST_TMPDIR "/biosim_snap_load_surv_small.bsm4";
    (void)remove(path);

    FILE *f = fopen(path, "w+b");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim)); /* pop = 4 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, &snap));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));
    (void)fclose(f);

    biosim_sim_t sim_read;
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK,
        sim_test_create(
            &sim_read,
            &(sim_test_scn_t){
                .population = 6U,
                .size_x = 4,
                .size_y = 4,
                .genome_max_len = 4U,
                .max_neurons = 2U,
                .los_range = 4U,
                .steps_per_gen = 1U,
                .sensor_radius = 1,
            }
        )
    );

    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_load_survivors(path, &sim_read, &rsnap));

    TEST_ASSERT_EQUAL_UINT32(4U, rsnap.count); /* all pop_file entries loaded */
    for (uint32_t s = 0U; s < rsnap.count; s++) {
        TEST_ASSERT_EQUAL_FLOAT(snap.scores[s], rsnap.scores[s]);
    }

    (void)remove(path);
    biosim_survivor_snap_free(&rsnap);
    biosim_sim_free(&sim_read);
}

/* biosim_snapshot_load_survivors_f: write to tmpfile, seek back, call _f.
 * _f does not update sim; only snap->gen / snap->gen_rng are set. */
void test_load_survivors_f_roundtrip(void) {
    FILE *f = tmpfile();
    TEST_ASSERT_NOT_NULL(f);

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_header(f, &sim));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_write_genome(f, &sim, &snap));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_snapshot_finalize(f, 1U));

    biosim_sim_t sim2;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim2));
    biosim_survivor_snap_t rsnap = {0};
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK, biosim_snapshot_load_survivors_f(f, sim2.genome.population, &rsnap)
    );

    TEST_ASSERT_EQUAL_UINT32(snap.count, rsnap.count);
    TEST_ASSERT_EQUAL_UINT32(snap.gen, rsnap.gen);
    TEST_ASSERT_EQUAL_UINT64(snap.gen_rng, rsnap.gen_rng);
    for (uint32_t s = 0U; s < rsnap.count; s++) {
        TEST_ASSERT_EQUAL_FLOAT(snap.scores[s], rsnap.scores[s]);
        TEST_ASSERT_EQUAL_UINT16(snap.len[s], rsnap.len[s]);
    }

    (void)fclose(f);
    biosim_survivor_snap_free(&rsnap);
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
    RUN_TEST(test_load_last_truncated_last_gen);
    RUN_TEST(test_load_last_no_complete_gen);
    RUN_TEST(test_load_survivors_pop_file_larger);
    RUN_TEST(test_load_survivors_pop_file_smaller);
    RUN_TEST(test_session_restore_identical_second_generation);
    RUN_TEST(test_load_survivors_f_roundtrip);
    return UNITY_END();
}
