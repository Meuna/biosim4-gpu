/*
 * HOST/DEVICE PORTABILITY: this header is included by OpenCL kernel sources.
 * Do NOT add <stdio.h>, <stdlib.h>, <string.h>, or any host-only header.
 * biosim_rng_next is static inline to avoid linkage issues in kernel code.
 */
#ifndef BIOSIM_CORE_RNG_H
#define BIOSIM_CORE_RNG_H

#ifdef __OPENCL_VERSION__
typedef ulong uint64_t;
typedef uint uint32_t;
#else
#include <stdint.h>
#endif

/*
 * xorshift64 — advance state and return the next pseudo-random value.
 * State 0 is a fixed point: biosim_rng_seed must never produce 0.
 */
static inline uint64_t biosim_rng_next(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return (*state = x);
}

/*
 * Deterministic per-agent seed — mixes agent_index and generation_seed
 * into a non-zero 64-bit state using a splitmix64 round.
 * Host-only; defined in rng.c.
 */
uint64_t biosim_rng_seed(uint32_t agent_index, uint64_t generation_seed);

#endif /* BIOSIM_CORE_RNG_H */
