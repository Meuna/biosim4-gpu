# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Kernel argument ordering** — `k_feedforward` (K1) and `k_challenge_step_eval` (K5)
  now follow the convention of all `__global` pointer arguments before scalar arguments.
  In K1, `rng_state`, `desired_x`, `desired_y`, `grid`, and `kill_marker` moved from
  indices 19–24 to 14–18; scalars shifted to 19–25. In K5, `barrier_ctrs` moved from
  index 12 to 6; scalars shifted to 7–13. Host-side `clSetKernelArg` calls and per-step
  index updates in `pipeline.c` and both test files updated accordingly.

### Added
- **`BIOSIM_SENSOR_SIGNAL0_FWD`** — forward signal-density probe implemented in
  both CPU reference (`core`) and GPU K1 kernel; walks up to `los_range`
  cells in the agent's last-move direction, sums signal values (clamped to 255),
  and returns the per-cell average normalised to [0, 1]
  (`sum / (visited × 255)`), or 0.0 if no in-bounds cells are found.
- **`BIOSIM_SENSOR_SIGNAL0_LR`** — lateral signed-ratio signal sensor implemented
  in both CPU reference (`core`) and GPU K1 kernel; sums normalised signal
  values across the left and right half-discs (radius `sensor_radius`)
  and returns `(L−R)/(L+R)` in [−1, 1], or 0.0 when both sides are zero.
- **`BIOSIM_SENSOR_BARRIER_FWD`** — forward half-disc barrier-density sensor
  implemented in both CPU reference (`core`) and GPU K1 kernel; uses
  `sensor_radius` as the disc radius, returns
  `barrier_cell_count / visited` in [0, 1].
- **`BIOSIM_SENSOR_BARRIER_LR`** — lateral signed-ratio barrier sensor
  implemented in both CPU reference (`core`) and GPU K1 kernel; returns
  `(L−R)/(L+R)` in [−1, 1] where L and R are barrier-cell counts in the
  left and right half-discs.
- **`BIOSIM_SENSOR_LONGPROBE_BAR_FWD`** — forward ray-cast barrier probe
  implemented in both CPU reference (`core`) and GPU K1 kernel; skips agents
  (agents are transparent), returns `steps_to_first_barrier / los_range`,
  0.0 if no barrier found before a grid boundary.
- **Barriers in `sim_test_make_32x32`** — the 32×32 simulation fixture used by
  GPU kernel tests now includes a fixed horizontal bar at centre (16, 16) with
  length 20, exercising barrier sensors during the K1 GPU/host comparison test.
  `sim_test_scn_t` gained optional `barrier_specs` / `n_barrier_specs` fields;
  `sim_test_create` threads them through to `biosim_sim_create`.
- **`BIOSIM_SENSOR_POPULATION` on GPU** — K1 now computes the population-density
  sensor via a circular-disc neighbourhood scan (radius `sensor_radius`)
  instead of returning the 0.5 stub.
- **`BIOSIM_SENSOR_POPULATION_FWD`** — forward half-disc population-density sensor
  implemented in both CPU reference (`core`) and GPU K1 kernel; counts occupied
  cells in the forward half of the disc (`dx·fwd_x + dy·fwd_y > 0`).
- **`BIOSIM_SENSOR_LONGPROBE_POP_FWD`** — forward ray-cast population probe
  implemented in both CPU reference (`core`) and GPU K1 kernel; returns
  `steps_to_first_agent / los_range`, 0.0 if no agent found before a
  barrier or grid boundary.
- **`BIOSIM_SENSOR_POPULATION_LR`** — lateral signed-ratio population sensor
  implemented in both CPU reference (`core`) and GPU K1 kernel; returns
  `(L−R)/(L+R)` in [−1, 1] where L and R are occupied-cell counts in the
  left and right half-discs (strict dot-product partition, axis cells excluded).
- **Cross-package test utilities** — `core-sim-test-utils` now exposes its
  header at `biosim/core/test_utils.h` via a PUBLIC include directory, making
  `sim_test_create`, `sim_test_make_8x8`, `sim_test_make_32x32`, and the
  `assert_*_equal` helpers available to any package that links it.  A new
  `sim-gpu-test-utils` static library wraps `gpu_test_opencl_available`,
  `gpu_test_runner_create`, and the `GPU_TEST_REL` macro, eliminating
  duplicated boilerplate from all six `sim-gpu` kernel test files.

