/*
 * HOST-ONLY: uses <stdbool.h>. Do NOT include from OpenCL kernel sources.
 */
#ifndef BIOSIM_CORE_CHALLENGES_H
#define BIOSIM_CORE_CHALLENGES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BIOSIM_CHALLENGE_X_BAND,
    BIOSIM_CHALLENGE_DISC,
    BIOSIM_CHALLENGE_CORNERS,
    BIOSIM_CHALLENGE_NEIGHBOR_COUNT,
    BIOSIM_CHALLENGE_CENTER_SPARSE,
    BIOSIM_CHALLENGE_AGAINST_WALL,
    BIOSIM_CHALLENGE_MIGRATE_DISTANCE,
    BIOSIM_CHALLENGE_TOUCH_ANY_WALL,
    BIOSIM_CHALLENGE_RADIOACTIVE_WALLS,
    BIOSIM_CHALLENGE_PAIRS,
    BIOSIM_CHALLENGE_LOCATION_SEQUENCE,
    BIOSIM_CHALLENGE_NEAR_BARRIER,
    BIOSIM_CHALLENGE_ALTRUISM,
} biosim_challenge_kind_t;

/*
 * Per-kind parameters. All positional fields (cx, cy, x_min, x_max, radii)
 * are fractions of the grid size in [0, 1]. Kinds with no parameters
 * (against_wall, migrate_distance, touch_any_wall, radioactive_walls, pairs,
 * location_sequence, altruism) use only the kind discriminant.
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
            float cx;
            float cy;
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
            float cx;
            float cy;
            float outer_r;
            float inner_r;
            float min_n;
            float max_n;
            bool weighted;
        } center_sparse;
        struct {
            float radius;
        } near_barrier;
    };
} biosim_challenge_spec_t;

typedef struct {
    bool passed;
    float score;
} biosim_challenge_result_t;

/*
 * Evaluate whether agent at (loc_x, loc_y) on a grid of size (size_x, size_y)
 * passes the challenge. challenge_bits is the per-agent bitmask accumulated
 * during the generation (used by touch_any_wall, location_sequence).
 *
 * Unimplemented kinds return {false, 0.0f}.
 */
biosim_challenge_result_t biosim_challenge_eval(const biosim_challenge_spec_t *spec, int16_t loc_x,
                                                int16_t loc_y, int16_t size_x, int16_t size_y,
                                                uint32_t challenge_bits);

#endif /* BIOSIM_CORE_CHALLENGES_H */
