/*
 * HOST-ONLY: uses <stdbool.h>. Do NOT include from OpenCL kernel sources.
 *
 * Isolated from challenges.h so that sim.h can embed biosim_challenge_spec_t
 * without a circular include. challenges.h provides the evaluation API and must
 * include sim.h (for grid, agents, and barrier centres), which means it
 * cannot itself be included by sim.h. Keeping the spec types here breaks
 * that cycle cleanly — sim.h includes this header only.
 *
 * biosim_challenge_kind_t is defined in challenge_kinds.h (HOST + DEVICE)
 * and included here.
 */
#ifndef BIOSIM_CORE_CHALLENGE_SPEC_H
#define BIOSIM_CORE_CHALLENGE_SPEC_H

#include "biosim/core/challenge_kinds.h"
#include <stdbool.h>

/*
 * Per-kind parameters. All positional fields (x, y, x_min, x_max, radii,
 * outer_r, inner_r) are fractions of the grid size in [0, 1]. Kinds with no
 * parameters (against_wall, migrate_distance, touch_any_wall, radioactive_walls,
 * pairs, altruism) use only the kind discriminant.
 */
typedef struct {
    biosim_challenge_kind_t kind;
    union {
        struct {
            float x_min;
            float x_max;
            bool mirror;
        } x_band;
        struct {
            float x;
            float y;
            float radius;
            bool weighted;
        } disc;
        struct {
            float radius;
            bool weighted;
        } corners;
        struct {
            float radius;
            float min_n;
            float max_n;
            bool exclude_border;
        } neighbor_count;
        struct {
            float x;
            float y;
            float outer_r;
            float inner_r;
            float min_n;
            float max_n;
            bool weighted;
        } center_sparse;
        struct {
            float radius;
        } near_barrier;
        struct {
            float radius;
        } location_sequence;
    };
} biosim_challenge_spec_t;

typedef struct {
    bool passed;
    float score;
} biosim_challenge_result_t;

#endif /* BIOSIM_CORE_CHALLENGE_SPEC_H */
