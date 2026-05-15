/* k1_feedforward.cl — K1: sensor evaluation, feedforward, actions, movement
 *
 * Preamble: grid_defs.h, rng.h, gene.h, and io_defs.h are prepended
 * as separate source strings by the build system (clCreateProgramWithSource).
 * Do NOT add #include directives for those files here.
 *
 * Sensor implementation:
 *   Group A (0-9)              — fully implemented, including OSC1 (uses built-in cos)
 *   POPULATION (10)            — circular-disc neighbourhood scan (grid read-only)
 *   POPULATION_FWD (11)        — forward half-disc density (strict dot-product filter)
 *   POPULATION_LR (12)         — lateral signed ratio (L−R)/(L+R), in [−1,1]
 *   BARRIER_FWD (13)           — forward half-disc barrier density
 *   BARRIER_LR (14)            — lateral signed barrier ratio (L−R)/(L+R), in [−1,1]
 *   LONGPROBE_BAR_FWD (16)     — forward ray-cast (skips agents), steps/dist to first barrier
 *   LONGPROBE_POP_FWD (15)     — forward ray-cast, returns steps/dist to first agent
 *   SIGNAL0 (17)               — reads signal buffer at agent position
 *   All others (18-20)         — stub returning 0.5
 *
 * Action implementation:
 *   Group A (0-2)   — SET_RESPONSIVENESS, SET_OSCILLATOR_PERIOD,
 *                     SET_LONGPROBE_DIST (uses built-in tanh, pow)
 *   Group B (3-14)  — movement accumulators
 *   EMIT_SIGNAL0    — atomic_add on signal buffer
 *   KILL_FORWARD    — marks forward agent dead and sets kill_marker (enable_kill gate)
 */

/* ── helpers ────────────────────────────────────────────────────────────── */

static float rng_float_k(__global ulong *rng_state, uint idx) {
    ulong state = rng_state[idx];
    state = biosim_rng_next(&state);
    rng_state[idx] = state;
    return (float)(state >> 40) * (1.0F / 16777216.0F);
}

