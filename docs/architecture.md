# Architecture

## Package structure

Two independent build trees share the `core` package but target different
toolchains. The native tree targets the host CPU; the webapp tree targets
WebAssembly via Emscripten.

**Native tree** (`debug`, `release`, `asan`, `ci` presets):

```
core (static lib — libc only)
  └── cfgparse (static lib — argtable3, tomlc17 PRIVATE)
      ├── sim-ref (executable — single-threaded CPU reference)
      └── sim-gpu     (static lib + executable — OpenCL GPU simulator)
```

**Webapp tree** (`wasm`, `webapp` presets — Emscripten):

```
core (static lib — libc only)
  └── sim-wasm (ES6 WASM module — biosim.mjs + biosim.wasm)
        └── webapp (Svelte SPA — loads sim-wasm in a Web Worker)
```

See [`STATUS.md`](../STATUS.md).

## Build

**Toolchain:** CMake 3.28+ with vcpkg (`VCPKG_ROOT` must be set).  
**Native compilers:** GCC and Clang on Linux; MSVC on Windows.  
**Webapp toolchain:** Emscripten (`EMSDK` must be set) + Bun.

| Preset | Tree | Purpose |
|---|---|---|
| `debug` | native | Debug info, assertions on, no optimisation |
| `release` | native | `-O3`, LTO, assertions off |
| `asan` | native | Debug + AddressSanitizer + UBSan |
| `ci` | native | Release + tests enabled |
| `webapp` | webapp | Emscripten + Bun/Vite — `sim-wasm` ES6 module + Svelte SPA |

Key targets (all via `cmake --build --preset <preset> --target <target>`):

- *(default)* — compile all packages
- `lint` — clang-tidy static analysis; required clean before merge (native only)
- `format` — clang-format all source files (native only)
- `dev` — start the Vite dev server (webapp preset only)

See [`docs/build.md`](build.md) for the full setup guide.

## `core`

**Location:** `packages/core/`  
**Type:** Static library. No external dependencies beyond the C standard
library.

Owns all shared simulation logic — everything that both the stepper and the
future GPU simulator need with identical behavior.

