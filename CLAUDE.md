# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

GPU port of [biosim4](https://github.com/davidrmiller/biosim4) (by David R.
Miller) using OpenCL. The project is in the **design phase** — no source code
exists yet. Design documents in `docs/design/` are the authoritative
specification; implementation follows them.

## Working with this repository

- **Design documents are the source of truth.** Before making any structural
  decision (new package, new directory, new top-level file), consult the
  relevant design document. When in doubt, ask the user rather than guessing.

- **Open design decisions require confirmation.** When encountering an open
  design point (config file format, binary format), do not settle them
  unilaterally, flag them to the user when they come up.

- **Keep the documentation up-to-date.** When implementing a feature,
  ensure that the documentation remain aligned with the implementation.
  Notably, remove the mentions like "feature not yet implemented" as
  needed when related to the feature you implemented.

- **Conventions are normative.** The rules in `02-implementation-conventions.md`
  (naming, error handling, host/device portability, no global state in
  `core`) apply to every file written in this repository. They are enforced
  by review, not by linters alone.

  - **Code Quality.** Every time you edit or create files, you MUST
  complete this sequence before considering the task done:
    1. `cmake --build --preset debug` — must compile with zero errors
    2. `cmake --build --preset debug --target check` — all tests must pass
    3. `cmake --build --preset debug --target lint` — fix every error or
       warning reported; repeat until the output is clean
    4. `cmake --build --preset debug --target format` — apply formatting
    5. `cmake --build --preset debug` — re-compile to confirm formatting
       did not break anything
    6. `cmake --build --preset debug --target check` — re-run tests to
       confirm formatting did not break anything
  A task is NOT complete while lint reports any error or warning.

## Build System

The build uses **CMake + vcpkg** (VCPKG_ROOT must be set, see `docs/build.md`):

```sh
# Configure
cmake --preset debug    # or: release, asan, ci

# Build
cmake --build --preset debug

# Common targets
cmake --build --preset debug --target check      # run all tests
cmake --build --preset debug --target format     # run clang-format
cmake --build --preset debug --target lint       # run clang-tidy
cmake --build --preset debug --target benchmark  # run benchmarks
```

## Portability Pitfalls

Three mistakes are easy to make and hard to spot. They break the build on
Windows or on OpenCL but compile cleanly on Linux host:

1. **Host-only includes in shared headers.** Headers that are consumed by
   OpenCL kernels (the POD types, the RNG — see `core/types.h` and
   `core/rng.h`) must not include `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
   or any other host-only standard header. OpenCL C does not have them.
   Every such header carries a prologue comment flagging the constraint —
   respect it.

2. **Non-portable compiler flags.** `-Wall`, `-Wextra`, `-fsanitize=address`
   are GCC/Clang only. MSVC uses `/W4`, `/permissive-`, `/fsanitize=address`.
   Never add flags directly to a `CMakeLists.txt`; add them to
   `cmake/CompilerWarnings.cmake` or `cmake/Sanitizers.cmake` where the
   branching on `CMAKE_C_COMPILER_ID` already happens.

## Architecture

Monorepo under `packages/` with four packages and a strict acyclic dependency
graph:

```
core (static lib, libc only)
  ├── sim-gpu   (executable — OpenCL batch simulator)
  └── sim-stepper (executable — single-threaded CPU reference)
                    └── viz (future — consumes stepper trace)
```

**`core`** — all shared simulation logic: genome operators, neural network
compilation, sensor/action catalogues, challenge evaluation, portable xorshift64
RNG, snapshot serialization. Nothing about *how* the simulation is scheduled.

**`sim-gpu`** — OpenCL host orchestration + 5-kernel-per-step pipeline:
`feedforward.cl` → `movement.cl` → `grid_cleanup.cl` → `signal_fade.cl` → `challenges.cl`.
Kernel sources live in `packages/sim-gpu/kernels/` and are compiled at runtime
by the OpenCL driver (not the host compiler).

**`sim-stepper`** — deterministic single-threaded CPU reference. Processes
agents in increasing-index order with immediate effect application. Serves as
ground truth for the cross-simulator equivalence test.

The **cross-simulator equivalence test** in `sim-gpu/tests/` is the most
critical test: runs one step on both simulators from the same input and verifies
per-agent neuron outputs match within tolerance.

## Module Granularity

The design documents list **module responsibilities**, not file names.
`01-repository-structure.md` Sections 5, 6, and 7 describe what each package
must cover; the split into `.c` / `.h` files is decided at implementation
time, guided by cohesion rather than a fixed list.

Example: the `core` package must provide "genome operators (mutation,
crossover, fingerprint)". Whether this lives in a single `genome.c` or is
split into `genome_mutation.c` + `genome_crossover.c` + `genome_fingerprint.c`
is decided when the code is written — based on the size each module actually
reaches, not preemptively.

## GPU Data Model

Per-agent data uses **Structure of Arrays (SoA)** for memory coalescing. Genome
storage is transposed: `genome[gene_slot][agent_i]`, not `genome[agent_i][gene_j]`.

Movement conflict resolution: atomic CAS (no move queues). Death writes:
idempotent (no death queues). Generation boundary (reproduction, mutation,
spawn) stays on the host.

## Code Conventions (`docs/design/02-implementation-conventions.md`)

- **Files:** `snake_case.c`, `snake_case.h`
- **Public API:** `biosim_` prefix — e.g. `biosim_genome_mutate`
- **Types:** `biosim_*_t` — e.g. `biosim_coord_t`
- **OpenCL kernels entrypoints:** `k_` prefix — e.g. `k_feedforward`
- **Scope prefix:** `k_` prefix for OpenCL kernel entrypoints
  — e.g. `k_feedforward`. No other scope prefix(no `s_`, no `g_` etc.)
- **No mutable global state** in `core` — all state passed by parameter
- **Error handling:** functions return `biosim_status_t`; asserts only for
  invariants that indicate bugs
- **Host/device portability:** headers shared with OpenCL kernels (POD types,
  RNG) must compile as both C11 and OpenCL C — no `<stdio.h>`, `<stdlib.h>`,
  `<string.h>`. Mark such headers with a prologue comment.
- **CMake:** target-first (`target_*` commands only); no `include_directories()`
  or `add_definitions()` at top level

## Key Files

- `docs/design/01-repository-structure.md` — monorepo layout, package
  boundaries, build conventions, naming rules
- `docs/design/02-implementation-conventions.md` — code layout invariants
  (naming, headers, error handling, host/device portability)
- `docs/design/03-portable-build.md` — CMake/vcpkg setup, preset list,
  CI matrix, platform-specific concerns
- `docs/design/05-gpu-data-model.md` — SoA layout, kernel decomposition,
  conflict resolution strategy
- `docs/design/06-external-icd.md` — config file format and snapshot binary format

## Build Files

Do not read from build/ — it is a derived artifact and may not exist

## Third Party Files

Vendored libraries in third_party/ are read only, except when instructed to
bump the version
