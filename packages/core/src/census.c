#include "biosim/core/census.h"

/* ── collection ─────────────────────────────────────────────────────────── */

void biosim_census_take(
    const biosim_sim_t *sim, const uint32_t *survivors, uint32_t n_survivors, biosim_census_t *out
) {
    (void)survivors;
    out->gen = sim->gen;
    out->population = sim->agents.population;
    out->survivors = n_survivors;
    out->kills = sim->kills;
}

/* ── display ────────────────────────────────────────────────────────────── */

void biosim_census_print_header(FILE *stream) {
    (void)fprintf(stream, "%5s %7s %7s %7s %6s\n", "gen", "pop", "surv", "kills", "surv%");
}

void biosim_census_print(FILE *stream, const biosim_census_t *c) {
    float rate = c->population > 0U ? (float)c->survivors / (float)c->population : 0.0F;
    (void)fprintf(
        stream,
        "%5u %7u %7u %7u %5.1f%%\n",
        c->gen,
        c->population,
        c->survivors,
        c->kills,
        (double)(rate * 100.0F)
    );
}