| Module | Key types | Key functions |
|--------|-----------|---------------|
| [`sim.h`](../packages/core/include/biosim/core/sim.h) | `biosim_sim_t` | <ul><li>`biosim_sim_step_agent` — sense, feedforward, act, and propose a move for one agent</li><li>`biosim_sim_next_step` — commit kills, grant moves, fade signal, run per-step challenge hook</li><li>`biosim_sim_retire_generation` — collect survivors into snap, take census, write snapshot, advance generation counter</li></ul> |
| [`agents.h`](../packages/core/include/biosim/core/agents.h) | `biosim_agents_t` | <ul><li>`biosim_agents_init_slot` — mark alive, set position, seed RNG</li></ul> |
| [`grid.h`](../packages/core/include/biosim/core/grid.h) | `biosim_grid_t` | <ul><li>`biosim_grid_at` — read a cell</li><li>`biosim_grid_set` — write a cell</li><li>`biosim_grid_find_empty` — random empty-cell search</li></ul> |
| [`genome.h`](../packages/core/include/biosim/core/genome.h) | `biosim_genome_t` | <ul><li>`biosim_genome_init_slot` — randomize one agent's genome</li><li>`biosim_genome_copy_slot` — copy genome between slots</li><li>`biosim_genome_mutate` — point / insertion / deletion mutations</li><li>`biosim_genome_crossover` — single-point two-parent crossover</li><li>`biosim_genome_sort_by_length` — sort agents by descending length</li></ul> |
| [`nnet.h`](../packages/core/include/biosim/core/nnet.h) | `biosim_nnet_t` | <ul><li>`biosim_nnet_compile` — compile genome into neural network</li><li>`biosim_nnet_feedforward` — run one feedforward pass</li></ul> |
| [`io_defs.h`](../packages/core/include/biosim/core/io_defs.h) | `biosim_sensor_t`, `biosim_action_t` | HOST/DEVICE portable enums (21 sensors, 17 actions) and direction tables (`BIOSIM_DIR_DX`, `BIOSIM_DIR_DY`). Included by both host code and OpenCL kernels. |
| [`io_eval.h`](../packages/core/include/biosim/core/io_eval.h) | — | HOST-only API: <ul><li>`biosim_sensor_eval` — evaluate one sensor for an agent</li><li>`biosim_action_apply` — apply one action output</li><li>`biosim_action_propose_move` — convert accumulated dx/dy into desired_x/desired_y</li></ul> |
| [`challenges.h`](../packages/core/include/biosim/core/challenges.h) | `biosim_challenge_spec_t` | <ul><li>`biosim_challenge_step` — per-step challenge hook</li></ul> |
| [`generation.h`](../packages/core/include/biosim/core/generation.h) | — | <ul><li>`biosim_generation_collect_survivors` — evaluate challenge, build compact snap (genomes + scores + gen + gen_rng)</li><li>`biosim_generation_breed` — reproduce next population from snap</li><li>`biosim_generation_spawn` — breed if snap has survivors, else randomise</li><li>`biosim_generation_init_random` — randomise all agent genomes and positions</li></ul> |
| [`snapshot.h`](../packages/core/include/biosim/core/snapshot.h) | `biosim_survivor_snap_t` | <ul><li>`biosim_survivor_snap_grow` — grow snap buffers to fit n survivors</li><li>`biosim_survivor_snap_free` — release snap heap memory</li><li>`biosim_snapshot_session_open` — open a write session</li><li>`biosim_snapshot_session_write` — write current generation's survivors</li><li>`biosim_snapshot_session_close` — finalize the file</li><li>`biosim_snapshot_load_survivors` — load last record into snap and sim (caller calls `biosim_generation_spawn` next)</li></ul> |
| [`rng.h`](../packages/core/include/biosim/core/rng.h) | — | <ul><li>`biosim_rng_next` — advance xorshift64 and return state</li><li>`biosim_rng_seed` — initialize from two uint64_t seeds</li></ul> |
| [`status.h`](../packages/core/include/biosim/core/status.h) | `biosim_status_t` | <ul><li>`biosim_strerror` — map status code to human-readable string</li></ul> |
| [`log.h`](../packages/core/include/biosim/core/log.h) | `biosim_log_ctx_t` | <ul><li>`biosim_log_init` — set threshold, detect terminal color</li><li>`BIOSIM_ERRORF` / `WARNF` / `INFOF` / `DEBUGF` / `TRACEF` — level-gated macros</li></ul> |
| [`params.h`](../packages/core/include/biosim/core/params.h) | `biosim_params_t`, `biosim_param_entry_t` | <ul><li>`biosim_params_set_int` / `_float` / `_bool` / `_string` — typed setters</li><li>`biosim_params_get_int` / `_float` / `_bool` / `_string` — typed getters (abort on type mismatch)</li><li>`biosim_params_find` — locate an entry by key</li></ul> |

`biosim_sim_t` is the complete simulation state. Call `biosim_sim_create` with a
parsed `biosim_params_t` and `biosim_challenge_spec_t` to configure all fields,
allocate all heap resources, and spawn the initial population.
`biosim_sim_free` releases everything, including any open snapshot session.

### SoA data layout

Per-agent data is stored in Structure of Arrays. Each field is a separate flat
buffer indexed by agent slot (0..population−1). `biosim_agents_t` holds all
per-agent buffers; `biosim_genome_t` and `biosim_nnet_t` store their
variable-length data in a transposed layout — gene slot `j` of agent `i` is
at index `j * population + i`. This layout is GPU-coalescing-friendly: when
all work-items advance to gene `j` together, they read contiguous memory.

