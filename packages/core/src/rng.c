#include "biosim/core/rng.h"

uint64_t biosim_rng_seed(uint32_t agent_index, uint64_t generation_seed) {
    /* Mix agent_index into the generation seed, then apply a splitmix64 round
     * to scatter all bits. The Knuth multiplicative constant spaces successive
     * agent indices far apart in the 64-bit state space.                      */
    uint64_t x = generation_seed + (uint64_t)agent_index * 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    if (x == 0) {
        x = 1; /* xorshift64 fixed-point guard: state 0 must never be used */
    }
    return x;
}
