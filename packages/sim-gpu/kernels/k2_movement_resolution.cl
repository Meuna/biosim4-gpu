/* k2_movement_resolution.cl — K2: commit desired movement to the grid
 *
 * Preamble: biosim/core/types.h, rng.h, gene.h, and io_defs.h are prepended
 * as separate source strings by the build system (clCreateProgramWithSource).
 * Do NOT add #include directives for those files here.
 *
 * First-version limitation: no chained-move logic.  An agent whose target cell
 * is still occupied (because a neighbour has not cleared it yet in this same
 * kernel launch) stays in place.  Results may therefore differ from the CPU
 * reference implementation, which is acceptable.
 *
 * Grid buffer: passed as __global uint * (32-bit per cell) to satisfy OpenCL
 * atomic requirements.  Sentinel values match the host uint16_t encoding:
 *   BIOSIM_GRID_EMPTY   = 0u
 *   BIOSIM_GRID_BARRIER = 65535u  (0xFFFF)
 *   occupied            = agent_index + 1
 */

/* ── helpers ────────────────────────────────────────────────────────────── */

static uchar k2_get_dir(int dx, int dy) {
    uchar d;
    for (d = 0u; d < 8u; d++) {
        if (BIOSIM_DIR_DX[d] == dx && BIOSIM_DIR_DY[d] == dy) {
            return d;
        }
    }
    return 8u; /* no-match sentinel */
}

/* ── K2 kernel ──────────────────────────────────────────────────────────── */

__kernel void k_movement_resolution(__global const uchar *alive, __global const short *desired_x,
                                    __global const short *desired_y, __global short *loc_x,
                                    __global short *loc_y, __global uchar *last_move_dir,
                                    __global uint *grid, int size_x, int size_y, uint pop) {
    uint idx = get_global_id(0);

    if (idx >= pop || !alive[idx]) {
        return;
    }

    short cx = loc_x[idx];
    short cy = loc_y[idx];
    short tx = desired_x[idx];
    short ty = desired_y[idx];
    int dx = (int)tx - (int)cx;
    int dy = (int)ty - (int)cy;

    if (dx == 0 && dy == 0) {
        return;
    }

    int ti = (int)ty * size_x + (int)tx;
    uint new_val = idx + 1u;

    /* Atomically claim the target cell.  Fails if occupied or is a barrier. */
    uint old = atomic_cmpxchg((__global volatile uint *)(grid + ti), 0u, new_val);
    if (old != 0u) {
        return; /* cell taken — stay in place */
    }

    /* Commit: clear old cell (safe — exclusively owned by this agent) and
     * update agent position and last-move direction. */
    int oi = (int)cy * size_x + (int)cx;
    grid[oi] = 0u;

    loc_x[idx] = tx;
    loc_y[idx] = ty;
    last_move_dir[idx] = k2_get_dir(dx, dy);
}