```c
/* packages/core/include/biosim/core/agents.h — flat per-agent SoA */
/* agent i state stored at index i */
typedef struct {
    uint32_t  population;
    /* Position — split for independent coalesced access on GPU */
    int32_t  *loc_x;
    int32_t  *loc_y;
    int32_t  *birth_x;
    int32_t  *birth_y;
    uint8_t  *alive;
    uint16_t *osc_period;
    float    *responsiveness;
    uint8_t  *los_range;
    uint8_t  *last_move_dir;
    uint32_t *challenge_bits;
    uint64_t *rng_state;
    uint64_t *genome_fingerprint;
    /* Transient per-step movement targets */
    int32_t  *desired_x;
    int32_t  *desired_y;
    float    *dx_sum;
    float    *dy_sum;
} biosim_agents_t;

/* packages/core/include/biosim/core/genome.h — transposed SoA        */
/* gene slot j of agent i is at index  j * population + i             */
typedef struct {
    uint32_t  population;
    uint16_t  max_genes;
    uint16_t *conn; /* [gene_slot * population + agent_idx] */
    int16_t  *wgt;  /* [gene_slot * population + agent_idx] */
    uint16_t *len;  /* [agent_idx] active gene count */
} biosim_genome_t;
```

### Generation boundary: `biosim_survivor_snap_t`

`biosim_survivor_snap_t` (defined in `snapshot_defs.h`) is the self-contained
record that crosses every generation boundary. `biosim_generation_collect_survivors`
fills it; `biosim_generation_breed` reads it; `biosim_snapshot_session_write` serialises
it to disk; `biosim_snapshot_load_survivors` deserialises it back.

```c
typedef struct {
    uint16_t *conn;      /* compact row-major: survivor s, gene j → s*stride_cap+j */
    int16_t  *wgt;       /* same layout as conn */
    uint16_t *len;       /* genome length per survivor */
    float    *scores;    /* challenge score per survivor */
    uint32_t  count;     /* live survivor count */
    uint32_t  pop_cap;   /* allocated survivor slots (grows on demand) */
    uint16_t  stride_cap;/* allocated columns per survivor (>= max genome len) */
    uint32_t  gen;       /* generation index at collection time */
    uint64_t  gen_rng;   /* sim RNG state before breed — sufficient to replay */
    uint16_t  max_genes; /* genome-length cap of the originating config */
    uint8_t   max_neurons;    /* neuron cap of the originating config */
} biosim_survivor_snap_t;
```

Together, the genome buffers, scores, `gen`, and `gen_rng` fully scope a
reproducible simulation boundary: restoring a snap and calling
`biosim_generation_breed` with the same `gen_rng` on `sim->gen_rng` produces
an identical next-generation population. `sim->gen_rng` remains the live
authoritative state; `snap->gen_rng` is the serialisation carrier.

### Host/device portability

Five headers are shared with OpenCL kernel sources and must compile as both
C11 and OpenCL C (see [`docs/conventions-c.md`](conventions-c.md) for the
`_defs.h` convention and portability rules):

- `core/grid_defs.h` — `biosim_coord_t`, `BIOSIM_GRID_EMPTY`,
  `BIOSIM_GRID_BARRIER`, fixed-width integer typedefs for OpenCL C
- `core/rng.h` — `biosim_rng_next`, `biosim_rng_seed`
- `core/gene.h` — gene bit-layout macros (`BIOSIM_GENE_PACK`,
  `BIOSIM_GENE_SRC_TYPE`, ...)
- `core/io_defs.h` — `biosim_sensor_t`, `biosim_action_t` enums and
  direction tables (`BIOSIM_DIR_DX`, `BIOSIM_DIR_DY`)
- `core/challenge_defs.h` — `biosim_challenge_kind_t` enum

## `cfgparse`

**Location:** `packages/cfgparse/`  
**Type:** Static library. Depends on `core` (PUBLIC), argtable3 and tomlc17
(PRIVATE).

Owns CLI argument parsing and TOML config file loading. The parameter data
model (`biosim_params_t`, `biosim_param_entry_t`, setters/getters) lives in
`core/params.h`. Each simulator's `main.c` declares a static
`biosim_param_entry_t[]` table — one entry per parameter — and calls
`biosim_params_parse` once at startup. The `cfgparse` package does the rest.

| Header | Role |
|--------|------|
| `biosim/cfgparse/barriers.h` | `biosim_barrier_params_load` — parse barrier specs from a TOML file |
| `biosim/cfgparse/challenges.h` | `biosim_challenge_spec_from_params` — build a challenge spec from resolved params |

