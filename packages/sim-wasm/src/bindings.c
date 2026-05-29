#include "biosim/core/barriers.h"
#include "biosim/core/census.h"
#include "biosim/core/challenge_spec.h"
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
 * passes this table to biosim_params_init so overrides survive reinit.        */

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
 * takes effect on the next reinitialisation.                                */
static biosim_challenge_spec_t s_challenge = {
    .kind = BIOSIM_CHALLENGE_X_BAND,
    .x_band = {.x_min = 0.5F, .x_max = 1.0F, .mirror = false},
};

/* ── barrier state ───────────────────────────────────────────────────────── */

/* Module-level barrier list. biosim_wasm_clear_barriers resets the list;
 * biosim_wasm_add_barrier appends one spec. biosim_wasm_init passes
 * s_barriers/s_n_barriers to biosim_sim_create. The array grows on demand
 * via realloc; there is no hard upper limit.                                */
static biosim_barrier_spec_t *s_barriers = NULL;
static size_t s_barriers_cap = 0U;
static uint32_t s_n_barriers = 0U;

/* ── module state ────────────────────────────────────────────────────────── */

static biosim_sim_t s_sim;
static uint32_t s_agent_cursor; /* index of next agent to process in step */
static uint32_t s_last_agent;   /* index of agent processed by last do_step_agent */
static biosim_census_t s_last_census;
static bool s_initialized = false;

/* ── lifecycle ───────────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE int biosim_wasm_init(void) {
    biosim_log_init(&biosim_log_default_ctx);

    if (s_initialized) {
        biosim_sim_free(&s_sim);
    }
    memset(&s_sim, 0, sizeof(s_sim));
    memset(&s_last_census, 0, sizeof(s_last_census));
    s_agent_cursor = 0U;
    s_last_agent = 0U;
    s_initialized = false;

    params_mut_ensure();

    biosim_params_t p;
    biosim_status_t rc = biosim_params_init(&p, s_params_mut, S_PARAMS_COUNT);
    if (rc != BIOSIM_OK) {
        return (int)rc;
    }

    rc = biosim_sim_create(&s_sim, &p, &s_challenge, s_barriers, s_n_barriers);
    biosim_params_free(&p);
    if (rc != BIOSIM_OK) {
        return (int)rc;
    }

    s_initialized = true;
    return BIOSIM_OK;
}

EMSCRIPTEN_KEEPALIVE void biosim_wasm_free(void) {
    if (s_initialized) {
        biosim_sim_free(&s_sim);
        s_initialized = false;
    }
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

/* clang-format off */
EMSCRIPTEN_KEEPALIVE void biosim_wasm_set_challenge_center_sparse(
    float x, float y, float outer_r, float inner_r,
    float min_n, float max_n, int weighted) {
    s_challenge.center_sparse.x       = x;
    s_challenge.center_sparse.y       = y;
    s_challenge.center_sparse.outer_r = outer_r;
    s_challenge.center_sparse.inner_r = inner_r;
    s_challenge.center_sparse.min_n   = min_n;
    s_challenge.center_sparse.max_n   = max_n;
    s_challenge.center_sparse.weighted = (weighted != 0);
}
/* clang-format on */

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

/* Append one barrier spec to the list. x and y are grid cell coordinates;
 * pass -32768 (INT16_MIN = BIOSIM_BARRIER_POS_UNSET) for random placement.
 * length and width are in cells; pass 0.0 (BIOSIM_BARRIER_DIM_UNSET) for a
 * random dimension. Returns BIOSIM_OK or BIOSIM_ERR_NOMEM on allocation
 * failure.                                                                  */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_add_barrier(
    int kind, int x, int y, float length, float width
) {
    if (s_n_barriers >= s_barriers_cap) {
        size_t new_cap = s_barriers_cap == 0U ? 8U : s_barriers_cap * 2U;
        biosim_barrier_spec_t *grown = realloc(s_barriers, new_cap * sizeof(biosim_barrier_spec_t));
        if (grown == NULL) {
            return BIOSIM_ERR_NOMEM;
        }
        s_barriers = grown;
        s_barriers_cap = new_cap;
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
    s_agent_cursor = 0U;
    for (uint32_t i = 0U; i < s_sim.agents.population; i++) {
        if (s_sim.agents.alive[i]) {
            biosim_sim_step_agent(&s_sim, i);
        }
    }
    biosim_sim_next_step(&s_sim);
    s_agent_cursor = 0U;
    return BIOSIM_OK;
}

/* Advance one alive agent within the current step.
 * When the last alive agent has been processed, finalizes the step. */
EMSCRIPTEN_KEEPALIVE int biosim_wasm_do_step_agent(void) {
    while (s_agent_cursor < s_sim.agents.population && !s_sim.agents.alive[s_agent_cursor]) {
        s_agent_cursor++;
    }

    if (s_agent_cursor < s_sim.agents.population) {
        s_last_agent = s_agent_cursor;
        biosim_sim_step_agent(&s_sim, s_agent_cursor);
        s_agent_cursor++;
    }

    /* skip dead agents to check if any remain */
    uint32_t lookahead = s_agent_cursor;
    while (lookahead < s_sim.agents.population && !s_sim.agents.alive[lookahead]) {
        lookahead++;
    }
    if (lookahead >= s_sim.agents.population) {
        biosim_sim_next_step(&s_sim);
        s_agent_cursor = 0U;
    }

    return BIOSIM_OK;
}

EMSCRIPTEN_KEEPALIVE int biosim_wasm_next_generation(void) {
    biosim_status_t rc = biosim_sim_next_generation(&s_sim, &s_last_census);
    return (int)rc;
}

/* ── state queries ───────────────────────────────────────────────────────── */

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_gen(void) {
    return s_sim.gen;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_step(void) {
    return s_sim.step;
}

EMSCRIPTEN_KEEPALIVE uint32_t biosim_wasm_get_last_agent(void) {
    return s_last_agent;
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
