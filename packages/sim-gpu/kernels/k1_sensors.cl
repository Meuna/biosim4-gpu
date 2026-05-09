/* k1_sensors.cl — K1 embryo: sensor evaluation for Group A (self-only) sensors
 *
 * Preamble: biosim/core/types.h, rng.h, and gene.h are prepended as separate
 * source strings by the build system (clCreateProgramWithSource). Do NOT
 * add #include directives for those files here.
 *
 * Supported sensor_id values (matching biosim_sensor_t in io_catalogue.h):
 *   0  LOC_X            — normalised x position in [0, 1]
 *   1  LOC_Y            — normalised y position in [0, 1]
 *   2  BOUNDARY_DIST_X  — normalised distance to nearest x boundary
 *   3  BOUNDARY_DIST_Y  — normalised distance to nearest y boundary
 *   4  BOUNDARY_DIST    — minimum of BOUNDARY_DIST_X and BOUNDARY_DIST_Y
 *   5  LAST_MOVE_DIR_X  — last move direction x component, mapped to [0, 1]
 *   6  LAST_MOVE_DIR_Y  — last move direction y component, mapped to [0, 1]
 *   8  AGE              — step / steps_per_gen in [0, 1)
 *   9  RANDOM           — xorshift64 draw in [0, 1) (mutates rng_state)
 *
 * Sensor 7 (OSC1) is omitted — it requires cos(), which is not available
 * without including math headers. Group B/C/D sensors are not yet implemented.
 * Unknown sensor_id values produce 0.5.
 */

__constant int DIR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
__constant int DIR_DY[8] = {0, -1, -1, -1, 0, 1, 1, 1};

__kernel void k_sensor_eval(__global const short *loc_x, __global const short *loc_y,
                            __global const uchar *last_move_dir, __global ulong *rng_state,
                            int size_x, int size_y, uint step, uint steps_per_gen, int sensor_id,
                            __global float *out) {
    uint idx = get_global_id(0);

    int x = (int)loc_x[idx];
    int y = (int)loc_y[idx];
    float val = 0.5F;

    switch (sensor_id) {
    case 0:
        val = (float)x / (float)(size_x - 1);
        break;
    case 1:
        val = (float)y / (float)(size_y - 1);
        break;
    case 2: {
        int edge = size_x - x - 1;
        int d = x < edge ? x : edge;
        val = 2.0F * (float)d / (float)size_x;
        break;
    }
    case 3: {
        int edge = size_y - y - 1;
        int d = y < edge ? y : edge;
        val = 2.0F * (float)d / (float)size_y;
        break;
    }
    case 4: {
        int xe = size_x - x - 1;
        int ye = size_y - y - 1;
        int dx = x < xe ? x : xe;
        int dy = y < ye ? y : ye;
        float fx = 2.0F * (float)dx / (float)size_x;
        float fy = 2.0F * (float)dy / (float)size_y;
        val = fx < fy ? fx : fy;
        break;
    }
    case 5: {
        int dir = (int)(last_move_dir[idx] & 7u);
        val = ((float)DIR_DX[dir] + 1.0F) * 0.5F;
        break;
    }
    case 6: {
        int dir = (int)(last_move_dir[idx] & 7u);
        val = ((float)DIR_DY[dir] + 1.0F) * 0.5F;
        break;
    }
    case 8:
        val = (float)step / (float)steps_per_gen;
        break;
    case 9: {
        ulong state = rng_state[idx];
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        rng_state[idx] = state;
        val = (float)(state >> 40) * (1.0F / 16777216.0F);
        break;
    }
    default:
        val = 0.5F;
        break;
    }

    out[idx] = val;
}
