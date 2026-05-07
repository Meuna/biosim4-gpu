#include "biosim/core/snapshot.h"

#include "biosim/core/generation.h"
#include "biosim/core/io_catalogue.h"

#include <stdlib.h>
#include <string.h>

/* ── file-format constants ───────────────────────────────────────────────── */

static const uint8_t snap_magic[4] = {0x42U, 0x53U, 0x4DU, 0x34U}; /* "BSM4" */

/* Byte offsets within the 32-byte file header. */
static const uint32_t snap_header_size = 32U;
static const uint32_t snap_gen_count_offset = 16U;

/* Fixed-size portion of every generation entry (bytes). */
static const uint32_t snap_gen_fixed_bytes = 24U;

/* Total size (bytes) of a generation entry with n survivors and genome_max_len genes. */
static uint64_t gen_entry_bytes(uint32_t n, uint16_t genome_max_len) {
    return (uint64_t)snap_gen_fixed_bytes + (uint64_t)n * 2U /* genome_length */
           + (uint64_t)n * (uint64_t)genome_max_len * 2U     /* genome_conn   */
           + (uint64_t)n * (uint64_t)genome_max_len * 2U     /* genome_wgt    */
           + (uint64_t)n * 4U;                               /* score         */
}

/* ── low-level I/O helpers ───────────────────────────────────────────────── */

static int write_u8(FILE *f, uint8_t v) {
    return fwrite(&v, 1U, 1U, f) == 1U;
}
static int write_u16(FILE *f, uint16_t v) {
    return fwrite(&v, 2U, 1U, f) == 1U;
}
static int write_u32(FILE *f, uint32_t v) {
    return fwrite(&v, 4U, 1U, f) == 1U;
}
static int write_u64(FILE *f, uint64_t v) {
    return fwrite(&v, 8U, 1U, f) == 1U;
}
static int read_u16(FILE *f, uint16_t *v) {
    return fread(v, 2U, 1U, f) == 1U;
}
static int read_u32(FILE *f, uint32_t *v) {
    return fread(v, 4U, 1U, f) == 1U;
}
static int read_u64(FILE *f, uint64_t *v) {
    return fread(v, 8U, 1U, f) == 1U;
}

/* ── write ───────────────────────────────────────────────────────────────── */

biosim_status_t biosim_snapshot_write_header(FILE *f, const biosim_sim_t *sim) {
    static const uint8_t reserved12[12] = {0};

    if (fwrite(snap_magic, 1U, 4U, f) != 4U) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u16(f, BIOSIM_SNAP_FORMAT_VERSION)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u16(f, BIOSIM_IO_SCHEMA_VERSION)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u16(f, (uint16_t)BIOSIM_NUM_SENSORS)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u16(f, (uint16_t)BIOSIM_NUM_ACTIONS)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u16(f, sim->genome.max_len)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u8(f, sim->nnet.max_neurons)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u8(f, 0U)) { /* reserved pad */
        return BIOSIM_ERR_IO;
    }
    if (!write_u32(f, 0U)) { /* generation_count: patched on finalize */
        return BIOSIM_ERR_IO;
    }
    if (fwrite(reserved12, 1U, 12U, f) != 12U) {
        return BIOSIM_ERR_IO;
    }
    return BIOSIM_OK;
}