### Changed
- **`sim-stepper` renamed to `sim-ref`** — the package is now named `sim-ref`
  and the binary `biosim-ref`, reflecting its permanent role as the
  deterministic single-threaded reference scheduler for `core`, not a step
  engine for a visualiser.


- **Portable header renames** — established `_defs.h` as the uniform suffix
  for device-portable (OpenCL-safe) type/enum/constant-only headers:
  `types.h` → `grid_defs.h`, `challenge_kinds.h` → `challenge_defs.h`.
  `io_defs.h` already followed this convention and is unchanged.
- **`io_catalogue` module renamed to `io_eval`** — `io_catalogue.h/c` →
  `io_eval.h/c`; the catalogue (sensor/action enum listing) lives in
  `io_defs.h`; the host-only evaluator is now `io_eval`. Test renamed to
  `test_io_eval.c`.
- **`docs/conventions.md` updated** — host/device portability section now
  lists all 5 preamble headers, documents the `_defs.h` convention, and
  standardizes the prologue comment style.

### Added
- **`pipeline` module (`sim-gpu-lib`)** — new `biosim_gpu_pipeline_t` type with
  `create` / `step` / `sync_to_host` / `sync_from_host` / `free` API; owns the
  five compiled programs, kernel handles, and all GPU buffers for one simulation
  session.  `step` enqueues K1→K5 with no host/device sync; generation boundaries
  use `sync_to_host` + `biosim_sim_next_generation` + `sync_from_host`.
  Kernel buffer creation and argument setup are factored out of `main.c` into
  this module; only the per-step `step` scalar arg is updated on each call.
  Integration-tested by `test_pipeline.c` (step loop, alive-count invariant,
  full generation-boundary cycle).
- **Full generation+step loop in `biosim-gpu`** — `main.c` now drives the
  complete outer generation / inner step loop identical in structure to
  `sim-ref`, replacing the single-step smoke test.
- **`challenge_kinds.h`** — split `biosim_challenge_kind_t` enum out of
  `challenge_spec.h` into a new host+device-compatible header with no `<stdbool.h>`
  dependency; prepended to all OpenCL kernel builds as a 5th preamble source
  (`BIOSIM_GPU_PREAMBLE_COUNT` bumped from 4 to 5).
- **K5 `challenge_step_eval` kernel** — per-step challenge hook porting
  `biosim_challenge_step()`; handles `TOUCH_ANY_WALL` (border detection),
  `RADIOACTIVE_WALLS` (probabilistic kill + immediate grid cell clear), and
  `LOCATION_SEQUENCE` (barrier proximity sequencing); dispatched over `population`
  work-items after K4. Killed agents' grid cells are cleared immediately (no atomics
  needed — each agent uniquely owns one cell after K3). Completes the 5-kernel
  per-step GPU pipeline.
- **K4 `signal_fade` kernel** — decays the signal layer by 1 per step
  (`signal[c] = max(0, signal[c] - 1)`); dispatched over `size_x × size_y`
  work-items after K3. Registered in the kernel registry and wired into the
  `sim-gpu` main dispatch loop. Unit-tested with POCL.

### Changed
- **`biosim_sim_create` signature extended** — now accepts `biosim_params_t *`,
  `biosim_challenge_spec_t *`, `biosim_barrier_spec_t *`, and `n_barriers`; reads all
  13 configuration fields internally and zeros the struct with `memset` before any
  allocation. Callers no longer pre-populate struct fields; the manual assignment
  boilerplate is removed from `sim-ref/main.c`, `sim-gpu/main.c`, and
  `wasm-sim-ref/main.c`. All GPU test files gain a local `make_test_sim` helper
  that builds a `biosim_params_t` inline; core test files use the new
  `sim_test_create` / `sim_test_make_8x8` / `sim_test_make_32x32` helpers from
  `sim_test_utils`.
- **`params` module moved from `params` package into `core`** — `biosim_params_t`,
  `biosim_param_entry_t`, and all typed getters/setters now live in
  `packages/core/include/biosim/core/params.h` and `packages/core/src/params.c`.
  The former `params` package is renamed to `cfgparse`; it retains only
  `biosim_params_parse` (CLI + TOML) and no longer owns the data model.
