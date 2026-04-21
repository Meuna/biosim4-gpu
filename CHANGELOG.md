# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
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
- **CLI/TOML dual parameter stack** (`sim-stepper`): three-pass resolution —
  defaults → TOML file → CLI flags.
- **Build version injection** (`cmake/BuildVersion.cmake`): `BIOSIM_GIT_VERSION`
  (from `git describe --tags --always --dirty`), `BIOSIM_BUILD_TIMESTAMP`, and
  `BIOSIM_BUILD_TYPE` injected as compile definitions into executables.

[Unreleased]: https://github.com/example/biosim4-gpu/compare/HEAD
