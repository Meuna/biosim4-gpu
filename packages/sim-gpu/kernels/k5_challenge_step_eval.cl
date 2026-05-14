/* k5_challenge_step_eval.cl — K5: per-step challenge hook
 *
 * Preamble: grid_defs.h, rng.h, gene.h, io_defs.h, and
 * challenge_defs.h are prepended as separate source strings by the build
 * system. Do NOT add #include here.
 *
 * Global work size: population (caller's responsibility).
 * Each work-item handles exactly one agent.
 *
 * Ported from biosim_challenge_step() in core/src/challenges.c.
 * Three challenge kinds are handled; all others are no-ops.
 *
 * Grid cell clearing for killed agents (RADIOACTIVE_WALLS):
 *   alive[gid]=0 AND grid cell cleared. After K3 each alive agent uniquely
 *   owns one cell, so this write is race-free without atomics.
 */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
__kernel void k_challenge_step_eval(__global uchar *alive, __global const int *loc_x,
                                    __global const int *loc_y, __global uint *challenge_bits,
                                    __global ulong *rng_state, __global uint *grid, int size_x,
                                    int size_y, uint step, uint steps_per_gen, uint challenge_kind,
                                    float location_sequence_radius,
                                    __global const int *barrier_ctrs, uint n_barrier_ctrs) {
    uint gid = get_global_id(0);
    if (!alive[gid]) {
        return;
    }

    int x = loc_x[gid];
    int y = loc_y[gid];

    switch (challenge_kind) {

    case BIOSIM_CHALLENGE_TOUCH_ANY_WALL:
        if (x == 0 || x == size_x - 1 || y == 0 || y == size_y - 1) {
            challenge_bits[gid] = 1U;
        }
        break;

    case BIOSIM_CHALLENGE_RADIOACTIVE_WALLS: {
        int radioactive_x = (step < steps_per_gen / 2U) ? 0 : size_x - 1;
        int dist = abs(x - radioactive_x);
        if (dist == 0) {
            alive[gid] = 0U;
            grid[(uint)y * (uint)size_x + (uint)x] = 0U;
        } else if (dist < size_x / 2) {
            ulong roll = biosim_rng_next(&rng_state[gid]);
            ulong max_u64 = (ulong)0 - 1u;
            if (roll < max_u64 / (ulong)dist) {
                alive[gid] = 0U;
                grid[(uint)y * (uint)size_x + (uint)x] = 0U;
            }
        }
        break;
    }

    case BIOSIM_CHALLENGE_LOCATION_SEQUENCE: {
        int rpx = (int)(location_sequence_radius * (float)size_x);
        int rpx_sq = rpx * rpx;
        for (uint b = 0U; b < n_barrier_ctrs && b < 32U; b++) {
            uint bit = 1U << b;
            if (challenge_bits[gid] & bit) {
                continue;
            }
            int dx = x - barrier_ctrs[b * 2U];
            int dy = y - barrier_ctrs[b * 2U + 1U];
            if (dx * dx + dy * dy <= rpx_sq) {
                challenge_bits[gid] |= bit;
            }
            break;
        }
        break;
    }

    default:
        break;
    }
}