Resolution order: compiled-in defaults → TOML file (`--config`) → CLI flags.
Each layer overrides the previous.

See [`docs/formats.md`](formats.md) for the TOML layout.  
See [`docs/usage.md`](usage.md) for the full parameter reference.

## `sim-ref`

**Location:** `packages/sim-ref/`  
**Type:** Executable. Depends on `core` and `cfgparse`.

A thin orchestration shell. `main.c` parses parameters, populates
`biosim_sim_t`, and drives a two-level loop — generation loop over step loop
over agents. All simulation logic lives in `core`; the stepper adds nothing
but the loop and the parameter table.

Agents are processed in increasing-index order with immediate effect
application. This is the deterministic reference ordering: conflicts resolve
first-come-first-served by index.

### Main loop

```c
// packages/sim-ref/src/main.c — main loop (simplified)
biosim_params_parse(&p, progname, version, argc, argv);
biosim_barrier_params_load(config_path, &barriers, &n_barriers);
biosim_challenge_spec_from_params(&p, &challenge);
biosim_sim_create(&sim, &p, &challenge, barriers, n_barriers);

biosim_survivor_snap_t snap = {0};
if (snap_in_path) {
    biosim_snapshot_load_survivors(snap_in_path, &sim, &snap);
    sim.gen++;  /* convention: snapshots hold the survivors' gen index */
}
biosim_generation_spawn(&sim, &snap);                  // breed or randomise
biosim_snapshot_session_open(&sim, snap_out_path, N);  // if --snapshot-out

while (sim.gen < sim.max_generations) {
    while (sim.step < sim.steps_per_gen) {
        for (uint32_t i = 0; i < sim.population; i++)
            if (sim.agents.alive[i])
                biosim_sim_step_agent(&sim, i);
        biosim_sim_next_step(&sim);
    }
    biosim_sim_retire_generation(&sim, &snap, &census); // collect, census, write, gen++
    biosim_census_print(stdout, &census);
    biosim_generation_spawn(&sim, &snap);               // breed survivors into next gen
}
biosim_sim_free(&sim);
```

See [`docs/usage.md`](usage.md) for the command-line reference.

## `sim-gpu`

**Location:** `packages/sim-gpu/`  
**Type:** Static library (`sim-gpu-lib`) + executable (`biosim-gpu`). Depends on
`core`, `cfgparse`, and OpenCL (vcpkg port `opencl`).

Implements the OpenCL GPU simulator. Kernel sources are embedded at build time as
C string literals so the binary is self-contained; a filesystem override mechanism
allows hot-swapping a kernel without rebuilding.

### Kernel registry — two-level lookup

`biosim_gpu_registry_get(kernel_name, exec_dir, &sources)` fills a
`biosim_gpu_kernel_sources_t` bundle in two steps:

1. **Filesystem override** — looks for `<exec_dir>/<kernel_name>.cl` alongside the
   running binary. If found, reads the file into a malloc'd buffer and uses it as the
   kernel source. Useful during development: edit the `.cl` file, re-run, no rebuild
   needed.

2. **Embedded fallback** — uses the C string literal compiled into the binary by
   `EmbedKernels.cmake`.

In both cases the bundle's `sources[]` array is:

```
sources[0] = biosim/core/grid_defs.h     (embedded preamble)
sources[1] = biosim/core/rng.h           (embedded preamble)
sources[2] = biosim/core/gene.h          (embedded preamble)
sources[3] = biosim/core/io_defs.h       (embedded preamble)
sources[4] = biosim/core/challenge_defs.h (embedded preamble)
sources[5] = <kernel source>             (override or embedded)
```

All five strings are passed to `clCreateProgramWithSource`, which concatenates them
before compilation. The portable headers provide type definitions, `biosim_rng_next`,
gene-encoding macros, and sensor/action enums to kernel code without needing
`#include` directives in the `.cl` file.

### K1 kernel — `k1_feedforward.cl`

Implements one full simulation step per agent in parallel (`k_feedforward`). Each
work-item runs the complete pipeline for one agent:

1. **Sensor evaluation** — all 21 sensors; Group A (LOC_X … RANDOM) and SIGNAL0
   are fully implemented; Group B/C/D sensors return 0.5 (stub).
