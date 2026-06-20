/* k1_feedforward.cl — K1: sensor evaluation, feedforward, actions, movement
 *
 * Preamble: grid_defs.h, rng.h, gene.h, and io_defs.h are prepended
 * as separate source strings by the build system (clCreateProgramWithSource).
 * Do NOT add #include directives for those files here.
 *
 * Sensor implementation:
 *   Group A (0-9)                 — fully implemented, including OSC1 (uses built-in cos)
 *   POPULATION (10)               — circular-disc neighbourhood scan (grid read-only)
 *   POPULATION_FWD (11)           — front/rear signed ratio (F−R)/(F+R), in [−1,1]
 *   POPULATION_LR (12)            — lateral signed ratio (L−R)/(L+R), in [−1,1]
 *   BARRIER_FWD (13)              — bidirectional barrier probe along fwd/rev axis, in [0,1]
 *   BARRIER_LR (14)               — bidirectional barrier probe along left/right axis, in [0,1]
 *   LONGPROBE_POP_FWD (15)        — forward ray-cast, returns steps/dist to first agent
 *   LONGPROBE_BAR_FWD (16)        — forward ray-cast (skips agents), steps/dist to first barrier
 *   SIGNAL0 (17)                  — reads signal buffer at agent position
 *   SIGNAL0_FWD (18)              — signal front/rear signed ratio (F−R)/(F+R), in [−1,1]
 *   BIOSIM_SENSOR_SIGNAL0_LR (19) — lateral signed signed ratio (L−R)/(L+R), in [−1,1]
 *   All others (20)               — stub returning 0.5
 *
 * Action implementation:
 *   Group A (0-2)   — SET_RESPONSIVENESS, SET_OSCILLATOR_PERIOD,
 *                     SET_LONGPROBE_DIST (uses built-in tanh, pow)
 *   Group B (3-13)  — movement accumulators
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

static float response_curve(float r, float k) {
    return pow(2.0F - r, -2.0F * k) - pow(2.0F, -2.0F * k) * (1.0F - r);
}

/* Commit one sink's accumulated feedforward input.  A neuron sink applies tanh
 * to driven neurons (undriven hold the quiescent baseline 0.5F); an action sink
 * stores the raw accumulator.  Each sink owns one contiguous run of sorted
 * connections, so a plain store is correct. */