- **`sim-gpu` fully configured** — `biosim-gpu` now parses challenges and barriers
  via `biosim_challenge_spec_from_params` / `biosim_barrier_params_load` and links
  `cfgparse`. The parameter table is expanded from 10 to 29 entries covering all
  simulation, genome, sensor, action, challenge, and OpenCL parameters.
- **Grid cell type widened from `uint16_t` to `uint32_t`; coordinates from `int16_t` to `int32_t`** —
  `OpenCL`'s `atomic_cmpxchg` requires 32-bit operands; matching the host types eliminates
  temporary conversion buffers at every host/device boundary:
  - `biosim_grid_t.cells` changed from `uint16_t *` to `uint32_t *`.
  - `biosim_grid_t.size_x/size_y` changed from `int16_t` to `int32_t`.
  - `BIOSIM_GRID_BARRIER` changed from `0xFFFFU` to `0xFFFFFFFFU`; encoding is now
    `0` = empty, `0xFFFFFFFF` = barrier, `[1, 0xFFFFFFFE]` = agent index + 1.
  - `biosim_coord_t.{x,y}` changed from `int16_t` to `int32_t`.
  - `biosim_sim_t.{size_x,size_y,sensor_radius}` changed from `int16_t` to `int32_t`.
  - Agent SoA arrays `loc_x`, `loc_y`, `birth_x`, `birth_y`, `desired_x`, `desired_y`
    changed from `int16_t *` to `int32_t *`.
  - OpenCL kernels K1/K2/K3 updated to use `int` for all coordinate parameters and
    locals; `short` casts removed throughout.
  - `sim-gpu/src/main.c` and all test files: temporary `uint32_t tmp_grid` conversion
    buffers removed; `sim->grid.cells` is now passed directly to `clCreateBuffer`.
  - `types.h` OpenCL portability branch gains `typedef int int32_t; typedef uint uint32_t;`.


- **`core` step pipeline aligned with GPU two-phase model** — `biosim_sim_step_agent`
  now only proposes a move (sense → feedforward → act → `biosim_action_propose_move`);
  kill commits and grid grants are deferred to `biosim_sim_next_step`, which runs a
  kill-commit loop (≈ K2) then a first-come-first-served grant loop (≈ K3) before
  fading the signal and running the challenge hook. `biosim_action_finalize_movement`
  is renamed `biosim_action_propose_move` to reflect that it produces a desired
  position rather than an executed move. `biosim_action_apply` KILL_FORWARD now stamps
  `kill_marker[target] = 1` instead of immediately zeroing `alive` and clearing the
  grid cell.
- **`KILL_FORWARD` refactored to two-phase death** — eliminates a race condition
  with population-density sensors that read the grid during K1:
  - K1 no longer clears the grid cell of a killed agent. Instead it sets
    `kill_marker[target] = 1` (new `uint8_t` SoA buffer in `biosim_agents_t`).
  - New **K2 `k2_kill_marked.cl`** sweeps `kill_marker` after K1 completes and
    clears each flagged agent's cell via `atomic_cmpxchg`. This guarantees the
    grid is a stable read-only snapshot for all K1 work-items.
  - Former **K2 `k2_movement_resolution.cl`** is renumbered to
    **K3 `k3_movement_resolution.cl`**; no logic changes.
  - `biosim_agents_t` gains `uint8_t *kill_marker` (allocated, freed, and
    zero-initialised alongside the existing per-agent buffers).
  - `k1_feedforward.cl` receives `kill_marker` as arg 24 (`__global uchar *`);
    `grid` (arg 22) is now `__global const uint *` (read-only in K1).
  - Test `test_k1_kill_forward_clears_grid` renamed to
    `test_k1_kill_forward_marks_kill_marker`; now verifies `kill_marker` is set
    and that the grid cell is *not* cleared by K1.
  - New test file `test_k2_kill_marked.c` covering compile+run, cell cleared for
    marked agent, cell unchanged for unmarked agent.