/* ── sensor evaluation ──────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static float eval_sensor(
    __global ulong *rng_state,
    __global const uint *signal,
    __global const uint *grid,
    int sensor_id,
    uint idx,
    int x,
    int y,
    uchar last_dir,
    ushort osc_per,
    uint step,
    uint steps_per_gen,
    int size_x,
    int size_y,
    int sensor_radius,
    uchar los_range_val
) {
    switch (sensor_id) {
    case BIOSIM_SENSOR_LOC_X:
        return (float)x / (float)(size_x - 1);

    case BIOSIM_SENSOR_LOC_Y:
        return (float)y / (float)(size_y - 1);

    case BIOSIM_SENSOR_BOUNDARY_DIST_X: {
        int edge = size_x - x - 1;
        int d = x < edge ? x : edge;
        return 2.0F * (float)d / (float)size_x;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST_Y: {
        int edge = size_y - y - 1;
        int d = y < edge ? y : edge;
        return 2.0F * (float)d / (float)size_y;
    }

    case BIOSIM_SENSOR_BOUNDARY_DIST: {
        int xe = size_x - (int)x - 1;
        int ye = size_y - (int)y - 1;
        int ddx = (int)x < xe ? (int)x : xe;
        int ddy = (int)y < ye ? (int)y : ye;
        float fx = 2.0F * (float)ddx / (float)size_x;
        float fy = 2.0F * (float)ddy / (float)size_y;
        return fx < fy ? fx : fy;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_X: {
        int dir = (int)(last_dir & 7u);
        return ((float)BIOSIM_DIR_DX[dir] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_LAST_MOVE_DIR_Y: {
        int dir = (int)(last_dir & 7u);
        return ((float)BIOSIM_DIR_DY[dir] + 1.0F) * 0.5F;
    }

    case BIOSIM_SENSOR_OSC1: {
        uint period = (uint)osc_per;
        if (period == 0u) {
            period = 1u;
        }
        float phase = (float)(step % period) / (float)period;
        return (1.0F - cos(phase * 6.28318530F)) * 0.5F;
    }

    case BIOSIM_SENSOR_AGE:
        return (float)step / (float)steps_per_gen;

    case BIOSIM_SENSOR_RANDOM: {
        ulong state = rng_state[idx];
        state = biosim_rng_next(&state);
        rng_state[idx] = state;
        return (float)(state >> 40) * (1.0F / 16777216.0F);
    }

    case BIOSIM_SENSOR_POPULATION: {
        int r = sensor_radius;
        uint visited = 0u;
        uint occupied = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                visited++;
                uint cell = grid[ny * size_x + nx];
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    occupied++;
                }
            }
        }
        if (visited == 0u) {
            return 0.0F;
        }
        return (float)occupied / (float)visited;
    }

    case BIOSIM_SENSOR_POPULATION_FWD: {
        int dir = (int)(last_dir & 7u);
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint visited = 0u;
        uint occupied = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                if (dx * fwd_x + dy * fwd_y <= 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                visited++;
                uint cell = grid[ny * size_x + nx];
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    occupied++;
                }
            }
        }
        if (visited == 0u) {
            return 0.0F;
        }
        return (float)occupied / (float)visited;
    }

    case BIOSIM_SENSOR_POPULATION_LR: {
        int dir = (int)(last_dir & 7u);
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        int l_occ = 0;
        int r_occ = 0;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                uint cell = grid[ny * size_x + nx];
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    if (lateral > 0) {
                        l_occ++;
                    } else {
                        r_occ++;
                    }
                }
            }
        }
        if (l_occ == 0 && r_occ == 0) {
            return 0.0F;
        }
        return ((float)l_occ - (float)r_occ) / (float)(l_occ + r_occ);
    }

    case BIOSIM_SENSOR_BARRIER_FWD: {
        int dir = (int)(last_dir & 7u);
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint visited = 0u;
        uint n_bar = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                if (dx * fwd_x + dy * fwd_y <= 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                visited++;
                if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                    n_bar++;
                }
            }
        }
        if (visited == 0u) {
            return 0.0F;
        }
        return (float)n_bar / (float)visited;
    }

    case BIOSIM_SENSOR_BARRIER_LR: {
        int dir = (int)(last_dir & 7u);
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        int l_bar = 0;
        int r_bar = 0;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                    if (lateral > 0) {
                        l_bar++;
                    } else {
                        r_bar++;
                    }
                }
            }
        }
        if (l_bar == 0 && r_bar == 0) {
            return 0.0F;
        }
        return ((float)l_bar - (float)r_bar) / (float)(l_bar + r_bar);
    }

    case BIOSIM_SENSOR_LONGPROBE_POP_FWD: {
        int dir = (int)(last_dir & 7u);
        int step_x = BIOSIM_DIR_DX[dir];
        int step_y = BIOSIM_DIR_DY[dir];
        uint dist = (uint)los_range_val;
        if (dist == 0u) {
            return 0.0F;
        }
        for (uint i = 1u; i <= dist; i++) {
            int nx = x + (int)i * step_x;
            int ny = y + (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                break;
            }
            uint cell = grid[ny * size_x + nx];
            if (cell == BIOSIM_GRID_BARRIER) {
                break;
            }
            if (cell != BIOSIM_GRID_EMPTY) {
                return (float)i / (float)dist;
            }
        }
        return 0.0F;
    }

    case BIOSIM_SENSOR_LONGPROBE_BAR_FWD: {
        int dir = (int)(last_dir & 7u);
        int step_x = BIOSIM_DIR_DX[dir];
        int step_y = BIOSIM_DIR_DY[dir];
        uint dist = (uint)los_range_val;
        if (dist == 0u) {
            return 0.0F;
        }
        for (uint i = 1u; i <= dist; i++) {
            int nx = x + (int)i * step_x;
            int ny = y + (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                break;
            }
            if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                return (float)i / (float)dist;
            }
        }
        return 0.0F;
    }

    case BIOSIM_SENSOR_SIGNAL0: {
        uint val = signal[(int)y * size_x + (int)x];
        if (val > 255u) {
            val = 255u;
        }
        return (float)val / 255.0F;
    }

    case BIOSIM_SENSOR_SIGNAL0_FWD: {
        int dir = (int)(last_dir & 7u);
        int step_x = BIOSIM_DIR_DX[dir];
        int step_y = BIOSIM_DIR_DY[dir];
        uint dist = (uint)los_range_val;
        if (dist == 0u) {
            return 0.0F;
        }
        uint total = 0u;
        uint visited = 0u;
        for (uint i = 1u; i <= dist; i++) {
            int nx = x + (int)i * step_x;
            int ny = y + (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                break;
            }
            visited++;
            uint val = signal[ny * size_x + nx];
            if (val > 255u) {
                val = 255u;
            }
            total += val;
        }
        if (visited == 0u) {
            return 0.0F;
        }
        return (float)total / (255.0F * (float)visited);
    }

    case BIOSIM_SENSOR_SIGNAL0_LR: {
        int dir = (int)(last_dir & 7u);
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint l_sum = 0u;
        uint r_sum = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int lateral = dx * fwd_y - dy * fwd_x;
                if (lateral == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                uint val = signal[ny * size_x + nx];
                if (val > 255u) {
                    val = 255u;
                }
                if (lateral > 0) {
                    l_sum += val;
                } else {
                    r_sum += val;
                }
            }
        }
        if (l_sum == 0u && r_sum == 0u) {
            return 0.0F;
        }
        return ((float)l_sum - (float)r_sum) / ((float)l_sum + (float)r_sum);
    }

    default:
        return 0.5F;
    }
}

/* ── K1 kernel ──────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
__kernel void k_feedforward(
    __global uchar *alive,
    __global const int *loc_x,
    __global const int *loc_y,
    __global ushort *osc_period,
    __global const uchar *last_move_dir,
    __global float *responsiveness,
    __global uchar *los_range,
    __global const ushort *conn_packed,
    __global const short *conn_weight,
    __global const ushort *conn_length,
    __global float *neuron_output,
    __global const uchar *neuron_driven,
    __global const uchar *neuron_count,
    __global uint *signal,
    __global ulong *rng_state,
    __global int *desired_x,
    __global int *desired_y,
    __global const uint *grid,
    __global uchar *kill_marker,
    int size_x,
    int size_y,
    uint step,
    uint steps_per_gen,
    uint pop,
    int enable_kill,
    int sensor_radius
) {
    uint idx = get_global_id(0);

    if (!alive[idx]) {
        return;
    }

    int x = loc_x[idx];
    int y = loc_y[idx];
    uchar ldir = last_move_dir[idx];
    ushort osc_per = osc_period[idx];
    float resp = responsiveness[idx];
    uchar los_range_val = los_range[idx];

    /* ── Phase 1: evaluate sensors ───────────────────────────────────────── */

    float sensor_vals[BIOSIM_NUM_SENSORS];
    for (int s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = eval_sensor(
            rng_state,
            signal,
            grid,
            s,
            idx,
            x,
            y,
            ldir,
            osc_per,
            step,
            steps_per_gen,
            size_x,
            size_y,
            sensor_radius,
            los_range_val
        );
    }

    /* ── Phase 2: feedforward ────────────────────────────────────────────── */

    float nacc[128];
    float aval[BIOSIM_NUM_ACTIONS];
    for (int k = 0; k < 128; k++) {
        nacc[k] = 0.0F;
    }
    for (int a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        aval[a] = 0.0F;
    }

    uint nconn = (uint)conn_length[idx];
    uint ncount = (uint)neuron_count[idx];

    for (uint j = 0u; j < nconn; j++) {
        uint slot = j * pop + idx;
        ushort packed = conn_packed[slot];
        short raw_wgt = conn_weight[slot];

        uint src_type = BIOSIM_GENE_SRC_TYPE(packed);
        uint src_num = BIOSIM_GENE_SRC_NUM(packed);
        uint sink_type = BIOSIM_GENE_SINK_TYPE(packed);
        uint sink_num = BIOSIM_GENE_SINK_NUM(packed);

        float source = (src_type == BIOSIM_GENE_IO) ? sensor_vals[src_num]
                                                    : neuron_output[src_num * pop + idx];

        float weighted = source * ((float)raw_wgt / BIOSIM_GENE_WEIGHT_SCALE);

        if (sink_type == BIOSIM_GENE_NEURON) {
            nacc[sink_num] += weighted;
        } else {
            aval[sink_num] += weighted;
        }
    }

    for (uint k = 0u; k < ncount; k++) {
        uint nslot = k * pop + idx;
        neuron_output[nslot] = neuron_driven[nslot] ? tanh(nacc[k]) : 0.5F;
    }

    /* ── Phase 3: apply actions ──────────────────────────────────────────── */

    float dx_sum = 0.0F;
    float dy_sum = 0.0F;

    /* Group A: self-field writers */
    {
        float t = tanh(aval[BIOSIM_ACTION_SET_RESPONSIVENESS]);
        responsiveness[idx] = t * 0.5F + 0.5F;
        resp = responsiveness[idx];
    }

    {
        float t = tanh(aval[BIOSIM_ACTION_SET_OSCILLATOR_PERIOD]);
        float f = 2.0F * pow(1024.0F, (t + 1.0F) * 0.5F);
        if (f < 2.0F) {
            f = 2.0F;
        }
        if (f > 2048.0F) {
            f = 2048.0F;
        }
        osc_period[idx] = (ushort)f;
    }

    {
        float t = tanh(aval[BIOSIM_ACTION_SET_LONGPROBE_DIST]);
        float f = 1.0F + 31.0F * (t + 1.0F) * 0.5F;
        if (f < 1.0F) {
            f = 1.0F;
        }
        if (f > 32.0F) {
            f = 32.0F;
        }
        los_range[idx] = (uchar)f;
    }

    /* Group B: movement accumulators */
    dx_sum += resp * aval[BIOSIM_ACTION_MOVE_X];
    dy_sum += resp * aval[BIOSIM_ACTION_MOVE_Y];

    {
        int dir = (int)(ldir & 7u);
        dx_sum += resp * aval[BIOSIM_ACTION_MOVE_FORWARD] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * aval[BIOSIM_ACTION_MOVE_FORWARD] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 4u) & 7u);
        dx_sum += resp * aval[BIOSIM_ACTION_MOVE_REVERSE] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * aval[BIOSIM_ACTION_MOVE_REVERSE] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 2u) & 7u);
        dx_sum += resp * aval[BIOSIM_ACTION_MOVE_LEFT] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * aval[BIOSIM_ACTION_MOVE_LEFT] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 6u) & 7u);
        dx_sum += resp * aval[BIOSIM_ACTION_MOVE_RIGHT] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * aval[BIOSIM_ACTION_MOVE_RIGHT] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int ldir_l = (int)((ldir + 2u) & 7u);
        int rdir = (int)((ldir + 6u) & 7u);
        float trl = tanh(aval[BIOSIM_ACTION_MOVE_RL]);
        float rw = (trl + 1.0F) * 0.5F;
        float lw = 1.0F - rw;
        dx_sum += resp * ((float)BIOSIM_DIR_DX[rdir] * rw + (float)BIOSIM_DIR_DX[ldir_l] * lw);
        dy_sum += resp * ((float)BIOSIM_DIR_DY[rdir] * rw + (float)BIOSIM_DIR_DY[ldir_l] * lw);
    }

    {
        ulong state = rng_state[idx];
        int rdir = (int)(biosim_rng_next(&state) % 8u);
        rng_state[idx] = state;
        dx_sum += resp * (float)BIOSIM_DIR_DX[rdir];
        dy_sum += resp * (float)BIOSIM_DIR_DY[rdir];
    }

    dx_sum += resp * (float)BIOSIM_DIR_DX[0]; /* MOVE_EAST */
    dy_sum += resp * (float)BIOSIM_DIR_DY[0];

    dx_sum += resp * (float)BIOSIM_DIR_DX[4]; /* MOVE_WEST */
    dy_sum += resp * (float)BIOSIM_DIR_DY[4];

    dx_sum += resp * (float)BIOSIM_DIR_DX[2]; /* MOVE_NORTH */
    dy_sum += resp * (float)BIOSIM_DIR_DY[2];

    dx_sum += resp * (float)BIOSIM_DIR_DX[6]; /* MOVE_SOUTH */
    dy_sum += resp * (float)BIOSIM_DIR_DY[6];

    /* Group C: signal emission */
    if (aval[BIOSIM_ACTION_EMIT_SIGNAL0] >= 0.5F) {
        int ex = (int)x;
        int ey = (int)y;
        atomic_add(&signal[ey * size_x + ex], 2u);
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                int nx = ex + dx;
                int ny = ey + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                atomic_add(&signal[ny * size_x + nx], 1u);
            }
        }
    }

    /* Group D: KILL_FORWARD */
    if (enable_kill && aval[BIOSIM_ACTION_KILL_FORWARD] >= 0.5F) {
        int dir = (int)(ldir & 7u);
        int fx = (int)x + BIOSIM_DIR_DX[dir];
        int fy = (int)y + BIOSIM_DIR_DY[dir];
        if (fx >= 0 && fx < size_x && fy >= 0 && fy < size_y) {
            uint cell = grid[(int)fy * size_x + fx];
            if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                alive[cell - 1u] = 0u;
                kill_marker[cell - 1u] = 1u;
            }
        }
    }

    /* ── Phase 4: finalize movement ──────────────────────────────────────── */

    float lx = tanh(dx_sum * 0.5F);
    float ly = tanh(dy_sum * 0.5F);

    float rx = rng_float_k(rng_state, idx);
    float ry = rng_float_k(rng_state, idx);

    int step_x = (fabs(lx) > rx) ? (lx >= 0.0F ? 1 : -1) : 0;
    int step_y = (fabs(ly) > ry) ? (ly >= 0.0F ? 1 : -1) : 0;

    int nx = x + step_x;
    int ny = y + step_y;

    if (nx < 0) {
        nx = 0;
    }
    if (nx >= size_x) {
        nx = size_x - 1;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (ny >= size_y) {
        ny = size_y - 1;
    }

    desired_x[idx] = nx;
    desired_y[idx] = ny;
}
