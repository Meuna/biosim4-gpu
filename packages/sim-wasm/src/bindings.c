#include "biosim/core/barriers.h"
#include "biosim/core/census.h"
#include "biosim/core/challenge_spec.h"
#include "biosim/core/generation.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── hardcoded simulation parameters (same defaults as sim-ref) ─────────── */

/* clang-format off */
static const biosim_param_entry_t s_params[] = {
    {"population",                "simulation", {.i = 3000},  PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-x",               "simulation", {.i = 128},   PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-y",               "simulation", {.i = 128},   PARAM_INT,   false, true, NULL, NULL},
    {"steps-per-gen",             "simulation", {.i = 300},   PARAM_INT,   false, true, NULL, NULL},
    {"max-generations",           "simulation", {.i = 1000},  PARAM_INT,   false, true, NULL, NULL},
    {"max-genome-len",            "genome",     {.i = 24},    PARAM_INT,   false, true, NULL, NULL},
    {"max-neurons",               "genome",     {.i = 5},     PARAM_INT,   false, true, NULL, NULL},
    {"point-mutation-rate",       "genome",     {.f = 0.001}, PARAM_FLOAT, false, true, NULL, NULL},
    {"sexual-reproduction",       "genome",     {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"choose-parents-by-fitness", "genome",     {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"los-range",                 "sensors",    {.i = 16},    PARAM_INT,   false, true, NULL, NULL},
    {"sensor-radius",             "sensors",    {.i = 2},     PARAM_INT,   false, true, NULL, NULL},
    {"enable-kill",               "actions",    {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"responsiveness-curve-k",    "actions",    {.f = 2.0F},  PARAM_FLOAT, false, true, NULL, NULL},
};
/* clang-format on */
#define S_PARAMS_COUNT (sizeof(s_params) / sizeof(s_params[0]))

/* ── mutable parameter override table ───────────────────────────────────────
 * Lazy copy of s_params. biosim_wasm_set_param_* writes here; biosim_wasm_init
 * passes this table to biosim_params_init so overrides survive reinit.  */

static biosim_param_entry_t s_params_mut[S_PARAMS_COUNT];
static bool s_params_mut_ready = false;

static void params_mut_ensure(void) {
    if (!s_params_mut_ready) {
        memcpy(s_params_mut, s_params, sizeof(s_params));
        s_params_mut_ready = true;
    }
}

/* ── challenge state ─────────────────────────────────────────────────────── */

/* Module-level challenge spec. biosim_wasm_set_challenge_kind resets and sets
 * the kind; per-kind setters fill the union fields. biosim_wasm_init passes
 * &s_challenge to biosim_sim_create, so any override applied before init
 * takes effect on the next reinitialisation. */
static biosim_challenge_spec_t s_challenge = {
    .kind = BIOSIM_CHALLENGE_X_BAND,
    .x_band = {.x_min = 0.5F, .x_max = 1.0F, .mirror = false},
};

/* ── barrier state ───────────────────────────────────────────────────────── */

/* Module-level barrier list. biosim_wasm_clear_barriers resets the list;
 * biosim_wasm_add_barrier appends one spec. biosim_wasm_init passes
 * s_barriers/s_n_barriers to biosim_sim_create. The array grows on demand
 * via realloc; there is no hard upper limit. */
static biosim_barrier_spec_t *s_barriers = NULL;
static size_t s_barriers_cap = 0U;
static uint32_t s_n_barriers = 0U;

/* ── module state ────────────────────────────────────────────────────────── */

static biosim_sim_t s_sim;
static biosim_census_t s_last_census;
static bool s_initialized = false;

/* ── survivors ───────────────────────────────────────────────────────────────
 * Compact genome snapshot of the challenge-passing agents from the most recent
 * generation boundary. Stored in row-major survivor order (s * stride + j)
 * where stride == s_surv_stride_cap.
 *
 * All four buffers are allocated lazily in snapshot_survivors and grow on
 * demand (doubling policy). s_surv_pop_cap tracks the allocated slot count
 * (population dimension); s_surv_stride_cap tracks the allocated stride
 * (genome dimension). The snapshot stride may exceed the current
 * s_sim.genome.max_len after a reinit — spawn_generation always uses
 * s_surv_stride_cap, never the live max_len, to index these buffers.
 * s_saved_gen_rng captures s_sim.gen_rng before reproduction so that
 * biosim_wasm_restart_from_survivors can re-seed the same deterministic run. */

static uint16_t *s_surv_conn = NULL; /* compact: s * s_surv_stride_cap + j */
static int16_t *s_surv_wgt = NULL;   /* compact: s * s_surv_stride_cap + j */
static uint16_t *s_surv_len = NULL;  /* per survivor                       */
static float *s_surv_scores = NULL;  /* per survivor                       */
static uint32_t s_surv_count = 0U;
static uint32_t s_surv_pop_cap = 0U;    /* allocated survivor slots        */
static uint16_t s_surv_stride_cap = 0U; /* allocated stride per survivor   */
static uint64_t s_saved_gen_rng = 0ULL;

/* ── forward declarations ────────────────────────────────────────────────── */

static biosim_status_t spawn_generation(void);
EMSCRIPTEN_KEEPALIVE void biosim_wasm_free(void);

/* ── lifecycle ───────────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE int biosim_wasm_init(void) {
    biosim_log_init(&biosim_log_default_ctx);
    if (s_initialized) {
        biosim_sim_free(&s_sim);
        s_initialized = false;
    }
    memset(&s_sim, 0, sizeof(s_sim));
    memset(&s_last_census, 0, sizeof(s_last_census));
    params_mut_ensure();

    biosim_params_t p;
    biosim_status_t rc = biosim_params_init(&p, s_params_mut, S_PARAMS_COUNT);
    if (rc != BIOSIM_OK) {
        return (int)rc;
    }

    rc = biosim_sim_create(&s_sim, &p, &s_challenge, s_barriers, s_n_barriers);
    if (rc != BIOSIM_OK) {
        return (int)rc;
    }
    s_initialized = true;
    biosim_params_free(&p);
    if (rc != BIOSIM_OK) {
        return (int)rc;
    }

    rc = spawn_generation();
    if (rc != BIOSIM_OK) {
        biosim_wasm_free();
    }
    return (int)rc;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_free(void) {
    if (s_initialized) {
        biosim_sim_free(&s_sim);
        s_initialized = false;
    }
    memset(&s_sim, 0, sizeof(s_sim));
    memset(&s_last_census, 0, sizeof(s_last_census));
    free(s_surv_conn);
    free(s_surv_wgt);
    free(s_surv_len);
    free(s_surv_scores);
    s_surv_conn = NULL;
    s_surv_wgt = NULL;
    s_surv_len = NULL;
    s_surv_scores = NULL;
    s_surv_count = 0U;
    s_surv_pop_cap = 0U;
    s_surv_stride_cap = 0U;
    s_saved_gen_rng = 0ULL;
    free(s_barriers);
    s_barriers = NULL;
    s_barriers_cap = 0U;
    s_n_barriers = 0U;
}

/* ── parameter setters ───────────────────────────────────────────────────── */

/* Set a named integer parameter before the next biosim_wasm_init call.
 * Returns BIOSIM_OK, BIOSIM_ERR_NOTFOUND, or BIOSIM_ERR_TYPE.             */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_int(const char *name, int val) {
    params_mut_ensure();
    for (size_t i = 0U; i < S_PARAMS_COUNT; i++) {
        if (strcmp(s_params_mut[i].name, name) == 0) {
            if (s_params_mut[i].type != PARAM_INT) {
                return BIOSIM_ERR_TYPE;
            }
            s_params_mut[i].value.i = val;
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* Set a named float parameter before the next biosim_wasm_init call.      */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_float(const char *name, double val) {
    params_mut_ensure();
    for (size_t i = 0U; i < S_PARAMS_COUNT; i++) {
        if (strcmp(s_params_mut[i].name, name) == 0) {
            if (s_params_mut[i].type != PARAM_FLOAT) {
                return BIOSIM_ERR_TYPE;
            }
            s_params_mut[i].value.f = val;
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* Set a named boolean parameter before the next biosim_wasm_init call.
 * val: non-zero = true, 0 = false.                                        */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_bool(const char *name, int val) {
    params_mut_ensure();
    for (size_t i = 0U; i < S_PARAMS_COUNT; i++) {
        if (strcmp(s_params_mut[i].name, name) == 0) {
            if (s_params_mut[i].type != PARAM_BOOL) {
                return BIOSIM_ERR_TYPE;
            }
            s_params_mut[i].value.b = (val != 0);
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* ── challenge setters ───────────────────────────────────────────────────── */

/* Reset the challenge union and set the kind discriminant. Call this first
 * before any per-kind setter; it zeroes all union fields.                  */
EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_kind(int32_t kind) {
    memset(&s_challenge, 0, sizeof(s_challenge));
    s_challenge.kind = (biosim_challenge_kind_t)kind;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_x_band(float x_min, float x_max, int mirror) {
    s_challenge.x_band.x_min = x_min;
    s_challenge.x_band.x_max = x_max;
    s_challenge.x_band.mirror = (mirror != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_disc(
    float x, float y, float radius, int weighted
) {
    s_challenge.disc.x = x;
    s_challenge.disc.y = y;
    s_challenge.disc.radius = radius;
    s_challenge.disc.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_corners(float radius, int weighted) {
    s_challenge.corners.radius = radius;
    s_challenge.corners.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_neighbor_count(
    float radius, float min_n, float max_n, int exclude_border
) {
    s_challenge.neighbor_count.radius = radius;
    s_challenge.neighbor_count.min_n = min_n;
    s_challenge.neighbor_count.max_n = max_n;
    s_challenge.neighbor_count.exclude_border = (exclude_border != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_center_sparse(
    float x, float y, float outer_r, float inner_r, float min_n, float max_n, int weighted
) {
    s_challenge.center_sparse.x = x;
    s_challenge.center_sparse.y = y;
    s_challenge.center_sparse.outer_r = outer_r;
    s_challenge.center_sparse.inner_r = inner_r;
    s_challenge.center_sparse.min_n = min_n;
    s_challenge.center_sparse.max_n = max_n;
    s_challenge.center_sparse.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_near_barrier(float radius) {
    s_challenge.near_barrier.radius = radius;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_location_sequence(float radius) {
    s_challenge.location_sequence.radius = radius;
}

/* ── barrier setters ─────────────────────────────────────────────────────── */

/* Reset the barrier list. Call before adding a new set of barriers.        */
EMSCRIPTEN_KEEPALIVE void biosim_wasm_clear_barriers(void) {
    s_n_barriers = 0U;
}

static int barriers_grow(void) {
    if (s_n_barriers < s_barriers_cap) {
        return BIOSIM_OK;
    }
    size_t new_cap = s_barriers_cap == 0U ? 8U : s_barriers_cap * 2U;
    biosim_barrier_spec_t *grown = realloc(s_barriers, new_cap * sizeof(biosim_barrier_spec_t));
    if (grown == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    s_barriers = grown;
    s_barriers_cap = new_cap;
    return BIOSIM_OK;
}

/* Append one barrier spec to the list. x and y are grid cell coordinates;
 * pass -32768 (INT16_MIN = BIOSIM_BARRIER_POS_UNSET) for random placement.
 * length and width are in cells; pass 0.0 (BIOSIM_BARRIER_DIM_UNSET) for a
 * random dimension. Returns BIOSIM_OK or BIOSIM_ERR_NOMEM on allocation
 * failure.                                                                  */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_add_barrier(
    int kind, int x, int y, float length, float width
) {
    int rc = barriers_grow();
    if (rc != BIOSIM_OK) {
        return rc;
    }
    biosim_barrier_spec_t *b = &s_barriers[s_n_barriers];
    b->kind = (biosim_barrier_kind_t)kind;
    b->x = (int16_t)x;
    b->y = (int16_t)y;
    b->length = length;
    b->width = width;
    s_n_barriers++;
    return BIOSIM_OK;
}

/* Number of barriers currently in the list.                                */
EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_n_barriers(void) {
    return s_n_barriers;
}

/* ── step-level operations ───────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE int biosim_wasm_do_step(void) {
    for (uint32_t i = 0U; i < s_sim.agents.population; i++) {
        if (s_sim.agents.alive[i]) {
            biosim_sim_step_agent(&s_sim, i);
        }
    }
    biosim_sim_next_step(&s_sim);
    return BIOSIM_OK;
}

/* ── survivors operations ────────────────────────────────────────────────── */

static int survivors_grow(uint32_t n_survivors, uint16_t max_len) {
    uint32_t new_pop = s_surv_pop_cap;
    uint16_t new_stride = s_surv_stride_cap;
    if (n_survivors > s_surv_pop_cap) {
        new_pop = (s_surv_pop_cap * 2U >= n_survivors) ? s_surv_pop_cap * 2U : n_survivors;
    }
    if (max_len > s_surv_stride_cap) {
        uint16_t doubled = (uint16_t)(s_surv_stride_cap * 2U);
        new_stride = (doubled >= max_len) ? doubled : max_len;
    }
    if (new_pop == s_surv_pop_cap && new_stride == s_surv_stride_cap) {
        return BIOSIM_OK;
    }
    uint16_t *nc = realloc(s_surv_conn, (size_t)new_stride * new_pop * sizeof(uint16_t));
    int16_t *nw = realloc(s_surv_wgt, (size_t)new_stride * new_pop * sizeof(int16_t));
    uint16_t *nl = realloc(s_surv_len, (size_t)new_pop * sizeof(uint16_t));
    float *ns = realloc(s_surv_scores, (size_t)new_pop * sizeof(float));
    if (!nc || !nw || !nl || !ns) {
        free(nc);
        free(nw);
        free(nl);
        free(ns);
        return BIOSIM_ERR_NOMEM;
    }
    s_surv_conn = nc;
    s_surv_wgt = nw;
    s_surv_len = nl;
    s_surv_scores = ns;
    s_surv_pop_cap = new_pop;
    s_surv_stride_cap = new_stride;
    return BIOSIM_OK;
}

/* Save compact survivor genomes and the generation RNG seed.
 * survivors[]/scores[] come from biosim_generation_collect_survivors.
 * gen_rng is captured before reproduction so biosim_wasm_restart_from_survivors
 * can deterministically regenerate the same population.                     */
static int snapshot_survivors(
    const uint32_t *survivors, const float *scores, uint32_t n_survivors
) {
    const uint32_t pop = s_sim.agents.population;
    const uint16_t max_len = s_sim.genome.max_len;

    int rc = survivors_grow(n_survivors, max_len);
    if (rc != BIOSIM_OK) {
        return rc;
    }

    for (uint32_t s = 0U; s < n_survivors; s++) {
        const uint32_t src = survivors[s];
        const uint16_t len = s_sim.genome.len[src];
        s_surv_len[s] = len;
        for (uint16_t j = 0U; j < len; j++) {
            s_surv_conn[(size_t)s * s_surv_stride_cap + j] =
                s_sim.genome.conn[(size_t)j * pop + src];
            s_surv_wgt[(size_t)s * s_surv_stride_cap + j] = s_sim.genome.wgt[(size_t)j * pop + src];
        }
        s_surv_scores[s] = scores[s];
    }

    s_surv_count = n_survivors;
    s_saved_gen_rng = s_sim.gen_rng;
    return BIOSIM_OK;
}

/* Populate the grid for a new generation run.
 * Branches on s_surv_count: if survivors exist, breeds from the compact
 * genome; randomises otherwise.
 * Resets s_sim.step and s_sim.kills to zero on success.
 * Must be called after biosim_sim_create (or after snapshot_survivors) before any
 * simulation steps are taken.                                                */
static biosim_status_t spawn_generation(void) {
    biosim_status_t rc;

    if (s_surv_count > 0U) {
        const uint32_t pop = s_sim.agents.population;

        for (uint32_t s = 0U; s < s_surv_count; s++) {
            s_sim.genome.len[s] = s_surv_len[s];
            for (uint16_t j = 0U; j < s_surv_len[s]; j++) {
                s_sim.genome.conn[(size_t)j * pop + s] =
                    s_surv_conn[(size_t)s * s_surv_stride_cap + j];
                s_sim.genome.wgt[(size_t)j * pop + s] =
                    s_surv_wgt[(size_t)s * s_surv_stride_cap + j];
            }
        }

        uint32_t *survivors = malloc((size_t)s_surv_count * sizeof(uint32_t));
        float *scores = malloc((size_t)s_surv_count * sizeof(float));
        if (!survivors || !scores) {
            free(survivors);
            free(scores);
            return BIOSIM_ERR_NOMEM;
        }
        for (uint32_t s = 0U; s < s_surv_count; s++) {
            survivors[s] = s;
            scores[s] = s_surv_scores[s];
        }
        rc = biosim_generation_reproduce(&s_sim, survivors, scores, s_surv_count);
        free(survivors);
        free(scores);
    } else {
        rc = biosim_generation_init_random(&s_sim);
    }

    if (rc == BIOSIM_OK) {
        s_sim.step = 0U;
        s_sim.kills = 0U;
    }
    return rc;
}

/* Discard the saved survivors so the next init() starts from a random genome. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_clear_genome(void) {
    if (!s_initialized) {
        return BIOSIM_ERR_INVALID;
    }
    s_surv_count = 0U;
    return BIOSIM_OK;
}

/* Reproduce a new population from the saved survivors, using the same gen_rng
 * seed as the original reproduction.  Thanks to determinism, calling this
 * after biosim_wasm_next_generation yields the same starting population. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_restart_from_survivors(void) {
    if (!s_initialized) {
        return BIOSIM_ERR_INVALID;
    }
    s_sim.gen_rng = s_saved_gen_rng;
    return (int)spawn_generation();
}

/* ── generation-level operations ─────────────────────────────────────────── */

/* Advance one generation: collect survivors, save them, reproduce (or randomise
 * if none passed the challenge), then advance the generation counter.
 * biosim_sim_next_generation cannot be used here because it destroys survivor
 * data before we can snapshot it.                                            */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_next_generation(void) {
    const uint32_t pop = s_sim.agents.population;

    uint32_t *survivors = malloc((size_t)pop * sizeof(uint32_t));
    float *scores = malloc((size_t)pop * sizeof(float));
    if (!survivors || !scores) {
        free(survivors);
        free(scores);
        return BIOSIM_ERR_NOMEM;
    }

    uint32_t n_surv = biosim_generation_collect_survivors(&s_sim, survivors, scores);
    biosim_census_take(&s_sim, survivors, n_surv, &s_last_census);
    snapshot_survivors(survivors, scores, n_surv);

    free(survivors);
    free(scores);

    biosim_status_t rc = spawn_generation();
    if (rc == BIOSIM_OK) {
        s_sim.gen++;
    }
    return (int)rc;
}

/* ── state queries ───────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_gen(void) {
    return s_sim.gen;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_step(void) {
    return s_sim.step;
}

EMSCRIPTEN_KEEPALIVE int biosim_wasm_is_gen_complete(void) {
    return (s_sim.step >= s_sim.steps_per_gen) ? 1 : 0;
}

/* ── census results (valid after biosim_wasm_next_generation) ────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_gen(void) {
    return s_last_census.gen;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_population(void) {
    return s_last_census.population;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_survivors(void) {
    return s_last_census.survivors;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_kills(void) {
    return s_last_census.kills;
}

/* ── rendering queries ───────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_population(void) {
    return s_sim.agents.population;
}

EMSCRIPTEN_KEEPALIVE int32_t biosim_wasm_get_size_x(void) {
    return s_sim.size_x;
}

EMSCRIPTEN_KEEPALIVE int32_t biosim_wasm_get_size_y(void) {
    return s_sim.size_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_loc_x_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.loc_x;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_loc_y_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.loc_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_alive_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.alive;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_grid_cells_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.grid.cells;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_birth_x_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.birth_x;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_birth_y_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.birth_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_last_move_dir_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.last_move_dir;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_osc_period_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.osc_period;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_responsiveness_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.responsiveness;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_los_range_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.los_range;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_challenge_bits_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.challenge_bits;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_fingerprint_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.agents.genome_fingerprint;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_conn_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.nnet.genome_conn;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_wgt_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.nnet.genome_wgt;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_conn_length_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.nnet.conn_length;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_neuron_count_ptr(void) {
    return (uint32_t)(uintptr_t)s_sim.nnet.neuron_count;
}

/* ── genome size queries ─────────────────────────────────────────────────────
 * Scan alive agents for the maximum genome length / neuron count in use.
 * Used by Sub-plan D's compatibility gate to detect when a config change
 * would truncate the current population's genome.                           */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_genome_max_len_used(void) {
    if (!s_initialized) {
        return 0U;
    }
    uint32_t max = 0U;
    const uint32_t pop = s_sim.agents.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (s_sim.agents.alive[i] && (uint32_t)s_sim.genome.len[i] > max) {
            max = (uint32_t)s_sim.genome.len[i];
        }
    }
    return max;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_genome_max_neurons_used(void) {
    if (!s_initialized) {
        return 0U;
    }
    uint32_t max = 0U;
    const uint32_t pop = s_sim.agents.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (s_sim.agents.alive[i] && (uint32_t)s_sim.nnet.neuron_count[i] > max) {
            max = (uint32_t)s_sim.nnet.neuron_count[i];
        }
    }
    return max;
}