### Added
- **`sim-gpu` K1 `KILL_FORWARD` action** — implemented Group D of `k1_feedforward.cl`:
  - When `enable_kill` is set and the action accumulator exceeds 0.5, marks
    the agent directly ahead dead (`alive[cell-1] = 0`) and clears its grid
    cell (`grid[pos] = BIOSIM_GRID_EMPTY`) so K2 sees a consistent grid.
  - Both writes are idempotent non-atomic stores; safety is guaranteed because
    multiple killers writing the same cell write the same values, and K2 has
    not yet launched.
  - `alive` kernel argument changed from read-only to read-write.
  - Two new K1 arguments: `grid` (arg 22, `__global uint *`) and `enable_kill`
    (arg 23, `int`), matching the existing K2 grid buffer.
  - `BIOSIM_GRID_EMPTY` / `BIOSIM_GRID_BARRIER` are now guarded in `types.h`
    with `__OPENCL_VERSION__` so kernel code gets `(uint)` casts matching the
    `uint32_t` grid buffer, while host code retains `(uint16_t)` casts.
  - Unit test addition: `test_k1_kill_forward_clears_grid` verifies that every
    agent newly killed by K1 has its grid cell cleared to `BIOSIM_GRID_EMPTY`.
- **`sim-gpu` K2 kernel** — `k2_movement_resolution.cl` atomically commits
  per-agent movement decisions (produced by K1) to the GPU grid:
  - Each work-item attempts `atomic_cmpxchg` on its desired cell; losers stay
    in place (lock-free, no serial drain).
  - On success: clears the old cell with a plain write (safe — each cell is
    exclusively owned by one agent), updates `loc_x`/`loc_y`, and records the
    move direction in `last_move_dir`.
  - Grid buffer is `uint32_t` per cell (required for OpenCL 1.x atomics); the
    host converts from/to `uint16_t` at the boundary.
  - First-version limitation: chained movement (agent A wants a cell that agent
    B is vacating in the same launch) is not resolved; results may diverge from
    the CPU reference.
  - Unit tests (`test_k2_movement_resolution.c`): smoke test, successful move
    to an empty cell, no-op when desired equals loc, blocked move into an
    occupied cell.
- **`sim-gpu` K1 kernel** — `k1_feedforward.cl` implements the full per-step
  simulation pipeline for all agents in parallel:
  - **Sensor evaluation**: Group A (LOC_X … RANDOM) + OSC1 (uses OpenCL built-in
    `cos()`) + SIGNAL0 (reads signal buffer); Group B/C/D sensors stub at 0.5.
  - **Neural network feedforward**: single pass over SoA-layout connection list;
    writes updated `neuron_output`.
  - **Action application**: self-fields (responsiveness, oscillator period,
    long-probe distance); movement accumulators via `BIOSIM_DIR_DX/DY`; signal
    emission via `atomic_add`; `KILL_FORWARD` is a no-op stub (deferred to K2).
  - **Movement finalization**: `tanh`-squashed accumulator compared against
    xorshift64 draws; writes clamped `desired_x`/`desired_y`.
  - Unit tests (`test_k1_feedforward.c`): compares GPU desired positions against
    the `core` reference per-agent pipeline; gracefully `IGNORE`s without OpenCL.
- **`core/io_defs.h`** — new HOST/DEVICE portable header: `biosim_sensor_t`,
  `biosim_action_t` enums, `BIOSIM_IO_SCHEMA_VERSION`, and `BIOSIM_DIR_DX/DY`
  direction tables (with `__OPENCL_VERSION__` guard for `__constant` vs
  `static const`). Included by host code via `io_catalogue.h` and embedded as a
  preamble in every kernel program (slot 3 of `sources[]`).
- **`sim-gpu` scaffold** — OpenCL GPU simulator package (`packages/sim-gpu/`):
  - `sim-gpu-lib` static library — kernel registry and OpenCL runner.
  - `biosim-gpu` executable — minimal parameter table (`population`, grid dimensions,
    `steps-per-gen`, `max-generations`, `max-genome-len`, `max-neurons`,
    `platform-index`, `device-index`) using the `params` library.
  - **Two-level kernel registry** (`registry.c`): filesystem override (`.cl` file
    alongside binary) falls back to C string literals embedded at build time via
    the new `cmake/EmbedKernels.cmake` module (`biosim_embed_opencl_source()`).
  - Preamble strategy: `types.h`, `rng.h`, `gene.h`, `io_defs.h` from `core` are
    always embedded and prepended as separate source strings to every
    `clCreateProgramWithSource` call.
  - `vcpkg.json`: added `opencl` dependency (Khronos ICD loader + headers).
  - `CMakeLists.txt`: `BIOSIM_BUILD_GPU` option (default `ON`) gates
    `packages/sim-gpu`; `EmbedKernels.cmake` included unconditionally.

