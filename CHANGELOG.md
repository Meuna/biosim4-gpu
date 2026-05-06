# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- **Snapshot CLI flags** (`sim-stepper`): `--snapshot-in PATH` restores from
  the last generation record in a file; `--snapshot-out PATH` writes survivor
  snapshots (errors if the file already exists); `--snapshot-interval N` writes
  every N generations (default 0 = final generation only).

### Fixed
- `biosim_generation_reproduce`: no longer crashes when `scores == NULL` and
  `choose_parents_by_fitness` is true; fitness-biased sort is skipped when
  scores are unavailable (uniform parent selection applied instead).

### Added
- **Sexual reproduction** (`core/generation.c`, `sim-stepper`): two new boolean
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
- **Multi-generation loop** (`sim-stepper`): outer generation loop driven by the new
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
  survivors), and mean challenge score. `sim-stepper/main.c` prints aligned
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
  `age`, `osc_period`, `responsiveness`, `long_probe_dist`, `last_move_dir`,
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
  configuration values (`steps_per_gen`, `population_sensor_radius`) that
  core algorithms need at runtime. Populated from `biosim_params_t` by the
  simulator before the simulation loop; replaces direct param lookups inside
  `core`.
- **CLI/TOML dual parameter stack** (`sim-stepper`): three-pass resolution —
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
  `biosim_gen_stats_t`) moved from `sim-stepper` into `core`. The stepper is
  now a thin orchestration shell — `main()` fills `biosim_context_t` and
  drives a triple-nested `for` loop with no simulation logic of its own.
- **`biosim_context_t` is now the complete simulation state**
  (`core/context.h`): six allocation-time configuration fields (`population`,
  `size_x`, `size_y`, `genome_max_len`, `max_neurons`, `long_probe_dist`) and
  four generation-state fields (`step`, `gen`, `mutation_rate`, `gen_rng`)
  added. `biosim_context_create` now takes only `(sim, barriers, n_barriers)`
  — the caller pre-populates the configuration fields, then `create` allocates
  all heap resources and spawns the initial population.
- **`biosim_stepper_t` removed** (`sim-stepper/step.h`): superseded by the
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