biosim_status_t biosim_snapshot_write_genome(FILE *f, const biosim_sim_t *sim,
                                             const uint32_t *survivors, const float *scores,
                                             uint32_t n_survivors) {
    const biosim_genome_t *genome = &sim->genome;
    const uint32_t pop = genome->population;
    const uint16_t genome_max_len = sim->genome.max_len;

    if (!write_u64(f, gen_entry_bytes(n_survivors, genome_max_len))) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u32(f, sim->gen)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u32(f, n_survivors)) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u64(f, sim->gen_rng)) {
        return BIOSIM_ERR_IO;
    }

    /* genome_length array */
    for (uint32_t s = 0U; s < n_survivors; s++) {
        if (!write_u16(f, genome->len[survivors[s]])) {
            return BIOSIM_ERR_IO;
        }
    }

    /* Reusable row buffer for genome_conn and genome_wgt (same element size). */
    /* alloc start here, free on exit label */
    uint16_t *row = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    row = (uint16_t *)malloc((size_t)n_survivors * sizeof(uint16_t));
    if (row == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    /* genome_conn: one row (gene slot) at a time */
    for (uint16_t j = 0U; j < genome_max_len; j++) {
        for (uint32_t s = 0U; s < n_survivors; s++) {
            row[s] = genome->conn[(size_t)j * pop + survivors[s]];
        }
        if (fwrite(row, sizeof(uint16_t), n_survivors, f) != n_survivors) {
            returncode = BIOSIM_ERR_IO;
            goto exit;
        }
    }

    /* genome_wgt: int16_t stored as uint16_t bits */
    for (uint16_t j = 0U; j < genome_max_len; j++) {
        for (uint32_t s = 0U; s < n_survivors; s++) {
            uint16_t bits;
            (void)memcpy(&bits, &genome->wgt[(size_t)j * pop + survivors[s]], sizeof(bits));
            row[s] = bits;
        }
        if (fwrite(row, sizeof(uint16_t), n_survivors, f) != n_survivors) {
            returncode = BIOSIM_ERR_IO;
            goto exit;
        }
    }

    /* scores */
    if (fwrite(scores, sizeof(float), n_survivors, f) != n_survivors) {
        returncode = BIOSIM_ERR_IO;
        goto exit;
    }

exit:
    free(row);
    return returncode;
}

biosim_status_t biosim_snapshot_finalize(FILE *f, uint32_t generation_count) {
    if (fseek(f, (long)snap_gen_count_offset, SEEK_SET) != 0) {
        return BIOSIM_ERR_IO;
    }
    if (!write_u32(f, generation_count)) {
        return BIOSIM_ERR_IO;
    }
    return BIOSIM_OK;
}

/* ── read ────────────────────────────────────────────────────────────────── */

biosim_status_t biosim_snapshot_read_header(FILE *f, biosim_snap_header_t *header_out) {
    if (fseek(f, 0, SEEK_SET) != 0) {
        return BIOSIM_ERR_IO;
    }

    uint8_t magic[4];
    if (fread(magic, 1U, 4U, f) != 4U) {
        return BIOSIM_ERR_IO;
    }
    if (memcmp(magic, snap_magic, 4U) != 0) {
        return BIOSIM_ERR_INVALID;
    }

    if (!read_u16(f, &header_out->format_version)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u16(f, &header_out->schema_version)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u16(f, &header_out->num_sensors)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u16(f, &header_out->num_actions)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u16(f, &header_out->genome_max_len)) {
        return BIOSIM_ERR_IO;
    }
    if (fread(&header_out->max_neurons, 1U, 1U, f) != 1U) {
        return BIOSIM_ERR_IO;
    }
    uint8_t pad;
    if (fread(&pad, 1U, 1U, f) != 1U) { /* reserved */
        return BIOSIM_ERR_IO;
    }
    if (!read_u32(f, &header_out->generation_count)) {
        return BIOSIM_ERR_IO;
    }
    /* skip 12 reserved bytes at offsets 20-31 → leaves f at offset 32 */
    if (fseek(f, 12, SEEK_CUR) != 0) {
        return BIOSIM_ERR_IO;
    }

    return BIOSIM_OK;
}