2. **Neural network feedforward** — single pass over the compiled connection list
   stored in SoA layout (`conn_packed`, `conn_weight`); writes updated
   `neuron_output`.
3. **Action application** — Group A self-fields (responsiveness, oscillator period,
   long-probe distance); Group B movement accumulators using `BIOSIM_DIR_DX/DY`;
   Group C signal emission via `atomic_add`; `KILL_FORWARD` is a no-op stub
   (requires grid buffer, deferred to K2).
4. **Movement finalization** — `tanh`-squashed accumulator compared against two
   xorshift64 draws; writes clamped `desired_x`/`desired_y`.

### K2 kernel — `k2_kill_marked.cl`

Runs after K1. One work-item per agent (`N` work-items). Clear the cell for each
agent whose `kill_marker` flag was set by K1's `KILL_FORWARD` action.

### K3 kernel — `k3_movement_resolution.cl`

Runs after K2. One work-item per alive agent (`N` work-items). Each agent attempts
to claim its `desired_x`/`desired_y` target with `atomic_cmpxchg`
(`BIOSIM_GRID_EMPTY → agent_index + 1`). On success: the old cell is cleared
(non-atomic; each agent exclusively owns its current cell at this point) and
`loc_x`, `loc_y`, `last_move_dir` are updated. On failure (cell already occupied
or is a barrier): the agent stays in place silently. Because multiple agents may
contest the same cell and the winner is determined by hardware scheduling order,
movement outcomes are non-deterministic and do not match the CPU reference.

### K4 kernel — `k4_signal_fade.cl`

Runs after K3. One work-item per grid cell (`SIZE_X × SIZE_Y` work-items). Each
work-item decrements its cell's signal value by 1, clamping at 0 to prevent
underflow: `signal[c] = (signal[c] > 0) ? signal[c] - 1 : 0`. Signal values are
`uint32_t` with meaningful range 0–255; the upper 24 bits are unused.

### K5 kernel — `k5_challenge_step_eval.cl`