### Documentation
- **`docs/architecture.md`**: added `sim-gpu` package entry with two-level registry
  lookup description, K1 pipeline description, and updated dependency graph; added
  `io_defs.h` row to the `core` header table.
- **`docs/gpu-design.md`**: updated status to reflect K1 implemented.
- **`docs/build.md`**: added OpenCL runtime prerequisite (POCL), updated "run the
  simulators" section to list both executables, added `BIOSIM_BUILD_GPU=OFF` tip.
- **`docs/usage.md`**: added `biosim-gpu` section covering prerequisites, CLI usage,
  kernel filesystem override, and OpenCL parameter table.

- **`wasm-sim-ref` PoC**: new package that compiles the core simulation to
  WebAssembly via Emscripten. Hardcoded defaults (population 3000, 128×128 grid,
  x\_band challenge). Build with `cmake --preset wasm && cmake --build --preset wasm`;
  open `build/wasm/packages/wasm-sim-ref/biosim-wasm-stepper.html` in a browser
  to see census data logged each generation. `BIOSIM_BUILD_PARAMS` CMake option added
  to gate the params/sim-ref packages (default ON).
- **Signal handling in `sim-ref`**: `SIGINT` and `SIGTERM` (plus `SIGBREAK`
  on Windows) now trigger a clean shutdown — the generation loop exits after the
  current generation, `biosim_sim_free` finalizes any open snapshot session, and
  the process exits 0.

### Documentation
- **Full documentation overhaul**: replaced design-phase documents with
  as-built documentation. New flat structure under `docs/`:
  `architecture.md`, `conventions.md`, `build.md`, `usage.md`,
  `gpu-design.md`, `formats.md`. New root-level `STATUS.md` with
  implementation status and open GPU design questions. Old `docs/design/`
  documents retired; content superseded or moved to `docs/ai-development.md`
  and `docs/legacy-data-model.md`.
- **`CLAUDE.md`** updated: removed design-phase language, added
  alloc/goto/free discipline and error logging discipline, updated
  architecture section to reflect implemented packages only.
- **`README.md`** rewritten to reflect as-built state.

### Changed
- **Snapshot format version bumped to 2** (`BIOSIM_SNAP_FORMAT_VERSION`):
  generation records now append a `float[]` array of per-survivor challenge
  scores (one `float` per survivor, in `[0, 1]`) after the genome data.
  `biosim_snapshot_write_genome` and `biosim_snapshot_session_write` require a
  non-NULL `scores` parameter; `biosim_snapshot_load` and
  `biosim_snapshot_load_last` accept an optional `float *scores_out` (pass
  `NULL` to discard). `biosim_snapshot_restore` discards scores (uniform
  parent selection on restore is unchanged).

### Added
- **Logging infrastructure** (`core/log`, `core/status`): multi-level logger
  requiring no external dependencies.
  - `biosim_log_level_t` — `BIOSIM_LOG_OFF/ERROR/WARN/INFO/DEBUG/TRACE` (0–5).
  - `biosim_log_ctx_t` — threshold, sink (`FILE *`, NULL → stderr), color flag
    (auto-detected via `isatty` at init). 
  - `biosim_log_init(ctx)` — sets WARN threshold and detects terminal color.
  - `biosim_log_emit(ctx, level, file, line, func, fmt, ...)` — formats and
    emits one log line with `[LEVEL] file:line func: message` layout.
  - `BIOSIM_ERRORF/WARNF/INFOF/DEBUGF/TRACEF(ctx, ...)` — short-circuit at
    compile time (`BIOSIM_LOG_MAX_LEVEL`, default 5; set to 3 in release/ci
    presets to strip TRACE and DEBUG) and at runtime (`ctx->threshold`).
  - `biosim_die(ctx, code, saved_errno, file, line, func, fmt, ...)` / macro
    `BIOSIM_DIE(ctx, code, ...)` — captures `errno` at the call site, prints a
    fatal message, and calls `exit(EXIT_FAILURE)`.
  - `biosim_strerror(code)` (`core/status`) — maps `biosim_status_t` to a
    human-readable string.
  - `PARAM_COUNT` param type added to the params library: a counted CLI flag
    (`arg_litn`) whose repetition count is stored as `int`; readable via
    `biosim_params_get_int`. Used for `-v/-vv` verbosity.
  - `--verbose/-v` flag in `biosim-ref`: `-v` → INFO threshold,
    `-vv` → DEBUG threshold. Default is WARN.
  - All `fprintf(stderr, ...)` calls in `snapshot.c` and `sim-ref/main.c`
    migrated to `BIOSIM_ERRORF`/`BIOSIM_WARNF`.
