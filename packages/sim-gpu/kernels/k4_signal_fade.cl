/* k4_signal_fade.cl — K4: decay the signal layer by 1 per step
 *
 * Preamble: biosim/core/types.h, rng.h, gene.h, and io_defs.h are prepended
 * as separate source strings by the build system (clCreateProgramWithSource).
 * Do NOT add #include directives for those files here.
 *
 * Global work size: size_x * size_y (caller's responsibility).
 * Each work-item handles exactly one cell; no bounds check required.
 *
 * Signal values are uint32_t with meaningful range 0–255 (upper 24 bits
 * reserved).  The conditional subtract prevents uint underflow to UINT_MAX.
 */

__kernel void k_signal_fade(__global uint *signal) {
    uint gid = get_global_id(0);
    signal[gid] = (signal[gid] > 0u) ? (signal[gid] - 1u) : 0u;
}