Runs after K4. One work-item per alive agent (`N` work-items). GPU port of
`biosim_challenge_step()`. Three challenge kinds are handled: `TOUCH_ANY_WALL`,
`RADIOACTIVE_WALLS` and `LOCATION_SEQUENCE.

### `pipeline` module — GPU simulation engine

`biosim_gpu_pipeline_t` is the GPU-side counterpart of `core`'s `biosim_sim_t`:
it owns the programs, kernels, and GPU buffers for one simulation session and
drives the K1→K5 kernel sequence each step.

| Function | Role |
|----------|------|
| `biosim_gpu_pipeline_create(sim, runner, exec_dir, out)` | Build programs, allocate GPU buffers, upload initial state; set all static kernel args once |
| `biosim_gpu_pipeline_step(p, sim)` | Enqueue one complete step (K1→K5); no clFinish |
| `biosim_gpu_pipeline_sync_to_host(p, sim)` | clFinish + download alive, loc, challenge_bits, signal for generation evaluation |
| `biosim_gpu_pipeline_sync_from_host(p, sim)` | Re-upload all mutable buffers after `biosim_generation_spawn` |
| `biosim_gpu_pipeline_free(p)` | Release all GPU resources |

Spawn creates the new HOST population; `sync_from_host` uploads it to the GPU
before the step kernels run; `sync_to_host` downloads results so `retire_generation`
can evaluate survivors.
```c
while (sim.gen < sim.max_generations) {
    biosim_generation_spawn(&sim, &snap);                      // new HOST population
    biosim_gpu_pipeline_sync_from_host(&pipeline, &sim);       // upload to GPU
    while (sim.step < sim.steps_per_gen) {
        biosim_gpu_pipeline_step(&pipeline, &sim);
        sim.step++;
    }
    biosim_gpu_pipeline_sync_to_host(&pipeline, &sim);
    biosim_sim_retire_generation(&sim, &snap, &census);        // collect, census, write, gen++
    biosim_census_print(stdout, &census);
}
```

### Key types

| Type | Role |
|------|------|
| `biosim_gpu_kernel_sources_t` | Bundle of source strings for one kernel program |
| `biosim_gpu_runner_t` | OpenCL platform, device, context, and queue |
| `biosim_gpu_pipeline_t` | Programs, kernels, and GPU buffers for one session |

See [`docs/gpu-design.md`](gpu-design.md) for the full kernel pipeline design.

## `sim-wasm`

**Location:** `packages/sim-wasm/`  
**Type:** Emscripten ES6 module (output: `biosim.mjs` + `biosim.wasm`).  
**Dependencies:** `core`.

Compiles `core` to WebAssembly and exposes a flat C API to JavaScript. The
single translation unit `src/bindings.c` wraps the `core` simulation behind
`EMSCRIPTEN_KEEPALIVE` functions reached from JS via `ccall`/`cwrap`. Built with
`-sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker`, the module loads inside the
webapp's Web Worker. `BIOSIM_KEEPALIVE` expands to `EMSCRIPTEN_KEEPALIVE` under
Emscripten and to nothing otherwise, so the bindings still compile natively.

No structs cross the boundary: configuration is set field-by-field before
`init`, and inspection getters return a WASM-heap pointer that JS reads through a
typed-array view.

| Group | Representative functions |
|---|---|
| Lifecycle & stepping | `biosim_wasm_init`, `_free`, `_do_step`, `_do_gen`, `_next_generation`, `_rewind`, `_clear_genome`, `_get_gen`/`_get_step`, `_is_gen_complete` |
| Configuration (pre-init) | `biosim_wasm_set_param_int`/`_float`/`_bool` (by name) |
| Challenge (pre-init) | `biosim_wasm_set_challenge_kind` + per-kind setters (`_disc`, `_corners`, `_x_band`, `_near_barrier`, …) |
| Barriers (pre-init) | `biosim_wasm_clear_barriers`, `_add_barrier`, `_get_n_barriers` |
| Snapshot I/O | export: `_snapshot_export`, `_snapshot_export_ptr`/`_size`; import: `_snapshot_import_alloc`, `_snapshot_import`, `_snapshot_max_genes`/`_snapshot_max_neurons` |
| Render getters | per-agent heap arrays (`_get_loc_x_ptr`/`_loc_y_ptr`, `_alive_ptr`, `_last_move_dir_ptr`, `_grid_cells_ptr`, …) plus `_get_population`/`_size_x`/`_size_y` |
| Brain getters | `_get_genome_conn_ptr`, `_get_genome_wgt_ptr`, `_get_conn_length_ptr`, `_get_neuron_count_ptr` (`slot * pop + id` stride) |
| Census | `_census_gen`, `_census_population`, `_census_survivors`, `_census_kills` |

The worker ↔ module call sequence is part of the webapp worker protocol (below).

## `webapp`

**Location:** `packages/webapp/`  
**Type:** Svelte 5 SPA built with Vite 6 and Bun.  
**Dependencies:** `sim-wasm` (loaded at runtime).

The browser front-end: configure a run, watch generations evolve on a canvas,
inspect agents and their brains, and round-trip TOML configs and snapshots with
the native CLI.

The heavy work runs off the UI thread. `src/main.ts` mounts `App.svelte`, which
owns the DOM, the canvas layout, and orchestration; a Web Worker
(`src/workers/sim.worker.ts`) loads `biosim.mjs` + `biosim.wasm`, steps the
simulation, and draws agents onto an `OffscreenCanvas`. CMake copies the WASM
artifacts into `public/wasm/` before every build or `dev` run; the worker uses a
`/* @vite-ignore */` dynamic import so Vite does not rebundle the Emscripten
output.

### State management

Three sibling `$state` holders, each a plain class with private `$state`, public
getters, and intent methods, and **no `$effect`** — so each is unit-testable
outside a component. `App.svelte` owns one of each, routes worker events into
them, and derives view props from their getters.

| Holder | File | Owns |
|---|---|---|
| `SimMachine` | `simMachine.svelte.ts` | The simulation lifecycle: a composite `phase` (`SimPhase`, a 9-value enum) × `dirty` (`$derived` config divergence) state, the draft/last-played configs, and every lifecycle worker command. |
| `AgentFocus` | `agentFocus.svelte.ts` | The inspection triad `{ selected, hovered, lastHovered }` and a `$derived` display agent with priority `hovered ?? lastHovered ?? selected`. |
| `SimTelemetry` | `simTelemetry.svelte.ts` | Display-only fields from worker replies: `gen`/`step`/`pop`, the survival-rate sparkline, the active grid/steps params, and the snapshot-export gate. Issues no commands. |

`SimMachine`'s `phase` moves only inside machine methods, and every gesture
guards itself so an illegal transition is a no-op:

```
WORKER_PENDING → WORKER_READY → GENERATION_SPAWNED ↔ STEPS_RUNNING ↔ CONFIRM
                                                    ↘ STEPS_PAUSED
                                                    ↘ GENERATION_ENDED
                              FREE_RUNNING ↔ FREE_RUN_STOPPING