- **Generation snapshot format** (`core/snapshot`): BSM4 binary file format
  (little-endian, 32-byte header + sequential generation records) for
  checkpointing survivor genomes at generation boundaries.
  - Write side: `biosim_snapshot_write_header`, `biosim_snapshot_write_gen`,
    `biosim_snapshot_finalize`.
  - Session API: `biosim_snapshot_session_open`, `biosim_snapshot_session_write`,
    `biosim_snapshot_session_close` — all take `biosim_sim_t *`; session state is
    carried in four `snap_*` fields on `biosim_sim_t` (`snap_f`, `snap_written_count`,
    `snap_interval`). `biosim_sim_free` closes the session automatically.
  - Read side: `biosim_snapshot_read_header`, `biosim_snapshot_load_gen`,
    `biosim_snapshot_load_last`; `generation_count = 0` triggers a scan-to-EOF
    fallback for streaming / unfinished files.
  - High-level restore: `biosim_snapshot_restore(path, sim)` opens the file,
    checks coherency (schema/catalogue mismatch = fatal; topology mismatch =
    warning with flag hint), loads the last record, calls
    `biosim_generation_reproduce`, and resets generation counters.
  - `BIOSIM_IO_SCHEMA_VERSION` added to `io_catalogue.h`; bumped when sensor/
    action indices change. `BIOSIM_SNAP_FORMAT_VERSION` in `snapshot.h`; bumped
    when the on-disk record layout changes.
- `biosim_sim_next_generation` now writes the snapshot session (when active),
  resets `step`/`kills`, and increments `gen` — callers no longer need to manage
  these counters or the snapshot write.
- **Snapshot CLI flags** (`sim-ref`): `--snapshot-in PATH` restores from
  the last generation record in a file; `--snapshot-out PATH` writes survivor
  snapshots (errors if the file already exists); `--snapshot-interval N` writes
  every N generations (default 0 = final generation only).

### Fixed
- `biosim_generation_reproduce`: no longer crashes when `scores == NULL` and
  `choose_parents_by_fitness` is true; fitness-biased sort is skipped when
  scores are unavailable (uniform parent selection applied instead).

### Added
- **Sexual reproduction** (`core/generation.c`, `sim-ref`): two new boolean
  parameters gate reproduction behaviour:
  - `[genome] sexual-reproduction` (default `false`): when true, each offspring
    genome is produced by single-point crossover of two parent genomes instead of
    copying a single parent.  The crossover logic mirrors `biosim_genome_crossover`
    but operates directly on the pre-generation snapshot buffers.
  - `[genome] choose-parents-by-fitness` (default `false`): when true, survivors
    are sorted by challenge score (descending) before selection, and parents are
    drawn with a harmonic bias toward higher-scoring candidates.  The selection
    distribution follows the reference (`biosim4`) algorithm: one parent index
    drawn uniformly from `[1, n-1]`, the other from `[0, first-1]`.
  - Both parameters default to `false`; all existing simulation behaviour is
    unchanged unless explicitly enabled.
  - `biosim_generation_collect_survivors` now also fills a parallel `float *scores`
    array; `biosim_generation_reproduce` accepts it to support fitness-biased
    selection.

### Fixed
- **`KILL_FORWARD` corpse bug** (`core/io_catalogue.c`): when `KILL_FORWARD` killed
  an agent it set `alive[B] = 0` but left the dead agent's grid cell occupied.
  Corpses accumulated throughout the generation, blocking movement and causing
  cascading kills. The fix clears the dead agent's grid cell immediately alongside
  marking it dead. This was the primary cause of the anomalously low initial
  survival rates.

