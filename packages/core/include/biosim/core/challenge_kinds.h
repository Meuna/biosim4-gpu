/*
 * HOST + DEVICE: safe for OpenCL kernel sources.
 *
 * biosim_challenge_kind_t — the challenge kind discriminant.
 * Kept separate from challenge_spec.h so that OpenCL kernels can include this
 * header without pulling in <stdbool.h> (which is HOST-ONLY).
 */
#ifndef BIOSIM_CORE_CHALLENGE_KINDS_H
#define BIOSIM_CORE_CHALLENGE_KINDS_H

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

#endif /* BIOSIM_CORE_CHALLENGE_KINDS_H */
