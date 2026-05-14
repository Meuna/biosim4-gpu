/* k3_movement_resolution.cl — K3: commit desired movement to the grid
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
 * atomic requirements.  Cell sentinels are BIOSIM_GRID_EMPTY and
 * BIOSIM_GRID_BARRIER from types.h (guarded for OpenCL to use uint).
 */

/* ── helpers ────────────────────────────────────────────────────────────── */

static uchar k3_get_dir(int dx, int dy) {
    uchar d;
    for (d = 0u; d < 8u; d++) {
        if (BIOSIM_DIR_DX[d] == dx && BIOSIM_DIR_DY[d] == dy) {
            return d;
        }
    }
    return 8u; /* no-match sentinel */
}

/* ── K3 kernel ──────────────────────────────────────────────────────────── */

__kernel void k_movement_resolution(__global const uchar *alive, __global const int *desired_x,
                                    __global const int *desired_y, __global int *loc_x,
                                    __global int *loc_y, __global uchar *last_move_dir,
                                    __global uint *grid, int size_x, int size_y, uint pop) {
    uint idx = get_global_id(0);

    if (idx >= pop || !alive[idx]) {
        return;
    }

    int cx = loc_x[idx];
    int cy = loc_y[idx];
    int tx = desired_x[idx];
    int ty = desired_y[idx];
    int dx = tx - cx;
    int dy = ty - cy;

    if (dx == 0 && dy == 0) {
        return;
    }

    int ti = ty * size_x + tx;
    uint new_val = idx + 1u;

    /* Atomically claim the target cell.  Fails if occupied or is a barrier. */
    uint old = atomic_cmpxchg((__global volatile uint *)(grid + ti), BIOSIM_GRID_EMPTY, new_val);
    if (old != BIOSIM_GRID_EMPTY) {
        return; /* cell taken — stay in place */
    }

    /* Commit: clear old cell (safe — exclusively owned by this agent) and
     * update agent position and last-move direction. */
    int oi = cy * size_x + cx;
    grid[oi] = BIOSIM_GRID_EMPTY;

    loc_x[idx] = tx;
    loc_y[idx] = ty;
    last_move_dir[idx] = k3_get_dir(dx, dy);
}