### Added
- **Multi-generation loop** (`sim-ref`): outer generation loop driven by the new
  `max-generations` parameter (default 1000). Each generation runs the full step
  loop, then calls `biosim_context_advance_gen` to evaluate the challenge, reproduce
  survivors (asexual: random-parent copy + point mutation), recompile all neural
  networks, and respawn the population.
- **`point-mutation-rate` parameter** (`genome` table, default 0.001): per-gene
  mutation rate passed to `biosim_genome_mutate` at each generation boundary.
- **`enable-kill` parameter** (`actions` table, default `false`): gates
  `BIOSIM_ACTION_KILL_FORWARD`. When `false` the kill action is a no-op, giving a
  clean baseline without inter-agent mortality. Set to `true` to enable.
- **Generation statistics** (`core/gen.h` + `core/gen.c`): `biosim_gen_stats_t`
  collects per-generation metrics — `surv` / `surv%` (agents that passed the
  challenge and will reproduce), `kills` (agents killed by `KILL_FORWARD` during
  the generation, 0 when `enable-kill` is false), mean/std-dev genome length
  (variability), phenotype diversity (% unique compiled-nnet fingerprints among
  survivors), and mean challenge score. `sim-ref/main.c` prints aligned
  fixed-width columns; each generation fits on one line.
- **`biosim_context_t` kill tracking** (`core/context.h`): `enable_kill` (bool,
  set by simulator at creation) and `kills` (uint32_t, reset to 0 each generation
  boundary by `biosim_context_advance_gen`).


- Repository skeleton: design documents, root config files, CMake/vcpkg scaffolding.
- **Portable primitive types** (`core/types.h`): `biosim_coord_t`,
  `BIOSIM_GRID_EMPTY`, `BIOSIM_GRID_BARRIER`. Compiles as both C11 and OpenCL C
  via `#ifdef __OPENCL_VERSION__` guard.
- **Grid module** (`core/grid.h` + `core/grid.c`): flat row-major `uint16_t`
  buffer, full lifecycle, cell predicates (`is_empty`, `is_barrier`,
  `is_occupied`), bounds checking, `zero_fill`, disc neighborhood scan
  (`visit_neighborhood`), and random empty-cell search (`find_empty` — 200-probe
  random phase + linear scan fallback). Implements GPU data model §7.
- **xorshift64 RNG** (`core/rng.h` + `core/rng.c`): OpenCL-compatible header
  with `static inline biosim_rng_next` and host-side `biosim_rng_seed` (splitmix64
  mixing, guards against zero state).
- **Per-agent SoA buffers** (`core/agents.h` + `core/agents.c`): all 13 fixed-size
  per-agent buffers from GPU data model §4.1 (`alive`, `loc_x/y`, `birth_x/y`,
  `age`, `osc_period`, `responsiveness`, `los_range`, `last_move_dir`,
  `challenge_bits`, `rng_state`, `genome_fingerprint`) plus transient movement
  targets (`desired_x/y`). Lifecycle: `biosim_agents_create` / `biosim_agents_free` /
  `biosim_agents_init_slot`.
