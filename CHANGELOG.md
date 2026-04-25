# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
  loop, then calls `biosim_stepper_advance_gen` to evaluate the challenge, reproduce
  survivors (asexual: random-parent copy + point mutation), recompile all neural
  networks, and respawn the population.
- **`point-mutation-rate` parameter** (`genome` table, default 0.001): per-gene
  mutation rate passed to `biosim_genome_mutate` at each generation boundary.
- **`enable-kill` parameter** (`actions` table, default `false`): gates
  `BIOSIM_ACTION_KILL_FORWARD`. When `false` the kill action is a no-op, giving a
  clean baseline without inter-agent mortality. Set to `true` to enable.
- **Generation statistics** (`sim-stepper/gen.h` + `gen.c`): `biosim_gen_stats_t`
  collects per-generation metrics — `surv` / `surv%` (agents that passed the
  challenge and will reproduce), `kills` (agents killed by `KILL_FORWARD` during
  the generation, 0 when `enable-kill` is false), mean/std-dev genome length
  (variability), phenotype diversity (% unique compiled-nnet fingerprints among
  survivors), and mean challenge score. `biosim_gen_stats_print_header` /
  `biosim_gen_stats_print` emit aligned fixed-width columns; each generation fits
  on one line and all fields stay aligned across all generations.
- **`biosim_stepper_t` extended** (`sim-stepper/step.h`): three new fields — `gen`
  (current generation index), `mutation_rate` (loaded from params), `gen_rng`
  (RNG state seeded at startup for all generation-boundary random choices).
- **`biosim_context_t` kill tracking** (`core/context.h`): `enable_kill` (bool,
  set by simulator at creation) and `kills` (uint32_t, reset to 0 each generation
  boundary by `biosim_stepper_advance_gen`).


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
  `biosim_context_create` gains two new parameters: `barriers` and `n_barriers`.
  `biosim_stepper_create` likewise; `main.c` loads specs via
  `biosim_barrier_params_load` and passes them through.
- **`BIOSIM_ERR_INVALID`** (`core/status.h`): new status code for malformed input
  (e.g. unknown barrier kind string), distinct from `BIOSIM_ERR_NOTFOUND`.

### Changed
- **`biosim_context_t` expanded** (`core/context.h` + new `core/context.c`):
  now holds the full simulation state — `agents`, `grid`, `genome`, `nnet`,
  `signal`, `signal_len` — in addition to the existing scalar config fields.
  `biosim_context_free` releases all owned resources in one call.
- **`biosim_stepper_t` refactored** (`sim-stepper/step.h`): uses C first-member
  embedding (`biosim_context_t base` at offset 0) so a stepper pointer up-casts
  safely to `biosim_context_t *`. Only stepper-specific state (`step`) remains
  outside the base.
- **`biosim_sense_ctx_t` and `biosim_act_ctx_t` removed** (`core/io_catalogue.h`):
  replaced by flat function parameters. `biosim_sensor_eval` now takes
  `(sensor, idx, ctx, sim_step)`; `biosim_action_apply` takes
  `(action, val, idx, ctx)`; `biosim_action_finalize_movement` takes
  `(idx, ctx)`. Removes two intermediate structs that bundled references already
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