static void flush_sink(
    __global float *neuron_output,
    __global const uchar *neuron_driven,
    float *action_vals,
    uint cur_type,
    uint cur_num,
    float acc,
    uint idx,
    uint pop
) {
    if (cur_type == BIOSIM_GENE_NEURON) {
        uint nslot = cur_num * pop + idx;
        neuron_output[nslot] = neuron_driven[nslot] ? tanh(acc) : 0.5F;
    } else {
        action_vals[cur_num] = acc;
    }
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
        uint front = 0u;
        uint rear = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int dot = dx * fwd_x + dy * fwd_y;
                if (dot == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                uint cell = grid[ny * size_x + nx];
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    if (dot > 0) {
                        front++;
                    } else {
                        rear++;
                    }
                }
            }
        }
        if (front == 0u && rear == 0u) {
            return 0.5F;
        }
        return ((float)front - (float)rear) / (float)(front + rear) * 0.5F + 0.5F;
    }

    case BIOSIM_SENSOR_POPULATION_LR: {
        int dir = (int)((last_dir + 2) & 7u); /* Note the +2 rotation */
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint front = 0u;
        uint rear = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int dot = dx * fwd_x + dy * fwd_y;
                if (dot == 0) {
                    continue;
                }
                int nx = x + dx;
                int ny = y + dy;
                if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                    continue;
                }
                uint cell = grid[ny * size_x + nx];
                if (cell != BIOSIM_GRID_EMPTY && cell != BIOSIM_GRID_BARRIER) {
                    if (dot > 0) {
                        front++;
                    } else {
                        rear++;
                    }
                }
            }
        }
        if (front == 0u && rear == 0u) {
            return 0.5F;
        }
        return ((float)front - (float)rear) / (float)(front + rear) * 0.5F + 0.5F;
    }

    case BIOSIM_SENSOR_BARRIER_FWD: {
        int dir = (int)(last_dir & 7u);
        int step_x = BIOSIM_DIR_DX[dir];
        int step_y = BIOSIM_DIR_DY[dir];
        uint probe = sensor_radius;
        if (probe == 0u) {
            return 0.5F;
        }
        uint count_fwd = 0u;
        for (uint i = 1u; i <= probe; i++) {
            int nx = x + (int)i * step_x;
            int ny = y + (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                count_fwd = probe;
                break;
            }
            if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                break;
            }
            count_fwd++;
        }
        uint count_rev = 0u;
        for (uint i = 1u; i <= probe; i++) {
            int nx = x - (int)i * step_x;
            int ny = y - (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                count_rev = probe;
                break;
            }
            if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                break;
            }
            count_rev++;
        }
        return ((float)count_fwd - (float)count_rev + (float)probe) / (2.0F * (float)probe);
    }

    case BIOSIM_SENSOR_BARRIER_LR: {
        int dir = (int)((last_dir + 2u) & 7u); /* Note the +2 rotation */
        int step_x = BIOSIM_DIR_DX[dir];
        int step_y = BIOSIM_DIR_DY[dir];
        uint probe = sensor_radius;
        if (probe == 0u) {
            return 0.5F;
        }
        uint count_fwd = 0u;
        for (uint i = 1u; i <= probe; i++) {
            int nx = x + (int)i * step_x;
            int ny = y + (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                count_fwd = probe;
                break;
            }
            if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                break;
            }
            count_fwd++;
        }
        uint count_rev = 0u;
        for (uint i = 1u; i <= probe; i++) {
            int nx = x - (int)i * step_x;
            int ny = y - (int)i * step_y;
            if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                count_rev = probe;
                break;
            }
            if (grid[ny * size_x + nx] == BIOSIM_GRID_BARRIER) {
                break;
            }
            count_rev++;
        }
        return ((float)count_fwd - (float)count_rev + (float)probe) / (2.0F * (float)probe);
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
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint front_sum = 0u;
        uint rear_sum = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int dot = dx * fwd_x + dy * fwd_y;
                if (dot == 0) {
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
                if (dot > 0) {
                    front_sum += val;
                } else {
                    rear_sum += val;
                }
            }
        }
        if (front_sum == 0u && rear_sum == 0u) {
            return 0.5F;
        }
        return ((float)front_sum - (float)rear_sum) / ((float)front_sum + (float)rear_sum) * 0.5F +
               0.5F;
    }

    case BIOSIM_SENSOR_SIGNAL0_LR: {
        int dir = (int)((last_dir + 2) & 7u); /* Note the +2 rotation*/
        int fwd_x = BIOSIM_DIR_DX[dir];
        int fwd_y = BIOSIM_DIR_DY[dir];
        int r = sensor_radius;
        uint front_sum = 0u;
        uint rear_sum = 0u;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx * dx + dy * dy > r * r) {
                    continue;
                }
                int dot = dx * fwd_x + dy * fwd_y;
                if (dot == 0) {
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
                if (dot > 0) {
                    front_sum += val;
                } else {
                    rear_sum += val;
                }
            }
        }
        if (front_sum == 0u && rear_sum == 0u) {
            return 0.5F;
        }
        return ((float)front_sum - (float)rear_sum) / ((float)front_sum + (float)rear_sum) * 0.5F +
               0.5F;
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
    int sensor_radius,
    float resp_curve_k
) {
    uint idx = get_global_id(0);

    if (!alive[idx]) {
        return;
    }

    int x = loc_x[idx];
    int y = loc_y[idx];
    uchar ldir = last_move_dir[idx];
    ushort osc_per = osc_period[idx];
    float resp = response_curve(responsiveness[idx], resp_curve_k);
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

    float action_vals[BIOSIM_NUM_ACTIONS];
    for (int a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        action_vals[a] = 0.0F;
    }

    uint nconn = (uint)conn_length[idx];

    /* Connections are sorted by sink (neuron sinks first, then actions, each
     * ascending by number), so a single scalar accumulator collects one sink's
     * inputs and is flushed when the sink changes.  Neuron outputs are committed
     * before later connections — including every action sink — read them. */
    if (nconn > 0u) {
        float acc = 0.0F;
        ushort packed0 = conn_packed[idx];
        uint cur_type = BIOSIM_GENE_SINK_TYPE(packed0);
        uint cur_num = BIOSIM_GENE_SINK_NUM(packed0);

        for (uint j = 0u; j < nconn; j++) {
            uint slot = j * pop + idx;
            ushort packed = conn_packed[slot];
            short raw_wgt = conn_weight[slot];

            uint src_type = BIOSIM_GENE_SRC_TYPE(packed);
            uint src_num = BIOSIM_GENE_SRC_NUM(packed);
            uint sink_type = BIOSIM_GENE_SINK_TYPE(packed);
            uint sink_num = BIOSIM_GENE_SINK_NUM(packed);

            if (sink_type != cur_type || sink_num != cur_num) {
                flush_sink(
                    neuron_output, neuron_driven, action_vals, cur_type, cur_num, acc, idx, pop
                );
                acc = 0.0F;
                cur_type = sink_type;
                cur_num = sink_num;
            }

            float source = (src_type == BIOSIM_GENE_IO) ? sensor_vals[src_num]
                                                        : neuron_output[src_num * pop + idx];

            acc += source * ((float)raw_wgt / BIOSIM_GENE_WEIGHT_SCALE);
        }

        flush_sink(neuron_output, neuron_driven, action_vals, cur_type, cur_num, acc, idx, pop);
    }

    /* ── Phase 3: apply actions ──────────────────────────────────────────── */

    float dx_sum = 0.0F;
    float dy_sum = 0.0F;

    /* Group A: self-field writers */
    {
        responsiveness[idx] = tanh(action_vals[BIOSIM_ACTION_SET_RESPONSIVENESS]) * 0.5F + 0.5F;
        resp = response_curve(responsiveness[idx], resp_curve_k);
    }

    {
        float s = tanh(action_vals[BIOSIM_ACTION_SET_OSCILLATOR_PERIOD]) * 0.5F + 0.5F;
        float f = 2.0F * pow(1024.0F, s);
        if (f < 2.0F) {
            f = 2.0F;
        }
        if (f > 2048.0F) {
            f = 2048.0F;
        }
        osc_period[idx] = (ushort)f;
    }

    {
        float s = tanh(action_vals[BIOSIM_ACTION_SET_LONGPROBE_DIST]) * 0.5F + 0.5F;
        float f = 1.0F + 31.0F * s;
        if (f < 1.0F) {
            f = 1.0F;
        }
        if (f > 32.0F) {
            f = 32.0F;
        }
        los_range[idx] = (uchar)f;
    }

    /* Group B: movement accumulators */
    dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_X];
    dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_Y];

    {
        int dir = (int)(ldir & 7u);
        dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_FORWARD] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_FORWARD] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 4u) & 7u);
        dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_REVERSE] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_REVERSE] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 2u) & 7u);
        dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_LEFT] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_LEFT] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        int dir = (int)((ldir + 6u) & 7u);
        dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_RIGHT] * (float)BIOSIM_DIR_DX[dir];
        dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_RIGHT] * (float)BIOSIM_DIR_DY[dir];
    }

    {
        ulong state = rng_state[idx];
        int rdir = (int)(biosim_rng_next(&state) % 8u);
        rng_state[idx] = state;
        dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_RANDOM] * (float)BIOSIM_DIR_DX[rdir];
        dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_RANDOM] * (float)BIOSIM_DIR_DY[rdir];
    }

    dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_EAST] * (float)BIOSIM_DIR_DX[0]; /* MOVE_EAST */
    dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_EAST] * (float)BIOSIM_DIR_DY[0];

    dx_sum += resp * action_vals[BIOSIM_ACTION_MOVE_WEST] * (float)BIOSIM_DIR_DX[4]; /* MOVE_WEST */
    dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_WEST] * (float)BIOSIM_DIR_DY[4];

    dx_sum +=
        resp * action_vals[BIOSIM_ACTION_MOVE_NORTH] * (float)BIOSIM_DIR_DX[2]; /* MOVE_NORTH */
    dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_NORTH] * (float)BIOSIM_DIR_DY[2];

    dx_sum +=
        resp * action_vals[BIOSIM_ACTION_MOVE_SOUTH] * (float)BIOSIM_DIR_DX[6]; /* MOVE_SOUTH */
    dy_sum += resp * action_vals[BIOSIM_ACTION_MOVE_SOUTH] * (float)BIOSIM_DIR_DY[6];

    /* Group C: signal emission */
    {
        float act = tanh(action_vals[BIOSIM_ACTION_EMIT_SIGNAL0]) * resp;
        if (act > 0.0F) {
            int r = 1 + (int)round(act * 4.0F);
            int center_mag = (int)round(2.0F + act * 3.0F);
            int ex = (int)x;
            int ey = (int)y;
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx * dx + dy * dy > r * r) {
                        continue;
                    }
                    int deposit = center_mag - (int)sqrt((float)(dx * dx + dy * dy));
                    if (deposit <= 0) {
                        continue;
                    }
                    int nx = ex + dx;
                    int ny = ey + dy;
                    if (nx < 0 || nx >= size_x || ny < 0 || ny >= size_y) {
                        continue;
                    }
                    atomic_add(&signal[ny * size_x + nx], (uint)deposit);
                }
            }
        }
    }

    /* Group D: KILL_FORWARD */
    if (enable_kill) {
        float s = (tanh(action_vals[BIOSIM_ACTION_KILL_FORWARD]) + 1.0F) * 0.5F * resp;
        if (s > 0.5F) {
            ulong kf_state = rng_state[idx];
            uint i = (uint)(s * 16777216.0F);
            uint rng = (uint)(biosim_rng_next(&kf_state) >> 40);
            rng_state[idx] = kf_state;
            if (rng < i) {
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