/*
 * Load a genome entry at the current file position into sim->genome slots
 * 0..n-1, where n = min(pop_file, pop_sim).
 * Reads the full entry even when pop_file > pop_sim or genome_len_file > genome_len_sim,
 * discarding excess to advance the file position correctly.
 */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static biosim_status_t load_genome(FILE *f, const biosim_snap_header_t *header, biosim_sim_t *sim,
                                   float *scores_out, uint32_t *n_survivors_out,
                                   uint32_t *gen_idx_out, uint64_t *gen_rng_out) {
    uint64_t entry_size;
    uint32_t pop_file;
    uint32_t gen_idx;
    uint64_t gen_rng;

    if (!read_u64(f, &entry_size)) {
        return BIOSIM_ERR_IO;
    }
    (void)entry_size; /* advances file position; value not needed here */
    if (!read_u32(f, &gen_idx)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u32(f, &pop_file)) {
        return BIOSIM_ERR_IO;
    }
    if (!read_u64(f, &gen_rng)) {
        return BIOSIM_ERR_IO;
    }

    const uint32_t pop_sim = sim->genome.population;
    const uint32_t pop_load = pop_file < pop_sim ? pop_file : pop_sim;
    const uint16_t g_max_len = header->genome_max_len;

    /* alloc start here, free on exit label */
    uint16_t *row = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    row = (uint16_t *)malloc((size_t)pop_file * sizeof(uint16_t));
    if (row == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    /* genome_length */
    if (fread(row, sizeof(uint16_t), pop_file, f) != pop_file) {
        returncode = BIOSIM_ERR_IO;
        goto exit;
    }
    for (uint32_t s = 0U; s < pop_load; s++) {
        uint16_t g_len = row[s];
        if (g_len > g_max_len) {
            BIOSIM_ERRORF(&sim->log,
                          "biosim-snapshot: corrupted file (genome length %u > max length %u)",
                          g_len, g_max_len);
            returncode = BIOSIM_ERR_INVALID;
            goto exit;
        }
        sim->genome.len[s] = g_len;
    }

    /* genome_conn */
    for (uint16_t j = 0U; j < g_max_len; j++) {
        if (fread(row, sizeof(uint16_t), pop_file, f) != pop_file) {
            returncode = BIOSIM_ERR_IO;
            goto exit;
        }
        for (uint32_t s = 0U; s < pop_load; s++) {
            sim->genome.conn[(size_t)j * pop_sim + s] = row[s];
        }
    }

    /* genome_wgt: int16_t stored as uint16_t bits */
    for (uint16_t j = 0U; j < g_max_len; j++) {
        if (fread(row, sizeof(uint16_t), pop_file, f) != pop_file) {
            returncode = BIOSIM_ERR_IO;
            goto exit;
        }
        for (uint32_t s = 0U; s < pop_load; s++) {
            int16_t w;
            (void)memcpy(&w, &row[s], sizeof(w));
            sim->genome.wgt[(size_t)j * pop_sim + s] = w;
        }
    }

    /* scores: read pop_load entries; seek past excess */
    if (fread(scores_out, sizeof(float), pop_load, f) != pop_load) {
        returncode = BIOSIM_ERR_IO;
        goto exit;
    }
    if (fseek(f, (long)(pop_file - pop_load) * 4L, SEEK_CUR) != 0) {
        returncode = BIOSIM_ERR_IO;
        goto exit;
    }

    *n_survivors_out = pop_load;
    *gen_idx_out = gen_idx;
    *gen_rng_out = gen_rng;

exit:
    free(row);
    return returncode;
}

biosim_status_t biosim_snapshot_load(FILE *f, uint32_t gen_idx, const biosim_snap_header_t *header,
                                     biosim_sim_t *sim, float *scores_out,
                                     uint32_t *n_survivors_out, uint32_t *gen_idx_out,
                                     uint64_t *gen_rng_out) {
    if (fseek(f, (long)snap_header_size, SEEK_SET) != 0) {
        return BIOSIM_ERR_IO;
    }

    for (uint32_t i = 0U; i < gen_idx; i++) {
        uint64_t entry_size;
        if (!read_u64(f, &entry_size)) {
            return BIOSIM_ERR_NOTFOUND;
        }
        if (entry_size < snap_gen_fixed_bytes) {
            return BIOSIM_ERR_INVALID;
        }
        /* skip remainder of this entry (we already read the 8-byte size field) */
        if (fseek(f, (long)(entry_size - 8U), SEEK_CUR) != 0) {
            return BIOSIM_ERR_IO;
        }
    }

    return load_genome(f, header, sim, scores_out, n_survivors_out, gen_idx_out, gen_rng_out);
}

biosim_status_t biosim_snapshot_load_last(FILE *f, const biosim_snap_header_t *header,
                                          biosim_sim_t *sim, float *scores_out,
                                          uint32_t *n_survivors_out, uint32_t *gen_idx_out,
                                          uint64_t *gen_rng_out) {
    if (header->generation_count > 0U) {
        return biosim_snapshot_load(f, header->generation_count - 1U, header, sim, scores_out,
                                    n_survivors_out, gen_idx_out, gen_rng_out);
    }

    /* generation_count unknown — scan forward to find the last valid entry */
    if (fseek(f, 0, SEEK_END) != 0) {
        return BIOSIM_ERR_IO;
    }
    long file_size = ftell(f);
    if (file_size < 0) {
        return BIOSIM_ERR_IO;
    }

    long pos = (long)snap_header_size;
    long last_pos = -1L;
    if (fseek(f, pos, SEEK_SET) != 0) {
        return BIOSIM_ERR_IO;
    }

    while (pos < file_size) {
        uint64_t entry_size;
        if (fread(&entry_size, 8U, 1U, f) != 1U) {
            break;
        }
        if (entry_size < (uint64_t)snap_gen_fixed_bytes) {
            break;
        }
        if (pos + (long)entry_size > file_size) { /* truncated entry */
            break;
        }
        last_pos = pos;
        pos += (long)entry_size;
        if (fseek(f, pos, SEEK_SET) != 0) {
            break;
        }
    }

    if (last_pos < 0L) {
        return BIOSIM_ERR_NOTFOUND;
    }
    if (fseek(f, last_pos, SEEK_SET) != 0) {
        return BIOSIM_ERR_IO;
    }
    return load_genome(f, header, sim, scores_out, n_survivors_out, gen_idx_out, gen_rng_out);
}

/* ── coherency check ────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static biosim_status_t check_compat(const biosim_snap_header_t *hdr, const biosim_sim_t *sim) {
    if (hdr->format_version != BIOSIM_SNAP_FORMAT_VERSION) {
        BIOSIM_ERRORF(&sim->log,
                      "biosim-snapshot: format version %u in file, built with %u — incompatible",
                      (unsigned)hdr->format_version, (unsigned)BIOSIM_SNAP_FORMAT_VERSION);
        return BIOSIM_ERR_INVALID;
    }
    if (hdr->schema_version != BIOSIM_IO_SCHEMA_VERSION) {
        BIOSIM_ERRORF(&sim->log,
                      "biosim-snapshot: schema version %u in file, built with %u — incompatible",
                      (unsigned)hdr->schema_version, (unsigned)BIOSIM_IO_SCHEMA_VERSION);
        return BIOSIM_ERR_INVALID;
    }
    if (hdr->num_sensors != (uint16_t)BIOSIM_NUM_SENSORS ||
        hdr->num_actions != (uint16_t)BIOSIM_NUM_ACTIONS) {
        BIOSIM_ERRORF(
            &sim->log,
            "biosim-snapshot: I/O catalogue (%u sensors, %u actions) does not match built-in"
            " (%u sensors, %u actions) — corrupted file or implementation issue",
            (unsigned)hdr->num_sensors, (unsigned)hdr->num_actions, (unsigned)BIOSIM_NUM_SENSORS,
            (unsigned)BIOSIM_NUM_ACTIONS);
        return BIOSIM_ERR_INVALID;
    }

    if (sim->genome.max_len < hdr->genome_max_len) {
        BIOSIM_WARNF(&sim->log,
                     "biosim-snapshot: file genome-max-len=%u exceeds current %u;"
                     " use --max-genome-len %u or larger",
                     (unsigned)hdr->genome_max_len, (unsigned)sim->genome.max_len,
                     (unsigned)hdr->genome_max_len);
        return BIOSIM_ERR_INVALID;
    }
    if (hdr->max_neurons != sim->nnet.max_neurons) {
        BIOSIM_WARNF(&sim->log,
                     "biosim-snapshot: file max-neurons=%u but current is %u;"
                     " use --max-neurons %u",
                     (unsigned)hdr->max_neurons, (unsigned)sim->nnet.max_neurons,
                     (unsigned)hdr->max_neurons);
        return BIOSIM_ERR_INVALID;
    }
    return BIOSIM_OK;
}

/* ── high-level read: restore ───────────────────────────────────────────── */

/* Opens path, reads and validates the header, and runs all compat checks.
 * On success, sets *f_out to the open file positioned past the header and
 * *hdr_out to the parsed header. Caller must fclose *f_out. */
static biosim_status_t restore_open(const char *path, biosim_sim_t *sim, FILE **f_out,
                                    biosim_snap_header_t *hdr_out) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: cannot open '%s'", path);
        return BIOSIM_ERR_IO;
    }
    biosim_status_t st = biosim_snapshot_read_header(f, hdr_out);
    if (st != BIOSIM_OK) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: invalid file '%s'", path);
        (void)fclose(f);
        return st;
    }
    st = check_compat(hdr_out, sim);
    if (st != BIOSIM_OK) {
        (void)fclose(f);
        return st;
    }
    *f_out = f;
    return BIOSIM_OK;
}