- **`params` package** (`packages/params/`): new static library owning all
  CLI/TOML/parameter logic. `biosim_params_init(p, entries, count)` accepts
  the caller's complete entry table; `biosim_params_parse(p, progname,
  version, argc, argv)` performs three-pass resolution (defaults → TOML →
  CLI). The extension mechanism (`biosim_params_extend`) is removed — each
  simulator's `main.c` defines its own exhaustive entry table.
- **`biosim_context_t`** (`core/context.h`): new core module holding scalar
  configuration values (`steps_per_gen`, `sensor_radius`) that
  core algorithms need at runtime. Populated from `biosim_params_t` by the
  simulator before the simulation loop; replaces direct param lookups inside
  `core`.
- **CLI/TOML dual parameter stack** (`sim-ref`): three-pass resolution —
  defaults → TOML file → CLI flags.
- **Genome module** (`core/genome.h` + `core/genome.c`): transposed SoA genome
  buffers (`conn[gene_slot * N + agent]`, `wgt[gene_slot * N + agent]`,
  `length[agent]`) implementing GPU data model §5. Gene field encoding macros
  (`BIOSIM_GENE_PACK`, `BIOSIM_GENE_SRC_TYPE`, etc.) are OpenCL-compatible.
  Host-side operators: `biosim_genome_init_slot`, `biosim_genome_copy_slot`,
  `biosim_genome_mutate` (point + insertion + deletion at configurable rate),
  `biosim_genome_crossover` (single-point), `biosim_genome_fingerprint`
  (Fibonacci hash), and `biosim_genome_sort_by_length` (counting sort +
  in-place reorder for warp-divergence mitigation). `max-neurons` parameter
  added to core defaults (default 5).
- **Build version injection** (`cmake/BuildVersion.cmake`): `BIOSIM_GIT_VERSION`
  (from `git describe --tags --always --dirty`), `BIOSIM_BUILD_TIMESTAMP`, and
  `BIOSIM_BUILD_TYPE` injected as compile definitions into executables.

- **Barrier system** (`core/barriers.h` + `core/barriers.c`, `params/barriers.h` +
  `params/barriers.c`): data-driven barrier placement driven entirely by the TOML
  config file. Supports four shapes — `hbar`, `vbar`, `square`, `circle` — each
  with optional position and dimension fields that default to grid-relative random
  values seeded from `biosim_rng_seed(0, 0)`. Configuration: `[barriers]
  num-barriers = N` plus `[barrier-1]` … `[barrier-N]` tables. No config file
  means no barriers. Barriers are placed on the grid before agents, so
  `biosim_grid_find_empty` (used during agent spawning) correctly avoids them.
  `biosim_context_create` accepts `barriers` and `n_barriers` parameters.
  `main.c` loads specs via `biosim_barrier_params_load` and passes them through.
- **`BIOSIM_ERR_INVALID`** (`core/status.h`): new status code for malformed input
  (e.g. unknown barrier kind string), distinct from `BIOSIM_ERR_NOTFOUND`.

### Changed
- **`core` package is now the single-thread reference implementation**
  (`core/step.h`, `core/step.c`, `core/gen.h`, `core/gen.c`): step logic
  (`step_agent`) and generation logic (`biosim_context_advance_gen`,
  `biosim_gen_stats_t`) moved from `sim-ref` into `core`. The stepper is
  now a thin orchestration shell — `main()` fills `biosim_context_t` and
  drives a triple-nested `for` loop with no simulation logic of its own.
- **`biosim_context_t` is now the complete simulation state**
  (`core/context.h`): six allocation-time configuration fields (`population`,
  `size_x`, `size_y`, `genome_max_len`, `max_neurons`, `los_range`) and
  four generation-state fields (`step`, `gen`, `mutation_rate`, `gen_rng`)
  added. `biosim_context_create` now takes only `(sim, barriers, n_barriers)`
  — the caller pre-populates the configuration fields, then `create` allocates
  all heap resources and spawns the initial population.
- **`biosim_stepper_t` removed** (`sim-ref/step.h`): superseded by the
  expanded `biosim_context_t`. All stepper-specific fields (`step`, `gen`,
  `mutation_rate`, `gen_rng`) now live directly on the context.
- **`biosim_sense_ctx_t` and `biosim_act_ctx_t` removed** (`core/io_catalogue.h`):
  replaced by flat function parameters. `biosim_sensor_eval` now takes
  `(sensor, idx, sim, sim_step)`; `biosim_action_apply` takes
  `(action, val, idx, sim)`; `biosim_action_finalize_movement` takes
  `(idx, sim)`. Removes two intermediate structs that bundled references already
  available in the context.
- **`dx_sum` / `dy_sum` added to `biosim_agents_t`** (`core/agents.h`): transient
  per-step movement accumulators now live in the agent SoA alongside
  `desired_x`/`desired_y`, making them GPU-buffer-compatible and eliminating
  the per-agent accumulator fields that previously lived in `biosim_act_ctx_t`.

### Removed
- `biosim_params_t` and all param functions removed from `core`; they now live
  exclusively in the `params` package.
- `biosim_params_extend` removed entirely (no extension mechanism needed).

[Unreleased]: https://github.com/example/biosim4-gpu/compare/HEAD
