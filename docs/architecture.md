# Architecture

## Package structure

Four packages are implemented, with a strict acyclic dependency graph:

```
core (static lib — libc only)
  └── params (static lib — argtable3, tomlc17 PRIVATE)
      ├── sim-stepper (executable — single-threaded CPU reference)
      └── sim-gpu     (static lib + executable — OpenCL GPU simulator)
```

`viz` is designed but not yet implemented.
See [`STATUS.md`](../STATUS.md).

## `core`

**Location:** `packages/core/`  
**Type:** Static library. No external dependencies beyond the C standard
library.

Owns all shared simulation logic — everything that both the stepper and the
future GPU simulator need with identical behavior.

| Module | Key types | Key functions |
|--------|-----------|---------------|
| [`sim.h`](../packages/core/include/biosim/core/sim.h) | `biosim_sim_t` | <ul><li>`biosim_sim_step_agent` — sense, feedforward, act for one agent</li><li>`biosim_sim_next_step` — fade signal, run per-step challenge hook</li><li>`biosim_sim_next_generation` — evaluate challenge, reproduce, respawn</li></ul> |
| [`agents.h`](../packages/core/include/biosim/core/agents.h) | `biosim_agents_t` | <ul><li>`biosim_agents_init_slot` — mark alive, set position, seed RNG</li></ul> |
| [`grid.h`](../packages/core/include/biosim/core/grid.h) | `biosim_grid_t` | <ul><li>`biosim_grid_at` — read a cell</li><li>`biosim_grid_set` — write a cell</li><li>`biosim_grid_find_empty` — random empty-cell search</li></ul> |
| [`genome.h`](../packages/core/include/biosim/core/genome.h) | `biosim_genome_t` | <ul><li>`biosim_genome_init_slot` — randomize one agent's genome</li><li>`biosim_genome_copy_slot` — copy genome between slots</li><li>`biosim_genome_mutate` — point / insertion / deletion mutations</li><li>`biosim_genome_crossover` — single-point two-parent crossover</li><li>`biosim_genome_sort_by_length` — sort agents by descending length</li></ul> |
| [`nnet.h`](../packages/core/include/biosim/core/nnet.h) | `biosim_nnet_t` | <ul><li>`biosim_nnet_compile` — compile genome into neural network</li><li>`biosim_nnet_feedforward` — run one feedforward pass</li></ul> |
| [`io_catalogue.h`](../packages/core/include/biosim/core/io_catalogue.h) | `biosim_sensor_t`, `biosim_action_t` | <ul><li>`biosim_sensor_eval` — evaluate one sensor for an agent</li><li>`biosim_action_apply` — apply one action output</li><li>`biosim_action_finalize_movement` — commit pending movement</li></ul> |
| [`challenges.h`](../packages/core/include/biosim/core/challenges.h) | `biosim_challenge_spec_t` | <ul><li>`biosim_challenge_step` — per-step challenge hook</li><li>`biosim_generation_collect_survivors` — evaluate agents at gen end</li></ul> |
| [`snapshot.h`](../packages/core/include/biosim/core/snapshot.h) | — | <ul><li>`biosim_snapshot_session_open` — open a write session</li><li>`biosim_snapshot_session_write` — write current generation's survivors</li><li>`biosim_snapshot_session_close` — finalize the file</li><li>`biosim_snapshot_restore` — load last record and reproduce</li></ul> |
| [`rng.h`](../packages/core/include/biosim/core/rng.h) | — | <ul><li>`biosim_rng_next` — advance xorshift64 and return state</li><li>`biosim_rng_seed` — initialize from two uint64_t seeds</li></ul> |
| [`status.h`](../packages/core/include/biosim/core/status.h) | `biosim_status_t` | <ul><li>`biosim_strerror` — map status code to human-readable string</li></ul> |
| [`log.h`](../packages/core/include/biosim/core/log.h) | `biosim_log_ctx_t` | <ul><li>`biosim_log_init` — set threshold, detect terminal color</li><li>`BIOSIM_ERRORF` / `WARNF` / `INFOF` / `DEBUGF` / `TRACEF` — level-gated macros</li></ul> |

`biosim_sim_t` is the complete simulation state. Set the configuration fields
(`population`, `size_x`, `size_y`, `genome_max_len`, `max_neurons`,
`long_probe_dist`, `steps_per_gen`, and others), then call `biosim_sim_create`
to allocate all heap resources and spawn the initial population.
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
    int16_t  *loc_x;
    int16_t  *loc_y;
    int16_t  *birth_x;
    int16_t  *birth_y;
    uint8_t  *alive;
    uint16_t *osc_period;
    float    *responsiveness;
    uint8_t  *long_probe_dist;
    uint8_t  *last_move_dir;
    uint32_t *challenge_bits;
    uint64_t *rng_state;
    uint64_t *genome_fingerprint;
    /* Transient per-step movement targets */
    int16_t  *desired_x;
    int16_t  *desired_y;
    float    *dx_sum;
    float    *dy_sum;
} biosim_agents_t;