biosim_status_t biosim_snapshot_restore(const char *path, biosim_sim_t *sim) {
    FILE *f = NULL;
    biosim_snap_header_t hdr;
    biosim_status_t st = restore_open(path, sim, &f, &hdr);
    if (st != BIOSIM_OK) {
        return st;
    }

    /* alloc start here, free on exit label */
    float *scores = NULL;
    uint32_t *survivors = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    scores = (float *)malloc((size_t)sim->genome.population * sizeof(float));
    if (scores == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    uint32_t n_surv = 0U;
    uint32_t gen_idx = 0U;
    uint64_t gen_rng = 0U;
    returncode = biosim_snapshot_load_last(f, &hdr, sim, scores, &n_surv, &gen_idx, &gen_rng);
    (void)fclose(f);

    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: failed to read from '%s'", path);
        goto exit;
    }

    if (n_surv == 0U) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: '%s' contains no survivors", path);
        returncode = BIOSIM_ERR_INVALID;
        goto exit;
    }

    sim->gen = gen_idx;
    sim->gen_rng = gen_rng;

    survivors = (uint32_t *)malloc((size_t)n_surv * sizeof(uint32_t));
    if (survivors == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    for (uint32_t s = 0U; s < n_surv; s++) {
        survivors[s] = s;
    }

    returncode = biosim_generation_reproduce(sim, survivors, scores, n_surv);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    sim->kills = 0U;
    sim->step = 0U;
    sim->gen++;

exit:
    free(scores);
    free(survivors);
    return returncode;
}

