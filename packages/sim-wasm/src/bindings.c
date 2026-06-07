#include "biosim/core/barriers.h"
#include "biosim/core/census.h"
#include "biosim/core/challenge_spec.h"
#include "biosim/core/generation.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"
#include "biosim/core/snapshot.h"
#include "biosim/core/status.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── hardcoded simulation parameters (same defaults as sim-ref) ─────────── */

/* clang-format off */
static const biosim_param_entry_t params[] = {
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
#define PARAMS_COUNT (sizeof(params) / sizeof(params[0]))

/* ── mutable parameter override table ───────────────────────────────────────
 * Lazy copy of params. biosim_wasm_set_param_* writes here; biosim_wasm_init
 * passes this table to biosim_params_init so overrides survive reinit. */

static biosim_param_entry_t params_mut[PARAMS_COUNT];
static bool params_mut_ready = false;

static void params_mut_ensure(void) {
    if (!params_mut_ready) {
        memcpy(params_mut, params, sizeof(params));
        params_mut_ready = true;
    }
}

/* ── challenge state ─────────────────────────────────────────────────────── */

/* Module-level challenge spec. biosim_wasm_set_challenge_kind resets and sets
 * the kind; per-kind setters fill the union fields. biosim_wasm_init passes
 * &challenge to biosim_sim_create, so any override applied before init
 * takes effect on the next reinitialisation. */
static biosim_challenge_spec_t challenge = {
    .kind = BIOSIM_CHALLENGE_X_BAND,
    .x_band = {.x_min = 0.5F, .x_max = 1.0F, .mirror = false},
};

/* ── barrier state ───────────────────────────────────────────────────────── */

/* Module-level barrier list. biosim_wasm_clear_barriers resets the list;
 * biosim_wasm_add_barrier appends one spec. biosim_wasm_init passes
 * barriers/n_barriers to biosim_sim_create. The array grows on demand
 * via realloc; there is no hard upper limit. */
static biosim_barrier_spec_t *barriers = NULL;
static size_t barriers_cap = 0U;
static uint32_t n_barriers = 0U;

/* ── module state ────────────────────────────────────────────────────────── */

static biosim_sim_t sim;
static biosim_census_t last_census;
static bool initialized = false;

/* ── survivors ───────────────────────────────────────────────────────────────
 * Compact genome snapshot of the challenge-passing agents from the most recent
 * generation boundary.  Populated by biosim_generation_collect_survivors at
 * each generation boundary; consumed by biosim_generation_spawn.
 * snap.gen_rng captures sim.gen_rng before reproduction so that
 * biosim_wasm_rewind can re-seed the same deterministic run. */

static biosim_survivor_snap_t snap = {0};

/* ── forward declarations ────────────────────────────────────────────────── */

static biosim_status_t spawn_generation(void);
EMSCRIPTEN_KEEPALIVE void biosim_wasm_free(void);

/* ── lifecycle ───────────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE int biosim_wasm_init(void) {
    biosim_log_init(&biosim_log_default_ctx);
    if (initialized) {
        biosim_sim_free(&sim);
        initialized = false;
    }
    memset(&sim, 0, sizeof(sim));
    memset(&last_census, 0, sizeof(last_census));
    params_mut_ensure();

    biosim_params_t p;
    memset(&p, 0, sizeof(p));
    biosim_status_t rc = biosim_params_init(&p, params_mut, PARAMS_COUNT);
    if (rc != BIOSIM_OK) {
        goto exit;
    }

    rc = biosim_sim_create(&sim, &p, &challenge, barriers, n_barriers);
    if (rc != BIOSIM_OK) {
        goto exit;
    }
    initialized = true;

    rc = spawn_generation();
    if (rc != BIOSIM_OK) {
        goto exit;
    }

exit:
    biosim_params_free(&p);
    if (rc != BIOSIM_OK) {
        biosim_wasm_free();
        BIOSIM_ERRORF("wasm init failed (%s)", biosim_strerror(rc));
    }
    return (int)rc;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_free(void) {
    if (initialized) {
        biosim_sim_free(&sim);
        initialized = false;
    }
    memset(&sim, 0, sizeof(sim));
    memset(&last_census, 0, sizeof(last_census));
    biosim_survivor_snap_free(&snap);
    snap = (biosim_survivor_snap_t){0};
    free(barriers);
    barriers = NULL;
    barriers_cap = 0U;
    n_barriers = 0U;
}

/* ── stepping ────────────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE int biosim_wasm_do_step(void) {
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            biosim_sim_step_agent(&sim, i);
        }
    }
    biosim_sim_next_step(&sim);
    return BIOSIM_OK;
}

/* ── generation operations ───────────────────────────────────────────────── */

/* Populate the grid for a new generation run via core's spawn logic.
 * Resets sim.step and sim.kills to zero on success.
 * Must be called after biosim_sim_create (or after collect_survivors) before
 * any simulation steps are taken. */
static biosim_status_t spawn_generation(void) {
    biosim_status_t rc = biosim_generation_spawn(&sim, &snap);
    if (rc == BIOSIM_OK) {
        sim.step = 0U;
        sim.kills = 0U;
    }
    return rc;
}

/* Discard the saved survivors so the next init() starts from a random genome. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_clear_genome(void) {
    if (!initialized) {
        return BIOSIM_ERR_INVALID;
    }
    snap.count = 0U;
    return (int)spawn_generation();
}

/* Reproduce a new population from the saved survivors, using the same gen_rng
 * seed as the original reproduction.  Thanks to determinism, calling this
 * after biosim_wasm_next_generation yields the same starting population. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_rewind(void) {
    if (!initialized) {
        return BIOSIM_ERR_INVALID;
    }
    if (snap.count > 0U) {
        sim.gen_rng = snap.gen_rng;
    }
    return (int)spawn_generation();
}

/* Reinitialise with the params already set via biosim_wasm_set_param_*,
 * biosim_wasm_set_challenge_*, and biosim_wasm_add_barrier, then rewind:
 * restore gen_rng and gen from snap so the population is deterministically
 * reproduced from the same survivors as the last generation boundary.
 * The internal spawn inside biosim_wasm_init is harmless — spawn clears the
 * grid on entry, so a second spawn simply overwrites it. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_rewind_configured(void) {
    int rc = biosim_wasm_init();
    if (rc != BIOSIM_OK) {
        return rc;
    }
    if (snap.count > 0U) {
        sim.gen_rng = snap.gen_rng;
        sim.gen = snap.gen;
    }
    return (int)spawn_generation();
}

/* Advance one generation: collect survivors, take census, then spawn the next
 * population (breed from survivors, or randomise if none passed the challenge),
 * then advance the generation counter. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_next_generation(void) {
    biosim_status_t rc = biosim_generation_collect_survivors(&sim, &snap);
    if (rc != BIOSIM_OK) {
        goto exit;
    }

    biosim_census_take(&sim, snap.count, &last_census);

    rc = spawn_generation();
    if (rc == BIOSIM_OK) {
        sim.gen++;
    }

exit:
    if (rc != BIOSIM_OK) {
        BIOSIM_ERRORF("next generation failed (%s)", biosim_strerror(rc));
    }
    return (int)rc;
}

/* Run a complete generation without yielding: loop all steps until gen is
 * complete, then advance the generation boundary. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_do_gen(void) {
    while (sim.step < sim.steps_per_gen) {
        int rc = biosim_wasm_do_step();
        if (rc != BIOSIM_OK) {
            return rc;
        }
    }
    return biosim_wasm_next_generation();
}

/* Collect survivors and census from the current sim, reinitialise with the
 * params already set via biosim_wasm_set_param_* / set_challenge / add_barrier,
 * then breed the next generation from those survivors.
 * biosim_wasm_init zeroes last_census, so save and restore it around the call
 * so that biosim_wasm_census_*() return valid data after this function. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_next_generation_configured(void) {
    biosim_status_t rc = biosim_generation_collect_survivors(&sim, &snap);
    if (rc != BIOSIM_OK) {
        goto exit;
    }
    biosim_census_take(&sim, snap.count, &last_census);
    {
        biosim_census_t saved = last_census;
        rc = (biosim_status_t)biosim_wasm_init();
        if (rc != BIOSIM_OK) {
            goto exit;
        }
        last_census = saved;
    }
    sim.gen_rng = snap.gen_rng;
    sim.gen = snap.gen + 1U;
    rc = spawn_generation();
exit:
    if (rc != BIOSIM_OK) {
        BIOSIM_ERRORF("next generation configured failed (%s)", biosim_strerror(rc));
    }
    return (int)rc;
}

/* ── parameter setters ───────────────────────────────────────────────────── */

/* Set a named integer parameter before the next biosim_wasm_init call.
 * Returns BIOSIM_OK, BIOSIM_ERR_NOTFOUND, or BIOSIM_ERR_TYPE. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_int(const char *name, int val) {
    params_mut_ensure();
    for (size_t i = 0U; i < PARAMS_COUNT; i++) {
        if (strcmp(params_mut[i].name, name) == 0) {
            if (params_mut[i].type != PARAM_INT) {
                return BIOSIM_ERR_TYPE;
            }
            params_mut[i].value.i = val;
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* Set a named float parameter before the next biosim_wasm_init call. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_float(const char *name, double val) {
    params_mut_ensure();
    for (size_t i = 0U; i < PARAMS_COUNT; i++) {
        if (strcmp(params_mut[i].name, name) == 0) {
            if (params_mut[i].type != PARAM_FLOAT) {
                return BIOSIM_ERR_TYPE;
            }
            params_mut[i].value.f = val;
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* Set a named boolean parameter before the next biosim_wasm_init call.
 * val: non-zero = true, 0 = false. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_set_param_bool(const char *name, int val) {
    params_mut_ensure();
    for (size_t i = 0U; i < PARAMS_COUNT; i++) {
        if (strcmp(params_mut[i].name, name) == 0) {
            if (params_mut[i].type != PARAM_BOOL) {
                return BIOSIM_ERR_TYPE;
            }
            params_mut[i].value.b = (val != 0);
            return BIOSIM_OK;
        }
    }
    return BIOSIM_ERR_NOTFOUND;
}

/* ── challenge setters ───────────────────────────────────────────────────── */

/* Reset the challenge union and set the kind discriminant. Call this first
 * before any per-kind setter; it zeroes all union fields. */
EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_kind(int32_t kind) {
    memset(&challenge, 0, sizeof(challenge));
    challenge.kind = (biosim_challenge_kind_t)kind;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_x_band(float x_min, float x_max, int mirror) {
    challenge.x_band.x_min = x_min;
    challenge.x_band.x_max = x_max;
    challenge.x_band.mirror = (mirror != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_disc(
    float x, float y, float radius, int weighted
) {
    challenge.disc.x = x;
    challenge.disc.y = y;
    challenge.disc.radius = radius;
    challenge.disc.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_corners(float radius, int weighted) {
    challenge.corners.radius = radius;
    challenge.corners.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_neighbor_count(
    float radius, float min_n, float max_n, int exclude_border
) {
    challenge.neighbor_count.radius = radius;
    challenge.neighbor_count.min_n = min_n;
    challenge.neighbor_count.max_n = max_n;
    challenge.neighbor_count.exclude_border = (exclude_border != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_center_sparse(
    float x, float y, float outer_r, float inner_r, float min_n, float max_n, int weighted
) {
    challenge.center_sparse.x = x;
    challenge.center_sparse.y = y;
    challenge.center_sparse.outer_r = outer_r;
    challenge.center_sparse.inner_r = inner_r;
    challenge.center_sparse.min_n = min_n;
    challenge.center_sparse.max_n = max_n;
    challenge.center_sparse.weighted = (weighted != 0);
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_near_barrier(float radius) {
    challenge.near_barrier.radius = radius;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_location_sequence(float radius) {
    challenge.location_sequence.radius = radius;
}

/* ── barrier setters ─────────────────────────────────────────────────────── */

/* Reset the barrier list. Call before adding a new set of barriers. */
EMSCRIPTEN_KEEPALIVE void biosim_wasm_clear_barriers(void) {
    n_barriers = 0U;
}

static int barriers_grow(void) {
    if (n_barriers < barriers_cap) {
        return BIOSIM_OK;
    }
    size_t new_cap = barriers_cap == 0U ? 8U : barriers_cap * 2U;
    biosim_barrier_spec_t *grown = realloc(barriers, new_cap * sizeof(biosim_barrier_spec_t));
    if (grown == NULL) {
        return BIOSIM_ERR_NOMEM;
    }
    barriers = grown;
    barriers_cap = new_cap;
    return BIOSIM_OK;
}

/* Append one barrier spec to the list. x and y are grid cell coordinates;
 * pass -32768 (INT16_MIN = BIOSIM_BARRIER_POS_UNSET) for random placement.
 * length and width are in cells; pass 0.0 (BIOSIM_BARRIER_DIM_UNSET) for a
 * random dimension. Returns BIOSIM_OK or BIOSIM_ERR_NOMEM on allocation
 * failure. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_add_barrier(
    int kind, int x, int y, float length, float width
) {
    int rc = barriers_grow();
    if (rc != BIOSIM_OK) {
        return rc;
    }
    biosim_barrier_spec_t *b = &barriers[n_barriers];
    b->kind = (biosim_barrier_kind_t)kind;
    b->x = (int16_t)x;
    b->y = (int16_t)y;
    b->length = length;
    b->width = width;
    n_barriers++;
    return BIOSIM_OK;
}

/* Number of barriers currently in the list. */
EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_n_barriers(void) {
    return n_barriers;
}

/* ── state queries ───────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_gen(void) {
    return sim.gen;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_step(void) {
    return sim.step;
}

EMSCRIPTEN_KEEPALIVE int biosim_wasm_is_gen_complete(void) {
    return (sim.step >= sim.steps_per_gen) ? 1 : 0;
}

/* ── census results (valid after biosim_wasm_next_generation) ────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_gen(void) {
    return last_census.gen;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_population(void) {
    return last_census.population;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_survivors(void) {
    return last_census.survivors;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_census_kills(void) {
    return last_census.kills;
}

/* ── rendering queries ───────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_population(void) {
    return sim.agents.population;
}

EMSCRIPTEN_KEEPALIVE int32_t biosim_wasm_get_size_x(void) {
    return sim.size_x;
}

EMSCRIPTEN_KEEPALIVE int32_t biosim_wasm_get_size_y(void) {
    return sim.size_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_loc_x_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.loc_x;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_loc_y_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.loc_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_alive_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.alive;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_grid_cells_ptr(void) {
    return (uint32_t)(uintptr_t)sim.grid.cells;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_birth_x_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.birth_x;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_birth_y_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.birth_y;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_last_move_dir_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.last_move_dir;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_osc_period_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.osc_period;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_responsiveness_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.responsiveness;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_los_range_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.los_range;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_challenge_bits_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.challenge_bits;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_fingerprint_ptr(void) {
    return (uint32_t)(uintptr_t)sim.agents.genome_fingerprint;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_conn_ptr(void) {
    return (uint32_t)(uintptr_t)sim.nnet.genome_conn;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_genome_wgt_ptr(void) {
    return (uint32_t)(uintptr_t)sim.nnet.genome_wgt;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_conn_length_ptr(void) {
    return (uint32_t)(uintptr_t)sim.nnet.conn_length;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_neuron_count_ptr(void) {
    return (uint32_t)(uintptr_t)sim.nnet.neuron_count;
}

/* ── genome size queries ─────────────────────────────────────────────────────
 * Scan alive agents for the maximum genome length / neuron count in use.
 * Used by Sub-plan D's compatibility gate to detect when a config change
 * would truncate the current population's genome. */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_genome_max_len_used(void) {
    if (!initialized) {
        return 0U;
    }
    uint32_t max = 0U;
    const uint32_t pop = sim.agents.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (sim.agents.alive[i] && (uint32_t)sim.genome.len[i] > max) {
            max = (uint32_t)sim.genome.len[i];
        }
    }
    return max;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_genome_max_neurons_used(void) {
    if (!initialized) {
        return 0U;
    }
    uint32_t max = 0U;
    const uint32_t pop = sim.agents.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (sim.agents.alive[i] && (uint32_t)sim.nnet.neuron_count[i] > max) {
            max = (uint32_t)sim.nnet.neuron_count[i];
        }
    }
    return max;
}