/* packages/core/include/biosim/core/genome.h — transposed SoA        */
/* gene slot j of agent i is at index  j * population + i             */
typedef struct {
    uint32_t  population;
    uint16_t  max_len;
    uint16_t *conn; /* [gene_slot * population + agent_idx] */
    int16_t  *wgt;  /* [gene_slot * population + agent_idx] */
    uint16_t *len;  /* [agent_idx] active gene count */
} biosim_genome_t;
```

### Host/device portability

Three headers are shared with OpenCL kernel sources and must compile as both
C11 and OpenCL C:

- `core/types.h` — `biosim_coord_t`, `BIOSIM_GRID_EMPTY`,
  `BIOSIM_GRID_BARRIER`
- `core/rng.h` — `biosim_rng_next`, `biosim_rng_seed`
- `core/gene.h` — gene bit-layout macros (`BIOSIM_GENE_PACK`,
  `BIOSIM_GENE_SRC_TYPE`, ...)

These headers carry no host-only includes. See
[`docs/conventions.md`](conventions.md) for the portability rules.

## `params`

**Location:** `packages/params/`  
**Type:** Static library. Depends on `core` (PUBLIC), argtable3 and tomlc17
(PRIVATE).

Owns all CLI and TOML parsing. Each simulator's `main.c` declares a static
`biosim_param_entry_t[]` table — one entry per parameter — and calls
`biosim_params_parse` once at startup. The `params` package does the rest.

| Type | Role |
|------|------|
| `biosim_param_entry_t` | One parameter: name, TOML table, default, type, CLI flag |
| `biosim_params_t` | Dynamic array of entries; resolved values after three-pass parse |

Resolution order: compiled-in defaults → TOML file (`--config`) → CLI flags.
Each layer overrides the previous.

See [`docs/formats.md`](formats.md) for the TOML layout.  
See [`docs/usage.md`](usage.md) for the full parameter reference.

## `sim-stepper`

**Location:** `packages/sim-stepper/`  
**Type:** Executable. Depends on `core` and `params`.

A thin orchestration shell. `main.c` parses parameters, populates
`biosim_sim_t`, and drives a two-level loop — generation loop over step loop
over agents. All simulation logic lives in `core`; the stepper adds nothing
but the loop and the parameter table.

Agents are processed in increasing-index order with immediate effect
application. This is the deterministic reference ordering: conflicts resolve
first-come-first-served by index.

### Main loop

```c
// packages/sim-stepper/src/main.c — main loop (simplified)
biosim_params_parse(&p, progname, version, argc, argv);
biosim_barrier_params_load(config_path, &barriers, &n_barriers);
biosim_challenge_spec_from_params(&p, &challenge);
// ... populate sim fields from params ...
biosim_sim_create(&sim, barriers, n_barriers);

biosim_snapshot_restore(snap_in_path, &sim);           // if --snapshot-in
biosim_snapshot_session_open(&sim, snap_out_path, N);  // if --snapshot-out

while (sim.gen < sim.max_generations) {
    while (sim.step < sim.steps_per_gen) {
        for (uint32_t i = 0; i < sim.population; i++)
            if (sim.agents.alive[i])
                biosim_sim_step_agent(&sim, i);
        biosim_sim_next_step(&sim);
    }
    biosim_sim_next_generation(&sim, &census);
    biosim_census_print(stdout, &census);
}
biosim_sim_free(&sim);
```

See [`docs/usage.md`](usage.md) for the command-line reference.

## `sim-gpu`

**Location:** `packages/sim-gpu/`  
**Type:** Static library (`sim-gpu-lib`) + executable (`biosim-gpu`). Depends on
`core`, `params`, and OpenCL (vcpkg port `opencl`).

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
sources[0] = biosim/core/types.h  (embedded preamble)
sources[1] = biosim/core/rng.h    (embedded preamble)
sources[2] = biosim/core/gene.h   (embedded preamble)
sources[3] = <kernel source>      (override or embedded)
```

All four strings are passed to `clCreateProgramWithSource`, which concatenates them
before compilation. The portable headers (types.h, rng.h, gene.h) provide type
definitions and `biosim_rng_next` to kernel code without needing `#include`
directives in the `.cl` file.

### Kernel embryo — `k1_sensors.cl`

Implements the sensor-evaluation phase of K1 (`feedforward_and_actions`). The
kernel `k_sensor_eval` evaluates one Group-A self-only sensor for all agents in
parallel:

| Sensor ID | Sensor | Notes |
|-----------|--------|-------|
| 0 | `LOC_X` | Normalised x position |
| 1 | `LOC_Y` | Normalised y position |
| 2 | `BOUNDARY_DIST_X` | Distance to nearest x boundary |
| 3 | `BOUNDARY_DIST_Y` | Distance to nearest y boundary |
| 4 | `BOUNDARY_DIST` | Minimum of DIST_X and DIST_Y |
| 5 | `LAST_MOVE_DIR_X` | Last move direction x component |
| 6 | `LAST_MOVE_DIR_Y` | Last move direction y component |
| 8 | `AGE` | step / steps_per_gen |
| 9 | `RANDOM` | xorshift64 draw (mutates rng_state) |

Sensor 7 (OSC1) requires `cos()` and is deferred. Group B/C/D sensors are not yet
implemented.

### Key types

| Type | Role |
|------|------|
| `biosim_gpu_kernel_sources_t` | Bundle of source strings for one kernel program |
| `biosim_gpu_runner_t` | OpenCL platform, device, context, and queue |

See [`docs/gpu-design.md`](gpu-design.md) for the full kernel pipeline design.

## Build

**Toolchain:** CMake 3.28+ with vcpkg (`VCPKG_ROOT` must be set).  
**Compilers:** GCC and Clang on Linux; MSVC on Windows.

| Preset | Purpose |
|--------|---------|
| `debug` | Debug info, assertions on, no optimisation |
| `release` | `-O3`, LTO, assertions off |
| `asan` | Debug + AddressSanitizer + UBSan |
| `ci` | Release + tests enabled |

Key targets (all via `cmake --build --preset <preset> --target <target>`):

- *(default)* — compile all packages
- `lint` — clang-tidy static analysis; required clean before merge
- `format` — clang-format all source files

See [`docs/build.md`](build.md) for the full setup guide.
