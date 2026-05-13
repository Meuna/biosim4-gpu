/* k2_kill_marked.cl — K2: clear grid cells for kill-marked agents
 *
 * Preamble: biosim/core/types.h, rng.h, gene.h, and io_defs.h are prepended
 * as separate source strings by the build system (clCreateProgramWithSource).
 * Do NOT add #include directives for those files here.
 *
 * For each agent whose kill_marker flag was set by K1 (KILL_FORWARD), this
 * kernel atomically clears that agent's grid cell.  Using atomic_cmpxchg
 * ensures the cell is only cleared if it still holds this agent's index,
 * which is always true at this pipeline stage (movement has not run yet).
 *
 * kill_marker is a one-step-lifetime flag: it is set by K1 and consumed here.
 * The host zeros all kill_marker slots at the generation boundary.
 */

__kernel void k_kill_marked(__global const uchar *kill_marker, __global const short *loc_x,
                            __global const short *loc_y, __global uint *grid, int size_x,
                            uint pop) {
    uint idx = get_global_id(0);
    if (idx >= pop || !kill_marker[idx]) {
        return;
    }

    int gx = (int)loc_x[idx];
    int gy = (int)loc_y[idx];
    uint expected = idx + 1u;
    atomic_cmpxchg((__global volatile uint *)(grid + gy * size_x + gx), expected,
                   BIOSIM_GRID_EMPTY);
}