```

`dirty` is orthogonal to `phase`: editing the config never interrupts a running
sim. Starting from a dirty draft sends `configure` and defers `play` until the
worker's `configured` reply. Components receive `phase` as a prop and compare it
inline rather than through shared query helpers.

### Worker protocol

`App.svelte` and the worker communicate through two discriminated unions defined
in `sim.worker.ts` — `WorkerCmd` (UI → worker) and `WorkerEvent` (worker → UI),
keyed on `type`. The `App.svelte` dispatcher is a `switch` that may route one
event to several holders (e.g. `census` feeds both `SimMachine` and
`SimTelemetry`).

| Family | Commands → | ← Events |
|---|---|---|
| Lifecycle | `play`, `stop`, `step`, `reset`, `configure`, `nextGeneration`, `rewind`, `clearGenom`, `startFreeRun`, `stopFreeRun` | `ready`, `stepped`, `genComplete`, `paused`, `census`, `configured`, … |
| Canvas | `canvas` (transfers an `OffscreenCanvas`), `layout` | — |
| Agent | `pickAgentAtCell`, `selectAgent`, `hoverAgent`, `selectAgentById`, `navigateAgent`, `randomAgent` | `agentPicked`, `agentMissed`, `agentUpdated` |
| Brain | `requestBrain` | `brainData` |
| Playback | `setSpeed` (`fps` ∈ `0 \| 1 \| 25 \| 50`, `0` = unlimited) | `fps` (measured steps/s, ~1 Hz) |
| Snapshot | `exportSnapshot`, `loadSnapshot` | `snapshotData`, `snapshotLoaded` |

### `lib` modules

| Module | Role |
|---|---|
| `tomlConfig.ts` | Parse/serialize the `SimParams` config to/from TOML (round-trips with the CLI). |
| `fileTransfer.ts` | Browser file IO: `pickFile`, `downloadBlob`, and the pure `classifyDroppedFiles` drop router. |
| `brain.ts` | DOM-free decode layer: `unpackConn` mirrors the `gene.h` bit-layout; `SENSOR_LABELS`/`ACTION_LABELS` plus full-name tables for the UI. |
| `kinematic.ts` | Interpolation helpers for smooth agent motion between steps. |
| `playbackRate.ts` | Step-rate throttle and rolling FPS window (unit-testable without WASM). |
| `agentFormat.ts` | Format agent fields (id, location, fingerprint) for display. |

### Brain explorer

`BrainExplorer.svelte` renders an agent's neural net as a d3-force graph —
sensors on the left, actions on the right, internal neurons relaxing between
them; connections are directed and signed (colour by sign, width ∝ |weight|).
The same component serves a docked preview inside `CellPanel.svelte` and a full
variant that replaces the grid stack. d3's internal timer is stopped; ticks run
on a self-cancelling `requestAnimationFrame` loop so the layout re-warms in place
when the charge/link sliders move.

Connection genes are decoded in the worker (TS) via `brain.ts`, not in WASM, so
the module interface stays four plain pointer getters (`genome_conn`,
`genome_wgt`, `conn_length`, `neuron_count`, read with the `slot * pop + id`
stride). The UI sends `requestBrain` when the displayed agent changes and
discards `brainData` whose `id` no longer matches.