/* ── high-level write: sessions ─────────────────────────────────────────── */

static int snap_should_write(uint32_t gen, uint32_t max_generation, uint32_t interval) {
    if (interval > 0) {
        return gen % interval == 0;
    }
    return gen == max_generation - 1;
}

biosim_status_t biosim_snapshot_session_open(biosim_sim_t *sim, const char *path, int interval) {
    FILE *probe = fopen(path, "rb");
    if (probe != NULL) {
        (void)fclose(probe);
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: output file '%s' already exists", path);
        return BIOSIM_ERR_INVALID;
    }

    FILE *f = fopen(path, "w+b");
    if (f == NULL) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: cannot create '%s'", path);
        return BIOSIM_ERR_IO;
    }

    biosim_status_t st = biosim_snapshot_write_header(f, sim);
    if (st != BIOSIM_OK) {
        (void)fclose(f);
        return st;
    }

    sim->snap_f = f;
    sim->snap_written_count = 0U;
    sim->snap_interval = interval;
    return BIOSIM_OK;
}

biosim_status_t biosim_snapshot_session_write(biosim_sim_t *sim, const uint32_t *survivors,
                                              const float *scores, uint32_t n_survivors) {
    if (sim->snap_f == NULL || n_survivors == 0U) {
        return BIOSIM_OK;
    }
    if (!snap_should_write(sim->gen, sim->max_generations, sim->snap_interval)) {
        return BIOSIM_OK;
    }

    biosim_status_t st =
        biosim_snapshot_write_genome(sim->snap_f, sim, survivors, scores, n_survivors);
    if (st != BIOSIM_OK) {
        BIOSIM_ERRORF(&sim->log, "biosim-snapshot: write failed (%s)", biosim_strerror(st));
        return st;
    }

    sim->snap_written_count++;
    return BIOSIM_OK;
}

biosim_status_t biosim_snapshot_session_close(biosim_sim_t *sim) {
    if (sim->snap_f == NULL) {
        return BIOSIM_OK;
    }
    biosim_status_t st = biosim_snapshot_finalize(sim->snap_f, sim->snap_written_count);
    (void)fclose(sim->snap_f);
    sim->snap_f = NULL;
    return st;
}
